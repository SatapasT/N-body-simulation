SHELL := /bin/bash
CXX := g++

COMMON := -std=c++17 -Wall -Wextra -pedantic
ARCH   := -march=znver2
OPT    := -O3

DEBUG ?= 0
ifeq ($(DEBUG),1)
  DBGFLAGS := -g
else
  DBGFLAGS :=
endif

VEC_REPORT ?= 0
ifeq ($(VEC_REPORT),1)
  VECINFO := -fopt-info-vec-optimized
else
  VECINFO :=
endif

VEC_ON  := -ftree-vectorize $(VECINFO)

VEC_OFF := -fno-tree-vectorize

OMP_ON := -fopenmp

# Headers
DEPS := IO.h NBodySimulation.h

ifeq ($(shell command -v module 2>/dev/null),)
  MODULE_CMD :=
else
  MODULE_CMD := module purge && module load gcc/12.2 &&
endif

# Executables
REF    := NBodySolver_ref
FULL   := NBodySolver
SCALAR := NBodySolver_scalar

# Source files
SRC_REF     := baseline.cpp IO.cpp
SRC_FULL    := main.cpp IO.cpp
SRC_SCALAR  := baseline.cpp IO.cpp

# Object directories
OBJDIR_REF     := build/ref
OBJDIR_FULL    := build/full
OBJDIR_SCALAR  := build/scalar

OBJ_REF     := $(patsubst %.cpp,$(OBJDIR_REF)/%.o,$(SRC_REF))
OBJ_FULL    := $(patsubst %.cpp,$(OBJDIR_FULL)/%.o,$(SRC_FULL))
OBJ_SCALAR  := $(patsubst %.cpp,$(OBJDIR_SCALAR)/%.o,$(SRC_SCALAR))

# Targets
.PHONY: all clean baseline full scalar info

all: baseline full scalar

baseline: $(REF)
full: $(FULL)
scalar: $(SCALAR)

info:
	@echo "DEBUG=$(DEBUG) VEC_REPORT=$(VEC_REPORT)"
	@echo "REF flags:     $(CXXFLAGS_ref)"
	@echo "FULL flags:    $(CXXFLAGS_full)"
	@echo "SCALAR flags:  $(CXXFLAGS_scalar)"

# Compiler flags
CXXFLAGS_ref     := $(COMMON) $(ARCH) $(OPT) $(DBGFLAGS) $(VEC_ON)
CXXFLAGS_full    := $(COMMON) $(ARCH) $(OPT) $(DBGFLAGS) $(VEC_ON) $(OMP_ON)
CXXFLAGS_scalar  := $(COMMON) $(ARCH) $(OPT) $(DBGFLAGS) $(VEC_OFF)

# Link rules
$(REF): $(OBJ_REF)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_ref) -o $@ $^

$(FULL): $(OBJ_FULL)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_full) -o $@ $^

$(SCALAR): $(OBJ_SCALAR)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_scalar) -o $@ $^

# Compile rules
$(OBJDIR_REF)/%.o: %.cpp $(DEPS)
	@mkdir -p $(OBJDIR_REF)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_ref) -c $< -o $@

$(OBJDIR_FULL)/%.o: %.cpp $(DEPS)
	@mkdir -p $(OBJDIR_FULL)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_full) -c $< -o $@

$(OBJDIR_SCALAR)/%.o: %.cpp $(DEPS)
	@mkdir -p $(OBJDIR_SCALAR)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_scalar) -c $< -o $@

clean:
	rm -rf build $(REF) $(FULL) $(SCALAR)