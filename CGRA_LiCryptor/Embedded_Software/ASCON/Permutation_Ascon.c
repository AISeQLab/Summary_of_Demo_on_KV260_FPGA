#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>

#include "../CGRA.h"
#include "../CGRAlib.c"

#define N 4

#define Config_File_Name "Config.txt"
#define LDM_File_Name "LDM.txt"

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
	
	#if defined(VIVADOSIL)
	CGRA_info.Config_File = fopen(Config_File_Name,"w");
	CGRA_info.LDM_File = fopen(LDM_File_Name,"w");
	#endif
	
	LOOP = 9;
	U32 OP;
	U32 ***LMM;

	LMM = (U32***) calloc(16, sizeof(U32**));
	for(i = 0; i < 16; i++) {
		LMM[i] = (U32**) calloc(2, sizeof(U32*));
    for(j = 0; j < 2; j++) {
        LMM[i][j] = (U32*) calloc(1024, sizeof(U32));
		}
	}

	U32 **PE_out;
	PE_out = (U32**) malloc((LOOP*2*16)*sizeof(U32*)); // Allocate memory for each row
	if (PE_out == NULL) {
		printf("Memory not allocated.\n");
		exit(0);
	} else {
		for (i = 0; i < LOOP*2*16; i ++) {
			PE_out[i] = (U32 *)calloc(8,sizeof(U32)); // Allocate memory for each column in a row
			if (PE_out[i] == NULL) {
				printf("Memory not allocated for PE_out[%d].\n", i);
				exit(0);
			}
		}
	}
		// PE_out =  (U32 **) calloc((LOOP*16)*16,sizeof(U32));
	// for (i = 0; i < (LOOP*16)*16; i ++){
		// PE_out[i] = (U32 *)calloc(8,sizeof(U32));
	// }
	
	U32 data[10] = {0x11112224, 0x22223333, 0x33334444, 0x44445555, 0x55556666, 0x66667777, 0x77778888, 0x88889999, 0x9999aaaa, 0xaaaabbbb};
	U32 cons[6] = {0x3, 0x6, 0x9, 0xC, 0x0, 0x0};
	
	#if defined(ARMZYNQ)
	for (i = 0; i < LOOP*2*16; i ++){	
		for (j = 0; j < 8; j ++){
			PE_out[i][j] = (i%4)*8+j;
		}
	}
	
	unsigned char* membase;
	if (cgra_open() == ((void *)0))
		exit(1);
	
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
	for (i = 0; i < LOOP*2*16; i ++){	
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
	LMM[0][0][0] = s.x0.e;
	LMM[0][1][0] = s.x0.o;
	
	//set data for local memory 1 (LMM 1)
	LMM[1][0][0] = s.x1.e;
	LMM[1][1][0] = s.x1.o;

	//set data for local memory 2 (LMM 2)
	LMM[2][0][0] = s.x2.e;
	LMM[2][1][0] = s.x2.o;
	
	//set data for local memory 3 (LMM 3)
	LMM[3][0][0] = s.x3.e;
	LMM[3][1][0] = s.x3.o;
	
	//set data for local memory 4 (LMM 4)
	LMM[4][0][0] = s.x4.e;
	LMM[4][1][0] = s.x4.o;
	
	//set data for local memory 5 (LMM 5)
	LMM[5][0][0] = RC0;
	LMM[5][1][0] = RC1;

	//set data for local memory 6 (LMM 6)
	LMM[6][0][0] = RC2;
	LMM[6][1][0] = RC3;
	
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
	
	for (j = 0; j < LOOP; j++){
		if( j == 0){
			i = j*2 +0;
			///*** row 0 ***///
			mop(OP_LDW, 0, 0, LMM[0], PE_out[0+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_LDW, 0, 0, LMM[1], PE_out[1+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_LDW, 0, 0, LMM[2], PE_out[2+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_LDW, 0, 0, LMM[3], PE_out[3+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			///*** row 1 ***///
			mop(OP_LDW, 0, 0, LMM[4], PE_out[4+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[0+i*NUM_PE][0], PE_out[0+i*NUM_PE][1], 0, 0);
			mop(OP_LDW, 0, 0, LMM[5], PE_out[5+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[1+i*NUM_PE][0], PE_out[1+i*NUM_PE][1], 0, 0);
			mop(OP_LDW, 0, 0, LMM[6], PE_out[6+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[2+i*NUM_PE][0], PE_out[2+i*NUM_PE][1], 0, 0);
			exe(OP_NOP, PE_out[7+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[3+i*NUM_PE][0], PE_out[3+i*NUM_PE][1], 0, 0);
			///*** row 2 ***///
			exe(OP_NOP, PE_out[8+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[4+i*NUM_PE][4], PE_out[4+i*NUM_PE][5], PE_out[4+i*NUM_PE][0], PE_out[4+i*NUM_PE][1]);
			exe(OP_NOP, PE_out[9+i*NUM_PE], /*ALU in*/ 0,0,PE_out[7+i*NUM_PE][4],PE_out[7+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[5+i*NUM_PE][4], PE_out[5+i*NUM_PE][5], PE_out[5+i*NUM_PE][0], PE_out[5+i*NUM_PE][1]);
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[10+i*NUM_PE], /*ALU in*/ PE_out[6+i*NUM_PE][4],PE_out[Ce_PE[j]+i*NUM_PE][Ce_Index[j]],PE_out[Co_PE[j]+i*NUM_PE][Co_Index[j]],PE_out[6+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, PE_out[6+i*NUM_PE][0], PE_out[6+i*NUM_PE][1]);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[7+i*NUM_PE][4], PE_out[7+i*NUM_PE][5], 0, 0);
			///*** row 3 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[12+i*NUM_PE], /*ALU in*/ PE_out[8+i*NUM_PE][4],PE_out[8+i*NUM_PE][6],PE_out[8+i*NUM_PE][7],PE_out[8+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[13+i*NUM_PE], /*ALU in*/ PE_out[11+i*NUM_PE][4],PE_out[8+i*NUM_PE][6],PE_out[8+i*NUM_PE][7],PE_out[11+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[9+i*NUM_PE][4], PE_out[9+i*NUM_PE][5], PE_out[9+i*NUM_PE][2], PE_out[9+i*NUM_PE][3]);
			exe(OP, PE_out[14+i*NUM_PE], /*ALU in*/ PE_out[10+i*NUM_PE][0],PE_out[9+i*NUM_PE][4],PE_out[9+i*NUM_PE][5],PE_out[10+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[10+i*NUM_PE][6], PE_out[10+i*NUM_PE][7]);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ 0,0,PE_out[9+i*NUM_PE][4],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[11+i*NUM_PE][4], PE_out[11+i*NUM_PE][5], 0, 0);
			
			i = j*2 +1;
			///*** row 0 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
			exe(OP, PE_out[0+i*NUM_PE], /*ALU in*/ PE_out[13+(i-1)*NUM_PE][4],PE_out[14+(i-1)*NUM_PE][0],PE_out[14+(i-1)*NUM_PE][2],PE_out[13+(i-1)*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[12+(i-1)*NUM_PE][0], PE_out[12+(i-1)*NUM_PE][2], 0, 0);
			exe(OP, PE_out[1+i*NUM_PE], /*ALU in*/ PE_out[15+(i-1)*NUM_PE][4],PE_out[13+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][2],PE_out[15+(i-1)*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[13+(i-1)*NUM_PE][0], PE_out[13+(i-1)*NUM_PE][2], PE_out[13+(i-1)*NUM_PE][6], PE_out[13+(i-1)*NUM_PE][7]);
			exe(OP, PE_out[2+i*NUM_PE], /*ALU in*/ PE_out[12+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][4],PE_out[13+(i-1)*NUM_PE][5],PE_out[12+(i-1)*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[14+(i-1)*NUM_PE][0], PE_out[14+(i-1)*NUM_PE][2], PE_out[14+(i-1)*NUM_PE][6], PE_out[14+(i-1)*NUM_PE][7]);
			exe(OP, PE_out[3+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][0],PE_out[15+(i-1)*NUM_PE][4],PE_out[15+(i-1)*NUM_PE][5],PE_out[14+(i-1)*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[15+(i-1)*NUM_PE][2], PE_out[15+(i-1)*NUM_PE][3]);
			///*** row 1 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
			exe(OP, PE_out[4+i*NUM_PE], /*ALU in*/ PE_out[1+i*NUM_PE][4],PE_out[0+i*NUM_PE][4],PE_out[0+i*NUM_PE][5],PE_out[1+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[5+i*NUM_PE], /*ALU in*/ PE_out[0+i*NUM_PE][0],PE_out[0+i*NUM_PE][4],PE_out[0+i*NUM_PE][5],PE_out[0+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, PE_out[1+i*NUM_PE][3], PE_out[1+i*NUM_PE][6],PE_out[1+i*NUM_PE][7]);
			exe(OP, PE_out[6+i*NUM_PE], /*ALU in*/ PE_out[1+i*NUM_PE][0],PE_out[2+i*NUM_PE][4],PE_out[2+i*NUM_PE][5],PE_out[1+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[2+i*NUM_PE][6],PE_out[2+i*NUM_PE][7]);
			exe(OP, PE_out[7+i*NUM_PE], /*ALU in*/ PE_out[2+i*NUM_PE][0],PE_out[1+i*NUM_PE][4],PE_out[1+i*NUM_PE][5],PE_out[2+i*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[3+i*NUM_PE][0],PE_out[3+i*NUM_PE][2], PE_out[3+i*NUM_PE][6],PE_out[3+i*NUM_PE][7]);
			///*** row 2 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[8+i*NUM_PE], /*ALU in*/ PE_out[7+i*NUM_PE][6],PE_out[7+i*NUM_PE][4],PE_out[7+i*NUM_PE][5],PE_out[7+i*NUM_PE][7], /*BUFF 8to1 in*/ PE_out[4+i*NUM_PE][0],PE_out[4+i*NUM_PE][2], 0, 0);
			exe(OP, PE_out[9+i*NUM_PE], /*ALU in*/ PE_out[4+i*NUM_PE][0],PE_out[5+i*NUM_PE][6],PE_out[5+i*NUM_PE][7],PE_out[4+i*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[5+i*NUM_PE][0],PE_out[5+i*NUM_PE][2], 0, 0);
			exe(OP_NOP, PE_out[10+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[6+i*NUM_PE][0],PE_out[6+i*NUM_PE][2], 0, 0);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[7+i*NUM_PE][0],PE_out[7+i*NUM_PE][2], 0, 0);
			///*** row 3 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[12+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][4],PE_out[8+i*NUM_PE][0],PE_out[8+i*NUM_PE][2],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[13+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][0],PE_out[10+i*NUM_PE][4],PE_out[10+i*NUM_PE][5],PE_out[9+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[14+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][4],PE_out[11+i*NUM_PE][4],PE_out[11+i*NUM_PE][5],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[10+i*NUM_PE][4],PE_out[10+i*NUM_PE][5], 0, 0);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[11+i*NUM_PE][4],PE_out[11+i*NUM_PE][5], 0, 0);
		}
		else if((j >0) && (j<(LOOP-1))){	
			i = j*2 +0;
			
			///*** row 0 ***///
			OP = CUSTOM_OP(OP_SIG0);
			exe(OP, PE_out[0+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][0],PE_out[14+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG1);                                                                                           
			exe(OP, PE_out[1+i*NUM_PE], /*ALU in*/ PE_out[12+(i-1)*NUM_PE][0],PE_out[12+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG2);                                                                                          
			exe(OP, PE_out[2+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][4],PE_out[14+(i-1)*NUM_PE][5], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG3);
			exe(OP, PE_out[3+i*NUM_PE], /*ALU in*/ PE_out[13+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ PE_out[15+(i-1)*NUM_PE][4], PE_out[15+(i-1)*NUM_PE][5], 0, 0);
			///*** row 1 ***///
			OP = CUSTOM_OP(OP_SIG4);
			exe(OP, PE_out[4+i*NUM_PE], /*ALU in*/ PE_out[3+i*NUM_PE][4],PE_out[3+i*NUM_PE][5], 0, 0, /*BUFF 8to1 in*/ PE_out[0+i*NUM_PE][0], PE_out[0+i*NUM_PE][1], 0, 0);
			mop(OP_LDW, 0, 0, LMM[5], PE_out[5+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[1+i*NUM_PE][0], PE_out[1+i*NUM_PE][1], 0, 0);
			mop(OP_LDW, 0, 0, LMM[6], PE_out[6+i*NUM_PE],/*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[2+i*NUM_PE][0], PE_out[2+i*NUM_PE][1], 0, 0);
			exe(OP_NOP, PE_out[7+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[3+i*NUM_PE][0], PE_out[3+i*NUM_PE][1], 0, 0);
			///*** row 2 ***///
			exe(OP_NOP, PE_out[8+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[4+i*NUM_PE][4], PE_out[4+i*NUM_PE][5], PE_out[4+i*NUM_PE][0], PE_out[4+i*NUM_PE][1]);
			exe(OP_NOP, PE_out[9+i*NUM_PE], /*ALU in*/ 0,0,PE_out[7+i*NUM_PE][4],PE_out[7+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[5+i*NUM_PE][4], PE_out[5+i*NUM_PE][5], PE_out[5+i*NUM_PE][0], PE_out[5+i*NUM_PE][1]);
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[10+i*NUM_PE], /*ALU in*/ PE_out[6+i*NUM_PE][4],PE_out[Ce_PE[j]+i*NUM_PE][Ce_Index[j]],PE_out[Co_PE[j]+i*NUM_PE][Co_Index[j]],PE_out[6+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, PE_out[6+i*NUM_PE][0], PE_out[6+i*NUM_PE][1]);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ 0,0,0,0, /*BUFF 8to1 in*/ PE_out[7+i*NUM_PE][4], PE_out[7+i*NUM_PE][5], 0, 0);
			///*** row 3 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[12+i*NUM_PE], /*ALU in*/ PE_out[8+i*NUM_PE][4],PE_out[8+i*NUM_PE][6],PE_out[8+i*NUM_PE][7],PE_out[8+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[13+i*NUM_PE], /*ALU in*/ PE_out[11+i*NUM_PE][4],PE_out[8+i*NUM_PE][6],PE_out[8+i*NUM_PE][7],PE_out[11+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[9+i*NUM_PE][4], PE_out[9+i*NUM_PE][5], PE_out[9+i*NUM_PE][2], PE_out[9+i*NUM_PE][3]);
			exe(OP, PE_out[14+i*NUM_PE], /*ALU in*/ PE_out[10+i*NUM_PE][0],PE_out[9+i*NUM_PE][4],PE_out[9+i*NUM_PE][5],PE_out[10+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[10+i*NUM_PE][6], PE_out[10+i*NUM_PE][7]);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ 0,0,PE_out[9+i*NUM_PE][4],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[11+i*NUM_PE][4], PE_out[11+i*NUM_PE][5], 0, 0);
			
			i = j*2 +1;
			///*** row 0 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
			exe(OP, PE_out[0+i*NUM_PE], /*ALU in*/ PE_out[13+(i-1)*NUM_PE][4],PE_out[14+(i-1)*NUM_PE][0],PE_out[14+(i-1)*NUM_PE][2],PE_out[13+(i-1)*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[12+(i-1)*NUM_PE][0], PE_out[12+(i-1)*NUM_PE][2], 0, 0);
			exe(OP, PE_out[1+i*NUM_PE], /*ALU in*/ PE_out[15+(i-1)*NUM_PE][4],PE_out[13+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][2],PE_out[15+(i-1)*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[13+(i-1)*NUM_PE][0], PE_out[13+(i-1)*NUM_PE][2], PE_out[13+(i-1)*NUM_PE][6], PE_out[13+(i-1)*NUM_PE][7]);
			exe(OP, PE_out[2+i*NUM_PE], /*ALU in*/ PE_out[12+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][4],PE_out[13+(i-1)*NUM_PE][5],PE_out[12+(i-1)*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[14+(i-1)*NUM_PE][0], PE_out[14+(i-1)*NUM_PE][2], PE_out[14+(i-1)*NUM_PE][6], PE_out[14+(i-1)*NUM_PE][7]);
			exe(OP, PE_out[3+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][0],PE_out[15+(i-1)*NUM_PE][4],PE_out[15+(i-1)*NUM_PE][5],PE_out[14+(i-1)*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[15+(i-1)*NUM_PE][2], PE_out[15+(i-1)*NUM_PE][3]);
			///*** row 1 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_NOT_AND);
			exe(OP, PE_out[4+i*NUM_PE], /*ALU in*/ PE_out[1+i*NUM_PE][4],PE_out[0+i*NUM_PE][4],PE_out[0+i*NUM_PE][5],PE_out[1+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[5+i*NUM_PE], /*ALU in*/ PE_out[0+i*NUM_PE][0],PE_out[0+i*NUM_PE][4],PE_out[0+i*NUM_PE][5],PE_out[0+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, PE_out[1+i*NUM_PE][3], PE_out[1+i*NUM_PE][6],PE_out[1+i*NUM_PE][7]);
			exe(OP, PE_out[6+i*NUM_PE], /*ALU in*/ PE_out[1+i*NUM_PE][0],PE_out[2+i*NUM_PE][4],PE_out[2+i*NUM_PE][5],PE_out[1+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, PE_out[2+i*NUM_PE][6],PE_out[2+i*NUM_PE][7]);
			exe(OP, PE_out[7+i*NUM_PE], /*ALU in*/ PE_out[2+i*NUM_PE][0],PE_out[1+i*NUM_PE][4],PE_out[1+i*NUM_PE][5],PE_out[2+i*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[3+i*NUM_PE][0],PE_out[3+i*NUM_PE][2], PE_out[3+i*NUM_PE][6],PE_out[3+i*NUM_PE][7]);
			///*** row 2 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[8+i*NUM_PE], /*ALU in*/ PE_out[7+i*NUM_PE][6],PE_out[7+i*NUM_PE][4],PE_out[7+i*NUM_PE][5],PE_out[7+i*NUM_PE][7], /*BUFF 8to1 in*/ PE_out[4+i*NUM_PE][0],PE_out[4+i*NUM_PE][2], 0, 0);
			exe(OP, PE_out[9+i*NUM_PE], /*ALU in*/ PE_out[4+i*NUM_PE][0],PE_out[5+i*NUM_PE][6],PE_out[5+i*NUM_PE][7],PE_out[4+i*NUM_PE][2], /*BUFF 8to1 in*/ PE_out[5+i*NUM_PE][0],PE_out[5+i*NUM_PE][2], 0, 0);
			exe(OP_NOP, PE_out[10+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[6+i*NUM_PE][0],PE_out[6+i*NUM_PE][2], 0, 0);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[7+i*NUM_PE][0],PE_out[7+i*NUM_PE][2], 0, 0);
			///*** row 3 ***///
			OP = BASIC_OP64(/*SRU*/OP_NOP,/*SRU_IM*/0, /*LU*/OP_XOR);
			exe(OP, PE_out[12+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][4],PE_out[8+i*NUM_PE][0],PE_out[8+i*NUM_PE][2],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[13+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][0],PE_out[10+i*NUM_PE][4],PE_out[10+i*NUM_PE][5],PE_out[9+i*NUM_PE][2], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP, PE_out[14+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][4],PE_out[11+i*NUM_PE][4],PE_out[11+i*NUM_PE][5],PE_out[9+i*NUM_PE][5], /*BUFF 8to1 in*/ PE_out[10+i*NUM_PE][4],PE_out[10+i*NUM_PE][5], 0, 0);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ PE_out[11+i*NUM_PE][4],PE_out[11+i*NUM_PE][5], 0, 0);
		}
		else {		
			i = j*2 +0;
			// /*** row 0 ***///
			OP = CUSTOM_OP(OP_SIG0);
			exe(OP, PE_out[0+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][0],PE_out[14+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG1);                                                                                           
			exe(OP, PE_out[1+i*NUM_PE], /*ALU in*/ PE_out[12+(i-1)*NUM_PE][0],PE_out[12+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG2);                                                                                          
			exe(OP, PE_out[2+i*NUM_PE], /*ALU in*/ PE_out[14+(i-1)*NUM_PE][4],PE_out[14+(i-1)*NUM_PE][5], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			OP = CUSTOM_OP(OP_SIG3);
			exe(OP, PE_out[3+i*NUM_PE], /*ALU in*/ PE_out[13+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][2], 0, 0, /*BUFF 8to1 in*/ PE_out[15+(i-1)*NUM_PE][4], PE_out[15+(i-1)*NUM_PE][5], 0, 0);
			/*** row 1 ***///
			OP = CUSTOM_OP(OP_SIG4);
			exe(OP, PE_out[4+i*NUM_PE], /*ALU in*/ PE_out[3+i*NUM_PE][4],PE_out[3+i*NUM_PE][5], 0, 0, /*BUFF 8to1 in*/ PE_out[0+i*NUM_PE][0], PE_out[0+i*NUM_PE][1], 0, 0);
			exe(OP_NOP, PE_out[5+i*NUM_PE], /*ALU in*/ PE_out[1+i*NUM_PE][0],PE_out[1+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[6+i*NUM_PE], /*ALU in*/ PE_out[2+i*NUM_PE][0],PE_out[2+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[7+i*NUM_PE], /*ALU in*/ PE_out[3+i*NUM_PE][0],PE_out[3+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			/*** row 2 ***///
			exe(OP_NOP, PE_out[8+i*NUM_PE], /*ALU in*/ PE_out[4+i*NUM_PE][0],PE_out[4+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ PE_out[4+i*NUM_PE][4], PE_out[4+i*NUM_PE][5], 0, 0);
			exe(OP_NOP, PE_out[9+i*NUM_PE], /*ALU in*/ PE_out[5+i*NUM_PE][0],PE_out[5+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[10+i*NUM_PE], /*ALU in*/ PE_out[6+i*NUM_PE][0],PE_out[6+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ PE_out[7+i*NUM_PE][0],PE_out[7+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			/*** row 3 ***///
			exe(OP_NOP, PE_out[12+i*NUM_PE], /*ALU in*/ PE_out[8+i*NUM_PE][0],PE_out[8+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ PE_out[8+i*NUM_PE][4], PE_out[8+i*NUM_PE][5], 0, 0);
			exe(OP_NOP, PE_out[13+i*NUM_PE], /*ALU in*/ PE_out[9+i*NUM_PE][0],PE_out[9+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[14+i*NUM_PE], /*ALU in*/ PE_out[10+i*NUM_PE][0],PE_out[10+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ PE_out[11+i*NUM_PE][0],PE_out[11+i*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			
			i = j*2 +1;
			/*** row 0 ***///
			mop(OP_STW, 0, 0, LMM[0], PE_out[0+i*NUM_PE],/*ALU in*/ PE_out[12+(i-1)*NUM_PE][4],PE_out[12+(i-1)*NUM_PE][5],PE_out[12+(i-1)*NUM_PE][0],PE_out[12+(i-1)*NUM_PE][1], /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_STW, 0, 0, LMM[1], PE_out[1+i*NUM_PE],/*ALU in*/ PE_out[13+(i-1)*NUM_PE][0],PE_out[13+(i-1)*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_STW, 0, 0, LMM[2], PE_out[2+i*NUM_PE],/*ALU in*/ PE_out[14+(i-1)*NUM_PE][0],PE_out[14+(i-1)*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_STW, 0, 0, LMM[3], PE_out[3+i*NUM_PE],/*ALU in*/ PE_out[15+(i-1)*NUM_PE][0],PE_out[15+(i-1)*NUM_PE][1], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			/*** row 1 ***///
			mop(OP_STW, 0, 0, LMM[4], PE_out[4+i*NUM_PE],/*ALU in*/ PE_out[0+(i)*NUM_PE][2],PE_out[0+(i)*NUM_PE][3], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			mop(OP_STW, 0, 0, LMM[5], PE_out[5+i*NUM_PE],/*ALU in*/ PE_out[0+(i)*NUM_PE][2],PE_out[0+(i)*NUM_PE][3], 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[6+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[7+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			/*** row 2 ***///
			exe(OP_NOP, PE_out[8+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[9+i*NUM_PE], /*ALU in*/ 0, 0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[10+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[11+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			/*** row 3 ***///                           
			exe(OP_NOP, PE_out[12+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[13+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[14+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
			exe(OP_NOP, PE_out[15+i*NUM_PE], /*ALU in*/ 0 ,0, 0, 0, /*BUFF 8to1 in*/ 0, 0, 0, 0);
		}
	}

	#if defined(ARMZYNQ)
	dma_write(ROW0_BASE_PHYS,8);
	dma_write(ROW1_BASE_PHYS,8);
	*(CGRA_info.reg_mmap+START_BASE_PHYS) = 1;
	
	// while(1){
		// if((*(CGRA_info.reg_mmap+FINISH_BASE_PHYS)) == 1)
			// break;
	// }
	for (i=0; i<1000; i++){
	}
	printf("*(CGRA_info.reg_mmap+FINISH_BASE_PHYS) = %08x\n",(*(CGRA_info.reg_mmap+FINISH_BASE_PHYS)));
	
	dma_read(ROW0_BASE_PHYS,8);
	dma_read(ROW1_BASE_PHYS,8);
	
	for (i=0; i<8; i++) {
		printf("Output %d = ",i);
		for (j=0; j<8; j++) {
			if((j%2==0)){
				printf("%08x ",A[i*8+j+1]);
			}
			else{
				printf("%08x ",A[i*8+j-1]);
			}
		}
		
		for (j=0; j<2; j++) {
			if((j%2==0)){
				printf("%08x ",B[i*8+j+1]);
			}
			else{
				printf("%08x ",B[i*8+j-1]);
			}
		}
		printf("\n");
	}
	
	#endif
	#if defined(ARMSIL)
	for(k = 0; k < LOOP*2; k++){
		printf ("-------------------------------------------------------------------------------------\n");
		printf ("|   LOOP %d \n",k);
		printf ("-------------------------------------------------------------------------------------\n");
		for (i = 0; i < NUM_PE; i++){
			printf ("PE_out[%d] =",i);
			for (j = 0; j < 8; j++){
				printf (" %08x",PE_out[i+k*NUM_PE][j]);
			}
			printf ("\n");
			if( ((i+1) % 4) == 0){
				printf ("-------------------------------------------------------------------------------------\n");
			}
		}
	}

	printf("\n Output =");
	for(i = 0; i < 2; i++){
		printf(" %08x",LMM[0][i][0]);
	}
	for(i = 0; i < 2; i++){
		printf(" %08x",LMM[1][i][0]);
	}
	for(i = 0; i < 2; i++){
		printf(" %08x",LMM[2][i][0]);
	}
	for(i = 0; i < 2; i++){
		printf(" %08x",LMM[3][i][0]);
	}
	for(i = 0; i < 2; i++){
		printf(" %08x",LMM[4][i][0]);
	}
	printf("\n");
	
	#elif defined(VIVADOSIL)	
		printf("Successfully write the configuration data to %s file\n",Config_File_Name);
	#endif
	for (i = 0; i < LOOP*2*16; i++) {
    free(PE_out[i]);
}
free(PE_out);

	return 0;
}
