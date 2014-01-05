#!/bin/sh

mkdir statistic_task3
make cache_task3
./cache_task3.bin tracefiles/fft_16_p1.trf > statistic_task3/fft1.txt 
./cache_task3.bin tracefiles/fft_16_p2.trf > statistic_task3/fft2.txt 
./cache_task3.bin tracefiles/fft_16_p4.trf > statistic_task3/fft4.txt 
./cache_task3.bin tracefiles/fft_16_p8.trf > statistic_task3/fft8.txt 


./cache_task3.bin tracefiles/dbg_p1.trf> statistic_task3/dbg1.txt 
./cache_task3.bin tracefiles/dbg_p2.trf> statistic_task3/dbg2.txt 
./cache_task3.bin tracefiles/dbg_p4.trf> statistic_task3/dbg4.txt 
./cache_task3.bin tracefiles/dbg_p8.trf> statistic_task3/dbg8.txt 


./cache_task3.bin tracefiles/rnd_p1.trf> statistic_task3/rnd1.txt 
./cache_task3.bin tracefiles/rnd_p2.trf> statistic_task3/rnd2.txt 
./cache_task3.bin tracefiles/rnd_p4.trf> statistic_task3/rnd4.txt 
./cache_task3.bin tracefiles/rnd_p8.trf> statistic_task3/rnd8.txt 

