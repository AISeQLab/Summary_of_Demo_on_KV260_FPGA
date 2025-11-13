#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define BILLION  1000000000

#define NO_OP                       0
#define H_GATE                      1
#define S_GATE                      2
#define CNOT_GATE                   3
#define RX_GATE                     4
#define RY_GATE                     5
#define RZ_GATE                     6

#define START_BASE_PHYS             (0x0000000)
#define LOAD_BASE_PHYS              (0x0000008>>3)
#define DONE_BASE_PHYS              (0x0000010>>3)
#define QUBIT_BASE_PHYS             (0x0000018>>3)
#define QUBIT_NUM_BASE_PHYS         (0x0000020>>3)
                                              
#define QCTR_N_CUT_MEM_BASE_PHYS    (0x0040000>>3)
#define CORDIC1_MEM_BASE_PHYS       (0x0080000>>3)
#define CORDIC2_MEM_BASE_PHYS       (0x00C0000>>3)
#define GATE_MEM_BASE_PHYS          (0x0100000>>3)

// Read control
#define COMPLETE_BASE_PHYS          (0x0000000)
    
// Write/Read Data    
#define PING_MEM_BASE_PHYS          0x1000000
#define PONG_MEM_BASE_PHYS          0x2000000

#include "CGRA.h"
#include "FPGA_Driver.c"

int qubit;
int qubit_num;
U64 *Ping_Mem, *Pong_Mem;

int addr = 0;
void Initialize_Data(){
    Ping_Mem[0] = 0x3c000000;
    for(int k = 1; k < qubit; k++){
        Ping_Mem[k] = 0;
    }
    for(int k = 0; k < qubit; k++){
        Pong_Mem[k] = 0;
    }
    dma_write(0x10000, PING_MEM_BASE_PHYS, qubit);
    dma_write(0x20000, PONG_MEM_BASE_PHYS, qubit);
    
    // dma_read(0x10000, PING_MEM_BASE_PHYS, qubit);
    
    // for (int k = 0; k < qubit; k++) {
        // printf("%08x\n", Ping_Mem[k]);
    // }
}

// Function to convert float to 16-bit floating point (half-precision)
uint32_t float_to_hex(float f) {
    return *((uint32_t*)&f);
}

int32_t floatToFixedPoint24(float input) {
    // Define the scaling factor for the fractional part
    const int32_t scaleFactor = 1 << 22;  // 2^22
    
    // Check for overflow: if input exceeds the representable range, clip it
    if (input > 1.0f) {
        input = 1.0f;
    } else if (input < -1.0f) {
        input = -1.0f;
    }
    
    // Convert the float to fixed-point
    int32_t fixedPointValue = (int32_t)(input * scaleFactor);
    
    return (fixedPointValue&0xFFFFFF);
}

// Function to convert 24-bit fixed-point to float
float fixedPoint24ToFloat(int32_t fixedPointValue) {
    // Handle sign extension for 24-bit negative numbers
    if (fixedPointValue & 0x800000) {  // Check the sign bit (23rd bit)
        fixedPointValue |= 0xFF000000; // Sign extend to 32-bit
    }

    // Define the scaling factor for the fractional part (22 bits)
    const float scaleFactor = 1.0f / (1 << 22);  // 1 / 2^22

    // Convert the fixed-point value to float
    float floatValue = fixedPointValue * scaleFactor;

    return floatValue;
}

void Load(){
    *(CGRA_info.reg_mmap + LOAD_BASE_PHYS) = 1;
}

void Start(){
    *(CGRA_info.reg_mmap + START_BASE_PHYS) = 1;
}

void Done(){
    *(CGRA_info.reg_mmap + DONE_BASE_PHYS) = 1;
}

void Wait(){
    while(1){
        if((*(CGRA_info.reg_mmap + COMPLETE_BASE_PHYS)) == 1){
            break;
        }
    }
}

void Qubit(int n){
    qubit_num = n;
    qubit = (U64)pow(2, n) - 1;
    *(CGRA_info.reg_mmap + QUBIT_BASE_PHYS) = qubit;
    *(CGRA_info.reg_mmap + QUBIT_NUM_BASE_PHYS) = n;
}

void H_Gate(int n){
    U64 cut = (U64)pow(2, (qubit_num - n - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64)n << 17) | cut;
    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = H_GATE;
    addr++;
}

void CNOT_Gate(int control, int target){
    U64 cut = (U64)pow(2, (qubit_num - target - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64) control << 22) |((U64)target << 17) | cut;
    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = CNOT_GATE;
    addr++;
}

