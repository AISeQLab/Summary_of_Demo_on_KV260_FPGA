#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <curl/curl.h>
#include <jansson.h>

#include "sha256.h"
#include "sha256_stage1.c"

#define UART_DEVICE "/dev/ttyUSB0"
#define BAUDRATE B115200

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

// ================= Send to UART =================
void sha256d_block_uart(int uart_fd,
                        const char *version,
                        const char *prevhash,
                        const char *merkleroot,
                        uint32_t time,
                        uint32_t bits,
                        uint32_t nonce) {

    WORD buf1[8], buf2[8], buf3[8];
    SHA256_CTX ctx1, ctx2, ctx3;

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

	for (int i = 0; i < 8; i++) {
		uint32_t word = buf1[i];
		for (int j = 0; j < 4; j++) {
			send_byte_uart(uart_fd, (word >> (8 * (3 - j))) & 0xFF);
			 }
	}

	for (int i = 0; i < 3; i++) {
		uint32_t word = W2_temp[i];
		for (int j = 0; j < 4; j++) {
			send_byte_uart(uart_fd, (word >> (8 * (3 - j))) & 0xFF);
		}
	}

	send_byte_uart(uart_fd, (bits >> 24) & 0xFF);
	send_byte_uart(uart_fd, (bits >> 16) & 0xFF);
	send_byte_uart(uart_fd, (bits >> 8) & 0xFF);
	send_byte_uart(uart_fd, (bits >> 0) & 0xFF);

	// Send 15 byte 0
	for (int i = 0; i < 15; i++) {
		send_byte_uart(uart_fd, 0);
	}

	// Send 1 byte 1
	send_byte_uart(uart_fd, 1);

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
			continue;
		}
		total += n;
	}

	if (total == num) {
		uint32_t response = (resp_buf[1] << 24) |
							(resp_buf[2] << 16) |
							(resp_buf[3] << 8)  |
							(resp_buf[4]);
		printf("Valid nonce = 0x%08X \n", response);
		fflush(stdout);
	} else {
		printf("UART response incomplete (received %d bytes)\n", total);
		fflush(stdout);
	}

	
}

//------------------------------------------------------------
// ================= Main =================
int main(int argc, char *argv[]) {
    int N = -1;
    int opt;
    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
            case 'n':
                N = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -n <N>\n", argv[0]);
                return 1;
        }
    }

    if (N <= 0) {
        fprintf(stderr, "Invalid or missing N value.\n");
        return 1;
    }

    // Setup UART
    int uart_fd = setup_uart(UART_DEVICE);

    // Fetch block info
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to init curl\n");
        return 1;
    }

    struct string s;
    char url[256], blockhash[65];

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

    // Write TEST FUNCTION
    write_uart_command(uart_fd, WRITE_REG, 1, MINING_MODE, 0, 0, 0x00, 0x00);
	
    // Disable read mode
    send_uint32_uart(uart_fd, UNSET_MODE);
    send_uint32_uart(uart_fd, UNSET_MODE);
    printf("Hash Board Verification!\n");
	fflush(stdout);
	for(int height = N; height >= 0; height--){
		
		init_string(&s);
		printf("height = %d,  ",height);
		snprintf(url, sizeof(url), "https://blockstream.info/api/block-height/%d", height);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
		if (curl_easy_perform(curl) != CURLE_OK) {
			fprintf(stderr, "Failed to get block hash\n");
			return 1;
		}
		strncpy(blockhash, s.ptr, 64);
		blockhash[64] = '\0';
		free(s.ptr);

		// Get block details
		init_string(&s);
		snprintf(url, sizeof(url), "https://blockstream.info/api/block/%s", blockhash);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
		if (curl_easy_perform(curl) != CURLE_OK) {
			fprintf(stderr, "Failed to get block info\n");
			return 1;
		}

		json_error_t err;
		json_t *root = json_loads(s.ptr, 0, &err);
		if (!root) {
			fprintf(stderr, "JSON parse error\n");
			return 1;
		}

		uint32_t version = (uint32_t)json_integer_value(json_object_get(root, "version"));
		char version_hex[9];
		snprintf(version_hex, sizeof(version_hex), "%08x", version);
		const char *prevhash = json_string_value(json_object_get(root, "previousblockhash"));
		const char *merkleroot = json_string_value(json_object_get(root, "merkle_root"));
		uint32_t time = (uint32_t)json_integer_value(json_object_get(root, "timestamp"));
		uint32_t bits = (uint32_t)json_integer_value(json_object_get(root, "bits"));
		uint32_t nonce = (uint32_t)json_integer_value(json_object_get(root, "nonce"));

		sha256d_block_uart(uart_fd, version_hex, prevhash, merkleroot, time, bits, nonce);
	
		json_decref(root);
		free(s.ptr);
	}
	curl_easy_cleanup(curl);
	
    close(uart_fd);
    return 0;
}