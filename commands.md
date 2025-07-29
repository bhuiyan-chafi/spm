# Linux commands

Following commands will produce stated results.

## Your CPU details

    lscpu

A better version is `cpuid`, that you can install:

    sudo apt install cpuid

and to check the details:

    cpuid

## The cache-line size of your machine

    getconf LEVEL1_DCACHE_LINESIZE

If it gives a output of 64 that means 64byte cache-line.

## Check the vector registers

    lscpu | grep -E 'Flags|SIMD'

In my machine I have avx and avx2 but no avx512f which means I have max vector registers which are 256 bits wide per thread(if your processor supports hyper-threading, which in most cases now is a yes!).

## Compile the code and give the executable a name

    g++ LoopTest.cpp -o LoopTest_ijk

## With optimization level

    g++ -O0 LoopTest.cpp -o LoopTest_ijk

## Run the executable

    ./LoopTest_ijk

## Run valgrind test

    valgrind --tool=cachegrind ./LoopTest_ijk

## Run perf test(in my case with 3 cache levels)

    sudo perf stat --event cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,L2_RQSTS.ALL_DEMAND_DATA_RD,L2_RQSTS.MISS,LLC-loads,LLC-load-misses ./LoopTest_ijk

## Compile parallel version using opencilk

    clang++ -fopencilk -O0 LoopTest_parallel.cpp -o LoopTest_parallel
