#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define BILLION  1000000000

///Write channel
#define START_BASE				(0x00000)
#define LDM_INPUT_BASE_PHYS		(0x10000>>2)		
#define CRAM_INPUT_BASE_PHYS	(0x20000>>2)
#define WRAM_INPUT_BASE_PHYS	(0x30000>>2)	
#define BRAM_INPUT_BASE_PHYS	(0x40000>>2)

///Read channel
#define DONE_BASE_PHYS			(0x00000)
#define LDM_OUTPUT_BASE_PHYS	(0x10000>>2)

#define FRACTIONAL_BITS 6
#define SCALE_FACTOR (1 << FRACTIONAL_BITS)

#define NumberOfPicture 15009
// #define NumberOfPicture 1
#define d 1
#pragma warning(disable : 4996)

#include "CGRA.h"
#include "FPGA_Driver.c"

float fixed_point_to_float(U32 fixed_point_value) {
    int sign = (fixed_point_value & 0x8000) ? -1 : 1;
    U32 magnitude = (fixed_point_value & 0x7FFF); // Mask to get 15-bit magnitude
    float float_value = (float)magnitude / SCALE_FACTOR;
    return sign * float_value;
}

U32 FX_convert(float input) {
    // Multiply the input by the scale factor
    float scaled_value = input * SCALE_FACTOR;

    // Proper rounding towards zero for negative numbers
    int16_t fixed_value;
    if (scaled_value >= 0) {
        fixed_value = (int16_t)(scaled_value + 0.5f);  // Round up for positive values
    } else {
        fixed_value = (int16_t)(scaled_value - 0.5f);  // Round down for negative values
    }

    // Handle overflow and underflow cases explicitly within 16-bit range
    if (fixed_value > 32767) {
        fixed_value = 32767;
    } else if (fixed_value < -32768) {
        fixed_value = -32768;
    }

    // Return the fixed-point value
    return (U32) fixed_value&0x0000FFFF;
}

int main(){

	unsigned char* membase;
	if (cgra_open() == 0)
		exit(1);

	cgra.dma_ctrl = CGRA_info.dma_mmap;
	membase = (unsigned char*) CGRA_info.ddr_mmap;
	
	printf("membase: %llx\n", (U64)membase);
	
	FILE *CRAM_file = fopen("CRAM_File.txt", "r");  // Open file in read mode
    if (CRAM_file == NULL) {
        perror("Unable to open CRAM_File.txt");
        return 1;
    }
	
	FILE *WRAM_file = fopen("WRAM_File.txt", "r");  // Open file in read mode
    if (WRAM_file == NULL) {
        perror("Unable to open WRAM_File.txt");
        return 1;
    }

	FILE *BRAM_file = fopen("BRAM_File.txt", "r");  // Open file in read mode
    if (BRAM_file == NULL) {
        perror("Unable to open BRAM_File.txt");
        return 1;
    }

	FILE *WRAM_2_file = fopen("WRAM_2_File.txt", "r");  // Open file in read mode
    if (WRAM_2_file == NULL) {
        perror("Unable to open WRAM_2_File.txt");
        return 1;
    }

	FILE *BRAM_2_file = fopen("BRAM_2_File.txt", "r");  // Open file in read mode
    if (BRAM_2_file == NULL) {
        perror("Unable to open BRAM_2_File.txt");
        return 1;
    }
	
	int i = 0;
	U32 value;
	float value_f;
	float weight_final[160], bias_final[5];
	U32 CRAM[42], WRAM[6096], BRAM[196];
	// printf("Write CRAM!\n");
	
	while (fscanf(CRAM_file, "%8x", &value) == 1) { 
		CRAM[i] = value;
		// printf("CRAM[%d] = %08x\n",i,CRAM[i]);		
		i++;
	}


    fclose(CRAM_file);
	// printf("\n\n\n");
	
	i = 0;
	// printf("Write WRAM!\n");
	
	while (fscanf(WRAM_file, "%4x", &value) == 1) { 
		WRAM[i] = value;
		// printf("WRAM[%d] = %08x\n",i,value);
		i++;
	}


    fclose(WRAM_file);

	// printf("\n\n\n");
	
	i = 0;
	// printf("Write BRAM!\n");
	
	while (fscanf(BRAM_file, "%4x", &value) == 1) { 
		BRAM[i] = value;
		// printf("BRAM[%d] = %08x\n",i,value);
		i++;
	}


    fclose(BRAM_file);
	
	for(int j =0; j<42;j++)
		*(CGRA_info.reg_mmap + CRAM_INPUT_BASE_PHYS + j) = CRAM[j];
	
	for(int j =0; j<6096;j++)
		*(CGRA_info.reg_mmap + WRAM_INPUT_BASE_PHYS + j) = WRAM[j];

	for(int j =0; j<196;j++)
		*(CGRA_info.reg_mmap + BRAM_INPUT_BASE_PHYS + j) = BRAM[j];	
	
	// printf("Read WRAM 2!\n");
	
	i = 0;
	
	while (fscanf(WRAM_2_file, "%f", &value_f) == 1) { 	
		// printf("WRAM_2[%d] = %f\n",i,value_f);
		weight_final[i] = value_f;
		i++;
	}

    fclose(WRAM_2_file);

	// printf("Read BRAM 2!\n");
	
	i = 0;
	
	while (fscanf(BRAM_2_file, "%f", &value_f) == 1) { 	
		// printf("BRAM_2[%d] = %f\n",i,value_f);
		bias_final[i] = value_f;
		i++;
	}

    fclose(BRAM_2_file);
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate memory for input data
    float* InModel = (float*)malloc(NumberOfPicture * d * 320 * sizeof(float));
    float tmp;
    FILE* Input = fopen("signals.txt", "r");
    if (Input == NULL) {
        perror("Failed to open signals.txt");
        return 1;
    }
    for (int i = 0; i < NumberOfPicture * d * 320; i++) {
        fscanf(Input, "%f", &tmp);
        InModel[i] = tmp;
    }
    fclose(Input);

    // Allocate memory for labels
    float* Label = (float*)malloc(NumberOfPicture * sizeof(float));
    FILE* Output = fopen("labels.txt", "r");
    if (Output == NULL) {
        perror("Failed to open labels.txt");
        free(InModel);
        return 1;
    }
    for (int i = 0; i < NumberOfPicture; i++) {
        fscanf(Output, "%f", &tmp);
        Label[i] = tmp;
    }
    fclose(Output);

    // Allocate memory for the output array
    float* OutArray = (float*)malloc(NumberOfPicture * sizeof(float));
	U32 CNN_output_FX[1280];
    float CNN_output[1280];
    float GlobalAveragePool1D[32];
    float out_Dense[5];
    float Image[d * 320];
	U32 Pixel[340];
	uint16_t address[1280];
	struct timespec start_CNN, end_CNN;
	unsigned long long time_spent_CNN = 0;
	
	for (int j = 0; j < 1280; j++) {
		int a = j/20;
		address[j] = (j + a*12) & 0xFFFF; // Mask to 16-bit address
		// printf("address[%d] = %d\n",j,address[j]);
	 }
    for (int i = 0; i < NumberOfPicture; i++) {
        int startIndex = i * d * 320;
        for (int k = 0; k < d * 320; k++) {
            Image[k] = InModel[startIndex + k];
        }
		
		for (int k = 0; k < 340; k++) {
			if(k<320)
				Pixel[k] = FX_convert(Image[k]);
			else 
				Pixel[k] = FX_convert(0);
        }
		
		clock_gettime(CLOCK_REALTIME, &start_CNN);
		for (int k = 0; k < 340; k++) {
				*(CGRA_info.reg_mmap + LDM_INPUT_BASE_PHYS + address[k]) = Pixel[k];
        }
		*(CGRA_info.reg_mmap + START_BASE) = 1;
        // Wait for computation to complete
        while (1){
			if(*(CGRA_info.reg_mmap + DONE_BASE_PHYS) == 1)
				break;
		}

        // Read CNN output
        for (int j = 0; j < 1280; j++) {
            CNN_output[j] = fixed_point_to_float(*(CGRA_info.reg_mmap + LDM_OUTPUT_BASE_PHYS + address[j]));
        }

        // Global Average Pooling
        for (int j = 0; j < 32; j++) {
            float avg = 0;
            for (int k = 0; k < 40; k++) {
                avg += CNN_output[40 * j + k];
            }
            GlobalAveragePool1D[j] = avg / 40;
        }

        // Dense Layer Calculation
        for (int j = 0; j < 5; j++) {
            float s = 0;
            for (int k = 0; k < 32; k++) {
                s += GlobalAveragePool1D[k] * weight_final[k * 5 + j];
            }
            out_Dense[j] = s + bias_final[j];
        }

        // Find the index of the maximum value in out_Dense
        int maxindex = 0;
        float max = out_Dense[0];
        for (int j = 1; j < 5; j++) {
            if (out_Dense[j] > max) {
                max = out_Dense[j];
                maxindex = j;
            }
        }

        // Store the result in OutArray
        OutArray[i] = (float)maxindex;
		clock_gettime(CLOCK_REALTIME, &end_CNN);
		time_spent_CNN = BILLION * (end_CNN.tv_sec - start_CNN.tv_sec) + (end_CNN.tv_nsec - start_CNN.tv_nsec) + time_spent_CNN;
		// printf("Predicted = %f\n", OutArray[i]);
    }

    // Calculate accuracy
    float countTrue = 0;
    for (int i = 0; i < NumberOfPicture; i++) {
        int labelValue = (int)Label[i];
        int predictValue = (int)OutArray[i];
        if (labelValue == predictValue) {
            countTrue += 1;
        }
		
		 // if (labelValue != predictValue) {
			// printf("Predicted = %f, Label = %f\n", predictValue, labelValue);
		// }
			
    }
    float accuracy = (countTrue / NumberOfPicture) * 100;
    printf("Accuracy of Model = %f\n", accuracy);
	printf("Execution time in second is  %f\n", (double)time_spent_CNN/ BILLION); //
    // Free allocated memory
    free(InModel);
    free(Label);
    free(OutArray);
	
	return 0;
}
