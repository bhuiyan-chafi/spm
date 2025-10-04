# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -fopenmp -O3

# Programs
MAP_SRC = map.cpp
FARM_SRC = farm.cpp

MAP_EXE = map
FARM_EXE = farm

# Default target: build both
all: $(MAP_EXE) $(FARM_EXE)

# Map build
$(MAP_EXE): $(MAP_SRC)
	$(CXX) $(CXXFLAGS) -o $(MAP_EXE) $(MAP_SRC)

# Farm build
$(FARM_EXE): $(FARM_SRC)
	$(CXX) $(CXXFLAGS) -o $(FARM_EXE) $(FARM_SRC)

# Run targets
run-map: $(MAP_EXE)
	OMP_NUM_THREADS=4 ./$(MAP_EXE)

run-farm: $(FARM_EXE)
	OMP_NUM_THREADS=4 ./$(FARM_EXE)

# Clean
clean:
	rm -f $(MAP_EXE) $(FARM_EXE)