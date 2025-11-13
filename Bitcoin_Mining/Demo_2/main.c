#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <openssl/bn.h>
#include <cjson/cJSON.h>
#include <ctype.h>
#include <time.h>

#include "FPGA_Driver.c"  
#include "sha256.h"
#include "sha256_stage1.c"

#define BILLION  1000000000
#define RECV_BUF_SIZE 4096

#define CLR_ADDR_BASE_PHYS          (0x00000000 >> 2)
#define WDATA_BASE_PHYS             (0x00000004 >> 2)
#define RDATA_BASE_PHYS             (0x00000000 >> 2)
#define WORD uint32_t

uint32_t nbit_target;
bool new_job_available = false;
cJSON *latest_notify_json = NULL;

int last_nonce = -1;
char last_job_id[128] = "";
char last_processed_job_id[128] = ""; 

char *pool_host = NULL;
int pool_port = 0;
char *pool_user = NULL;
char *pool_pass = NULL;

void parse_pool_url(const char *url) {
    // Format: stratum+tcp://host:port
    const char *p = strstr(url, "://");
    if (!p) p = url;
    else p += 3;

    char *colon = strchr(p, ':');
    if (!colon) {
        fprintf(stderr, "Invalid URL format (missing port): %s\n", url);
        exit(1);
    }

    size_t host_len = colon - p;
    pool_host = (char*)malloc(host_len + 1);
    strncpy(pool_host, p, host_len);
    pool_host[host_len] = '\0';

    pool_port = atoi(colon + 1);
}

int hex_to_bytes(const char *hex, uint8_t *out, size_t out_size) {
    size_t len = strlen(hex);
    if (len % 2 != 0 || len / 2 > out_size) return -1;
    for (size_t i = 0; i < len / 2; i++) {
        sscanf(hex + 2*i, "%2hhx", &out[i]);
    }
    return (int)(len / 2);
}

int connect_to_pool(const char *host, int port) {
    struct hostent *server = gethostbyname(host);
    if (!server) return -1;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port) };
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    return connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0 ? -1 : sockfd;
}

void send_stratum(int sockfd, const char *msg) {
    send(sockfd, msg, strlen(msg), 0);
}

void send_subscribe(int sockfd) {
    char msg[256];
    snprintf(msg, sizeof(msg), "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"fpga-miner\"]}\n");
    send_stratum(sockfd, msg);
}

void send_authorize(int sockfd) {
    char msg[512];
    snprintf(msg, sizeof(msg), "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n", pool_user, pool_pass);
    send_stratum(sockfd, msg);
}

void send_suggest_diff(int sockfd, double diff) {
    char msg[128];
    snprintf(msg, sizeof(msg), "{\"id\":3,\"method\":\"mining.suggest_difficulty\",\"params\":[%.2f]}\n", diff);
    send_stratum(sockfd, msg);
}

uint32_t target_to_nbits(BIGNUM *bn_target) {
    int num_bytes = BN_num_bytes(bn_target);
    uint8_t buf[32] = {0};
    BN_bn2binpad(bn_target, buf, 32); 

    int i = 0;
    while (i < 32 && buf[i] == 0x00) i++; 

    int exponent = 32 - i;
    uint32_t nbits = 0;

    if (exponent <= 3) {
        uint32_t coeff = 0;
        for (int j = 0; j < 3; j++) {
            coeff <<= 8;
            if (i + j < 32) coeff |= buf[i + j];
        }
        nbits = (exponent << 24) | coeff;
    } else {
        uint32_t coeff = (buf[i] << 16) | (buf[i + 1] << 8) | buf[i + 2];
        nbits = (exponent << 24) | coeff;
    }

    return nbits;
}

// Utility function to print hex arrays
void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s = ", label);
	fflush(stdout);
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", data[i]);
		fflush(stdout);
    }
    printf("\n");
	fflush(stdout);
}

// Convert ASCII hex (e.g., "3230303030303030") to actual hex string (e.g., "20000000")
void ascii_hex_to_raw_hex(char *output, const char *input) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len / 2; ++i) {
        char buf[3] = { input[i * 2], input[i * 2 + 1], 0 };
        output[i] = (char)strtol(buf, NULL, 16);
    }
    output[len / 2] = '\0';
}

