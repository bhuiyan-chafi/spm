# Loop unrolling basic tests

First we tried taking parameters from the terminal along with code compilation to have dynamic values to test with. The perfect way to run this is:

    g++ -O3 auto_loop_unroll.cpp -o auto_loop && ./auto_loop 1024

Where we are executing two commands serially. `./auto_loop` is the compiled program name and `1024` is the size of the `vector` to run.

## Why `cstdint`?

The `int` values varies based on machine. So, to make sure in every machine you get the same int data-type, we fix the size. For example: `u_int64_t` give an integer of size 64 bits.

## why `-INFINITY`?

We can initialize the result as `0` but if we deal with negative values in our vector and we do some sort of conditional operation that involves comparing values then it might produce wrong results. Because negatives are always smaller than `0`.

## loop unrolling test with the help of compiler and factors of 2,4 and 8

Run this [code](/spmcode1/my_test/auto_loop_unroll.cpp), which doesn't contain manual loop unrolling. We will take the help of the compiler to see the difference. 

    