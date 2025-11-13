# Compiler setup
COMPILER = g++
COMPILE_MPI = mpicxx
FLAGS += -std=c++20 -O3 -Wall
INCLUDES += -I../include/ -I../include/test/ -I../include/spdlog/include

LINKS ?=

# Default target
all: generator sequential openmp farm mpiff verify
 
# --- data_generator ---
generator: LINKS += ../include/main.cpp ./generator.cpp
generator: INCLUDES += -I../util/ -I../include/fastflow/
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./generator

# --- sequential sort ---
sequential: LINKS += ../include/test/main_test.cpp ./sequential.cpp
sequential: INCLUDES = -I../include/test/ -I../include/spdlog/include -I../util/ -I../include/fastflow/
sequential:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./sequential

# --- openmpv1 ---
openmpv1: FLAGS += -fopenmp -pthread -ffast-math
openmpv1: INCLUDES += -I../util/ -I../include/fastflow/
openmpv1: LINKS += ../include/main.cpp ./openmpv1.cpp
openmpv1:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./openmpv1

# --- openmp ---
openmp: FLAGS += -fopenmp -pthread -ffast-math
openmp: INCLUDES += -I../util/ -I../include/fastflow/
openmp: LINKS += ../include/main.cpp ./openmp.cpp
openmp:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./openmp

# --- farm ---
farm: FLAGS += -pthread
farm: INCLUDES += -I../include/fastflow/ -I../util/
farm: LINKS += ../include/main.cpp ./farm.cpp
farm:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./farm

# --- farm ---
mpiff: FLAGS += -pthread
mpiff: INCLUDES += -I../include/fastflow/ -I../util/
mpiff: LINKS += ../include/main.cpp ./mpiff.cpp
mpiff:
	$(COMPILE_MPI) $(FLAGS) $(INCLUDES) $(LINKS) -o ./mpiff

# --- verify ---
verify: FLAGS += -pthread
verify: INCLUDES += -I../include/fastflow/ -I../util/
verify: LINKS += ../include/main.cpp ./verify.cpp
verify:
	$(COMPILER) $(FLAGS) $(INCLUDES) $(LINKS) -o ./verify

# --- clean ---
.PHONY: clean
clean:
	rm -rf generator ./sequential ./openmp ./farm ./mpiff ./verify
	
