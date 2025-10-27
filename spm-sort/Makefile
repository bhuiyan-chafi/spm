# Compiler setup
COMPILER = g++-12
FLAGS += -std=c++20 -O3 -Wall -Wextra -pedantic
INCLUDES += -I./include/ -I./include/spdlog/include

# Default target
all: seq_sort verify ooc_omp generator

# --- data_generator ---
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/generator.cpp -o ./src/generator
# --- seq_sort ---
seq_sort:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/seq_sort.cpp ./src/helpers/helper.cpp -o ./src/seq_sort

# --- verify ---
verify: FLAGS += -fopenmp -pthread
verify:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/verify.cpp -o ./src/verify

# --- ooc_omp ---
ooc_omp: FLAGS += -fopenmp -pthread
ifdef THREADS
ooc_omp: FLAGS += -DDEFAULT_MAX_THREADS=$(THREADS)
endif

ooc_omp:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/ooc_omp.cpp ./src/helpers/helper.cpp -o ./src/ooc_omp

# --- clean ---
.PHONY: clean
clean:
	rm -f ./src/seq_sort ./src/verify ./src/ooc_omp