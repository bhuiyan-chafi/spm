# Compiler setup
COMPILER = g++-12
COMPILEMPI = mpicxx
FLAGS += -std=c++20 -O3 -Wall
INCLUDES += -I../include/ -I../include/spdlog/include

LINKS ?=

# Default target
all: generator verifier_ff seq_sort farm ooc_omp ooc_omp_farm
 
# --- data_generator ---
generator: LINKS += ../include/main.cpp ./generator.cpp
generator: INCLUDES += -I../util/ -I../include/fastflow/
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./generator

# --- seq_sort ---
seq_sort: LINKS += ../include/test/main_test.cpp ./seq_sort.cpp
seq_sort: INCLUDES = -I../include/test/ -I../include/spdlog/include -I../util/ -I../include/fastflow/
seq_sort:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./seq_sort

# --- ooc_omp ---
ooc_omp: FLAGS += -fopenmp -pthread -ffast-math
ooc_omp: INCLUDES += -I../util/ -I../include/fastflow/
ooc_omp: LINKS += ../include/main.cpp ./ooc_omp.cpp
ooc_omp:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ooc_omp

# --- ooc_omp_farm ---
ooc_omp_farm: FLAGS += -fopenmp -pthread -ffast-math
ooc_omp_farm: INCLUDES += -I../util/ -I../include/fastflow/
ooc_omp_farm: LINKS += ../include/main.cpp ./ooc_omp_farm.cpp
ooc_omp_farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ooc_omp_farm

# --- ff_pipe ---
ff_pipe: FLAGS += -pthread
ff_pipe: INCLUDES += -I../include/fastflow/
ff_pipe: LINKS += ./ff_pipe.cpp ../helpers/common.cpp ../helpers/helper_ff.cpp
ff_pipe:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_pipe

# --- ff_farm ---

ff_farm: FLAGS += -pthread
ff_farm: INCLUDES += -I../include/fastflow/
ff_farm: LINKS += ../include/main.cpp ./ff_farm.cpp
ff_farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_farm

# --- farm ---
farm: FLAGS += -pthread
farm: INCLUDES += -I../include/fastflow/ -I../util/
farm: LINKS += ../include/main.cpp ./farm.cpp
farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./farm

# --- MPI + FARM ---

ff_mpi:	FLAGS += -pthread
ff_mpi: INCLUDES += -I./include/fastflow/
ff_mpi: LINKS += ./ff_mpi.cpp ../helpers/common.cpp ../helpers/helper_ff.cpp
ff_mpi:
	$(COMPILEMPI) $(FLAGS) $(INCLUDES) $(LINKS) -o ./ff_mpi

mpi_farm: FLAGS += -pthread
mpi_farm: INCLUDES += -I../include/fastflow/ -I../util/
mpi_farm: LINKS += ../include/main.cpp ./mpi_farm.cpp
mpi_farm:
	$(COMPILEMPI) $(FLAGS) $(INCLUDES) $(LINKS) -o ./mpi_farm
# --- verify ---
verify: FLAGS += -fopenmp -pthread
verify:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./verify.cpp -o ./verify

verifier_ff: FLAGS += -pthread
verifier_ff: INCLUDES += -I../include/fastflow/ -I../util/
verifier_ff: LINKS += ../include/main.cpp ./verifier_ff.cpp
verifier_ff:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./verifier_ff

read_data: INCLUDES += -I../include/fastflow/ -I../util/
read_data: LINKS += ../include/main.cpp
read_data:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) ./read_data.cpp -o ./read_data

# --- clean ---
.PHONY: clean
clean:
	rm -f ./generator ./verifier_ff ./seq_sort ./farm ./ooc_omp
