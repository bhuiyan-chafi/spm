# Solutions that I tried step by step

I have tested the same solution in different steps to see the differences.

## softmax_auto.cpp

Forcing the compiler to apply auto vectorizing on the SIMD. We have several loops here and all of them are independent. So, we will see how each of the compiler behaves with each of the instricts.

### Removing the `aliasing issue`

First we will ensure the compiler that our two pointers in the `softmax` function is not pointing to the same memory location.

```cpp
    void softmax_plain(const float *input, float *output, size_t K){}
    //into
    void softmax_auto(const float *__restrict__ input, float *__restrict__ output, size_t K){}
```

### providing vectorizing hint through `OpenMP` for the first loop

The loop is totally independent because we are just trying to find the maximum value inside the vector. Even if we segregate the vector, nothing will be changed.

```cpp
    #pragma omp simd
        for (size_t i = 0; i < K; ++i)
        {
            max_val = std::max(max_val, input[i]);
        }
```

### second loop `expf()` instead of `std::exp()`

`std::exp()` also gives the compiler a hint that the data-type can be double as well. Stating it as float `expf()` gives the compiler a static decision.

## Applying the reduction

When we accumulate several values into one scalar variable, we should use the `reduction` pragma. It tells explicitly that the loop is not dependent and we can divide it into SIMD lanes and perform the operations. And later the value can be accumulated into one scalar variable. If we don't mention `reduction(operation:variable)` the compiler later finds it out if we use flags. But mentioning gives the compiler an explicit hint that it is independent.

```cpp
    #pragma omp simd reduction(max : max_val)
    #pragma omp simd reduction(+ : sum)
```

## Performances

I did the test in two machines: my own dumb laptop(2 core, 4 thread), and our spmcluster.

|Machine|Program|Size|Performance(seconds)|
|:---|:----:|:---:|----:|
|dumb|plain|16'000'000(16M)|0.132719s|
|dumb|auto|16'000'000(16M)|0.0345315s|
|dumb|avx|16'000'000(16M)|0.0364775s|
|spmcluster|plain|16'000'000(16M)|0.127965s|
|spmcluster|auto|16'000'000(16M)|0.0304838s|
|spmcluster|avx|16'000'000(16M)|0.0315648s|

## which part the compiler couldn't vectorize automatically?

First we tested the vectorization without the flags:

```bash
g++ -O3 -fopt-info-vec-missed softmax_auto.cpp 2>&1 | grep -E "^softmax_auto.cpp:"
```

Then with proper flags:

```bash
g++ -O3 -march=native -ftree-vectorize -ffast-math -fopt-info-vec-missed softmax_auto.cpp 2>&1 | grep -E "^softmax_auto.cpp:"
```

The results are pretty overwhelming. With proper flag all the loops in softmax_auto() were vectorized.

## Let us compare our results now

```bash
$ ./softmax_avx 16000000
# elapsed time (softime_avx): 0.0391605s
$ ./softmax_auto 16000000
# elapsed time (softime_auto): 0.031002s
```

### Why the auto version beats my manual `avx`?

**`Aligned vs Unaligned`** memory addresses:

>When we created our input vector the memory aligned was by default unaligned. It's 16byte off because by default std::vector gives 16bytes alignment.

**What is this alignment**?

Aligned means the starting and ending memory address of our allocator is exactly 32bytes distant. Because avx2 is 256bits=32bytes(4byte each float, total 8float). If the starting address is 0x00 then the next address will be 0x20, 0x40 and so on. 

```cpp
std::vector<float> input = generate_random_input(K);
 std::vector<float> output(K);
```

If you want to check the alignment:

```cpp
#include <cstdint>
#include <cstdio>

void check_alignment(const float* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    if (addr % 32 == 0) {
        printf("Pointer %p is 32-byte aligned\n", ptr);
    } else {
        printf("Pointer %p is NOT 32-byte aligned (mod 32 = %zu)\n",
               ptr, addr % 32);
    }
}
```

Which for sure will reply as unaligned. If you want an aligned input array:

```cpp
float* data = static_cast<float*>(::operator new[](N * sizeof(float), std::align_val_t(32)));
```

In our case it was hard because we don't know if the size will be multiple of x32(FP32).

**`Why unaligned allocator is slow?`**

Cache line reads 256bits(32bytes) at once or 512bits(64bytes) at once. But if the next line is not 32/64 bytes distant then we do another memory read which is expensive and a size K which is much larger it's catastrophic.
