#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "permutations.c"
#include "hash.c"
#include <time.h>
#define BILLION  1000000000
int main() {
    FILE *file = fopen("Input.txt", "r");
    if (file == NULL) {
        perror("Failed to open file");
        return 1;
    }

    char line[1024];
    char msg[1024];
    char md[1024];
    unsigned char output[CRYPTO_BYTES];

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "Count =", 7) == 0) {
            // Read Msg and MD lines
            fgets(msg, sizeof(msg), file);
            fgets(md, sizeof(md), file);

            // Extract message part from Msg line
            char *msgStart = strchr(msg, '=');
            char *mdStart = strchr(md, '=');
            if (msgStart && mdStart && strlen(msgStart) > 2 && strlen(mdStart) > 2) {
                msgStart += 2; // skip "= "
                mdStart += 2; // skip "= "

                // Convert hex string to bytes for the message
                int msgLen = (strlen(msgStart) - 1) / 2; // -1 to exclude newline character
                unsigned char msgBytes[msgLen];
                for (int i = 0; i < msgLen; i++) {
                    sscanf(msgStart + 2*i, "%2hhx", &msgBytes[i]);
                }
                // Before hashing
                // printf("Message bytes:\n");
                // for (int i = 0; i < msgLen; i++) {
                //     printf("%02x", msgBytes[i]);
                // }
                // printf("\n");

                // Hash the message
				struct timespec start_cpu, end_cpu;
				unsigned long long time_spent_cpu = 0;
                crypto_hash(output, msgBytes, msgLen);
				   clock_gettime(CLOCK_REALTIME, &start_cpu);
    for (int l = 0; l < BILLION; l++) {
		crypto_hash(output, msgBytes, msgLen);
    }
    clock_gettime(CLOCK_REALTIME, &end_cpu);

    time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + end_cpu.tv_nsec - start_cpu.tv_nsec;
    printf("CPU execution time of 131072 in milliseconds is %f\n", (double)time_spent_cpu / 1000000);

	clock_gettime(CLOCK_REALTIME, &start_cpu);
	
	time_spent_cpu = 0;
	for( int l = 0; l<196608;l++){
		crypto_hash(output, msgBytes, msgLen);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 196608 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<262144;l++){
		crypto_hash(output, msgBytes, msgLen);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 262144 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<327680;l++){
		crypto_hash(output, msgBytes, msgLen);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 327680 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity


	time_spent_cpu = 0;
	for( int l = 0; l<393216;l++){
		crypto_hash(output, msgBytes, msgLen);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 393216 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity
						
                printf("\nHash out = ");
                for (int i = 0; i < CRYPTO_BYTES; i++) {
                    if ((i %8)==0) printf(" ");
                    printf("%02x",output[i]);
                    
                }
                printf("\n");

                // Expected hash
                // printf("Expected hash:\n%.*s", CRYPTO_BYTES * 2, mdStart);
                // printf("\n");
                // Compare the output hash with expected hash
                int match = 1;
                for (int i = 0; i < CRYPTO_BYTES; i++) {
                    char expected[3];
                    snprintf(expected, 3, "%02x", output[i]);
                    if (strncasecmp(expected, mdStart + 2*i, 2) != 0) {
                        match = 0;
                        break;
                    }
                }

                // Print the result
                // printf("%s", line); // Print the Count
                // printf("Hash Match: %s\n", match ? "Yes" : "No");
            }
        }
    }

    fclose(file);
    return 0;
}
