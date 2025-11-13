#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
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
#include <sys/time.h>

#include "sha256.h"
#include "sha256_stage1.c"

#define UART_DEVICE "/dev/ttyUSB0"
#define BAUDRATE B115200

#define RECV_BUF_SIZE 4096

// Magic codes
#define WRD_SET_MODE  0xAAAAAAAA
#define UNSET_MODE    0xBBBBBBBB

// Command direction
#define WRITE_REG     1
#define READ_REG      0

// Modes
#define IDLE_MODE     0
#define CONFIG_MODE   1
#define TEST_MODE     2
#define MINING_MODE   3

// Submodes
#define IDLE_CONFIG            0
#define FREQ_CONFIG            1
#define BASE_NONCE_MSB_CONFIG  2
#define BASE_NONCE_LSB_CONFIG  3
#define RANGE_NONCE_CONFIG     4

#define IDLE_TEST     0
#define FUNCTION_TEST 1

// Readback
#define ASICID_READ     0
#define FREQ_READ       1
#define BASE_NONCE_READ 2
#define RANGE_NONCE_READ 3

double timeout_sec;
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

//------------------------------------------------------------

int byte_send_counter = 0;
// ================= UART Functions =================
// Configure and open UART device
int setup_uart(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    struct termios t;
    tcgetattr(fd, &t);

    cfsetospeed(&t, BAUDRATE);
    cfsetispeed(&t, BAUDRATE);

    t.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    t.c_iflag = IGNPAR;
    t.c_oflag = 0;
    t.c_lflag = 0;

    t.c_cc[VMIN]  = 0;   // read returns even if no data
    t.c_cc[VTIME] = 10;  // timeout = 1.0 seconds :contentReference[oaicite:1]{index=1}

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &t);

    return fd;
}

// Read data from UART with specified length
uint32_t read_uart(int fd, int length) {
    uint8_t buf[length];
    int total = 0;
    while (total < length) {
        int n = read(fd, buf + total, length - total);
        usleep(87);
        if (n <= 0) {
            perror("ERROR: UART read failed or timed out!");
            return 0xFFFFFFFF;
        }
        total += n;
    }
    if (length == 4)
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    else if (length == 2)
        return (buf[0] << 8) | buf[1];
    else
        return 0;
}

void send_uint32_uart(int uart_fd, uint32_t value) {
    uint8_t buf[4];
    for (int i = 0; i < 4; i++) {
        buf[i] = (value >> (8 * (3 - i))) & 0xFF;
    }
    write(uart_fd, buf, 4);
}

// Read 4 bytes (Big Endian) from UART and return as uint32_t
uint32_t read_uint32_uart(int uart_fd, int length) {
    uint8_t buf[length];
    int total = 0;
    while (total < length) {
        int n = read(uart_fd, buf + total, length - total);
        if (n <= 0) {
            perror("UART read failed or timed out");
            return 0xFFFFFFFF;
        }
        total += n;
    }
	if(length == 4)
		return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	else if (length == 2)
		return (buf[0] << 8) | buf[1];
	else return 0;
}

// ================= CRC5 =================
uint8_t crc5_usb(uint32_t data27) {
    uint8_t crc = 0;
    for (int i = 26; i >= 0; i--) {
        uint8_t bit = (data27 >> i) & 1;
        uint8_t fb = ((crc >> 4) & 1) ^ bit;
        crc = (crc << 1) & 0x1F;
        if (fb) crc ^= 0x05;
    }
    return crc;
}

// ================= Command Packet =================
void write_uart_command(int uart_fd,
                        uint8_t is_write,
                        uint8_t is_broadcast,
                        uint8_t mode,
                        uint8_t submode,
                        uint8_t is_msb,
                        uint8_t val1,
                        uint8_t val2) {
    uint32_t cmd = 0;
    cmd |= ((uint32_t)is_write     & 0x1) << 31;
    cmd |= ((uint32_t)is_broadcast & 0x1) << 30;
    cmd |= ((uint32_t)mode         & 0x3) << 28;
    cmd |= ((uint32_t)submode      & 0x7) << 25;
    cmd |= ((uint32_t)is_msb       & 0x1) << 24;
    cmd |= ((uint32_t)val1         & 0xFF) << 16;
    cmd |= ((uint32_t)val2         & 0xFF) << 8;

    uint32_t data27 = cmd >> 5;
    uint8_t crc = crc5_usb(data27);
    cmd |= (crc & 0x1F);

    send_uint32_uart(uart_fd, cmd);
}

