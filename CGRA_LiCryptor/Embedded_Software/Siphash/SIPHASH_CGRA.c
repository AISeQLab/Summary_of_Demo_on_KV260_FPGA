#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <byteswap.h>
#include "../CGRA.h"
#include "../CGRAlib.c"

///////////////////////////////////// 	SIPHASH - CGRA 	/////////////////////////////////////////////////////////////
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
	__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define _le64toh(x) ((uint64_t)(x))
#elif defined(_WIN32)
/* Windows is always little endian, unless you're on xbox360
   http://msdn.microsoft.com/en-us/library/b0084kay(v=vs.80).aspx */
#  define _le64toh(x) ((uint64_t)(x))
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define _le64toh(x) OSSwapLittleToHostInt64(x)
#else

/* See: http://sourceforge.net/p/predef/wiki/Endianness/ */
#  if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#    include <sys/endian.h>
#  else
#    include <endian.h>
#  endif
#  if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
	__BYTE_ORDER == __LITTLE_ENDIAN
#    define _le64toh(x) ((uint64_t)(x))
#  else
#    define _le64toh(x) le64toh(x)
#  endif

#endif


void DOUBLE_ROUND(uint64_t v0, uint64_t v1, uint64_t v2, uint64_t v3){
	LMM[0][0][0] = v0;
	LMM[0][1][0] = v1;
	
	LMM[1][0][0] = v2;
	LMM[1][1][0] = v3;
	U64 OP;
	mop64(OP_LDW, 0, PE_out[0],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[12][4],PE_out[12][5],PE_out[12][6],PE_out[12][7], /*BUFF 8to1 in*/ PE_out[12][8],PE_out[12][9],PE_out[12][10],PE_out[12][11]); // a		b		X		X
	mop64(OP_LDW, 0, PE_out[1],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[13][4],PE_out[13][5],PE_out[13][6],PE_out[13][7], /*BUFF 8to1 in*/ PE_out[13][8],PE_out[13][9],PE_out[13][10],PE_out[13][11]); // c		d		X		X
	mop64(OP_LDW, 0, PE_out[2],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[14][4],PE_out[14][5],PE_out[14][6],PE_out[14][7], /*BUFF 8to1 in*/ PE_out[14][8],PE_out[14][9],PE_out[14][10],PE_out[14][11]);
	mop64(OP_LDW, 0, PE_out[3],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[15][4],PE_out[15][5],PE_out[15][6],PE_out[15][7], /*BUFF 8to1 in*/ PE_out[15][8],PE_out[15][9],PE_out[15][10],PE_out[15][11]);
//HALF_ROUND_0 (V0, V1, V2, V3, 13, 16)
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 13,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[4],/*ALU in*/  PE_out[0][0],PE_out[0][1],0,PE_out[0][1], /*BUFF 16to1 in*/  	PE_out[0][4],PE_out[0][5],PE_out[0][6],PE_out[0][7], /*BUFF 8to1 in*/ PE_out[0][8],PE_out[0][9],PE_out[0][10],PE_out[0][11]); 	//	x	b	x	x
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 16,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[5],/*ALU in*/  PE_out[1][0],PE_out[1][1],0,PE_out[1][1], /*BUFF 16to1 in*/	PE_out[1][4],PE_out[1][5],PE_out[1][6],PE_out[1][7], /*BUFF 8to1 in*/ PE_out[1][8],PE_out[1][9],PE_out[1][10],PE_out[1][11]); 	//x		d	x	x
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_ROL,/*SRU1_IM*/ 32,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[6],/*ALU in*/  PE_out[0][0],PE_out[0][1],0,0, /*BUFF 16to1 in*/  PE_out[2][4],PE_out[2][5],PE_out[2][6],PE_out[2][7], /*BUFF 8to1 in*/ PE_out[2][8],PE_out[2][9],PE_out[2][10],PE_out[2][11]);//	x	x	a	x
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[7],/*ALU in*/  PE_out[1][0],PE_out[1][1],0,0, /*BUFF 16to1 in*/  PE_out[3][4],PE_out[3][5],PE_out[3][6],PE_out[3][7], /*BUFF 8to1 in*/ PE_out[3][8],PE_out[3][9],PE_out[3][10],PE_out[3][11]); //	x	x	c	x
/*		MATRIX SAMPLE:
		PE[0] = X	v1	X	X		X		X		X		X			X		X		X		X		
		PE[1] = X 	v3	X	X		X		X		X		X			X		X		X		X		
		PE[2] = X	X	v0	X 		X		X		X		X			X		X		X		X		
		PE[3] = X	X	v2	X		X		X		X		X			X		X		X		X	
		*/
