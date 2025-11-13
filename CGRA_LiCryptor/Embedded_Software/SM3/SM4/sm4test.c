/*
 * SM4/SMS4 algorithm test programme
 * 2012-4-21
 */

#include <string.h>
#include <stdio.h>
#include "sm4.c"
#include <time.h>

#define BILLION  1000000000
int main()
{
	unsigned char key[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
	unsigned char input[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
	unsigned char output[16];
	sm4_context ctx;
	unsigned long i;

	//encrypt standard testing vector
	sm4_setkey_enc(&ctx,key);
	sm4_crypt_ecb(&ctx,1,16,input,output);
	for(i=0;i<16;i++)
		printf("%02x ", output[i]);
	printf("\n");

	//decrypt testing
	sm4_setkey_dec(&ctx,key);
	sm4_crypt_ecb(&ctx,0,16,output,output);
	for(i=0;i<16;i++)
		printf("%02x ", output[i]);
	printf("\n");

	//decrypt 1M times testing vector based on standards.
	i = 0;
	sm4_setkey_enc(&ctx,key);
	// while (i<1000000) 
    // {
		sm4_crypt_ecb(&ctx,1,16,input,input);
		// i++;
    // }
	
			struct timespec start_cpu, end_cpu;
	unsigned long long time_spent_cpu = 0;
	 unsigned char ciphertext[16];
    int len;
    
    clock_gettime(CLOCK_REALTIME, &start_cpu);
    for (int l = 0; l < BILLION; l++) {
		sm4_crypt_ecb(&ctx,1,16,input,input);
    }
    clock_gettime(CLOCK_REALTIME, &end_cpu);

    time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + end_cpu.tv_nsec - start_cpu.tv_nsec;
    printf("CPU execution time of 131072 in milliseconds is %f\n", (double)time_spent_cpu / 1000000);

	clock_gettime(CLOCK_REALTIME, &start_cpu);
	
	time_spent_cpu = 0;
	for( int l = 0; l<196608;l++){
		sm4_crypt_ecb(&ctx,1,16,input,input);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 196608 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<262144;l++){
		sm4_crypt_ecb(&ctx,1,16,input,input);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 262144 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<327680;l++){
		sm4_crypt_ecb(&ctx,1,16,input,input);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 327680 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity


	time_spent_cpu = 0;
	for( int l = 0; l<393216;l++){
		sm4_crypt_ecb(&ctx,1,16,input,input);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 393216 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity
						
						
	for(i=0;i<16;i++)
		printf("%02x ", input[i]);
	printf("\n");
	
    return 0;
}