void S_Gate(int n){
    U64 cut = (U64)pow(2, (qubit_num - n - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64)n << 17) | 0;
    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = 0;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = S_GATE;
    addr++;
}

void RX_Gate(int n, float phi){
    U64 cut = (U64)pow(2, (qubit_num - n - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64)n << 17) | cut;
	    // Calculate sine and cosine
    float sin_phi = sin(phi/2);
    float cos_phi = cos(phi/2);

    // Convert to 16-bit floating point
    uint32_t sin_n_half = floatToFixedPoint24(sin_phi);
    uint32_t cos_n_half = floatToFixedPoint24(cos_phi);
	
    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = cos_n_half;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = sin_n_half;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = RX_GATE;
    addr++;
}

void RZ_Gate(int n, float phi){
    U64 cut = (U64)pow(2, (qubit_num - n - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64)n << 17) | cut;
	    // Calculate sine and cosine
    float sin_phi = sin(phi/2);
    float cos_phi = cos(phi/2);

    // Convert to 16-bit floating point
    uint32_t sin_n_half = floatToFixedPoint24(sin_phi);
    uint32_t cos_n_half = floatToFixedPoint24(cos_phi);

    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = cos_n_half;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = sin_n_half;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = RZ_GATE;
    addr++;
}

void RY_Gate(int n, float phi){
    U64 cut = (U64)pow(2, (qubit_num - n - 1));
    // printf("Applying H_Gate on qubit %d with cut %u\n", n, cut);
    *(CGRA_info.reg_mmap + QCTR_N_CUT_MEM_BASE_PHYS + addr) = ((U64)n << 17) | cut;
	    // Calculate sine and cosine
    float sin_phi = sin(phi/2);
    float cos_phi = cos(phi/2);

    // Convert to 16-bit floating point
    uint32_t sin_n_half = floatToFixedPoint24(sin_phi);
    uint32_t cos_n_half = floatToFixedPoint24(cos_phi);
	
    *(CGRA_info.reg_mmap + CORDIC1_MEM_BASE_PHYS + addr) = cos_n_half;
    *(CGRA_info.reg_mmap + CORDIC2_MEM_BASE_PHYS + addr) = sin_n_half;
    *(CGRA_info.reg_mmap + GATE_MEM_BASE_PHYS + addr) = RY_GATE;
    addr++;
}

// Function to convert half-precision floating point to single-precision floating point
float half_to_float(uint32_t h) {
    uint32_t h_exp = (h >> 10) & 0x1F;  // Extract exponent
    uint32_t h_mant = h & 0x3FF;        // Extract mantissa
    uint32_t h_sign = h >> 15;          // Extract sign

    uint32_t f_sign = h_sign << 31;     // Sign bit for float
    uint32_t f_exp;
    uint32_t f_mant;

    if (h_exp == 0) {
        // Zero / Subnormal
        if (h_mant == 0) {
            // Zero
            f_exp = 0;
            f_mant = 0;
        } else {
            // Subnormal
            int shift = __builtin_clz(h_mant) - 21; // Adjust shift for 32-bit float
            f_exp = (127 - 15 - shift) << 23;
            f_mant = (h_mant << (shift + 1)) & 0x7FFFFF;
        }
    } else if (h_exp == 0x1F) {
        // Infinity / NaN
        f_exp = 0xFF << 23;
        f_mant = h_mant << 13;
    } else {
        // Normalized
        f_exp = (h_exp + (127 - 15)) << 23;
        f_mant = h_mant << 13;
    }

    uint32_t f = f_sign | f_exp | f_mant;
    float result;
    memcpy(&result, &f, sizeof(result));
    return result;
}

void print_single_precision(uint64_t value) {
    // Extract the most significant 32 bits (real part)
    uint32_t msb = value >> 32; 
    
    // Extract the least significant 32 bits (imaginary part)
    uint32_t lsb = value & 0xFFFFFFFF; 
    
    // Interpret the extracted bits as floats
    float real = *((float*)&msb);
    float imag = *((float*)&lsb);
    
    // Print the real and imaginary parts
    printf("%f + %fj\n", real, imag);
}

