# What is OpenMP?

An API that enables automatic parallelization within the same shared memory space. It runs in one single machine having multiple cores, the threads in each core shares the workload that is managed by the API library.

`Confusion`: OpenMP doesn't run in a cluster because clusters are multiple machines connected through high speed network connections. For example if I run an OpenMP program in UniPi SPMcluster, it will run in one of the machine(most probably the first frontend of the entirenode).

It uses compiler directives to control the parallel execution.

## Directives?

You still have to use -O3 for aggressive compiler optimizations, `-fopenmp` just enables the `#pragma` for OpenMP.
