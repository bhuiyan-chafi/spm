## Compile the code and give the executable a name

> g++ LoopTest.cpp -o LoopTest_ijk

## With optimization level

> g++ -O0 LoopTest.cpp -o LoopTest_ijk

## Run the executable

> ./LoopTest_ijk

## Run valgrind test

> valgrind --tool=cachegrind ./LoopTest_ijk

## Run perf test(in my case with 3 cache levels)

> sudo perf stat --event cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,L2_RQSTS.ALL_DEMAND_DATA_RD,L2_RQSTS.MISS,LLC-loads,LLC-load-misses ./LoopTest_ijk

## Compile parallel version using opencilk

> clang++ -fopencilk -O0 LoopTest_parallel.cpp -o LoopTest_parallel

## Run the executable

> ./LoopTest_parallel
