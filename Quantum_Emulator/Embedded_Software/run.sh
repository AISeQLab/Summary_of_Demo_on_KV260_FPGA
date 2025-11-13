#!/bin/bash

# Compile and run the main program
gcc QFT_FX24_Write_Output.c -o main -I. -DARMZYNQ -lm
./main
rm -f main