void handle_job_and_mine(const char *job_id, const char *prevhash_str_raw, const char *coinb1_str, const char *coinb2_str,
                         const cJSON *merkle_item, const char *version_str_raw, const char *nbits_str_raw, const char *ntime_str_raw,
                         const char *extranonce1, int extranonce2_size, BIGNUM *bn_target, int sockfd, uint32_t nbit_target) {
    if (strcmp(job_id, last_processed_job_id) == 0) {
        printf("[DEBUG] Job already processed. Skipping.\n");
		fflush(stdout);
        return;
    }

    char version_str[9], prevhash_str[65], nbits_str[9];
    ascii_hex_to_raw_hex(version_str, version_str_raw);
    ascii_hex_to_raw_hex(prevhash_str, prevhash_str_raw);
    ascii_hex_to_raw_hex(nbits_str, nbits_str_raw);

    uint8_t hash_temp[32], merkle_root[32], header[80], target_bytes[32];
    BIGNUM *bn_ctx_tmp = BN_new();
    BN_CTX *bnctx = BN_CTX_new();
    static uint32_t global_extranonce2 = 0;

    // printf("[DEBUG] nBits target = 0x%08x\n", nbit_target);

    int target_len = BN_num_bytes(bn_target);
    memset(target_bytes, 0, 32);
    BN_bn2bin(bn_target, target_bytes + (32 - target_len));

    // print_hex("[DEBUG] target_bytes", target_bytes, 32);

    bool job_completed = false;

    while (!job_completed) {
        // printf("[DEBUG] global_extranonce2 = %u\n", global_extranonce2);
        // printf("[DEBUG] extranonce2_size = %d\n", extranonce2_size);

        char extranonce2_hex[32];
        snprintf(extranonce2_hex, sizeof(extranonce2_hex), "%0*x", extranonce2_size * 2, global_extranonce2);

        char coinbase_hex[1024];
        snprintf(coinbase_hex, sizeof(coinbase_hex), "%s%s%s%s", coinb1_str, extranonce1, extranonce2_hex, coinb2_str);

        size_t coinbase_len = strlen(coinbase_hex) / 2;
        uint8_t *coinbase_bytes = malloc(coinbase_len);
        if (!coinbase_bytes) return;
        if (hex_to_bytes(coinbase_hex, coinbase_bytes, coinbase_len) != (int)coinbase_len) {
            fprintf(stderr, "[ERROR] Invalid coinbase hex format!\n");
            free(coinbase_bytes);
            return;
        }

        SHA256(coinbase_bytes, coinbase_len, hash_temp);
        SHA256(hash_temp, 32, merkle_root);
        free(coinbase_bytes);

        memcpy(hash_temp, merkle_root, 32);
        for (int i = 0; i < cJSON_GetArraySize(merkle_item); i++) {
            cJSON *branch = cJSON_GetArrayItem(merkle_item, i);
            if (!cJSON_IsString(branch)) return;
            uint8_t branch_bytes[32], concat[64];
            if (hex_to_bytes(branch->valuestring, branch_bytes, 32) != 32) return;
            memcpy(concat, hash_temp, 32);
            memcpy(concat + 32, branch_bytes, 32);
            SHA256(concat, 64, merkle_root);
            memcpy(hash_temp, merkle_root, 32);
        }

        uint8_t ver_bytes[4], prev_bytes[32], time_bytes[4], nbits_bytes[4];
        hex_to_bytes(version_str, ver_bytes, 4);
        hex_to_bytes(prevhash_str, prev_bytes, 32);
        hex_to_bytes(ntime_str_raw, time_bytes, 4);
        hex_to_bytes(nbits_str, nbits_bytes, 4);

        memcpy(header, (uint8_t[]){ver_bytes[3], ver_bytes[2], ver_bytes[1], ver_bytes[0]}, 4);
        for (int i = 0; i < 32; ++i) header[4+i] = prev_bytes[31-i];
        memcpy(header + 36, merkle_root, 32);
        memcpy(header + 68, (uint8_t[]){time_bytes[3], time_bytes[2], time_bytes[1], time_bytes[0]}, 4);
        memcpy(header + 72, (uint8_t[]){nbits_bytes[3], nbits_bytes[2], nbits_bytes[1], nbits_bytes[0]}, 4);
        memset(header + 76, 0x00, 4);

        char ntime_fixed[9];
        strncpy(ntime_fixed, ntime_str_raw, 8);
        ntime_fixed[8] = '\0';

        uint32_t W1[16], buf1[8];
        for (int i = 0; i < 16; i++) {
            W1[i] = ((uint32_t)header[i * 4 + 0] << 24) |
                    ((uint32_t)header[i * 4 + 1] << 16) |
                    ((uint32_t)header[i * 4 + 2] << 8) |
                    ((uint32_t)header[i * 4 + 3]);
        }
        SHA256_CTX2 ctx1;
        sha256_init_1(&ctx1);
        sha256_update_1(&ctx1, W1, buf1);
		
		struct timespec start_sha256d, end_sha256d;
		unsigned long long time_spent_sha256d = 0;
		clock_gettime(CLOCK_REALTIME, &start_sha256d);
		
        for (int i = 0; i < 8; i++) {
            uint32_t word = buf1[i];
            for (int j = 0; j < 4; j++) {
                *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (word >> (8 * (3 - j))) & 0xFF;
            }
        }

        for (int i = 0; i < 12; i++) *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = header[64 + i];

        uint32_t nonce_range = 0xFFFFFFFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >> 24) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >> 16) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >> 8) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >> 0) & 0xFF;

        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nbit_target >> 24) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nbit_target >> 16) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nbit_target >> 8) & 0xFF;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nbit_target >> 0) & 0xFF;

        for (int i = 0; i < 11; i++) *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = 0;
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = 1;

        uint32_t valid_nonce[4];
        while (1) {
            *(MY_IP_info.pio_32_mmap + CLR_ADDR_BASE_PHYS) = 1;
            int out_status = *(MY_IP_info.pio_32_mmap + RDATA_BASE_PHYS);

            if (out_status == 3) {
                for (int j = 0; j < 4; j++) valid_nonce[j] = *(MY_IP_info.pio_32_mmap + RDATA_BASE_PHYS);
                uint32_t nonce_val = (valid_nonce[3] << 24) | (valid_nonce[2] << 16) | (valid_nonce[1] << 8) | valid_nonce[0];

                printf("[DEBUG] nonce_val = 0x%08x\n", nonce_val);
				fflush(stdout);

                header[76] = (nonce_val >> 0) & 0xFF;
                header[77] = (nonce_val >> 8) & 0xFF;
                header[78] = (nonce_val >> 16) & 0xFF;
                header[79] = (nonce_val >> 24) & 0xFF;

                SHA256(header, 80, hash_temp);
                SHA256(hash_temp, 32, hash_temp);

                uint8_t hash_le[32];
                for (int i = 0; i < 32; i++) hash_le[i] = hash_temp[31 - i];

                print_hex("[DEBUG] block_hash_le", hash_le, 32);
                print_hex("[DEBUG] target_bytes ", target_bytes, 32);

                if (memcmp(hash_le, target_bytes, 32) > 0) {
                    // printf("[DEBUG] Invalid share: hash is higher than target!\n");
                    continue;
                }

                if (nonce_val == last_nonce && strcmp(job_id, last_job_id) == 0) {
                    // printf("[DEBUG] Duplicate share. Skipping submission.\n");
                } else {
                    last_nonce = nonce_val;
                    strncpy(last_job_id, job_id, sizeof(last_job_id));
                    last_job_id[sizeof(last_job_id) - 1] = '\0';

                    char nonce_hex[16], extranonce2_hex_sub[32];
                    snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce_val);
                    snprintf(extranonce2_hex_sub, sizeof(extranonce2_hex_sub), "%0*x", extranonce2_size * 2, global_extranonce2);

                    char submit_msg[256];
                    snprintf(submit_msg, sizeof(submit_msg),
                             "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]}\n",
                             pool_user, job_id, extranonce2_hex_sub, ntime_fixed, nonce_hex);

                    send_stratum(sockfd, submit_msg);
                }

                job_completed = true;
                break;
            } else if (out_status == 2) {
				clock_gettime(CLOCK_REALTIME, &end_sha256d);
				time_spent_sha256d = BILLION * (end_sha256d.tv_sec - start_sha256d.tv_sec) + (end_sha256d.tv_nsec - start_sha256d.tv_nsec) + time_spent_sha256d;
				printf("[DEBUG] No valid nonce found in 2^32 nonce values!\n");
				fflush(stdout);
                
				uint64_t total_hashes = 0xFFFFFFFF; // +1 to include both endpoints
				double hash_rate = ((double) total_hashes / time_spent_sha256d) * BILLION / 1e6;
				printf("======> Hash rate (Self-Calculating) is %.3f MHash/s\n", hash_rate); 
				fflush(stdout);
				
				global_extranonce2++;
                job_completed = true;
                break;
            }
        }
    }

    strncpy(last_processed_job_id, job_id, sizeof(last_processed_job_id));
    last_processed_job_id[sizeof(last_processed_job_id) - 1] = '\0';

    BN_free(bn_ctx_tmp);
    BN_CTX_free(bnctx);
}



