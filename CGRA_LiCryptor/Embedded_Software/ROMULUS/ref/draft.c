
#include<stdio.h>
#include<stdint.h>

const unsigned char TWEAKEY_P[16] = {9,15,8,13,10,14,12,11,0,1,2,3,4,5,6,7};



void AddKey(unsigned char state[4][4], unsigned char key0[4][4], unsigned char key1[4][4], unsigned char key2[4][4])
{
	int i, j, k;
	unsigned char pos;
	unsigned char keyCells_tmp[3][4][4];

    // apply the subtweakey to the internal state
    for(i = 0; i <= 1; i++)   //mapping
    {
        for(j = 0; j < 4; j++)
        {
            state[i][j] ^= key0[i][j] ^ key1[i][j] ^ key2[i][j];            
        }
    }

    // update the subtweakey states with the permutation

        for(i = 0; i < 4; i++){
            for(j = 0; j < 4; j++){
                //application of the TWEAKEY permutation
                pos=TWEAKEY_P[j+4*i];
                keyCells_tmp[0][i][j]=key0[pos>>2][pos&0x3];
                keyCells_tmp[1][i][j]=key1[pos>>2][pos&0x3];
                keyCells_tmp[2][i][j]=key2[pos>>2][pos&0x3];

            }
        }

    for(k = 0; k <3; k++){
        for(i = 0; i < 4; i++){
            for(j = 0; j < 4; j++){
                key0[i][j]=keyCells_tmp[0][i][j];
                key1[i][j]=keyCells_tmp[1][i][j];
                key2[i][j]=keyCells_tmp[2][i][j];          
            }
        }
     }
    // update the subtweakey states with the LFSRs
    for(k = 0; k <3; k++){
        for(i = 0; i <= 1; i++){
            for(j = 0; j < 4; j++){
                //application of LFSRs for TK updates
                if (k==1)
                {
		  			keyCells_tmp[k][i][j]=((key1[i][j]<<1)&0xFE)^((key1[i][j]>>7)&0x01)^((key1[i][j]>>5)&0x01);
                }

                else if (k==2)
                {
		  			keyCells_tmp[k][i][j]=((key2[i][j]>>1)&0x7F)^((key2[i][j]<<7)&0x80)^((key2[i][j]<<1)&0x80);
                }
            }
        }
    }

    for(k = 0; k <3; k++){
        for(i = 0; i < 4; i++){
            for(j = 0; j < 4; j++){
                key0[i][j]=keyCells_tmp[0][i][j];
                key1[i][j]=keyCells_tmp[1][i][j];
                key2[i][j]=keyCells_tmp[2][i][j];          
            }
        }
     }

    printf("STATE: \n");
    show(state);

    printf("TWK0: \n");
    show(keyCells_tmp[0]);

    printf("TWK1: \n");
    show(keyCells_tmp[1]);

    printf("TWK2: \n");
    show(keyCells_tmp[2]);
    
}
const unsigned char RC[40] = {
		0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3E, 0x3D, 0x3B, 0x37, 0x2F,
		0x1E, 0x3C, 0x39, 0x33, 0x27, 0x0E, 0x1D, 0x3A, 0x35, 0x2B,
		0x16, 0x2C, 0x18, 0x30, 0x21, 0x02, 0x05, 0x0B, 0x17, 0x2E,
		0x1C, 0x38, 0x31, 0x23, 0x06, 0x0D, 0x1B, 0x36, 0x2D, 0x1A};
void AddConstants(unsigned char state[4][4], int r)
{
	state[0][0] ^= (RC[r] & 0xf);
	state[1][0] ^= ((RC[r]>>4) & 0x3);
	state[2][0] ^= 0x2;

    show(state);


}