void send_byte_uart(int uart_fd, uint8_t data) {
    ssize_t written = write(uart_fd, &data, 1);
    if (written == 1) {
    } else {
        perror("UART write failed");
    }
}



// ================= Helper Functions =================
struct string {
    char *ptr;
    size_t len;
};

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(1);
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

uint32_t LitToBigEndian(uint32_t x) {
    return (((x >> 24) & 0xFF) |
            ((x >> 8) & 0xFF00) |
            ((x << 8) & 0xFF0000) |
            ((x << 24) & 0xFF000000));
}

void hexstr_to_le_uint32_array(const char *hexstr, uint32_t *out) {
    char temp[9]; temp[8] = '\0';
    for (int i = 0; i < 8; i++) {
        memcpy(temp, hexstr + i * 8, 8);
        out[7 - i] = LitToBigEndian((uint32_t)strtoul(temp, NULL, 16));
    }
}


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
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", data[i]);
    }
    printf("\n");
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

void handle_job_and_mine(int uart_fd, const char *job_id, const char *prevhash_str_raw, const char *coinb1_str, const char *coinb2_str,
                         const cJSON *merkle_item, const char *version_str_raw, const char *nbits_str_raw, const char *ntime_str_raw,
                         const char *extranonce1, int extranonce2_size, BIGNUM *bn_target, int sockfd, uint32_t nbit_target) {
    // If this job was already processed, skip it
    if (strcmp(job_id, last_processed_job_id) == 0) {
        printf("[DEBUG] Job already processed. Skipping.\n");
		fflush(stdout);
        return;
    }
    // New job: reset extranonce2 counter
    static uint32_t global_extranonce2 = 0;
    global_extranonce2 = 0;
    
    // Convert ASCII hex fields to raw bytes (little-endian where needed)
    char version_str[9], prevhash_str[65], nbits_str[9];
    ascii_hex_to_raw_hex(version_str, version_str_raw);
    ascii_hex_to_raw_hex(prevhash_str, prevhash_str_raw);
    ascii_hex_to_raw_hex(nbits_str, nbits_str_raw);
    
    uint8_t hash_temp[32], merkle_root[32], header[80], target_bytes[32];
    BIGNUM *bn_ctx_tmp = BN_new();
    BN_CTX *bnctx = BN_CTX_new();
    
    // Prepare target as 32-byte big-endian for comparison
    int target_len = BN_num_bytes(bn_target);
    memset(target_bytes, 0, 32);
    BN_bn2bin(bn_target, target_bytes + (32 - target_len));
    
    bool job_done = false;
    // Loop over extranonce2 until a valid nonce is found or job becomes stale
    while (!job_done) {
        // Format extranonce2 as hex string with required length (pad with zeros)
        char extranonce2_hex[32];
        snprintf(extranonce2_hex, sizeof(extranonce2_hex), "%0*x", extranonce2_size * 2, global_extranonce2);
        
        // Build full coinbase hex string: coinb1 + extranonce1 + extranonce2 + coinb2
        char coinbase_hex[1024];
        snprintf(coinbase_hex, sizeof(coinbase_hex), "%s%s%s%s", coinb1_str, extranonce1, extranonce2_hex, coinb2_str);
        
        // Convert coinbase hex to bytes and hash it (double SHA256)
        size_t coinbase_len = strlen(coinbase_hex) / 2;
        uint8_t *coinbase_bytes = (uint8_t *)malloc(coinbase_len);
        if (!coinbase_bytes) return;
        if (hex_to_bytes(coinbase_hex, coinbase_bytes, coinbase_len) != (int)coinbase_len) {
            fprintf(stderr, "[ERROR] Invalid coinbase hex format!\n");
            free(coinbase_bytes);
            return;
        }
        SHA256(coinbase_bytes, coinbase_len, hash_temp);
        SHA256(hash_temp, 32, merkle_root);
        free(coinbase_bytes);
        
        // Compute merkle root by hashing with each branch from merkle_item
        memcpy(hash_temp, merkle_root, 32);
        for (int i = 0; i < cJSON_GetArraySize(merkle_item); i++) {
            cJSON *branch = cJSON_GetArrayItem(merkle_item, i);
            if (!cJSON_IsString(branch)) return;
            uint8_t branch_bytes[32], concat[64];
            if (hex_to_bytes(branch->valuestring, branch_bytes, 32) != 32) return;
            // concatenate current hash (hash_temp) and this branch, then double-hash
            memcpy(concat, hash_temp, 32);
            memcpy(concat + 32, branch_bytes, 32);
            SHA256(concat, 64, merkle_root);
            SHA256(merkle_root, 32, merkle_root);
            // Copy result into hash_temp for next iteration
            memcpy(hash_temp, merkle_root, 32);
        }
        
        // Build the 80-byte block header:
        // version (4 bytes LE), prevhash (32 bytes LE), merkle root (32 bytes LE), ntime (4 bytes LE), nbits (4 bytes LE), nonce (4 bytes LE initially 0)
        uint8_t ver_bytes[4], prev_bytes[32], time_bytes[4], nbits_bytes[4];
        hex_to_bytes(version_str, ver_bytes, 4);
        hex_to_bytes(prevhash_str, prev_bytes, 32);
        hex_to_bytes(ntime_str_raw, time_bytes, 4);
        hex_to_bytes(nbits_str, nbits_bytes, 4);
        // Version
        header[0] = ver_bytes[3]; header[1] = ver_bytes[2]; header[2] = ver_bytes[1]; header[3] = ver_bytes[0];
        // Previous block hash (reverse byte order)
        for (int i = 0; i < 32; ++i) {
            header[4 + i] = prev_bytes[31 - i];
        }
        // Merkle root (reverse byte order)
        for (int i = 0; i < 32; ++i) {
            header[36 + i] = merkle_root[31 - i];
        }
        // Timestamp
        header[68] = time_bytes[3]; header[69] = time_bytes[2];
        header[70] = time_bytes[1]; header[71] = time_bytes[0];
        // Bits
        header[72] = nbits_bytes[3]; header[73] = nbits_bytes[2];
        header[74] = nbits_bytes[1]; header[75] = nbits_bytes[0];
        // Nonce (will be set during search)
        header[76] = 0x00; header[77] = 0x00;
        header[78] = 0x00; header[79] = 0x00;
        
        // Prepare midstate (SHA-256 state after processing first 64 bytes of header)
        uint32_t W1[16], buf1[8];
        for (int i = 0; i < 16; i++) {
            W1[i] = ((uint32_t)header[i*4 + 0] << 24) |
                    ((uint32_t)header[i*4 + 1] << 16) |
                    ((uint32_t)header[i*4 + 2] << 8)  |
                    ((uint32_t)header[i*4 + 3]);
        }
        SHA256_CTX2 ctx1;
        sha256_init_1(&ctx1);
        sha256_update_1(&ctx1, W1, buf1);
        
        // Load midstate and remaining header data into mining hardware (FPGA/ASIC)
        for (int i = 0; i < 8; i++) {
            uint32_t word = buf1[i];
            // Push each 32-bit word of midstate to hardware (big-endian order)
			send_byte_uart(uart_fd, (word >> 24) & 0xFF);
			send_byte_uart(uart_fd, (word >> 16) & 0xFF);
			send_byte_uart(uart_fd, (word >> 8)  & 0xFF);
			send_byte_uart(uart_fd, (word >> 0)  & 0xFF);
        }
        // Provide the remaining 12 bytes of header (merkle root last 4 + nTime + nBits) to hardware
        for (int i = 64; i < 76; i++) {
			send_byte_uart(uart_fd, header[i]);
        }
        
        // Provide target (in compact nBits form) to hardware
		send_byte_uart(uart_fd, (nbit_target >> 24) & 0xFF);
		send_byte_uart(uart_fd, (nbit_target >> 16) & 0xFF);
		send_byte_uart(uart_fd, (nbit_target >> 8) & 0xFF);
		send_byte_uart(uart_fd, (nbit_target >> 0) & 0xFF);
        // Fill the rest of the input buffer and trigger hardware mining start
        for (int i = 0; i < 15; i++) send_byte_uart(uart_fd, 0);
        // Send 1 byte 1
		send_byte_uart(uart_fd, 1);
        
        
		struct timeval start, now;
		gettimeofday(&start, NULL);

		int num = 5;
		uint8_t resp_buf[num];
		int total = 0;
		
		while (total < num) {
			int n = read(uart_fd, resp_buf + total, num - total);
			if (n < 0) {
				perror("UART read error");
				break;
			} else if (n == 0) {
				usleep(87);
				gettimeofday(&now, NULL);
				double elapsed = (now.tv_sec - start.tv_sec) +
								 (now.tv_usec - start.tv_usec) / 1000000.0;

				if (elapsed > timeout_sec) {
					break;
				}
				continue;
			}
			total += n;
		}
		
        if (total == num) {
			uint32_t nonce_val = (resp_buf[1] << 24) |
								(resp_buf[2] << 16) |
								(resp_buf[3] << 8)  |
								(resp_buf[4]);
								
			printf("[DEBUG] Found nonce = 0x%08x\n", nonce_val);
			fflush(stdout);
			// Place the found nonce into the header (little-endian)
			header[76] = (nonce_val >> 0) & 0xFF;
			header[77] = (nonce_val >> 8) & 0xFF;
			header[78] = (nonce_val >> 16) & 0xFF;
			header[79] = (nonce_val >> 24) & 0xFF;
			// Double SHA-256 the full header to get the block hash
			SHA256(header, 80, hash_temp);
			SHA256(hash_temp, 32, hash_temp);
			// Convert hash to little-endian for comparison
			uint8_t hash_le[32];
			for (int i = 0; i < 32; i++) {
				hash_le[i] = hash_temp[31 - i];
			}
			// Check if hash <= target (memcmp on big-endian byte arrays)
			if (memcmp(hash_le, target_bytes, 32) > 0) {
				// Hash is higher than target, invalid solution – continue searching
				continue;
			}
			// Avoid duplicate submissions for the same job/nonce
			if (nonce_val == last_nonce && strcmp(job_id, last_job_id) == 0) {
			printf("[DEBUG] Duplicate share found, not submitting.\n");
			fflush(stdout);
			} else {
				last_nonce = nonce_val;
				strncpy(last_job_id, job_id, sizeof(last_job_id));
				last_job_id[sizeof(last_job_id) - 1] = '\0';
				// Prepare submission message with hex strings
				char nonce_hex[16], extranonce2_hex_sub[32];
				snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce_val);
				snprintf(extranonce2_hex_sub, sizeof(extranonce2_hex_sub), "%0*x", extranonce2_size * 2, global_extranonce2);
				char submit_msg[256];
				snprintf(submit_msg, sizeof(submit_msg),
						"{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]}\n",
						pool_user, job_id, extranonce2_hex_sub, ntime_str_raw, nonce_hex);
				send_stratum(sockfd, submit_msg);
			}
			// Found a valid share for this job; mark job as done
			job_done = true;
        } 
		else {  // Searched full 2^32 nonce range, no result
			// Log hashrate (optional debug)
			printf("[DEBUG] No valid nonce found in 2^32 range for extranonce2 = %u\n", global_extranonce2);
			fflush(stdout);
            // Increment extranonce2 and break out to try again in outer loop
            global_extranonce2++;
        }
          // If out_status indicates "working" or partial status, the loop continues polling
        
        // If we broke out of inner loop with a found share, `job_done` will be true.
        // If we broke out due to nonce range exhaustion, job_done remains false and loop continues with extranonce2++.
    } // end while (!job_done)
    
    // Mark this job as processed to avoid duplication
    strncpy(last_processed_job_id, job_id, sizeof(last_processed_job_id));
    last_processed_job_id[sizeof(last_processed_job_id) - 1] = '\0';
    
    BN_free(bn_ctx_tmp);
    BN_CTX_free(bnctx);
}




