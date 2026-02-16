CXX := g++
# You may modify this file however you like, but make sure that on hamilton8 running
# make will compile your submission without errors. We will not debug non-compiling submissions.
CXXFLAGS := -O3 -fopenmp -std=c++17 -Wall -Wextra -pedantic 

TARGET := NBodySolver
SRC := main.cpp IO.cpp 
OBJ := $(SRC:.cpp=.o)
DEPS := IO.h NBodySimulation.h

ifeq ($(shell command -v module 2>/dev/null),)
    MODULE_CMD := 
else
    MODULE_CMD := module purge && module load gcc/12.2
endif

all: $(TARGET)

$(TARGET): $(OBJ)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(DEPS)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean

