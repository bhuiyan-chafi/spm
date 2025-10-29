# Compiler setup
COMPILER = g++-12
FLAGS += -std=c++20 -O3 -Wall
INCLUDES += -I./include/ -I./include/spdlog/include

LINKS ?=

# Default target
all: seq_sort verify ooc_omp generator ff_pipe

# --- data_generator ---
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/generator.cpp -o ./src/generator

# --- seq_sort ---
seq_sort: LINKS += ./src/seq_sort.cpp ./src/helpers/common.cpp ./src/helpers/helper.cpp
seq_sort:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./src/seq_sort

# --- verify ---
verify: FLAGS += -fopenmp -pthread
verify:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./src/verify.cpp -o ./src/verify

# --- ooc_omp ---
ooc_omp: FLAGS += -fopenmp -pthread -ffast-math
ifdef THREADS
ooc_omp: FLAGS += -DDEFAULT_MAX_THREADS=$(THREADS)
endif
ooc_omp: LINKS += ./src/ooc_omp.cpp ./src/helpers/common.cpp ./src/helpers/helper.cpp
ooc_omp:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./src/ooc_omp

# --- ff_pipe ---

ff_pipe: FLAGS += -pthread
ff_pipe: INCLUDES += -I./include/fastflow/
ff_pipe: LINKS += ./src/ff_pipe.cpp ./src/helpers/common.cpp ./src/helpers/helper_ff.cpp
ff_pipe:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(FF_INCLUDE) $(LINKS) -o ./src/ff_pipe

# --- clean ---
.PHONY: clean
clean:
	rm -f ./src/seq_sort ./src/verify ./src/ooc_omp ./src/ff_pipe