void debug_job_id(const char *tag, const char *job_id) {
    printf("[DEBUG] %s job_id = ", tag);
	fflush(stdout);
    for (int i = 0; i < strlen(job_id); i++) {
        if (isprint(job_id[i])) 
			putchar(job_id[i]);
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
        if (id == 1 && result_item && cJSON_IsArray(result_item)) {
            cJSON *ex1 = cJSON_GetArrayItem(result_item, 1);
            cJSON *ex2 = cJSON_GetArrayItem(result_item, 2);
            if (cJSON_IsString(ex1) && cJSON_IsNumber(ex2)) {
                strncpy(extranonce1, ex1->valuestring, 31);
                *extranonce2_size = ex2->valueint;
                *subscribed = true;
                printf("[DEBUG] Subscribed successfully. extranonce1 = %s, extranonce2_size = %d\n", extranonce1, *extranonce2_size);
				fflush(stdout);
                send_authorize(sockfd);
            }
        } else if (id == 2 && result_item) {
            *authorized = cJSON_IsTrue(result_item);
            printf("[DEBUG] Authorization response: %s\n", *authorized ? "SUCCESS" : "FAILURE");
			fflush(stdout);
        } else if (id == 4 && result_item) {
            if (cJSON_IsTrue(result_item)) {
                printf("[DEBUG] Share accepted by pool !!!!!!!!!!!!\n");
				fflush(stdout);
            } else {
                printf("[DEBUG] Share rejected by pool !!!!!!!!!!!!\n");
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
					if(diff < 1){
						printf("[DEBUG] Pool difficulty set to %.8f\n", diff);
						fflush(stdout);
						BIGNUM *bn_base = BN_new();
						BN_set_word(bn_base, 0xFFFF);
						BN_lshift(bn_base, bn_base, 8 * (0x1d - 3));

						BIGNUM *bn_target_calc = BN_new();
						BN_CTX *ctx = BN_CTX_new();

						double inv_diff = 1.0 / diff;
						char inv_diff_str[64];
						snprintf(inv_diff_str, sizeof(inv_diff_str), "%.8f", inv_diff);

						BIGNUM *bn_inv_diff = BN_new();
						BN_dec2bn(&bn_inv_diff, inv_diff_str);

						BN_mul(bn_target_calc, bn_base, bn_inv_diff, ctx);

						BN_copy(bn_target, bn_target_calc);
						nbit_target = target_to_nbits(bn_target);
						printf("[DEBUG] nbit_target = 0x%08x\n", nbit_target);

						BN_free(bn_base);
						BN_free(bn_inv_diff);
						BN_free(bn_target_calc);
						BN_CTX_free(ctx);
					}
					else{
					
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
						
						uint8_t target_bytes[32];
						int target_len = BN_num_bytes(bn_target);
						memset(target_bytes, 0, 32);
						BN_bn2bin(bn_target, target_bytes + (32 - target_len));
						print_hex("[DEBUG] target_bytes ", target_bytes, 32);
					}
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
	uint32_t freq;
	// Setup UART
    int uart_fd = setup_uart(UART_DEVICE);
	
    // Enable write mode
    send_uint32_uart(uart_fd, WRD_SET_MODE);
    send_uint32_uart(uart_fd, WRD_SET_MODE);
	
    // Write BASE_NONCE_MSB_CONFIG ASIC 0
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 1, 0, 0);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 0, 0, 0);

    // Write BASE_NONCE_LSB_CONFIG ASIC 0
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 1, 0, 0);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 0, 0, 0);

    // Write BASE_NONCE_MSB_CONFIG ASIC 1
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 1, 1, 0x55);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 0, 1, 0x55);

    // Write BASE_NONCE_LSB_CONFIG ASIC 1
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 1, 1, 0x55);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 0, 1, 0x56);

    // Write BASE_NONCE_MSB_CONFIG ASIC 2
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 1, 2, 0xAA);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 0, 2, 0xAA);

    // Write BASE_NONCE_LSB_CONFIG ASIC 2
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 1, 2, 0xAA);
    write_uart_command(uart_fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 0, 2, 0xAC);
	
    // Write RANGE_NONCE_CONFIG broadcast 2AAA AAAB
    write_uart_command(uart_fd, WRITE_REG, 1, CONFIG_MODE, RANGE_NONCE_CONFIG, 1, 0x2A, 0xAA);
    write_uart_command(uart_fd, WRITE_REG, 1, CONFIG_MODE, RANGE_NONCE_CONFIG, 0, 0xAA, 0xAB);

	write_uart_command(uart_fd, READ_REG, 0, CONFIG_MODE, 0, 0, 0, FREQ_READ);
	freq = read_uart(uart_fd, 2);
	
    // Write TEST FUNCTION
    write_uart_command(uart_fd, WRITE_REG, 1, MINING_MODE, 0, 0, 0x00, 0x00);
	
    // Disable read mode
    send_uint32_uart(uart_fd, UNSET_MODE);
    send_uint32_uart(uart_fd, UNSET_MODE);

   // Calculate execution time for mining session
    printf("[DEBUG] ASIC chips are mining at %d MHz\n", freq/10);
	fflush(stdout);
    timeout_sec = (double)(0x2AAAAAAB + 256) / (double)(freq*100000);
	
	
    if (!pool_host || !pool_user || !pool_pass || pool_port == 0) {
        fprintf(stderr, "Missing required arguments\n");
        return 1;
    }
	
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

                handle_job_and_mine(uart_fd, job_id, prevhash, coinb1, coinb2, merkle_branch,
                                    version, nbits, ntime, extranonce1,
                                    extranonce2_size, bn_target, sockfd, nbit_target);
									
                cJSON_Delete(copied);
            }
        }
    }
	
	close(uart_fd);
    close(sockfd);
    BN_free(bn_target);
	free(pool_host);
    free(pool_user);
    free(pool_pass);
    return 0;
}

