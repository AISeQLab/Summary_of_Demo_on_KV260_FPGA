/* <MIT License>
 Copyright (c) 2013  Marek Majkowski <marek@popcount.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 </MIT License>

 Original location:
    https://github.com/majek/csiphash/

 Solution inspired by code from:
    Samuel Neves (supercop/crypto_auth/siphash24/little)
    djb (supercop/crypto_auth/siphash24/little2)
    Jean-Philippe Aumasson (https://131002.net/siphash/siphash24.c)
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

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


#define ROTATE(x, b) (uint64_t)( ((x) << (b)) | ( (x) >> (64 - (b))) )

#define HALF_ROUND(a,b,c,d,s,t)			\
	a += b; c += d;				\
	b = ROTATE(b, s) ^ a;			\
	d = ROTATE(d, t) ^ c;			\
	a = ROTATE(a, 32);

#define DOUBLE_ROUND(v0,v1,v2,v3)		\
	HALF_ROUND(v0,v1,v2,v3,13,16);		\
	HALF_ROUND(v2,v1,v0,v3,17,21);		\
	HALF_ROUND(v0,v1,v2,v3,13,16);		\
	HALF_ROUND(v2,v1,v0,v3,17,21);

#include <time.h>

#define N 726
#define BILLION  1000000000
unsigned long long total = 0;
uint64_t siphash24(const void *src, unsigned long src_sz, const char key[16]) {
	const uint64_t *_key = (uint64_t *)key;
	uint64_t k0 = _le64toh(_key[0]);
	uint64_t k1 = _le64toh(_key[1]);
	uint64_t b = (uint64_t)src_sz << 56;
	const uint64_t *in = (uint64_t*)src;
	 struct timespec start, end;

	uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
	uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
	uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
	uint64_t v3 = k1 ^ 0x7465646279746573ULL;
	unsigned long long time_spent = 0;
	while (src_sz >= 8) {
		uint64_t mi = _le64toh(*in);
		in += 1; src_sz -= 8;
		v3 ^= mi;
		clock_gettime(CLOCK_REALTIME, &start);
		DOUBLE_ROUND(v0,v1,v2,v3);
		clock_gettime(CLOCK_REALTIME, &end);
		time_spent= BILLION * (end.tv_sec - start.tv_sec) +  (end.tv_nsec - start.tv_nsec);
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
	clock_gettime(CLOCK_REALTIME, &start);
	DOUBLE_ROUND(v0,v1,v2,v3);
	clock_gettime(CLOCK_REALTIME, &end);
	time_spent= BILLION * (end.tv_sec - start.tv_sec) +  (end.tv_nsec - start.tv_nsec) + time_spent;
	v0 ^= b; v2 ^= 0xff;
	clock_gettime(CLOCK_REALTIME, &start);
	DOUBLE_ROUND(v0,v1,v2,v3);
	DOUBLE_ROUND(v0,v1,v2,v3);
	clock_gettime(CLOCK_REALTIME, &end);
	time_spent= BILLION * (end.tv_sec - start.tv_sec) +  (end.tv_nsec - start.tv_nsec) + time_spent;
	total = total + time_spent;
	return (v0 ^ v1) ^ (v2 ^ v3);
}

int main(){

    char key[16];
    for (int i = 0; i<16; i++) key[i] = i;
    unsigned long src_sz;
    uint64_t root[4], result;
    uint64_t *src = root;
    root[0] = 0xa66be1dc7f37fb80;
    root[1] = 0x13931921bba211f6;
    root[2] = 0xbff00febbae335e6;
    root[3] = 0x201f4a6fe5211611;
	// for (int i = 0; i < 819200; i++){
		// result = siphash24(src, 4, key);
	// }
    // //printf("%16lx\n", result);
	// printf("The elapsed time is %llu nanoseconds\n", total);
    // printf("The elapsed time in seconds is %f\n", (double)total / BILLION); // Added this line for clarity
			struct timespec start_cpu, end_cpu;
	unsigned long long time_spent_cpu = 0;
	 unsigned char ciphertext[16];
    int len;
    
    clock_gettime(CLOCK_REALTIME, &start_cpu);
    for (int l = 0; l < BILLION; l++) {
		result = siphash24(src, 4, key);
    }
    clock_gettime(CLOCK_REALTIME, &end_cpu);

    time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + end_cpu.tv_nsec - start_cpu.tv_nsec;
    printf("CPU execution time of 131072 in milliseconds is %f\n", (double)time_spent_cpu / 1000000);

	clock_gettime(CLOCK_REALTIME, &start_cpu);
	
	time_spent_cpu = 0;
	for( int l = 0; l<196608;l++){
		result = siphash24(src, 4, key);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 196608 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<262144;l++){
		result = siphash24(src, 4, key);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 262144 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity

	time_spent_cpu = 0;
	for( int l = 0; l<327680;l++){
		result = siphash24(src, 4, key);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 327680 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity


	time_spent_cpu = 0;
	for( int l = 0; l<393216;l++){
		result = siphash24(src, 4, key);
	}
	clock_gettime(CLOCK_REALTIME, &end_cpu);
	time_spent_cpu = BILLION * (end_cpu.tv_sec - start_cpu.tv_sec) + (end_cpu.tv_nsec - start_cpu.tv_nsec) + time_spent_cpu;
	
	 printf("CPU execution time of 393216 in miliseconds is %f\n", (double)time_spent_cpu*1000/ BILLION); // Added this line for clarity
						
    return 0;
}