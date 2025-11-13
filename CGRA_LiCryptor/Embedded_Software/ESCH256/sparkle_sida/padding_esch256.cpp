#include <stdio.h>
#include <stdint.h>
#include <string.h>
using namespace std;
#define ROT(x, n) (((x) >> (n)) | ((x) << (32-(n))))
#define ELL(x) (ROT(((x) ^ ((x) << 16)), 16))

typedef unsigned char UChar;
typedef unsigned long long int ULLInt;
// Round constants
static const uint32_t RCON[8] = {      \
  0xB7E15162, 0xBF715880, 0x38B4DA56, 0x324E7738, \
  0xBB1185EB, 0x4F7C7B57, 0xCFBFA1C8, 0xC2B3293D  \
};

#define STATE_BYTES     48
#define RATE_BYTES      16
#define STEPS_SLIM       7
#define STEPS_BIG       11


#define ESCH_DIGEST_WORDS (ESCH_DIGEST_BYTES/4)

#define STATE_BRANS (STATE_BYTES/8) //6
#define STATE_WORDS (STATE_BYTES/4) //12
#define RATE_BRANS  (RATE_BYTES/8)
#define RATE_WORDS  (RATE_BYTES/4) //4
#define CAP_BYTES   (STATE_BYTES-RATE_BYTES)
#define CAP_BRANS   (CAP_BYTES/8)
#define CAP_WORDS   (CAP_BYTES/4)

#define CONST_M1 (((uint32_t) 1) << 24)
#define CONST_M2 (((uint32_t) 2) << 24)
void sparkle(uint32_t *state, int brans, int steps)
{
  int i, j;  // Step and branch counter
  uint32_t rc, tmpx, tmpy, x0, y0;
  
  for(i = 0; i < steps; i ++) {
    // Add round constant
    state[1] ^= RCON[i%8];
    state[3] ^= i;
    // ARXBOX layer
    for(j = 0; j < 2*brans; j += 2) {
      rc = RCON[j>>1];
      state[j] += ROT(state[j+1], 31);
      state[j+1] ^= ROT(state[j], 24);
      state[j] ^= rc;
      state[j] += ROT(state[j+1], 17);
      state[j+1] ^= ROT(state[j], 17);
      state[j] ^= rc;
      state[j] += state[j+1];
      state[j+1] ^= ROT(state[j], 31);
      state[j] ^= rc;
      state[j] += ROT(state[j+1], 24);
      state[j+1] ^= ROT(state[j], 16);
      state[j] ^= rc;
    }
    // Linear layer
    tmpx = x0 = state[0];
    tmpy = y0 = state[1];
    for(j = 2; j < brans; j += 2) {
      tmpx ^= state[j];
      tmpy ^= state[j+1];
    }
    tmpx = ELL(tmpx);
    tmpy = ELL(tmpy);
    for (j = 2; j < brans; j += 2) {
      state[j-2] = state[j+brans] ^ state[j] ^ tmpy;
      state[j+brans] = state[j];
      state[j-1] = state[j+brans+1] ^ state[j+1] ^ tmpx;
      state[j+brans+1] = state[j+1];
    }
    state[brans-2] = state[brans] ^ x0 ^ tmpy;
    state[brans] = x0;
    state[brans-1] = state[brans+1] ^ y0 ^ tmpx;
    state[brans+1] = y0;
  }
}

static void add_msg_blk(uint32_t *state, const uint8_t *in, int aligned)
{
  uint32_t buffer[RATE_WORDS];
  uint32_t *in32;
  uint32_t tmpx = 0, tmpy = 0;
  int i;
  
  if (aligned) {  // `in` can be casted to uint32_t pointer
    in32 = (uint32_t *) in;
  } else {  // `in` is not sufficiently aligned for casting
    memcpy(buffer, in, RATE_BYTES);
    in32 = (uint32_t *) buffer;
  }
  printf("Add message block: ");
  for (int i = 0; i < 4; i++)
    printf("%02X ", in32[i]);
  printf("\n");
  
}

static void add_msg_blk_last(uint32_t *state, const uint8_t *in, size_t inlen)
{
  uint32_t buffer[RATE_WORDS];
  uint8_t *bufptr;
  uint32_t tmpx = 0, tmpy = 0;
  int i;
  printf("Add last message block in: ");
  for (int i = 0; i < inlen; i++)
  printf("%02X ", in[i]);
  printf("\n");

  memcpy(buffer, in, inlen);
  printf("Add last message block buffer: ");
  for (int i = 0; i < 4; i++)
  printf("%02X ", buffer[i]);
  printf("\n");
  if (inlen < RATE_BYTES) {  // padding
    bufptr = ((uint8_t *) buffer) + inlen;
    memset(bufptr, 0, (RATE_BYTES - inlen));
    *bufptr = 0x80;
  }
  printf("Add last message block: ");
  for (int i = 0; i < 4; i++)
    printf("%02X ", buffer[i]);
  printf("\n");
  
}

