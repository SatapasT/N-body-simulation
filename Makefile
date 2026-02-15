CXX := g++
# You may modify this file however you like, but make sure that on hamilton8 running
# make will compile your submission without errors. We will not debug non-compiling submissions.
CXXFLAGS := -O3 -march=znver2 -fopenmp -std=c++17 -Wall -Wextra -pedantic

TARGET := NBodySolver
SRC := main.cpp IO.cpp NBodySimulation.cpp
OBJ := $(SRC:.cpp=.o)
DEPS := IO.h NBodySimulation.h

all: $(TARGET)

$(TARGET): $(OBJ)
	module purge && module load gcc/12.2 && $(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(DEPS)
	module purge && module load gcc/12.2 && $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean

