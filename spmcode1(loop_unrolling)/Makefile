CXX                = g++ -std=c++17
OPTFLAGS	   = -O3
AVXFLAGS           = 
AUTOFLAGS          = 
CXXFLAGS          += -Wall 
INCLUDES	   = -I. -I./include
LIBS               = 
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< $(LIBS)

all: $(TARGET)


launch-benchmark: LIBS += -pthread

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
