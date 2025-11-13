#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#define u64 unsigned long long
#include <time.h>

#define N 726
#define BILLION  1000000000

#define ROR64(x,r) (((x)>>(r))|((x)<<(64-(r))))
#define ROL64(x,r) (((x)<<(r))|((x)>>(64-(r))))
#define R(x,y,k) (x=ROR64(x,8), x+=y, x^=k, y=ROL64(y,3), y^=x)
#define RI(x,y,k) (y^=x, y=ROR64(y,3), x^=k, x-=y, x=ROL64(x,8))





unsigned long long total;
int Encrypt(u64 *u,u64 *v,u64 key[])
{
  u64 i,x=*u,y=*v;
    // printf("x = %16lx\n", x);
  // printf("y = %16lx\n", y);
struct timespec start, end;
  for(i=0;i<32;i++) {
    clock_gettime(CLOCK_REALTIME, &start);
    R(x,y,key[i]);
    clock_gettime(CLOCK_REALTIME, &end);
    unsigned long long time_spent = BILLION * (end.tv_sec - start.tv_sec) +  (end.tv_nsec - start.tv_nsec);
    total = total + time_spent;
    }

  *u=x; *v=y;

  return 0;
}



int Decrypt(u64 *u,u64 *v,u64 *key)
{
  int i;
  u64 x=*u,y=*v;

  for(i=31;i>=0;i--) RI(x,y,key[i]);

  *u=x; *v=y;
  // printf("%16lx\n", x);
  // printf("%16lx\n", y);
  return 0;
}



int ExpandKey(u64 K[],u64 key[])
{
	
  u64 i,B=K[1],A=K[0];
  // printf("K[0] = %16lx\n", K[0]);
  // printf("K[0] = %16lx\n", K[1]);

struct timespec start, end;
  for(i=0;i<32;i++){
    key[i]=A;
    clock_gettime(CLOCK_REALTIME, &start);
     R(B,A,i);
    clock_gettime(CLOCK_REALTIME, &end);
    unsigned long long time_spent = BILLION * (end.tv_sec - start.tv_sec) +  (end.tv_nsec - start.tv_nsec);
    total = total + time_spent;
  }

  return 0;
}

int crypto_stream_speck128128ctr_ref(
  unsigned char *out, 
  unsigned long long outlen, 
  const unsigned char *n, 
  const unsigned char *k
)
{
  u64 i,nonce[2],K[2],key[32],x,y,t;
  unsigned char *block= (unsigned char *)malloc(16);   

  if (!outlen) {free(block); return 0;}

  nonce[0]=((u64*)n)[0];
  nonce[1]=((u64*)n)[1];

  for(i=0;i<2;i++) K[i]=((u64 *)k)[i];

  ExpandKey(K,key);

  t=0;
  while(outlen>=16){
    x=nonce[1]; y=nonce[0]; nonce[0]++;
    Encrypt(&x,&y,key);                          
    ((u64 *)out)[1+t]=x; 
    ((u64 *)out)[0+t]=y; 
    t+=2;                                      
    outlen-=16;                                  
  }

  if (outlen>0){
    x=nonce[1]; y=nonce[0];
    Encrypt(&x,&y,key);
    ((u64 *)block)[1]=x; ((u64 *)block)[0]=y;
    for(i=0;i<outlen;i++) out[i+8*t]=block[i];
  }

  free(block);

  return 0;
}




int crypto_stream_speck128128ctr_ref_xor(
  unsigned char *out, 
  const unsigned char *in, 
  unsigned long long inlen, 
  const unsigned char *n, 
  const unsigned char *k)
{
  u64 i,nonce[2],K[2],key[32],x,y,t;
  unsigned char *block= (unsigned char *) malloc(16); 
 
  if (!inlen) {free(block); return 0;}

  nonce[0]=((u64*)n)[0];
  nonce[1]=((u64*)n)[1];

  for(i=0;i<2;i++) K[i]=((u64 *)k)[i];

  ExpandKey(K,key);

  t=0;
  while(inlen>=16){
    x=nonce[1]; y=nonce[0]; nonce[0]++;
    Encrypt(&x,&y,key);                          
    ((u64 *)out)[1+t]=x^((u64 *)in)[1+t]; 
    ((u64 *)out)[0+t]=y^((u64 *)in)[0+t]; 
    t+=2;                                      
    inlen-=16;                                  
  }
  if (inlen>0){
    x=nonce[1]; y=nonce[0];       
    Encrypt(&x,&y,key);
    ((u64 *)block)[1]=x; ((u64 *)block)[0]=y;
    for(i=0;i<inlen;i++) out[i+8*t]=block[i]^in[i+8*t];
  }

  free(block);

  return 0;
}



int main(){
	unsigned char output[16]; 
	unsigned char *out = output;
	uint64_t outlen = 16;
	unsigned char data[16];
	unsigned char *n = data;
	unsigned char key[16];

  u64 kk[32];
	//intit 
	printf("Data: \n");
	for (int i = 0; i<16; i++){
		data[i] = i*8-5;
		printf("%02x", data[i]);
		key[i] = i;
	}
  
  unsigned char *k = key;
	total = 0;
  for (int i = 0 ; i < BILLION; i++){
	int c = crypto_stream_speck128128ctr_ref(out, outlen, n, k);
  }

  // printf("\n");
    // printf("\nOutput:\n");
  // for (int  i = 0; i<16; i++) printf("%02x", out[i]);

    
   // printf("The elapsed time is %llu nanoseconds\n", total);
    printf("The elapsed time of 131072 in miliseconds is %f\n", (double)total*1000 / BILLION); // Added this line for clarity
	total = 0;
	for (int i = 0 ; i < 196608; i++){
	int c = crypto_stream_speck128128ctr_ref(out, outlen, n, k);
  }

    printf("The elapsed time of 196608 in miliseconds is %f\n", (double)total*1000 / BILLION); // Added this line for clarity
	total = 0;
	for (int i = 0 ; i < 262144; i++){
	int c = crypto_stream_speck128128ctr_ref(out, outlen, n, k);
  }

    printf("The elapsed time of 262144 in miliseconds is %f\n", (double)total*1000 / BILLION); // Added this line for clarity
	total = 0;
	for (int i = 0 ; i < 327680; i++){
	int c = crypto_stream_speck128128ctr_ref(out, outlen, n, k);
  }

    printf("The elapsed time of 327680 in miliseconds is %f\n", (double)total*1000 / BILLION); // Added this line for clarity	
		total = 0;
	for (int i = 0 ; i < 393216; i++){
	int c = crypto_stream_speck128128ctr_ref(out, outlen, n, k);
  }

    printf("The elapsed time of 393216 in miliseconds is %f\n", (double)total*1000 / BILLION); // Added this line for clarity	
	return 0;
}
