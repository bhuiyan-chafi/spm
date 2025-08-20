CXX                = g++ -std=c++17
OPTFLAGS	   = -O3 -march=native
CXXFLAGS          += -Wall 
INCLUDES	   = -I. -I../include
LIBS               = -pthread -lX11
SOURCES            = $(wildcard *.cpp)
TARGET             = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 

%:%.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTFLAGS) -o $@ $< gfx.c $(LIBS)

all:$(TARGET)

nodisplay: CXXFLAGS += -DNO_DISPLAY
nodisplay: cleanall $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET)
