#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <curl/curl.h>
#include <jansson.h>
#include <time.h>

#include <fcntl.h>
#include <math.h>

#include "./FPGA_Driver.c"
#include "sha256.h"
#include "sha256_stage1.c"
#include "sha256_stage2.c"
#include "sha256_stage3.c"

/// Write channel addresses
#define CLR_ADDR_BASE_PHYS          (0x00000000 >> 2)

#define WDATA_BASE_PHYS			    (0x00000004 >> 2) 
/// Read channel addresses
#define RDATA_BASE_PHYS             (0x00000000 >> 2)

#define WORD uint32_t
uint32_t case_num = 0, correct_case_num = 0;
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
    return (((x >> 24) & 0xff) |
            ((x >> 8) & 0xff00) |
            ((x << 8) & 0xff0000) |
            ((x << 24) & 0xff000000));
}

void hexstr_to_le_uint32_array(const char *hexstr, uint32_t *out) {
    char temp[9]; temp[8] = '\0';
    for (int i = 0; i < 8; i++) {
        memcpy(temp, hexstr + i * 8, 8);
        out[7 - i] = LitToBigEndian((uint32_t)strtoul(temp, NULL, 16));
    }
}

void nBits_to_Target(uint32_t nBits, uint32_t target[8]) {
    uint8_t exponent = nBits >> 24;
    uint32_t mantissa = nBits & 0x007fffff;

    memset(target, 0, 8 * sizeof(uint32_t));
    uint8_t temp[32] = {0};

    if (exponent <= 32 && exponent >= 3) {
        int mantissa_pos = 32 - exponent;  // byte offset
        temp[mantissa_pos]     = (mantissa >> 16) & 0xff;
        temp[mantissa_pos + 1] = (mantissa >> 8) & 0xff;
        temp[mantissa_pos + 2] = mantissa & 0xff;
    } else if (exponent < 3) {
        uint32_t shifted_mantissa = mantissa >> (8 * (3 - exponent));
        temp[29] = (shifted_mantissa >> 16) & 0xff;
        temp[30] = (shifted_mantissa >> 8) & 0xff;
        temp[31] = shifted_mantissa & 0xff;
    } else {
        return;
    }

    // Fill target[0..7] as big-endian 256-bit integer (Bitcoin style)
    for (int i = 0; i < 8; i++) {
        target[7 - i] = ((uint32_t)temp[4 * i] << 24) |
                        ((uint32_t)temp[4 * i + 1] << 16) |
                        ((uint32_t)temp[4 * i + 2] << 8) |
                        ((uint32_t)temp[4 * i + 3]);
    }
}

int compare_uint256(const uint32_t a[8], const uint32_t b[8]) {
    for (int i = 7; i >= 0; i--) { // MSB → LSB
        if (a[i] < b[i]) return -1;  // a < b
        if (a[i] > b[i]) return 1;   // a > b
    }
    return 0; // a == b
}

