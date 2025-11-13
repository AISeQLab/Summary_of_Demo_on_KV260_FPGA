#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#define UART_DEVICE "/dev/ttyUSB0"
#define BAUDRATE B115200

// Magic codes
#define WRD_SET_MODE 0xAAAAAAAA
#define UNSET_MODE   0xBBBBBBBB

// Command direction
#define WRITE_REG 1
#define READ_REG  0

// Modes
#define CONFIG_MODE 1

// Submodes for CONFIG_MODE
#define FREQ_CONFIG           1
#define BASE_NONCE_MSB_CONFIG 2
#define BASE_NONCE_LSB_CONFIG 3
#define RANGE_NONCE_CONFIG    4

// Readback codes
#define FREQ_READ       1
#define BASE_NONCE_READ 2
#define RANGE_NONCE_READ 3

//------------------------------------------------------------
// Calculate CRC5 for 27-bit data
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

//------------------------------------------------------------
// Send 32-bit data to UART (big-endian) byte-by-byte with delay
void send_uint32_uart(int fd, uint32_t value) {
    uint8_t buf[4];
    for (int i = 0; i < 4; i++) {
        buf[i] = (value >> (8 * (3 - i))) & 0xFF;
        // Write one byte at a time
        if (write(fd, &buf[i], 1) != 1) {
            perror("UART write failed");
            return;
        }
        usleep(87); // small delay between bytes
    }
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

//------------------------------------------------------------
// Format and send a UART command packet
void write_uart_command(int uart_fd, uint8_t is_write, uint8_t is_broadcast,
                        uint8_t mode, uint8_t submode, uint8_t is_msb,
                        uint8_t val1, uint8_t val2) {
    
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

//------------------------------------------------------------
// Configure and open UART
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
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 10;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &t);
    return fd;
}

//------------------------------------------------------------
// High-level command functions
void set_frequency(int fd, uint16_t freq) {
    uint8_t high = (freq >> 8) & 0xFF;
    uint8_t low  = freq & 0xFF;
	// Enable write mode
	send_uint32_uart(fd, WRD_SET_MODE);
    send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, WRITE_REG, 1, CONFIG_MODE, FREQ_CONFIG, 0, high, low);
	
	// Disable write mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
}

void set_base_nonce(int fd, uint8_t asic_id, uint32_t value) {
    uint8_t msb_msb = (value >> 24) & 0xFF;
    uint8_t msb_lsb = (value >> 16) & 0xFF;
    uint8_t lsb_msb = (value >> 8) & 0xFF;
    uint8_t lsb_lsb = (value >> 0) & 0xFF;
	// Enable write mode
	send_uint32_uart(fd, WRD_SET_MODE);
	send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 1, asic_id, msb_msb);
    write_uart_command(fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_MSB_CONFIG, 0, asic_id, msb_lsb);
    write_uart_command(fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 1, asic_id, lsb_msb);
    write_uart_command(fd, WRITE_REG, 0, CONFIG_MODE, BASE_NONCE_LSB_CONFIG, 0, asic_id, lsb_lsb);
	
	// Disable write mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
}

void set_range_nonce(int fd, uint32_t range) {
	
	// Enable write mode
	send_uint32_uart(fd, WRD_SET_MODE);
    send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, WRITE_REG, 1, CONFIG_MODE, RANGE_NONCE_CONFIG, 1, (range >> 24) & 0xFF, (range >> 16) & 0xFF);
    write_uart_command(fd, WRITE_REG, 1, CONFIG_MODE, RANGE_NONCE_CONFIG, 0, (range >> 8) & 0xFF, range & 0xFF);
	
	// Disable write mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
}

uint32_t read_frequency(int fd, uint8_t asic_id) {

	// Enable read mode
	send_uint32_uart(fd, WRD_SET_MODE);
    send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, READ_REG, 0, CONFIG_MODE, 0, 0, asic_id, FREQ_READ);
	
	// Disable read mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
	
    return read_uart(fd, 2);
}

uint32_t read_base_nonce(int fd, uint8_t asic_id) {
	
	// Enable read mode
	send_uint32_uart(fd, WRD_SET_MODE);
    send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, READ_REG, 0, CONFIG_MODE, 0, 0, asic_id, BASE_NONCE_READ);
	
	// Disable read mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
	
    return read_uart(fd, 4);
}

uint32_t read_range_nonce(int fd, uint8_t asic_id) {
	
	// Enable read mode
	send_uint32_uart(fd, WRD_SET_MODE);
    send_uint32_uart(fd, WRD_SET_MODE);
	
    write_uart_command(fd, READ_REG, 0, CONFIG_MODE, RANGE_NONCE_READ, 0, asic_id, RANGE_NONCE_READ);
	
	// Disable read mode
    send_uint32_uart(fd, UNSET_MODE);
    send_uint32_uart(fd, UNSET_MODE);
	
    return read_uart(fd, 4);
}

//------------------------------------------------------------
// Print usage information
void print_usage(const char *progname) {
    printf("Usage:\n");
    printf("  %s --set_frequency <freq>\n", progname);            // freq is 16-bit
    printf("  %s --set_base_nonce <asic_id> <value>\n", progname); // value is 32-bit
    printf("  %s --set_range_nonce <range>\n", progname);          // range is 32-bit
    printf("  %s --read_frequency <asic_id>\n", progname);
    printf("  %s --read_base_nonce <asic_id>\n", progname);
    printf("  %s --read_range_nonce <asic_id>\n", progname);
}

//------------------------------------------------------------
// Main entry
int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    int fd = setup_uart(UART_DEVICE);
	
    if (strcmp(argv[1], "--set_frequency") == 0 && argc >= 3) {
        uint16_t freq = (uint16_t)strtoul(argv[2], NULL, 0);
        set_frequency(fd, freq*10);
    }
    else if (strcmp(argv[1], "--set_base_nonce") == 0 && argc >= 4) {
        uint8_t asic_id = (uint8_t)strtoul(argv[2], NULL, 0);
        uint32_t value = (uint32_t)strtoul(argv[3], NULL, 0);
        set_base_nonce(fd, asic_id, value);
    }
    else if (strcmp(argv[1], "--set_range_nonce") == 0 && argc >= 3) {
        uint32_t range = (uint32_t)strtoul(argv[2], NULL, 0);
        set_range_nonce(fd, range);
    }
    else if (strcmp(argv[1], "--read_frequency") == 0 && argc >= 3) {
        uint8_t asic_id = (uint8_t)strtoul(argv[2], NULL, 0);
        uint32_t val = read_frequency(fd, asic_id);
        printf("%d\n", val/10);
    }
    else if (strcmp(argv[1], "--read_base_nonce") == 0 && argc >= 3) {
        uint8_t asic_id = (uint8_t)strtoul(argv[2], NULL, 0);
        uint32_t val = read_base_nonce(fd, asic_id);
        printf("0x%08X\n", val);
    }
    else if (strcmp(argv[1], "--read_range_nonce") == 0 && argc >= 3) {
        uint8_t asic_id = (uint8_t)strtoul(argv[2], NULL, 0);
        uint32_t val = read_range_nonce(fd, asic_id);
        printf("0x%08X\n", val);
    }
    else {
        printf("Invalid command or missing arguments.\n");
        print_usage(argv[0]);
    }

    close(fd);
    return 0;
}