void Initialize(uint32_t *state)
{
  int i;
  
  for (i = 0; i < STATE_WORDS; i++) {
    state[i] = 0;
  }
}
void init_buffer(UChar *buffer, ULLInt numbytes)
{
  ULLInt i;
  
  for (i = 0; i < numbytes; i++)
    buffer[i] = (UChar) i;
}


void fprint_bstr(FILE *fp, const char *label, const UChar *data, ULLInt length)
{ 
  ULLInt i;
  
  fprintf(fp, "%s", label); 
  for (i = 0; i < length; i++)
    fprintf(fp, "%02X", data[i]);
  fprintf(fp, "\n");
}

void ProcessMessage(uint32_t *state, const uint8_t *in, size_t inlen)
{
  // check whether `in` can be casted to uint32_t pointer (we can use here
  // size_t instead of uintptr_t since ALIGN_OF_UI32 is either 1, 2, or 4)
  int aligned = ((size_t) in) % 1 == 0;
  printf("aligned: %0d\n", aligned);
  // printf("Address of `in`: %p\n", in);
  
  // Main Hashing Loop
  
  while (inlen > RATE_BYTES) {
    // addition of a message block to the state
    add_msg_blk(state, in, aligned);
    // execute SPARKLE with slim number of steps
    sparkle(state, STATE_BRANS, STEPS_SLIM);
    inlen -= RATE_BYTES;
    in += RATE_BYTES;
  }
  
  // Hashing of Last Block
  
  // addition of constant M1 or M2 to the state
  state[STATE_BRANS-1] ^= ((inlen < RATE_BYTES) ? CONST_M1 : CONST_M2);
  printf("STATE: ");
  for (int i = 0; i < 12; i++)
  printf("%02X ", state[i]);
  printf("\n");
  // addition of last msg block (incl. padding)
  add_msg_blk_last(state, in, inlen);
  // execute SPARKLE with big number of steps
  sparkle(state, STATE_BRANS, STEPS_BIG);
}

void Finalize(uint32_t *state, uint8_t *out)
{
  size_t outlen;
  
  memcpy(out, state, RATE_BYTES);
  outlen = RATE_BYTES;
  out += RATE_BYTES;
  while (outlen < 32) {
    sparkle(state, STATE_BRANS, STEPS_SLIM);
    memcpy(out, state, RATE_BYTES);
    outlen += RATE_BYTES;
    out += RATE_BYTES;
  }
  printf("Final: ");
  for (int i = -32; i < 0; i++)
  printf("%02X ", out[i]);
  printf("\n");

}
#include <time.h>
 unsigned long long total = 0;
#define N 726
#define BILLION  1000000000
int crypto_hash(UChar *out, const UChar *in, ULLInt inlen)
{
  uint32_t state[STATE_WORDS];
  size_t insize = (size_t) inlen;
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  Initialize(state);
  ProcessMessage(state, in, insize);
  Finalize(state, out);
   clock_gettime(CLOCK_REALTIME, &end);
   unsigned long long time_spent = BILLION * (end.tv_sec - start.tv_sec) +
                                    (end.tv_nsec - start.tv_nsec);
    
    printf("The elapsed time is %llu nanoseconds\n", time_spent);
    printf("The elapsed time in seconds is %f\n", (double)time_spent / BILLION); // Added this line for clarity
    total = total + time_spent;
  
  return 0;
}

int main()
{
  FILE *fp;
  char fileName[256];
  uint8_t msg[1024], digest[32];
  int ret_val = 0, count = 1;
  ULLInt mlen;
  msg[0] = (uint8_t) 0x0;
  msg[1] = (uint8_t) 0x1;
  msg[2] = (uint8_t) 0x2;  
  msg[3] = (uint8_t) 0x3;
  msg[4] = (uint8_t) 0x4;
  msg[5] = (uint8_t) 0x5;
  msg[6] = (uint8_t) 0x6;
  msg[7] = (uint8_t) 0x7;
  msg[8] = (uint8_t) 0x8;
  msg[9] = (uint8_t) 0x9;
  msg[10] = (uint8_t)0x10;
  msg[11] = (uint8_t)0x11;
  msg[12] = (uint8_t)0x12;
  msg[13] = (uint8_t)0x13;
  msg[14] = (uint8_t)0x14;
  msg[15] = (uint8_t)0x15;
  msg[16] = (uint8_t)0x16;
  msg[17] = (uint8_t)0x17;
  for (int c = 0; c < 819200; c++){
  ret_val = crypto_hash(digest, msg, 3);
  for (int i = 0; i < 32; i++)
    printf("%02X", digest[i]);
  }
  printf("The elapsed time is %llu nanoseconds\n", total);
    printf("The elapsed time in seconds is %f\n", (double)total / BILLION); // Added this line for clarity
  return 0;
}


