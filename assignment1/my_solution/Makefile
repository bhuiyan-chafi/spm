# Compiler
CXX := g++

# Flags
CXXFLAGS_AUTO  := -std=c++17 -O3 -march=native -ftree-vectorize -ffast-math
CXXFLAGS_PLAIN := -std=c++17 -O3

# Targets
TARGETS := softmax_auto softmax_plain

all: $(TARGETS)

# Build softmax_auto
softmax_auto: softmax_auto.cpp
	$(CXX) $(CXXFLAGS_AUTO) $< -o $@

# Build softmax_plain
softmax_plain: softmax_plain.cpp
	$(CXX) $(CXXFLAGS_PLAIN) $< -o $@

# Clean rule
clean:
	rm -f $(TARGETS)

