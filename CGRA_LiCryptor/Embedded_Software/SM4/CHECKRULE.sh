gcc SM4_FPGA.c -o main -I. -DCHECKRULE -DMODE32
./main 
rm -f main