void debug_job_id(const char *tag, const char *job_id) {
    printf("[DEBUG] %s job_id = ", tag);
	fflush(stdout);
    for (int i = 0; i < strlen(job_id); i++) {
        if (isprint(job_id[i])) putchar(job_id[i]);
        else {
			printf("%02x", (unsigned char)job_id[i]);
			fflush(stdout);
		}
    }
    printf("\n");
	fflush(stdout);
}


void process_json_line(cJSON *json, int sockfd, char *extranonce1, int *extranonce2_size, bool *subscribed, bool *authorized, BIGNUM *bn_target) {
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *method_item = cJSON_GetObjectItemCaseSensitive(json, "method");
    cJSON *result_item = cJSON_GetObjectItemCaseSensitive(json, "result");

    if (cJSON_IsNumber(id_item)) {
        int id = id_item->valueint;
        printf("[DEBUG] Received message with id = %d\n", id);
		fflush(stdout);
		fflush(stdout);

        if (id == 1 && result_item && cJSON_IsArray(result_item)) {
            cJSON *ex1 = cJSON_GetArrayItem(result_item, 1);
            cJSON *ex2 = cJSON_GetArrayItem(result_item, 2);
            if (cJSON_IsString(ex1) && cJSON_IsNumber(ex2)) {
                strncpy(extranonce1, ex1->valuestring, 31);
                *extranonce2_size = ex2->valueint;
                *subscribed = true;
                // printf("[DEBUG] Subscribed successfully. extranonce1 = %s, extranonce2_size = %d\n", extranonce1, *extranonce2_size);
                send_authorize(sockfd);
            }
        } else if (id == 2 && result_item) {
            *authorized = cJSON_IsTrue(result_item);
            printf("[DEBUG] Authorization response: %s\n", *authorized ? "SUCCESS" : "FAILURE");
			fflush(stdout);
        } else if (id == 4 && result_item) {
            if (cJSON_IsTrue(result_item)) {
                printf("[DEBUG] Share accepted by pool !!!\n");
				fflush(stdout);
            } else {
                printf("[DEBUG] Share rejected by pool !!!\n");
				fflush(stdout);
            }
        }
    }

    if (method_item && cJSON_IsString(method_item)) {
        const char *method = method_item->valuestring;
        printf("[DEBUG] Received method: %s\n", method);
		fflush(stdout);

        if (strcmp(method, "mining.set_difficulty") == 0) {
            cJSON *params = cJSON_GetObjectItemCaseSensitive(json, "params");
            if (cJSON_IsArray(params)) {
                cJSON *diff_val = cJSON_GetArrayItem(params, 0);
                if (cJSON_IsNumber(diff_val)) {
					double diff = diff_val->valuedouble;
					printf("[DEBUG] Pool difficulty set to %.8f\n", diff);
					fflush(stdout);
					BIGNUM *bn_base = BN_new();
					BN_hex2bn(&bn_base, "00000000FFFF0000000000000000000000000000000000000000000000000000"); // diff = 1

					BIGNUM *bn_diff = BN_new();
					BN_set_word(bn_diff, (unsigned long)diff);  // diff = 8192

					BN_CTX *ctx = BN_CTX_new();
					BN_div(bn_target, NULL, bn_base, bn_diff, ctx);  // target = base / diff

					nbit_target = target_to_nbits(bn_target);
					printf("[DEBUG] nbit_target = 0x%08x\n", nbit_target);
					fflush(stdout);
					
					uint8_t target_bytes[32];
					int target_len = BN_num_bytes(bn_target);
					memset(target_bytes, 0, 32);
					BN_bn2bin(bn_target, target_bytes + (32 - target_len));
					print_hex("[DEBUG] target_bytes ", target_bytes, 32);
					
				}
            }
        } else if (strcmp(method, "mining.notify") == 0) {
            if (!*authorized) {
                *authorized = true;
                printf("[DEBUG] No authorize response, assuming success.\n");
				fflush(stdout);
            }

            cJSON *params = cJSON_GetObjectItemCaseSensitive(json, "params");
            const char *job_id = cJSON_GetArrayItem(params, 0)->valuestring;
            debug_job_id("Received notify for", job_id);

            if (!new_job_available && strcmp(job_id, last_processed_job_id) != 0) {
                latest_notify_json = cJSON_Duplicate(json, 1);
                new_job_available = true;
                debug_job_id("Accepted new job", job_id);
            } else {
                debug_job_id("Ignored job (already processed)", job_id);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "o:u:p:")) != -1) {
        switch (opt) {
            case 'o':
                parse_pool_url(optarg);
                break;
            case 'u':
                pool_user = strdup(optarg);
                break;
            case 'p':
                pool_pass = strdup(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -o <pool_url> -u <user> -p <pass>\n", argv[0]);
                return 1;
        }
    }

    if (!pool_host || !pool_user || !pool_pass || pool_port == 0) {
        fprintf(stderr, "Missing required arguments\n");
        return 1;
    }
	
    if (fpga_open() != 1) return 1;
    fpga.dma_ctrl = MY_IP_info.dma_mmap;

    int sockfd = connect_to_pool(pool_host, pool_port);
    if (sockfd < 0) {
        fprintf(stderr, "Can't connect to pool\n");
        return 1;
    }
    printf("Connected to %s:%d\n", pool_host, pool_port);
	fflush(stdout);
	
    send_subscribe(sockfd);
    send_suggest_diff(sockfd, 0.01);

    char buf[RECV_BUF_SIZE] = {0};
    int buf_len = 0;
    bool subscribed = false, authorized = false;
    char extranonce1[32] = {0};
    int extranonce2_size = 0;
    BIGNUM *bn_target = BN_new();

    while (true) {
        int n = recv(sockfd, buf + buf_len, RECV_BUF_SIZE - buf_len - 1, 0);
        if (n <= 0) break;
        buf_len += n;
        buf[buf_len] = '\0';

        char *line = strtok(buf, "\n");
        while (line) {
            cJSON *json = cJSON_Parse(line);
            if (json) {
                process_json_line(json, sockfd, extranonce1, &extranonce2_size, &subscribed, &authorized, bn_target);
                cJSON_Delete(json);
            }
            line = strtok(NULL, "\n");
        }
        buf_len = 0;

        if (new_job_available && latest_notify_json) {
            cJSON *params = cJSON_GetObjectItemCaseSensitive(latest_notify_json, "params");
            if (cJSON_IsArray(params)) {
                const char *job_id      = cJSON_GetArrayItem(params, 0)->valuestring;
                const char *prevhash    = cJSON_GetArrayItem(params, 1)->valuestring;
                const char *coinb1      = cJSON_GetArrayItem(params, 2)->valuestring;
                const char *coinb2      = cJSON_GetArrayItem(params, 3)->valuestring;
                cJSON *merkle_branch    = cJSON_GetArrayItem(params, 4);
                const char *version     = cJSON_GetArrayItem(params, 5)->valuestring;
                const char *nbits       = cJSON_GetArrayItem(params, 6)->valuestring;
                const char *ntime       = cJSON_GetArrayItem(params, 7)->valuestring;

                new_job_available = false;

                cJSON *copied = latest_notify_json;
                latest_notify_json = NULL;

                handle_job_and_mine(job_id, prevhash, coinb1, coinb2, merkle_branch,
                                    version, nbits, ntime, extranonce1,
                                    extranonce2_size, bn_target, sockfd, nbit_target);
									
                cJSON_Delete(copied);
            }
        }
    }

    close(sockfd);
    BN_free(bn_target);
	free(pool_host);
    free(pool_user);
    free(pool_pass);
    return 0;
}

