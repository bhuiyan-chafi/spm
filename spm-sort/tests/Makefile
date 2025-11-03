# Compiler setup
COMPILER = g++-12
COMPILEMPI = mpicxx
FLAGS += -std=c++20 -O3 -Wall
INCLUDES += -I../include/ -I../include/spdlog/include

LINKS ?=

# Default target
all: seq_sort verify ooc_omp generator ff_pipe read_data

# --- data_generator ---
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./generator.cpp -o ./generator

# --- seq_sort ---
seq_sort: LINKS += ./seq_sort.cpp ../helpers/common.cpp ../helpers/helper.cpp
seq_sort:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./seq_sort

# --- ooc_omp ---
ooc_omp: FLAGS += -fopenmp -pthread -ffast-math
ooc_omp: LINKS += ./ooc_omp.cpp ../helpers/common.cpp ../helpers/helper.cpp
ooc_omp:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ooc_omp

# --- ff_pipe ---
ff_pipe: FLAGS += -pthread
ff_pipe: INCLUDES += -I../include/fastflow/
ff_pipe: LINKS += ./ff_pipe.cpp ../helpers/common.cpp ../helpers/helper_ff.cpp
ff_pipe:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_pipe

# --- ff_farm ---

ff_farm: FLAGS += -pthread
ff_farm: INCLUDES += -I../include/fastflow/
ff_farm: LINKS += ./ff_farm.cpp ../helpers/common.cpp ../helpers/helper_ff.cpp
ff_farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_farm

# --- farm ---
farm: FLAGS += -pthread
farm: INCLUDES += -I../include/fastflow/ -I../util/
farm: LINKS += ./farm.cpp ../helpers/common.cpp
farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./farm

# --- MPI + FARM ---

ff_mpi:	FLAGS += -pthread
ff_mpi: INCLUDES += -I./include/fastflow/
ff_mpi: LINKS += ./ff_mpi.cpp ../helpers/common.cpp ../helpers/helper_ff.cpp
ff_mpi:
	$(COMPILEMPI) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_mpi
# --- verify ---
verify: FLAGS += -fopenmp -pthread
verify:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./verify.cpp -o ./verify

verifier_ff: FLAGS += -pthread
verifier_ff: INCLUDES += -I../include/fastflow/
verifier_ff: LINKS += ./verifier_ff.cpp
verifier_ff:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./verifier_ff

read_data:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./read_data.cpp -o ./read_data

# --- clean ---
.PHONY: clean
clean:
	rm -f ./seq_sort ./verify ./ooc_omp ./ff_pipe ./ff_farm ./farm ./ff_mpi ./read_data ./write_data ./verifier_ff ./verify ./generator
