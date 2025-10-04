# Discussion on OpenMP

We will try to exploit OpenMP features to test our several parallel programs. Since the library enables the features by itself, in these tests we will just adjust inputs and building blocks to exploit the benefits.

## The MAP

The program does nothing but creating a batch of 4 inputs at a time and assigns it to 4 physical threads(since I have 4 in my machine). The data is not a stream, it's a fixed input where we are creating a batch of 4(just in theory, we didn't even create a vector). Each of the functions have equal work-load and distribution. Check out the program [in here](./map.cpp).