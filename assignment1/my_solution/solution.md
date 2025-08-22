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

When we accumulate several values into one scalar variable, we should use the `reduction` pragma. It tells explicitly that the loop is not dependent and we can divide it into SIMD lanes and perform the operations. And later the value can be accumulated into one scalar variable later. If we don't mention `reduction(operation:variable)` the compiler later finds it out if we use flags. But mentioning gives the compiler an explicit hint that it is independent.

```cpp
    #pragma omp simd reduction(max : max_val)
    #pragma omp simd reduction(+ : sum)
```

## Performances

I did the test in two machines: my own dumb laptop(2 core, 4 thread), and our spmcluster.

|Machine|Program|Size|Performance(seconds)|
|:---|:----:|:---:|----:|
|dumb|plain|100'000'000|0.826157|
|dumb|auto|100'000'000|0.177588|
|spmcluster|plain|100'000'000||
|spmcluster|auto|100'000'000||
|dumb|plain|900'000'000|7.48695|
|dumb|auto|900'000'000|1.59248|
|spmcluster|plain|900'000'000||
|spmcluster|auto|900'000'000||
