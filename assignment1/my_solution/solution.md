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
|spmcluster|plain|16'000'000(16M)||
|spmcluster|auto|16'000'000(16M)||
|spmcluster|avx|16'000'000(16M)||

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
