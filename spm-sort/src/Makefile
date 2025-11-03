# Compiler setup
COMPILER = g++
COMPILE_MPI = mpicxx
FLAGS += -std=c++20 -O3 -Wall
INCLUDES += -I../include/ -I../include/spdlog/include

LINKS ?=

# Default target
all: generator

# --- data_generator ---
generator:
	$(COMPILER) $(FLAGS) $(INCLUDES) ./generator.cpp -o ./generator

# --- clean ---
.PHONY: clean
clean:
	./generator
	
