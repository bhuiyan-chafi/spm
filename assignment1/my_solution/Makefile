# Compiler
CXX := g++

# Flags
CXXFLAGS_PLAIN := -std=c++17 -O3
CXXFLAGS_AUTO  := -std=c++17 -O3 -march=native -ffast-math
CXXFLAGS_AVX   := -std=c++17 -O3 -march=native -ffast-math

# Targets
TARGETS := softmax_auto softmax_plain softmax_avx

all: $(TARGETS)

# Build softmax_plain
softmax_plain: softmax_plain.cpp
	$(CXX) $(CXXFLAGS_PLAIN) $< -o $@

# Build softmax_auto
softmax_auto: softmax_auto.cpp
	$(CXX) $(CXXFLAGS_AUTO) $< -o $@

# Build softmax_avx
softmax_avx: softmax_avx.cpp
	$(CXX) $(CXXFLAGS_AVX) $< -o $@

# Clean rule
clean:
	rm -f $(TARGETS)