//HALF_ROUND_1 (V2, V1, V0, V3, 17, 21)
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 17,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[8],/*ALU in*/  PE_out[7][2],PE_out[4][1],0,PE_out[4][1],  /*BUFF 16to1 in*/ PE_out[4][4],PE_out[4][5],PE_out[4][6],PE_out[4][7], /*BUFF 8to1 in*/ PE_out[4][8],PE_out[4][9],PE_out[4][10],PE_out[4][11]);  
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 21,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[9],/*ALU in*/  PE_out[6][2],PE_out[5][1],0,PE_out[5][1],  /*BUFF 16to1 in*/ PE_out[5][4],PE_out[5][5],PE_out[5][6],PE_out[5][7], /*BUFF 8to1 in*/ PE_out[5][8],PE_out[5][9],PE_out[5][10],PE_out[5][11]);  
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_ROL,/*SRU1_IM*/ 32,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[10],/*ALU in*/ PE_out[7][2],PE_out[4][1],0,0, /*BUFF 16to1 in*/ 	PE_out[6][4],PE_out[6][5],PE_out[6][6],PE_out[6][7], /*BUFF 8to1 in*/ PE_out[6][8],PE_out[6][9],PE_out[6][10],PE_out[6][11]); 
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[11],/*ALU in*/ PE_out[6][2],PE_out[5][1],0,0,  /*BUFF 16to1 in*/ 	PE_out[7][4],PE_out[7][5],PE_out[7][6],PE_out[7][7], /*BUFF 8to1 in*/ PE_out[7][8],PE_out[7][9],PE_out[7][10],PE_out[7][11]); 	

/*		MATRIX SAMPLE:
		PE[8] = X	v1	X	X		X		X		X		X			X		X		X		X		
		PE[9] = X 	v3	X	X		X		X		X		X			X		X		X		X		
		PE[10] = X	X	v2	X 		X		X		X		X			X		X		X		X		
		PE[11] = X	X	v0	X		X		X		X		X			X		X		X		X	
*/
//HALF_ROUND_2 (V0, V1, V2, V3, 13, 16)
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 13,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[12],/*ALU in*/ PE_out[11][2],PE_out[8][1],0,PE_out[8][1], /*BUFF 16to1 in*/ PE_out[8][4],PE_out[8][5],PE_out[8][6],PE_out[8][7], /*BUFF 8to1 in*/ PE_out[8][8],PE_out[8][9],PE_out[8][10],PE_out[8][11]); 
 	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 16,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[13],/*ALU in*/ PE_out[10][2],PE_out[9][1],0,PE_out[9][1], /*BUFF 16to1 in*/ PE_out[9][4],PE_out[9][5],PE_out[9][6],PE_out[9][7], /*BUFF 8to1 in*/ PE_out[9][8],PE_out[9][9],PE_out[9][10],PE_out[9][11]); 
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_ROL,/*SRU1_IM*/ 32,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
 	exe64(OP, 0, PE_out[14],/*ALU in*/ PE_out[11][2],PE_out[8][1],0,0, /*BUFF 16to1 in*/ PE_out[10][4],PE_out[10][5],PE_out[10][6],PE_out[10][7], /*BUFF 8to1 in*/ PE_out[10][8],PE_out[10][9],PE_out[10][10],PE_out[10][11]); 
 	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[15],/*ALU in*/ PE_out[10][2],PE_out[9][1],0,0, /*BUFF 16to1 in*/ PE_out[11][4],PE_out[11][5],PE_out[11][6],PE_out[11][7], /*BUFF 8to1 in*/ PE_out[11][8],PE_out[11][9],PE_out[11][10],PE_out[11][11]); 
