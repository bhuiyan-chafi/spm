# Basic tests with our machine

## Installation of valgrind(suggested by the professor)

    sudo apt install valgrind

## Installation of `perf`(not suggested but gives better results)

Before installing you must check if it already exists because mostly it's integrated:

    perf --version

if not found then proceed with the installation:

    sudo apt install linux-tools-common linux-tools-generic linux-tools-$(uname -r)

## Loop Test

With n = 4096 and loop order i,j,k we achieved a time execution of 1357.75 seconds. For order i,k,j 336.656 and k,i,j 336.003.

## Why `vector<T>`, not traditional data types?

We can use but vectors allow us to to do more when the size is dynamic. Traditional data-type operations usually live in the call stack while vector by default lives in the Heap(dynamic size allocation is performed automatically). On the otherhand it's safe for value copying, assigning and freeing from the memory.

## Pointer, Reference and Variables

A pointer simply points to the memory location of a variables. But a pointer also is stored in a memory location. So, int *pointer = &x; here the `*pointer` = the numerical value of the variable, `pointer` = the memory location of the variable and `&pointer` = is the memory location of the pointer itself.

## Cache miss test using `valgrind` and `perf`

For Valgrind:

    valgrind --tool=cachegrind ./loop_unroll 

And for perf: 

    sudo perf stat --event cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,L2_RQSTS.ALL_DEMAND_DATA_RD,L2_RQSTS.MISS,LLC-loads,LLC-load-misses ./loop_unroll

For `perf` some features may not be supported by the CPU, so you can check the CPU details first and perform your desired test.