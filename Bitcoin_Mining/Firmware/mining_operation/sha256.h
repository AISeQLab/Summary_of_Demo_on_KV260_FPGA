/*********************************************************************
* Filename:   sha256.h
* Author:     HoaiLuan
* Reference: Brad Conte (brad AT bradconte.com)
*********************************************************************/

#ifndef SHA256_H
#define SHA256_H

/*************************** HEADER FILES ***************************/
#include <stddef.h>

/**************************** DATA TYPES ****************************/
typedef unsigned int  WORD;            

typedef struct {
	WORD data[16];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];
} SHA256_CTX2;

/*********************** FUNCTION DECLARATIONS **********************/

void sha256_init_1(SHA256_CTX2 *ctx);
void sha256_update_1(SHA256_CTX2 *ctx, const WORD data[], WORD hash[]);

void sha256_init_2(SHA256_CTX2 *ctx, const WORD data[]);
void sha256_update_2(SHA256_CTX2 *ctx, const WORD data[], WORD hash[]);

void sha256_init_3(SHA256_CTX2 *ctx);
void sha256_update_3(SHA256_CTX2 *ctx, const WORD data[], WORD hash[]);

#endif   
