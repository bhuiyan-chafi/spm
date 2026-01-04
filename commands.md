# Linux commands

Following commands will produce stated results. **But these are not project related commands, instead for tests. If you are looking for project related commands please check this [file](./spm-sort/README.md)**.

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

## Sync files local<->remote

    rsync -avz Downloads/spmcode7 chafi@131.114.52.245:/home/chafi/spm

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

## Modern logging library

We will use `spdlog` for logging our programs. In my ubuntu machine I am going to use the library `libspdlog-dev` for that. Here is how I configured it:

    sudo apt install libspdlog-dev libfmt-dev

After that in your c++ programs use the header:

    #include <spdlog/spdlog.h>
    //and for compilation
    mpic++ -O2 mpi_ring_com.cpp -o mpi_ring_com -lspdlog -lfmt