/*		MATRIX SAMPLE:
		PE[12] = X	v1	X	X		X		X		X		X			X		X		X		X		
		PE[13] = X 	v3	X	X		X		X		X		X			X		X		X		X		
		PE[14] = X	X	v0	X 		X		X		X		X			X		X		X		X		
		PE[15] = X	X	v2	X		X		X		X		X			X		X		X		X	
*/
//HALF_ROUND_3 (V2, V1, V0, V3, 17, 21)
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 17,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[0],/*ALU in*/ PE_out[15][2],PE_out[12][1],0,PE_out[12][1], /*BUFF 16to1 in*/ PE_out[12][4],PE_out[12][5],PE_out[12][6],PE_out[12][7], /*BUFF 8to1 in*/ PE_out[12][8],PE_out[12][9],PE_out[12][10],PE_out[12][11]); 	
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_ROL,/*SRU2_IM*/ 21,/*LU3*/OP_XOR);
	exe64(OP, 0, PE_out[1],/*ALU in*/ PE_out[14][2],PE_out[13][1],0,PE_out[13][1], /*BUFF 16to1 in*/ PE_out[13][4],PE_out[13][5],PE_out[13][6],PE_out[13][7], /*BUFF 8to1 in*/ PE_out[13][8],PE_out[13][9],PE_out[13][10],PE_out[13][11]); 	
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_ROL,/*SRU1_IM*/ 32,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[2],/*ALU in*/ PE_out[15][2],PE_out[12][1],0,0, /*BUFF 16to1 in*/ PE_out[14][1],PE_out[15][1],PE_out[14][6],PE_out[14][7], /*BUFF 8to1 in*/ PE_out[14][8],PE_out[14][9],PE_out[14][10],PE_out[14][11]);
	OP = BASIC_OP(/*AU*/OP_ADD2,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	exe64(OP, 0, PE_out[3],/*ALU in*/ PE_out[14][2],PE_out[13][1],0,0, /*BUFF 16to1 in*/ PE_out[15][4],PE_out[15][5],PE_out[15][6],PE_out[15][7], /*BUFF 8to1 in*/ PE_out[15][8],PE_out[15][9],PE_out[15][10],PE_out[15][11]);
/*		MATRIX SAMPLE:
		PE[12] = X	v1	X	X		X		X		X		X			X		X		X		X		
		PE[13] = X 	v3	X	X		X		X		X		X			X		X		X		X		
		PE[14] = X	X	v2	X 		X		X		X		X			X		X		X		X		
		PE[15] = X	X	v0	X		X		X		X		X			X		X		X		X	
*/
	OP = BASIC_OP(/*AU*/OP_NOP,/*LU1*/OP_NOP,/*LU2*/OP_NOP,/*SRU1*/OP_NOP,/*SRU1_IM*/ OP_NOP,/*SRU2*/OP_NOP,/*SRU2_IM*/ OP_NOP,/*LU3*/OP_NOP);
	mop64(OP_STW, 0, PE_out[4],/*ALU in*/ PE_out[3][2],PE_out[0][1],0,0, /*BUFF 16to1 in*/ PE_out[0][0],PE_out[0][1],PE_out[0][6],PE_out[0][7], /*BUFF 8to1 in*/ PE_out[0][8],PE_out[0][9],PE_out[0][10],PE_out[0][11]); 
	mop64(OP_STW, 0, PE_out[5],/*ALU in*/ PE_out[2][2],PE_out[1][1],0,0, /*BUFF 16to1 in*/ PE_out[1][0],PE_out[1][1],PE_out[1][6],PE_out[1][7], /*BUFF 8to1 in*/ PE_out[1][8],PE_out[1][9],PE_out[1][10],PE_out[1][11]); 
	exe64(OP, 0, PE_out[6],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[2][0],PE_out[2][1],PE_out[2][6],PE_out[2][7], /*BUFF 8to1 in*/ PE_out[2][8],PE_out[2][9],PE_out[2][10],PE_out[2][11]);
	exe64(OP, 0, PE_out[7],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/ PE_out[3][0],PE_out[3][1],PE_out[3][6],PE_out[3][7], /*BUFF 8to1 in*/ PE_out[3][8],PE_out[3][9],PE_out[3][10],PE_out[3][11]);

	exe64(OP, 0, PE_out[8],/*ALU in*/ 	0,0,0,0, /*BUFF 16to1 in*/ PE_out[4][4],PE_out[4][5],PE_out[4][0],PE_out[4][1], /*BUFF 8to1 in*/ PE_out[4][8],PE_out[4][9],PE_out[4][10],PE_out[4][11]);	
	exe64(OP, 0, PE_out[9],/*ALU in*/ 	0,0,0,0, /*BUFF 16to1 in*/ PE_out[5][4],PE_out[5][5],PE_out[5][0],PE_out[5][1], /*BUFF 8to1 in*/ PE_out[5][8],PE_out[5][9],PE_out[5][10],PE_out[5][11]);					
	exe64(OP, 0, PE_out[10],/*ALU in*/ 	0,0,0,0, /*BUFF 16to1 in*/ PE_out[6][4],PE_out[6][5],PE_out[6][0],PE_out[6][1], /*BUFF 8to1 in*/ PE_out[6][8],PE_out[6][9],PE_out[6][10],PE_out[6][11]);	
	exe64(OP, 0, PE_out[11],/*ALU in*/ 	0,0,0,0, /*BUFF 16to1 in*/ PE_out[7][4],PE_out[7][5],PE_out[7][0],PE_out[7][1], /*BUFF 8to1 in*/ PE_out[7][8],PE_out[7][9],PE_out[7][10],PE_out[7][11]);		

	exe64(OP, 0, PE_out[12],/*ALU in*/ 0,0,0,0,/*BUFF 16to1 in*/    PE_out[8][1],PE_out[8][1],PE_out[8][1],PE_out[8][1], /*BUFF 8to1 in*/ PE_out[8][4],PE_out[8][5],PE_out[8][6],PE_out[8][7]); 				
	exe64(OP, 0, PE_out[13],/*ALU in*/ 0,0,0,0,/*BUFF 16to1 in*/    PE_out[9][1],PE_out[9][1],PE_out[9][1],PE_out[9][1], /*BUFF 8to1 in*/ PE_out[9][4],PE_out[9][5],PE_out[9][6],PE_out[9][7]); 				
	exe64(OP, 0, PE_out[14],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/   PE_out[10][1],PE_out[10][1],PE_out[10][1],PE_out[10][1], /*BUFF 8to1 in*/ PE_out[10][4],PE_out[10][5],PE_out[10][6],PE_out[10][7]);
	exe64(OP, 0, PE_out[15],/*ALU in*/ 0,0,0,0, /*BUFF 16to1 in*/   PE_out[11][1],PE_out[11][1],PE_out[11][1],PE_out[11][1], /*BUFF 8to1 in*/ PE_out[11][4],PE_out[11][5],PE_out[11][6],PE_out[11][7]);
}



uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16]) {
	const uint64_t *_key = (uint64_t *)key;
	uint64_t k0 = _le64toh(_key[0]);
	uint64_t k1 = _le64toh(_key[1]);
	uint64_t b = (uint64_t)src_sz << 56;
	const uint64_t *in = (uint64_t*)src;

	uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
	uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
	uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
	uint64_t v3 = k1 ^ 0x7465646279746573ULL;

	while (src_sz >= 8) {
		uint64_t mi = _le64toh(*in);
		in += 1; src_sz -= 8;
		v3 ^= mi;
		DOUBLE_ROUND(v0,v1,v2,v3);
			v0 = LMM[4][0][0];
			v1 = LMM[4][1][0];
			v2 = LMM[5][0][0];
			v3 = LMM[5][1][0];
		v0 ^= mi;
	}

	uint64_t t = 0; uint8_t *pt = (uint8_t *)&t; uint8_t *m = (uint8_t *)in;
	switch (src_sz) {
	case 7: pt[6] = m[6];
	case 6: pt[5] = m[5];
	case 5: pt[4] = m[4];
	case 4: *((uint32_t*)&pt[0]) = *((uint32_t*)&m[0]); break;
	case 3: pt[2] = m[2];
	case 2: pt[1] = m[1];
	case 1: pt[0] = m[0];
	}
	b |= _le64toh(t);

	v3 ^= b;
	DOUBLE_ROUND(v0,v1,v2,v3);
		v0 = LMM[4][0][0];
		v1 = LMM[4][1][0];
		v2 = LMM[5][0][0];
		v3 = LMM[5][1][0];
		v0 ^= b; v2 ^= 0xff;
	DOUBLE_ROUND(v0,v1,v2,v3);
		v0 = LMM[4][0][0];
		v1 = LMM[4][1][0];
		v2 = LMM[5][0][0];
		v3 = LMM[5][1][0];
	DOUBLE_ROUND(v0,v1,v2,v3);
		v0 = LMM[4][0][0];
		v1 = LMM[4][1][0];
		v2 = LMM[5][0][0];
		v3 = LMM[5][1][0];
	return (v0 ^ v1) ^ (v2 ^ v3);
}