void ShiftRows(unsigned char state[4][4])
{
    unsigned char tmp;
    tmp = state[1][3];
    state[1][3] = state[1][2];
    state[1][2] = state[1][1];
    state[1][1] = state[1][0];
    state[1][0] = tmp;
 
    tmp = state[2][0];
    state[2][0] = state[2][2];
    state[2][2] = tmp;
    tmp = state[2][1];
    state[2][1] = state[2][3];
    state[2][3] = tmp;
 
    tmp = state[3][0];
    state[3][0] = state[3][1];
    state[3][1] = state[3][2];
    state[3][2] = state[3][3];
    state[3][3] = tmp;

    show(state); 
    
}
const unsigned char sbox_8[256] = {0x65 , 0x4c , 0x6a , 0x42 , 0x4b , 0x63 , 0x43 , 0x6b , 0x55 , 0x75 , 0x5a , 0x7a , 0x53 , 0x73 , 0x5b , 0x7b ,0x35 , 0x8c , 0x3a , 0x81 , 0x89 , 0x33 , 0x80 , 0x3b , 0x95 , 0x25 , 0x98 , 0x2a , 0x90 , 0x23 , 0x99 , 0x2b ,0xe5 , 0xcc , 0xe8 , 0xc1 , 0xc9 , 0xe0 , 0xc0 , 0xe9 , 0xd5 , 0xf5 , 0xd8 , 0xf8 , 0xd0 , 0xf0 , 0xd9 , 0xf9 ,0xa5 , 0x1c , 0xa8 , 0x12 , 0x1b , 0xa0 , 0x13 , 0xa9 , 0x05 , 0xb5 , 0x0a , 0xb8 , 0x03 , 0xb0 , 0x0b , 0xb9 ,0x32 , 0x88 , 0x3c , 0x85 , 0x8d , 0x34 , 0x84 , 0x3d , 0x91 , 0x22 , 0x9c , 0x2c , 0x94 , 0x24 , 0x9d , 0x2d ,0x62 , 0x4a , 0x6c , 0x45 , 0x4d , 0x64 , 0x44 , 0x6d , 0x52 , 0x72 , 0x5c , 0x7c , 0x54 , 0x74 , 0x5d , 0x7d ,0xa1 , 0x1a , 0xac , 0x15 , 0x1d , 0xa4 , 0x14 , 0xad , 0x02 , 0xb1 , 0x0c , 0xbc , 0x04 , 0xb4 , 0x0d , 0xbd ,0xe1 , 0xc8 , 0xec , 0xc5 , 0xcd , 0xe4 , 0xc4 , 0xed , 0xd1 , 0xf1 , 0xdc , 0xfc , 0xd4 , 0xf4 , 0xdd , 0xfd ,0x36 , 0x8e , 0x38 , 0x82 , 0x8b , 0x30 , 0x83 , 0x39 , 0x96 , 0x26 , 0x9a , 0x28 , 0x93 , 0x20 , 0x9b , 0x29 ,0x66 , 0x4e , 0x68 , 0x41 , 0x49 , 0x60 , 0x40 , 0x69 , 0x56 , 0x76 , 0x58 , 0x78 , 0x50 , 0x70 , 0x59 , 0x79 ,0xa6 , 0x1e , 0xaa , 0x11 , 0x19 , 0xa3 , 0x10 , 0xab , 0x06 , 0xb6 , 0x08 , 0xba , 0x00 , 0xb3 , 0x09 , 0xbb ,0xe6 , 0xce , 0xea , 0xc2 , 0xcb , 0xe3 , 0xc3 , 0xeb , 0xd6 , 0xf6 , 0xda , 0xfa , 0xd3 , 0xf3 , 0xdb , 0xfb ,0x31 , 0x8a , 0x3e , 0x86 , 0x8f , 0x37 , 0x87 , 0x3f , 0x92 , 0x21 , 0x9e , 0x2e , 0x97 , 0x27 , 0x9f , 0x2f ,0x61 , 0x48 , 0x6e , 0x46 , 0x4f , 0x67 , 0x47 , 0x6f , 0x51 , 0x71 , 0x5e , 0x7e , 0x57 , 0x77 , 0x5f , 0x7f ,0xa2 , 0x18 , 0xae , 0x16 , 0x1f , 0xa7 , 0x17 , 0xaf , 0x01 , 0xb2 , 0x0e , 0xbe , 0x07 , 0xb7 , 0x0f , 0xbf ,0xe2 , 0xca , 0xee , 0xc6 , 0xcf ,0xe7 , 0xc7 , 0xef , 0xd2 , 0xf2 , 0xde , 0xfe , 0xd7 , 0xf7 , 0xdf , 0xff};

void MixColumn(unsigned char state[4][4])
{
	int j;
    unsigned char temp;

	for(j = 0; j < 4; j++){
        state[1][j]^=state[2][j];
        state[2][j]^=state[0][j];
        state[3][j]^=state[2][j];

        temp=state[3][j];
        state[3][j]=state[2][j];
        state[2][j]=state[1][j];
        state[1][j]=state[0][j];
        state[0][j]=temp;
	}

    show(state);

}

void SubCell8(unsigned char state[4][4])
{
	int i,j;
	for(i = 0; i < 4; i++){
		for(j = 0; j <  4; j++){
			state[i][j] = sbox_8[state[i][j]];
		}
	}
    show(state);
}
void Skinny(unsigned char s[4][4], unsigned char k0[4][4], unsigned char k1[4][4], unsigned char k2[4][4], int r){

	 SubCell8(s);
	 AddConstants(s, r);
	 AddKey(s, k0, k1, k2);
	 ShiftRows(s);
	 MixColumn(s);
}

void show(unsigned char s[4][4]){
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 4; j++){
               printf("%02x", s[i][j]); 
            }
            printf("    ");
        }
        printf("\n");
}
void show_list(unsigned char s[4][4], unsigned char k0[4][4], unsigned char k1[4][4], unsigned char k2[4][4]){
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 4; j++){
               printf("%02x", s[i][j]); 
            }
            printf("\n");
        }
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 4; j++){
               printf("%02x", k0[i][j]); 
            }
            printf("\n");
        }
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 4; j++){
               printf("%02x", k1[i][j]); 
            }
            printf("\n");
        }
        for(int i = 0; i < 4; i++){
            for(int j = 0; j < 4; j++){
               printf("%02x", k2[i][j]); 
            }
            printf("\n");
        }

}
int main(){

//     unsigned char pos;
//     unsigned char keyCells_tmp[4][4], key0[4][4], key1[4][4], key2[4][4], state[4][4];
//     //2da96eb2 332f0211 08cf9556 0f814c25
//     key0[0][0] = 0x2d;
//     key0[0][1] = 0xa9;
//     key0[0][2] = 0x6e;
//     key0[0][3] = 0xb2;
//     key0[1][0] = 0x33;
//     key0[1][1] = 0x2f;
//     key0[1][2] = 0x02;
//     key0[1][3] = 0x11;
//     key0[2][0] = 0x08;
//     key0[2][1] = 0xcf;
//     key0[2][2] = 0x95;
//     key0[2][3] = 0x56;
//     key0[3][0] = 0x0f;
//     key0[3][1] = 0x81;
//     key0[3][2] = 0x4c;
//     key0[3][3] = 0x25;
// //6eb22da9 0211332f b8ca01f8 e33e8248
//     key1[0][0] = 0x6e;
//     key1[0][1] = 0xb2;
//     key1[0][2] = 0x2d;
//     key1[0][3] = 0xa9;
//     key1[1][0] = 0x02;
//     key1[1][1] = 0x11;
//     key1[1][2] = 0x33;
//     key1[1][3] = 0x2f;
//     key1[2][0] = 0xb8;
//     key1[2][1] = 0xca;
//     key1[2][2] = 0x01;
//     key1[2][3] = 0xf8;
//     key1[3][0] = 0xe3;
//     key1[3][1] = 0x3e;
//     key1[3][2] = 0x82;
//     key1[3][3] = 0x48;
// //332f0211 2da96eb2 08cf9556 0f814c25
//     key2[0][0] = 0x3c;
//     key2[0][1] = 0x12;
//     key2[0][2] = 0x6a;
//     key2[0][3] = 0x1e;
//     key2[1][0] = 0xba;
//     key2[1][1] = 0x69;
//     key2[1][2] = 0x0c;
//     key2[1][3] = 0x12;
//     key2[2][0] = 0x88;
//     key2[2][1] = 0x16;
//     key2[2][2] = 0xaf;
//     key2[2][3] = 0xea;
//     key2[3][0] = 0x64;
//     key2[3][1] = 0x8a;
//     key2[3][2] = 0x36;
//     key2[3][3] = 0xcb;
// //0xbba2118f,0x7f37fb8e,0xbae335e6,0xe5211611
//     state[0][0] = 0xbb;
//     state[0][1] = 0xa2;
//     state[0][2] = 0x11;
//     state[0][3] = 0x8f;
//     state[1][0] = 0x7f;
//     state[1][1] = 0x37;
//     state[1][2] = 0xfb;
//     state[1][3] = 0x8e;
//     state[2][0] = 0xba;
//     state[2][1] = 0xe3;
//     state[2][2] = 0x35;
//     state[2][3] = 0xe6;
//     state[3][0] = 0xe5;
//     state[3][1] = 0x21;
//     state[3][2] = 0x16;
//     state[3][3] = 0x11;

    unsigned char state[4][4];
	unsigned char keyCells[3][4][4];
	int i;
    
	unsigned char* input;
    input = 0x0994720807;
    unsigned char key;
	unsigned char* userkey = key;
    printf("%02x", input[1]);
	//memset(keyCells, 0, 48);	
	for(i = 0; i < 16; i++) {
        state[i>>2][i&0x3] = input[i]&0xFF;
        keyCells[0][i>>2][i&0x3] = userkey[i]&0xFF;
	    keyCells[1][i>>2][i&0x3] = userkey[i+16]&0xFF;
	    keyCells[2][i>>2][i&0x3] = userkey[i+32]&0xFF;
	}


    return 0;
}