
#include <string.h>
#include <stdio.h>
#include "sm3.h"
#include <time.h>

#define BILLION  1000000000

int main( int argc, char *argv[] )
{
	unsigned char *input = "abc";
	int ilen = 3;
	unsigned char output[32];
	int i;
	sm3_context ctx;

	printf("Message:\n");
	printf("%s\n",input);

	sm3(input, ilen, output);
	
		struct timespec start_cpu, end_cpu;
	unsigned long long time_spent_cpu = 0;
	 unsigned char ciphertext[16];
    int len;
    
    clock_gettime(CLOCK_REALTIME, &start_cpu);
    for (int l = 0; l < 131072; l++) {
		sm3(input, ilen, output);
    }
    clock_gettime(CLOCK_REALTIME, &end_cpu);

    time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + end_cpu.tv_nsec - start_cpu.tv_nsec;
    printf("CPU execution time of 131072 in milliseconds is %f\n", (double)time_spent_cpu / 1000000);

	clock_gettime(CLOCK_REALTIME, &start_cpu);
	
	time_spent_cpu = 0;
	for( int l = 0; l<196608;l++){
		sm3(input, ilen, output);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 196608 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<262144;l++){
		sm3(input, ilen, output);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 262144 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<327680;l++){
		sm3(input, ilen, output);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 327680 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity


	time_spent_cpu = 0;
	for( int l = 0; l<393216;l++){
		sm3(input, ilen, output);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 393216 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity
						
						
	printf("Hash:\n   ");
	for(i=0; i<32; i++)
	{
		printf("%02x",output[i]);
		if (((i+1) % 4 ) == 0) printf(" ");
	} 
	printf("\n");

	// printf("Message:\n");
	// for(i=0; i < 16; i++)
	// 	printf("abcd");
	// printf("\n");

    // sm3_starts( &ctx );
	// for(i=0; i < 16; i++)
	// 	sm3_update( &ctx, "abcd", 4 );
    // sm3_finish( &ctx, output );
    // memset( &ctx, 0, sizeof( sm3_context ) );
	
	// printf("Hash:\n   ");
	// for(i=0; i<32; i++)
	// {
	// 	printf("%02x",output[i]);
	// 	if (((i+1) % 4 ) == 0) printf(" ");
	// }   
	// printf("\n");
    //getch();	//VS2008 
}