void sha256d_block(const char *version, const char *prevhash, const char *merkleroot,
                   uint32_t time, uint32_t bits, uint32_t nonce, int height) {
    WORD buf1[8], buf2[8], buf3[8];
    SHA256_CTX ctx1, ctx2, ctx3;
	case_num++;
    uint32_t version_le = LitToBigEndian((uint32_t)strtoul(version, NULL, 16));
    uint32_t time_le = LitToBigEndian(time);
    uint32_t bits_le = LitToBigEndian(bits);
    uint32_t nonce_le = LitToBigEndian(nonce);

    uint32_t prev_hash_le[8], merkle_root_le[8];
    hexstr_to_le_uint32_array(prevhash, prev_hash_le);
    hexstr_to_le_uint32_array(merkleroot, merkle_root_le);

    WORD W1[16] = {
        version_le,
        prev_hash_le[0], prev_hash_le[1], prev_hash_le[2], prev_hash_le[3],
        prev_hash_le[4], prev_hash_le[5], prev_hash_le[6], prev_hash_le[7],
        merkle_root_le[0], merkle_root_le[1], merkle_root_le[2], merkle_root_le[3],
        merkle_root_le[4], merkle_root_le[5], merkle_root_le[6]
    };

    sha256_init_1(&ctx1);
    sha256_update_1(&ctx1, W1, buf1);
	
	WORD W2_temp[16] = {
				merkle_root_le[7], time_le, bits_le, 0,
				0x80000000, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0x00000280
			};
			
	*(MY_IP_info.pio_32_mmap + CLR_ADDR_BASE_PHYS)  = 1;
	
	for (int i = 0; i < 8; i++) {
        uint32_t word = buf1[i];
        for (int j = 0; j < 4; j++) {
            *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (word >> (8 * (3-j))) & 0xFF;
        }
    }
	for (int i = 0; i < 3; i++) {
        uint32_t word = W2_temp[i];
        for (int j = 0; j < 4; j++) {
            *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (word >> (8 * (3-j))) & 0xFF;
        }
    }
	
	uint32_t nonce_range = 0xFFFFFFFF;
    *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >>24) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >>16) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >>8) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (nonce_range >>0) & 0xFF;
	
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (bits >>24) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (bits >>16) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (bits >>8) & 0xFF;
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = (bits >>0) & 0xFF;
		
	for (int i = 0; i < 11; i++) {
        *(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = 0;
    }
	
	*(MY_IP_info.pio_32_mmap + WDATA_BASE_PHYS) = 1;

	uint32_t valid_nonce[4];
	int out_status;
	while (1) {
		*(MY_IP_info.pio_32_mmap + CLR_ADDR_BASE_PHYS)  = 1;
		out_status = *(MY_IP_info.pio_32_mmap + RDATA_BASE_PHYS);
		if (out_status == 2) {
			printf("Valid nonce is NOT found!!! ");
			fflush(stdout);
			break;
		}
		else if(out_status == 3) {
			for (int j = 0; j < 4; j++) {
            valid_nonce[j] = *(MY_IP_info.pio_32_mmap + RDATA_BASE_PHYS);
            }
			uint32_t nonce_found = (valid_nonce[3] << 24) |
                       (valid_nonce[2] << 16) |
                       (valid_nonce[1] << 8)  |
                       (valid_nonce[0] << 0);
			if (nonce_found == nonce) {
				printf("Found nonce = %08x matched with mined nonce!!! ", nonce_found);
				fflush(stdout);
				correct_case_num++;
			} else {
				WORD W2[16] = {
					merkle_root_le[7], time_le, bits_le, ((valid_nonce[0]<<24)|(valid_nonce[1]<<16)|(valid_nonce[2]<<8)|(valid_nonce[3]<<0)),
					0x80000000, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, 0x00000280
				};

				sha256_init_2(&ctx2, buf1);
				sha256_update_1(&ctx2, W2, buf2);

				sha256_init_3(&ctx3);
				sha256_update_3(&ctx3, buf2, buf3);

				int smaller_than_target = 1;
				uint32_t target[8];
				
				nBits_to_Target(bits, target);
				
				int target_non_zero = 0;
				
				int r = compare_uint256(target , buf3);

				if (r > 0) {
					printf("Found nonce =%08x (alternative nonce)!!! ",(valid_nonce[3]<<24)|(valid_nonce[2]<<16)|(valid_nonce[1]<<8)|(valid_nonce[0]<<0));
					fflush(stdout);
					correct_case_num++;
				} else {
					printf("Valid nonce is NOT found!!! ");
					fflush(stdout);
				}
		}
		break;
		}
    }
}


int main(int argc, char *argv[]) {
    int N = -1;  // default height (invalid)
    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
            case 'n':
                N = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -n <height>\n", argv[0]);
                return 1;
        }
    }

    if (N <= 0) {
        fprintf(stderr, "Invalid or missing height value.\n");
        return 1;
    }
	
	printf("=============================================================\n");
	fflush(stdout);
	printf("We verify correct nonce detection from block %d backward.\n", N);
	fflush(stdout);
	printf("=============================================================\n");
	fflush(stdout);
	
    unsigned char* membase;
    if (fpga_open() == 0)
        exit(1);

    fpga.dma_ctrl = MY_IP_info.dma_mmap;
    membase = (unsigned char*)MY_IP_info.ddr_mmap;

    struct string s;
    CURL *curl = curl_easy_init();  // Only once

    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        return 1;
    }

    char url[256], blockhash[65];
	
	
	
    for (int height = N; height >= 1; height--) {
		
		printf("Verifying Block %d... ", height);
		fflush(stdout);
        init_string(&s);  
		
        snprintf(url, sizeof(url), "https://blockstream.info/api/block-height/%d", height);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        if (curl_easy_perform(curl) != CURLE_OK) {
            free(s.ptr);
            continue;
        }

        strncpy(blockhash, s.ptr, 64);
        blockhash[64] = '\0';
        free(s.ptr);

        init_string(&s);
        snprintf(url, sizeof(url), "https://blockstream.info/api/block/%s", blockhash);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        if (curl_easy_perform(curl) != CURLE_OK) {
            free(s.ptr);
            continue;
        }

        json_error_t error;
        json_t *root = json_loads(s.ptr, 0, &error);
        if (!root) {
            free(s.ptr);
            continue;
        }

        uint32_t version_int = (uint32_t)json_integer_value(json_object_get(root, "version"));
        char version_hex[9];
        snprintf(version_hex, sizeof(version_hex), "%08x", version_int);

        const char *prevhash = json_string_value(json_object_get(root, "previousblockhash"));
        const char *merkleroot = json_string_value(json_object_get(root, "merkle_root"));
        uint32_t timestamp = (uint32_t)json_integer_value(json_object_get(root, "timestamp"));
        uint32_t bits = (uint32_t)json_integer_value(json_object_get(root, "bits"));
        uint32_t nonce = (uint32_t)json_integer_value(json_object_get(root, "nonce"));

        sha256d_block(version_hex, prevhash, merkleroot, timestamp, bits, nonce, height);
		printf("Corrected Case = %d/%d => Accuracy = %.0f%%\n", correct_case_num, case_num,
       ((float)correct_case_num / (float)case_num) * 100);
	   fflush(stdout);
		json_decref(root);
        free(s.ptr);
    }

    curl_easy_cleanup(curl);
    return 0;
}
