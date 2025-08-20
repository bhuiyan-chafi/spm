# What is the ThreadPool?

A helper class which allows to create thread automatically and assign them to each tasks. It's a queue of thread that is binded to a queue of tasks. The result is handled by a future promise.

## The Helper Class

You can check this class example provided from our professor: [threadPool.hpp](../include/threadPool.hpp)

## What tests we made?

We tested a pool with a simple task involving a square of an integer. The results are as followed:

- with just 1 single thread our program performed the best!

**Why** is that

    Because our program granularity is very less. The overhead beats our program thus increasing number of threads introduces latency.

Check out this [program](./thread_pool.cpp), which gives an output(problem size 1 Million):

- 1 thread 9.89s
- 2 threads 10.20s
- 4 threads 13.27s
- 8 threads 14.36s
