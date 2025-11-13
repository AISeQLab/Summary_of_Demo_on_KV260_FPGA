#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "../CGRA.h"
#include "../CGRAlib.c"
#define BILLION  1000000000
#define N 4


#define RIGHT_ROTATE_64(value, shift) ((value >> shift) | (value << (64 - shift)))

typedef struct {
  U32 e;
  U32 o;
} word_t;

typedef struct {
  word_t x0, x1, x2, x3, x4;
} state_t;

int main(){
	int k,i,j, LOOP;
	#if defined(ARMZYNQ)
	unsigned char* membase;
	if (cgra_open() == ((void *)0))
		exit(1);
	#endif
	#if defined(VIVADOSIL)
	CGRA_info.Config_File = fopen(Config_File_Name,"w");
	CGRA_info.LDM_File = fopen(LDM_File_Name,"w");
	#endif
	
	int Message_Length = 5;
	Init();
	LOOP = 9;
	U64 OP;

		// PE_out =  (U32 **) calloc((LOOP*16)*16,sizeof(U32));
	// for (i = 0; i < (LOOP*16)*16; i ++){
		// PE_out[i] = (U32 *)calloc(8,sizeof(U32));
	// }
	
	U32 data[10] = {0x11112224, 0x22223333, 0x33334444, 0x44445555, 0x55556666, 0x66667777, 0x77778888, 0x88889999, 0x9999aaaa, 0xaaaabbbb};
	
	U32 message_in[10] = {0xf2a1b0e9, 0x8c7d654a, 0x3b9e8e7a, 0x8f6045d2, 0xc1b3a2d9, 0xf8e7c6a5, 0xb4d3c2e1, 0xf0a91857, 0xf2a1b0ef, 0x8e7c6a52};
	U32 digest_in[10]  = {0x01470194, 0xfc6528a6, 0x738ec38a, 0xc0adffa7, 0x2ec8e329, 0x6c76384c, 0xd6f6a54d, 0x7f52377d, 0xa13c42a2, 0x23be8d87};
	U32 cons[6] = {0x3, 0x6, 0x9, 0xC, 0x0, 0x0};
	
	#if defined(ARMZYNQ)

	
	cgra.dma_ctrl = CGRA_info.dma_mmap;
	membase = CGRA_info.ddr_mmap;
	
	printf("membase: %llx\n", (U64)membase);
	
	U32 *A;
	A = (U64*)(membase + ROW0_BASE_PHYS);
	
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			if((j%2==0)){
				A[i*8 +j] = data[j+1];
			}
			else{
				A[i*8 +j] = data[j-1];
			}
		}
	}
  
	U32 *B;
	B = (U64*)(membase + ROW1_BASE_PHYS);
	
	for (i=0; i<8; i++) {
		for (j=0; j<2; j++) {
			if((j%2==0)){
				B[i*8 +j] = data[8+j+1];
			}
			else{
				B[i*8 +j] = data[8+j-1];
			}
		}
		for (j=2; j<8; j++) {
			if((j%2==0)){
				B[i*8 +j] = cons[j+1-2];
			}
			else{
				B[i*8 +j] = cons[j-1-2];
			}
		}
	}	
	#elif defined(VIVADOSIL)
	for (i = 0; i < (LOOP*2-1)*16; i ++){	
		for (j = 0; j < 8; j ++){
			PE_out[i][j] = (i%4)*8+j;
		}
	}

		for(k = 0; k < LOOP; k++){
		printf ("-----------------------------------------------------------------------------------------------------------------------\n");
		printf ("|   LOOP %d \n",k);
		printf ("-----------------------------------------------------------------------------------------------------------------------\n");
		for (i = 0; i < NUM_PE; i++){
			printf ("PE_out[%d] =",i);
			for (j = 0; j < 8; j++){
				printf (" %08x",PE_out[i+k*NUM_PE][j]);
			}
			printf ("\n");
			if( ((i+1) % 4) == 0){
				printf ("-----------------------------------------------------------------------------------------------------------------------\n");
			}
		}
		}
	#endif

	state_t s;

	// Initialize the state_t variable s
	s.x0.e = 0x11112222;
	s.x0.o = 0x22223333;
	s.x1.e = 0x33334444;
	s.x1.o = 0x44445555;
	s.x2.e = 0x55556666;
	s.x2.o = 0x66667777;
	s.x3.e = 0x77778888;
	s.x3.o = 0x88889999;
	s.x4.e = 0x9999aaaa;
	s.x4.o = 0xaaaabbbb;

	
	U32 RC0 = 0x3;
	U32 RC1 = 0x6;
	U32 RC2 = 0x9;
	U32 RC3 = 0xC;
	
	//set data for local memory 0 (LMM 0)
	LMM[0][0][0] = message_in[1];
	LMM[0][1][0] = message_in[0];
	
	//set data for local memory 1 (LMM 1)
	LMM[1][0][0] = message_in[3];
	LMM[1][1][0] = message_in[2];

	//set data for local memory 2 (LMM 2)
	LMM[2][0][0] = message_in[5];
	LMM[2][1][0] = message_in[4];
	
	//set data for local memory 3 (LMM 3)
	LMM[3][0][0] = message_in[7];
	LMM[3][1][0] = message_in[6];
	
	//set data for local memory 4 (LMM 4)
	LMM[4][0][0] = message_in[9];
	LMM[4][1][0] = message_in[8];
	
	//set data for local memory 5 (LMM 0)
	LMM[5][0][0] = digest_in[1];
	LMM[5][1][0] = digest_in[0];
	
	//set data for local memory 6 (LMM 1)
	LMM[6][0][0] = digest_in[3];
	LMM[6][1][0] = digest_in[2];

	//set data for local memory 7 (LMM 2)
	LMM[7][0][0] = digest_in[5];
	LMM[7][1][0] = digest_in[4];
	
	//set data for local memory 8 (LMM 3)
	LMM[8][0][0] = digest_in[7];
	LMM[8][1][0] = digest_in[6];
	
	//set data for local memory 9 (LMM 4)
	LMM[9][0][0] = digest_in[9];
	LMM[9][1][0] = digest_in[8];
	
	static const U32 Ce_PE[8] = {
		5, 5, 5, 5,
		6, 6, 6, 6
	};
	
	static const U32 Ce_Index[8] = {
		1, 0, 1, 0,
		1, 0, 1, 0
	};
	
	static const U32 Co_PE[8] = {
		6, 6, 6, 6,
		5, 5, 5, 5
	};
	
	static const U32 Co_Index[8] = {
		1, 1, 0, 0,
		1, 1, 0, 0
	};	
	
	
	static const U32 RC_8[8] = {
		0xb4, 0xa5, 0x96, 0x87,
		0x78, 0x69, 0x5a, 0x4b
	};
	#if defined(ARMZYNQ)

	#elif defined(VIVADOSIL)
		CGRA_info.LDM_Offset = 0;
		
		for (i = 0; i < 8; i ++){	
			fprintf(CGRA_info.LDM_File,"%08x_%08x%08x%08x%08x%08x%08x%08x%08x\n",ROW0_BASE_PHYS+CGRA_info.LDM_Offset,LMM[3][0][0],LMM[3][1][0],LMM[2][0][0],LMM[2][1][0],LMM[1][0][0],LMM[1][1][0],LMM[0][0][0],LMM[0][1][0]);
			CGRA_info.LDM_Offset = CGRA_info.LDM_Offset+32;
		}

		CGRA_info.LDM_Offset = 0;
		
		for (i = 0; i < 8; i ++){	
			fprintf(CGRA_info.LDM_File,"%08x_%08x%08x%08x%08x%08x%08x%08x%08x\n",ROW1_BASE_PHYS+CGRA_info.LDM_Offset,LMM[7][0][0],LMM[7][1][0],LMM[6][0][0],LMM[6][1][0],LMM[5][0][0],LMM[5][1][0],LMM[4][0][0],LMM[4][1][0]);
			CGRA_info.LDM_Offset = CGRA_info.LDM_Offset+32;
		}

		CGRA_info.LDM_Offset = 0;
		
		// for (i = 0; i < 8; i ++){	
			// fprintf(CGRA_info.LDM_File,"%08x_%08x%08x%08x%08x%08x%08x%08x%08x\n",ROW2_BASE_PHYS+CGRA_info.LDM_Offset,LMM[11][0][0],LMM[11][1][0],LMM[10][0][0],LMM[10][1][0],LMM[9][0][0],LMM[9][1][0],LMM[8][0][0],LMM[8][1][0]);
			// CGRA_info.LDM_Offset = CGRA_info.LDM_Offset+32;
		// }
		
		// CGRA_info.LDM_Offset = 0;		
	#endif
	
	struct timespec start_compile, end_compile;
	unsigned long long time_spent_compile = 0;
	clock_gettime(CLOCK_REALTIME, &start_compile);
	for( k = 0; k < Message_Length; k++){
		for (j = 0; j < LOOP; j++){
			if( j == 0){
				if(k == 0){
					i = j;
					///*** row 0 ***///
					mop(OP_LDW, 0,  PE_out[0],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					exe(OP_NOP, 0,PE_out[1], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					exe(OP_NOP, 0,PE_out[2], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					exe(OP_NOP, 0,PE_out[3], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					///*** row 1 ***///
					exe(OP_NOP, 0,PE_out[4],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[0][1], PE_out[0][0], 0, 0);
					mop(OP_LDW, 0,  PE_out[5],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					mop(OP_LDW, 0,  PE_out[6],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					mop(OP_LDW, 0,  PE_out[7],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					///*** row 2 ***///
					mop(OP_LDW, 0,  PE_out[8],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[4][4], PE_out[4][5], 0,0);
					mop(OP_LDW, 0,  PE_out[9],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[5][1], PE_out[5][0], 0, 0);
					exe(OP_NOP, 0,PE_out[10], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[6][1], PE_out[6][0], 0, 0);
					exe(OP_NOP, 0,PE_out[11], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[7][1], PE_out[7][0], 0, 0);
					///*** row 3 ***///
					OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
					exe(OP,  0,PE_out[12], /*ALU in*/ PE_out[8][4],PE_out[9][4],PE_out[9][5],PE_out[8][5], /*BUFF 8to1 in*/ PE_out[8][1], PE_out[8][0], 0,0);
					OP = BASIC_OP32(/*LU1*/OP_XOR,/*LU2*/OP_NOP, /*SRU1*/OP_NOP,/*SRU1_IM*/OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/OP_NOP,/*LU3*/OP_NOP,/*OP_IM*/RC_8[j]);
					exe(OP,  0,PE_out[13], /*ALU in*/ PE_out[11][5],0,PE_out[11][4],0, /*BUFF 8to1 in*/ PE_out[9][4], PE_out[9][5], PE_out[9][1], PE_out[9][0]);
					OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
					exe(OP,  0,PE_out[14], /*ALU in*/ PE_out[9][1],PE_out[8][1], PE_out[8][0],PE_out[9][0], /*BUFF 8to1 in*/ PE_out[10][4], PE_out[10][5], 0,0);
					exe(OP_NOP, 0,PE_out[15], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[11][4], PE_out[11][5], 0, 0);
				}
				else if(k > 0){
					i = j;
					if( k == 1) {
						///*** row 0 ***///
						exe(OP_NOP, 0,PE_out[0], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						mop(OP_LDW, 0,  PE_out[1],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);					
						exe(OP_NOP, 0,PE_out[2], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[3], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						///*** row 1 ***///
						exe(OP_NOP, 0,PE_out[4],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[1][1], PE_out[1][0], 0, 0);
					}
					else if( k == 2) {
						///*** row 0 ***///
						exe(OP_NOP, 0,PE_out[0], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[1], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						mop(OP_LDW, 0,  PE_out[2],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);								
						exe(OP_NOP, 0,PE_out[3], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						///*** row 1 ***///
						exe(OP_NOP, 0,PE_out[4],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[2][1], PE_out[2][0], 0, 0);
					}
					else if( k == 3) {
						///*** row 0 ***///
						exe(OP_NOP, 0,PE_out[0], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[1], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[2], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						mop(OP_LDW, 0,  PE_out[3],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);			
						///*** row 1 ***///
						exe(OP_NOP, 0,PE_out[4],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[3][1], PE_out[3][0], 0, 0);
					}
					else if( k == 4) {
						///*** row 0 ***///
						exe(OP_NOP, 0,PE_out[0], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[1], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[2], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						exe(OP_NOP, 0,PE_out[3], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
						
						///*** row 1 ***///
						mop(OP_LDW, 0,  PE_out[4],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);			
					}
					mop(OP_LDW, 0,  PE_out[5],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					mop(OP_LDW, 0,  PE_out[6],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					mop(OP_LDW, 0,  PE_out[7],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
					///*** row 2 ***///
					
					mop(OP_LDW, 0,  PE_out[8],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[4][4], PE_out[4][5], 0,0);
					mop(OP_LDW, 0,  PE_out[9],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[5][1], PE_out[5][0], 0, 0);
					exe(OP_NOP, 0,PE_out[10], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[6][1], PE_out[6][0], 0, 0);
					exe(OP_NOP, 0,PE_out[11], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[7][1], PE_out[7][0], 0, 0);
					///*** row 3 ***///
					OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
					exe(OP,  0,PE_out[12], /*ALU in*/ PE_out[8][4],PE_out[9][4],PE_out[9][5],PE_out[8][5], /*BUFF 8to1 in*/ PE_out[8][1], PE_out[8][0], 0,0);
					OP = BASIC_OP32(/*LU1*/OP_XOR,/*LU2*/OP_NOP, /*SRU1*/OP_NOP,/*SRU1_IM*/OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/OP_NOP,/*LU3*/OP_NOP,/*OP_IM*/RC_8[j]);
					exe(OP,  0,PE_out[13], /*ALU in*/ PE_out[11][5],0,PE_out[11][4],0, /*BUFF 8to1 in*/ PE_out[9][4], PE_out[9][5], PE_out[9][1], PE_out[9][0]);
					OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
					exe(OP,  0,PE_out[14], /*ALU in*/ PE_out[9][1],PE_out[8][1], PE_out[8][0],PE_out[9][0], /*BUFF 8to1 in*/ PE_out[10][4], PE_out[10][5], 0,0);
					exe(OP_NOP, 0,PE_out[15], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[11][4], PE_out[11][5], 0, 0);
				}
			}
			else if ( j >= 1){	
				i = j*2 -1;
				///*** row 0 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[0], /*ALU in*/ PE_out[12][0],PE_out[13][6],PE_out[13][7],PE_out[12][2], /*BUFF 8to1 in*/ PE_out[12][4], PE_out[12][5], PE_out[12][0], PE_out[12][2]);
				exe(OP,  0,PE_out[1], /*ALU in*/ PE_out[13][2],PE_out[14][4],PE_out[14][5],PE_out[13][0], /*BUFF 8to1 in*/ PE_out[13][0], PE_out[13][2], PE_out[13][6], PE_out[13][7]);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
				exe(OP,  0,PE_out[2], /*ALU in*/ PE_out[12][4],PE_out[14][0],PE_out[14][2],PE_out[12][5], /*BUFF 8to1 in*/ PE_out[14][4], PE_out[14][5], PE_out[14][0], PE_out[14][2]);
				exe(OP_NOP, 0,PE_out[3], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[15][4], PE_out[15][5],0, 0);
				///*** row 1 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
				exe(OP,  0,PE_out[4], /*ALU in*/ PE_out[1][4],PE_out[0][4],PE_out[0][5],PE_out[1][5], /*BUFF 8to1 in*/ PE_out[0][4], PE_out[0][5], PE_out[0][0], PE_out[0][2]);
				exe(OP,  0,PE_out[5], /*ALU in*/ PE_out[2][4],PE_out[1][0],PE_out[1][2],PE_out[2][5], /*BUFF 8to1 in*/ PE_out[1][0], PE_out[1][2], PE_out[1][6],PE_out[1][7]);
				exe(OP,  0,PE_out[6], /*ALU in*/ PE_out[0][0],PE_out[2][4],PE_out[2][5],PE_out[0][2], /*BUFF 8to1 in*/ PE_out[2][4],PE_out[2][5], PE_out[2][6],PE_out[2][7]);
				exe(OP,  0,PE_out[7], /*ALU in*/ PE_out[1][0],PE_out[0][4],PE_out[0][5],PE_out[1][2], /*BUFF 8to1 in*/ PE_out[3][0],PE_out[3][2], PE_out[2][0],PE_out[2][2]);
				///*** row 2 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
				exe(OP,  0,PE_out[8], /*ALU in*/ PE_out[6][6],PE_out[4][6],PE_out[4][7],PE_out[6][7], /*BUFF 8to1 in*/ PE_out[4][4],PE_out[4][5], 0, 0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);			
				exe(OP,  0,PE_out[9], /*ALU in*/ PE_out[5][0],PE_out[4][6],PE_out[4][7],PE_out[5][2], /*BUFF 8to1 in*/ 0, 0, 0, 0);
				exe(OP,  0,PE_out[10], /*ALU in*/ PE_out[7][6],PE_out[5][4],PE_out[5][5],PE_out[7][7], /*BUFF 8to1 in*/ PE_out[6][4],PE_out[6][5], 0, 0);
				exe(OP,  0,PE_out[11], /*ALU in*/ PE_out[6][0],PE_out[6][6],PE_out[6][7],PE_out[6][2], /*BUFF 8to1 in*/ PE_out[7][0],PE_out[7][2], 0, 0);
				///*** row 3 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);		
				exe(OP,  0,PE_out[12], /*ALU in*/ PE_out[11][4],PE_out[10][4],PE_out[10][5],PE_out[11][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);			
				exe(OP,  0,PE_out[13], /*ALU in*/ PE_out[8][0],PE_out[8][4],PE_out[8][5],PE_out[8][2], /*BUFF 8to1 in*/ PE_out[9][0],PE_out[9][2], 0, 0);
				exe(OP,  0,PE_out[14], /*ALU in*/ PE_out[9][0],PE_out[11][0],PE_out[11][2],PE_out[9][2], /*BUFF 8to1 in*/ PE_out[10][0],PE_out[10][2], 0, 0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/5, /*LU*/OP_XOR);	
				exe(OP,  0,PE_out[15], /*ALU in*/ PE_out[10][0], PE_out[10][0],PE_out[10][2], PE_out[10][2], /*BUFF 8to1 in*/ PE_out[11][0],PE_out[11][2], 0, 0);
				
				i = j*2 +0;
				///*** row 0 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[0], /*ALU in*/ PE_out[13][4],PE_out[12][0],PE_out[12][2],PE_out[13][5], /*BUFF 8to1 in*/ 0,0,0,0);	
				exe(OP,  0,PE_out[1], /*ALU in*/ PE_out[14][4],PE_out[13][0],PE_out[13][2],PE_out[14][5], /*BUFF 8to1 in*/ 0,0,0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/34, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[2], /*ALU in*/ PE_out[15][4],PE_out[15][4],PE_out[15][5],PE_out[15][5], /*BUFF 8to1 in*/ PE_out[14][4],PE_out[14][5],PE_out[14][0],PE_out[14][2]);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/9, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[3], /*ALU in*/ PE_out[14][0],PE_out[14][0],PE_out[14][2],PE_out[14][2], /*BUFF 8to1 in*/ PE_out[15][4],PE_out[15][5],PE_out[15][0],PE_out[15][2]);
				///*** row 1 ***///
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/7, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[4], /*ALU in*/ PE_out[1][0],PE_out[1][0],PE_out[1][2],PE_out[1][2], /*BUFF 8to1 in*/ PE_out[0][0],PE_out[0][2],0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/22, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[5], /*ALU in*/ PE_out[0][0],PE_out[0][0],PE_out[0][2],PE_out[0][2], /*BUFF 8to1 in*/ PE_out[1][0],PE_out[1][2],0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/1, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[6], /*ALU in*/ PE_out[2][4],PE_out[3][6],PE_out[3][7],PE_out[2][5], /*BUFF 8to1 in*/ 0,0,PE_out[2][6],PE_out[2][7]);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/7, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[7], /*ALU in*/ PE_out[3][4],PE_out[2][0],PE_out[2][2],PE_out[3][5], /*BUFF 8to1 in*/ PE_out[3][0],PE_out[3][2],0,0);
				///*** row 2 ***///			
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/10, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[8], /*ALU in*/ PE_out[5][4],PE_out[4][0],PE_out[4][2],PE_out[5][5], /*BUFF 8to1 in*/ 0,0,0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/19, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[9], /*ALU in*/ PE_out[6][6],PE_out[7][4],PE_out[7][5],PE_out[6][7], /*BUFF 8to1 in*/ 0,0,0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_ROR,/*SRU_IM*/39, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[10], /*ALU in*/ PE_out[4][4],PE_out[5][0],PE_out[5][2],PE_out[4][5], /*BUFF 8to1 in*/ 0,0,0,0);
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT);
				exe(OP,  0,PE_out[11], /*ALU in*/ PE_out[6][0],0,0,PE_out[6][2], /*BUFF 8to1 in*/ PE_out[7][0],PE_out[7][2],0,0);
				///*** row 3 ***///
				
				exe(OP_NOP, 0,PE_out[12], /*ALU in*/ PE_out[9][0],0,PE_out[9][2],0, /*BUFF 8to1 in*/ PE_out[8][0], PE_out[8][2], 0,0);			
				OP = BASIC_OP32(/*LU1*/OP_XOR,/*LU2*/OP_NOP, /*SRU1*/OP_NOP,/*SRU1_IM*/OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/OP_NOP,/*LU3*/OP_NOP,/*OP_IM*/RC_8[j]);
				exe(OP,  0,PE_out[13], /*ALU in*/ PE_out[11][2],0,PE_out[11][0],0, /*BUFF 8to1 in*/ PE_out[9][0], PE_out[9][2], PE_out[11][4], PE_out[11][5]);			
				OP = BASIC_OP64(/*AU*/ OP_NOP, /*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
				exe(OP,  0,PE_out[14], /*ALU in*/ PE_out[11][4],PE_out[8][0], PE_out[8][2],PE_out[11][5], /*BUFF 8to1 in*/ PE_out[10][0], PE_out[10][2], 0,0);
				
				exe(OP_NOP, 0,PE_out[15], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[11][0], PE_out[11][2], 0, 0);
			}
		}
	}
clock_gettime(CLOCK_REALTIME, &end_compile);
	time_spent_compile = BILLION * (end_compile.tv_sec - start_compile.tv_sec) + (end_compile.tv_nsec - start_compile.tv_nsec) + time_spent_compile;
	printf("Compiler execution time in millisecond is %f\n", (double)time_spent_compile*1000 / BILLION); // Added this line for clarity	
	#if defined(ARMZYNQ)
	dma_write_U32(ROW0_BASE_PHYS,8);
	dma_write_U32(ROW1_BASE_PHYS,8);
	*(CGRA_info.ctx_pe_mmap+START_BASE_IP) = 1;
	
	while(1){
		if(*(CGRA_info.ctx_pe_mmap+FINISH_BASE_IP) == 1)
			break;
	}

	
	dma_read_U32(ROW0_BASE_PHYS,8);
	dma_read_U32(ROW1_BASE_PHYS,8);
	
	// for (i=0; i<8; i++) {
		// printf("Output %d = ",i);
		// for (j=0; j<8; j++) {
			// if((j%2==0)){
				// printf("%08x ",A[i*8+j+1]);
			// }
			// else{
				// printf("%08x ",A[i*8+j-1]);
			// }
		// }
		
		// for (j=0; j<2; j++) {
			// if((j%2==0)){
				// printf("%08x ",B[i*8+j+1]);
			// }
			// else{
				// printf("%08x ",B[i*8+j-1]);
			// }
		// }
		// printf("\n");
	// }
	
	#endif
	#if defined(ARMSIL)
	// for(k = 0; k < (LOOP*2-1); k++){
		// printf ("-------------------------------------------------------------------------------------\n");
		// printf ("|   LOOP %d \n",k);
		// printf ("-------------------------------------------------------------------------------------\n");
		// for (i = 0; i < NUM_PE; i++){
			// printf ("PE_out[%d] =",i);
			// for (j = 0; j < 8; j++){
				// printf (" %08x",PE_out[i+k*NUM_PE][j]);
			// }
			// printf ("\n");
			// if( ((i+1) % 4) == 0){
				// printf ("-------------------------------------------------------------------------------------\n");
			// }
		// }
	// }

	// printf("\n Output =");
	// for(i = 0; i < 2; i++){
		// printf(" %08x",LMM[0][i][0]);
	// }
	// for(i = 0; i < 2; i++){
		// printf(" %08x",LMM[1][i][0]);
	// }
	// for(i = 0; i < 2; i++){
		// printf(" %08x",LMM[2][i][0]);
	// }
	// for(i = 0; i < 2; i++){
		// printf(" %08x",LMM[3][i][0]);
	// }
	// for(i = 0; i < 2; i++){
		// printf(" %08x",LMM[4][i][0]);
	// }
	// printf("\n");
	
	#elif defined(VIVADOSIL)	
		printf("Successfully write the configuration data to %s file\n",Config_File_Name);
	#endif

	Final();
	return 0;
}
