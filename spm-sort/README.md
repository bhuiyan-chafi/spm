# About

- Course: Parallel and Distributed Systems: Paradigms and Models
- Professor Massimo Torquati,  Universita di Pisa, Italy
- Project Name: Memory Out-of-Core Merge-Sort
- Technologies: c++, FastFlow, OpenMP, MPI

## Simulation

The details have been discussed in the [report](./report/main.pdf), here we will see a few things to clone and run the program.

### Cloning the project

```bash
git clone https://github.com/bhuiyan-chafi/spm.git
git checkout main
cd spm-sort/include
git clone https://github.com/fastflow/fastflow.git
git clone https://github.com/gabime/spdlog.git
```

### Building the Executables

```bash
cd ..
cd src/
make all # requires minimum version of gcc 11
```

### Usage

Detailed usage has been reported in the [report](./report/main.pdf). But let us see one example of each:

1. Generate Data:

    ```bash
    ./generator 1M 256 #make sure you are in /src directory
    ```

2. Verify Input Data:

    ```bash
    ./reader 100 input ../data/rec_1M_256.bin
    ```

3. OpenMP

    ```bash
    ./openmp 1M 256 4 4 # ./program records payload workers memory_cap
    ```

4. FastFlow

    ```bash
    ./farm 1M 256 4 4 # requires fastflow
    ```

5. MPI+FastFlow

    ```bash
    mpirun -np 2 ./farm 1M 256 4 4 # make sure OpenMPI is installed and -np at least 2
    ```

6. Verify Output Records

    ```bash
    ./reader 100 output
    ```

### Commands for the spmcluster.unipi.it

1. Run the OpenMP and FastFlow with varied number of records and workers:

    ```bash
    #directory: /spm/spm-sort/src/
    bash run_simulation.sh ALL 32 > /dev/null 2>&1 & 
    ```

2. Run the MPI+FastFLow:

    ```bash
    #directory: /spm/spm-sort/src/
    bash run_mpi.sh 32 > /dev/null 2>&1 & 
    ```

To modify your tests you can check these [scripts](./src/run_simulation.sh) in this [folder](./src/).

## Conclusion

You are free to use this program for any educational purpose. Do not use it for any professional use. You are most welcome to conduct your tests on it.