void show_result(){
	printf("RESULT:\n");
	printf("%16lx\n",  PE_out[3][2]);
	printf("%16lx\n",  PE_out[0][1]);
	printf("%16lx\n",  PE_out[2][2]);
	printf("%16lx\n",  PE_out[1][1]);


}
int main(){

	Init();
	char key[16];
    for (int i = 0; i<16; i++) key[i] = i;
    unsigned long src_sz;
    uint64_t root[4], root1[4], root2[4], root3[4], root4[4], root5[4], root6[4], root7[4], result;
    uint64_t *src = root7;
    root[0] = 0xa66be1dc7f37fb80;
    root[1] = 0x13931921bba211f6;
    root[2] = 0xbff00febbae335e6;
    root[3] = 0x201f4a6fe5211611;

	root1[0] = 0x4189b9f2dc587bc8;
    root1[3] = 0x8d9abe9ba6ebd865;
    root1[1] = 0x9747076c4316f2f8;
    root1[2] = 0xaaac251e26cc6779;
//de5c649fc6eb5596cd73f3de5ca1ce3f0ef75fba45cafaff8dd5d20d3c
//bc15f50a9eb0e913bdd0c792df94068d5d28301342f265034b30228030a544da5232d7
	uint64_t *src2 = root2;
    root2[2] = 0x65034b30228030a5;
    root2[3] = 0x3f0ef75fba45cafa;
    root2[0] = 0xce3f0ef75fba45ca;
    root2[1] = 0xc649fc6eb5596cd7;
//4d51ba772b0d7448d61ba698fb98eca3a3db5a7cb968575b45d3358708e
//ada2e996f8c98931a0c910ed4936f944771ceecf85f6f8a06a05cd840f2776746f5ca
	uint64_t *src3 = root3;
    root3[1] = 0x4d51ba772b0d7446;
    root3[3] = 0x5a7cb968575b45d3;
    root3[2] = 0xcd840f2776746f5c;
    root3[0] = 0x6f8c98931a0c910e;

	uint64_t *src4 = root4;
    root4[0] = 0x3db5a7cb968575b4;
    root4[2] = 0x448d61ba698fb98a;
    root4[1] = 0xada2e996f8c989f8;
    root4[3] = 0x968575b45d335870;

	uint64_t *src5 = root5;
    root5[3] = 0x13bdd0c792df9406;
    root5[1] = 0xcf85f6f8a06afb98;
    root5[2] = 0x3a3db5a7cb968adf;
    root5[0] = 0x201f4a6fe5211611;
//c9d4fcfa00aa040e6089ddf67b9030c930f9cfd65f87d8743cc6423
//c0e3187fdd35898f7f2c4516668c83d54e3d9bab04b85a389027e47352aafbec060b17ea9
	uint64_t *src6 = root6;
    root6[3] = 0x027e47352aafbec0;
    root6[0] = 0xf7f2c4516668c83d;
    root6[2] = 0xfd65f87d8743cc64;
    root6[1] = 0xc9d4fcfa00aa040e;

	uint64_t *src7 = root7;
    root7[0] = 0x6089ddf67b9030c2;
    root7[1] = 0xafbec060b17ea924;
    root7[3] = 0x87fdd35898f7f2c4;
    root7[2] = 0x65f87d8743cc6423;
    result = siphash24(src, 4, key);
    printf("%16lx\n", result);
	Final();

	return 0;
}


/*
uint64_t *src1 = root1;
    root1[0] = 0xa66be1dc7f37fb80;
    root1[3] = 0x13931921bba211f6;
    root1[1] = 0xbff00febbae335e6;
    root1[2] = 0x201f4a6fe5211611;

	uint64_t *src2 = root2;
    root2[2] = 0xa66be1dc7f37fb80;
    root2[3] = 0x13931921bba211f6;
    root2[0] = 0xbff00febbae335e6;
    root2[1] = 0x201f4a6fe5211611;

	uint64_t *src3 = root3;
    root3[1] = 0xa66be1dc7f37fb80;
    root3[3] = 0x13931921bba211f6;
    root3[2] = 0xbff00febbae335e6;
    root3[0] = 0x201f4a6fe5211611;

	uint64_t *src4 = root4;
    root4[0] = 0xa66be1dc7f37fb80;
    root4[2] = 0x13931921bba211f6;
    root4[1] = 0xbff00febbae335e6;
    root4[3] = 0x201f4a6fe5211611;

	uint64_t *src5 = root5;
    root5[3] = 0xa66be1dc7f37fb80;
    root5[1] = 0x13931921bba211f6;
    root5[2] = 0xbff00febbae335e6;
    root5[0] = 0x201f4a6fe5211611;

	uint64_t *src6 = root6;
    root6[3] = 0xa66be1dc7f37fb80;
    root6[0] = 0x13931921bba211f6;
    root6[2] = 0xbff00febbae335e6;
    root6[1] = 0x201f4a6fe5211611;

	uint64_t *src7 = root7;
    root7[0] = 0xa66be1dc7f37fb80;
    root7[1] = 0x13931921bba211f6;
    root7[3] = 0xbff00febbae335e6;
    root7[2] = 0x201f4a6fe5211611;*/
