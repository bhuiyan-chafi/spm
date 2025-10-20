# -Wall     → Enable common useful warnings (unused vars, uninitialized values, etc.)
# -Wextra   → Enable extra warnings (unused params, signed/unsigned comparisons, etc.)
# -pedantic → Enforce strict ISO C++ compliance (warn on non-standard extensions)

CXX ?= g++
CXXFLAGS += -std=c++20 -O3 -Wall -Wextra -pedantic
CPPFLAGS += -I../include -I../include/spdlog/include

PROGRAMS := write_data read_data
OBJS := $(PROGRAMS:%=%.o)
CLEAN_FILES := $(PROGRAMS) $(OBJS)

.PHONY: all clean

all: $(PROGRAMS)

$(PROGRAMS): %: %.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp ../include/record.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(CLEAN_FILES)