void qft_rotations_Qsun(int num_qubits) {
    if (num_qubits == 0) {
        return;
    }

    num_qubits -= 1;
    H_Gate(num_qubits);

    for (int j = 0; j < num_qubits; j++) {
        RZ_Gate(num_qubits, 3.14159265359 / pow(2, (num_qubits - j)) / 2);
        CNOT_Gate(j, num_qubits);
        RZ_Gate(num_qubits, -3.14159265359 / pow(2, (num_qubits - j)) / 2);
        CNOT_Gate(j, num_qubits);
        RZ_Gate(num_qubits, 3.14159265359 / pow(2, (num_qubits - j)) / 2);
    }

    qft_rotations_Qsun(num_qubits);  // Đệ quy để tiếp tục cho tới khi num_qubits == 0
}

void swap_registers_Qsun(int num_qubits) {
    for (int j = 0; j < num_qubits / 2; j++) {
        CNOT_Gate(j, num_qubits - j - 1);
        CNOT_Gate(num_qubits - j - 1, j);
        CNOT_Gate(j, num_qubits - j - 1);
    }
}

void psr(int num_qubits) {
    for (int j = 0; j < num_qubits; j++) {
        RZ_Gate(j, 1);
        RX_Gate(j, 1);
        RZ_Gate(j, 1);
    }
}

void write_single_precision_to_file(FILE *file, uint64_t value) {
    // Extract the most significant 32 bits (real part)
    uint32_t msb = value >> 24;
    
    // Extract the least significant 32 bits (imaginary part)
    uint32_t lsb = value & 0xFFFFFF;
    
    // Interpret the extracted bits as floats
    float real = fixedPoint24ToFloat(msb);
    float imag = fixedPoint24ToFloat(lsb);
    
    // Write the real and imaginary parts to the file
    fprintf(file, "%f\n%f\n", real, imag);
}

int main(){
    unsigned char* membase;
    if (cgra_open() == 0)
        exit(1);
    
    cgra.dma_ctrl = CGRA_info.dma_mmap;
    membase = (unsigned char*) CGRA_info.ddr_mmap;
    
    printf("membase: %llx\n", (U64)membase);
    
    Ping_Mem = (U64*)(membase + PING_MEM_BASE_PHYS);
    Pong_Mem = (U64*)(membase + PONG_MEM_BASE_PHYS);
    

	struct timespec start_execution, end_execution;
	unsigned long long time_spent_execution = 0;
	clock_gettime(CLOCK_REALTIME, &start_execution);
	for(int num_qubits = 3; num_qubits < 18; num_qubits++){
		addr = 0;
		Qubit(num_qubits);  // Set the number of qubits before initializing and printing memory
		Ping_Mem[0] = 0x400000000000;
		for(int k = 1; k < qubit+1; k++){
			Ping_Mem[k] = 0;
		}
		
		for(int k = 0; k < qubit+1; k++){
			Pong_Mem[k] = 0;
		}
		
		Load();
		char filename[20]; 

		sprintf(filename, "PSR_Output_%d_qubit.txt", num_qubits);

		FILE *file = fopen(filename, "w");

		if (file == NULL) {
			printf("Cannot open the file!\n");
			return 1;
		}
		psr(num_qubits);

		dma_write(PING_MEM_BASE_PHYS, PING_MEM_BASE_PHYS, qubit+1);
		dma_write(PONG_MEM_BASE_PHYS, PONG_MEM_BASE_PHYS, qubit+1);
		printf("---------------------------\n");
		Start();
		Wait();
		if((addr&0x1) == 1){
			dma_read(PONG_MEM_BASE_PHYS, PONG_MEM_BASE_PHYS, qubit+1);

			printf("---------------------------\n");
			 for (int k = 0; k < qubit+1; k++) {	
				//printf("Pong_Mem[%d]: %08x\n", k, Pong_Mem[k]);
				//print_single_precision(Pong_Mem[k]);
				write_single_precision_to_file(file,Pong_Mem[k]);
			}
		}
		else {
			dma_read(PING_MEM_BASE_PHYS, PING_MEM_BASE_PHYS, qubit+1);

			printf("---------------------------\n");
			 for (int k = 0; k < qubit+1; k++) {
				//printf("Pong_Mem[%d]: %08x\n", k, Pong_Mem[k]);
				//print_single_precision(Ping_Mem[k]);
				write_single_precision_to_file(file,Ping_Mem[k]);
			}
		}
		Done();
	}
	clock_gettime(CLOCK_REALTIME, &end_execution);
	// time_spent_execution = BILLION * (end_execution.tv_sec - start_execution.tv_sec) + (end_execution.tv_nsec - start_execution.tv_nsec) + time_spent_execution;
	// printf("Execution time of FQsun for %d qubit in second is %f\n", qubit_num,(double)(time_spent_execution)/ BILLION); // 	
    
    return 0;
}
