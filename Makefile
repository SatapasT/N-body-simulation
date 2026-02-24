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
VEC_ON := -ftree-vectorize $(VECINFO)

OMP_ON := -fopenmp

# Headers
DEPS := IO.h NBodySimulation.h

ifeq ($(shell command -v module 2>/dev/null),)
  MODULE_CMD :=
else
  MODULE_CMD := module purge && module load gcc/12.2 &&
endif

REF  := NBodySolver_ref
FULL := NBodySolver

SRC_REF  := baseline.cpp IO.cpp
SRC_FULL := main.cpp IO.cpp

OBJDIR_REF  := build/ref
OBJDIR_FULL := build/full

OBJ_REF  := $(patsubst %.cpp,$(OBJDIR_REF)/%.o,$(SRC_REF))
OBJ_FULL := $(patsubst %.cpp,$(OBJDIR_FULL)/%.o,$(SRC_FULL))

.PHONY: all clean baseline full info

all: baseline full
baseline: $(REF)
full: $(FULL)

info:
	@echo "DEBUG=$(DEBUG) VEC_REPORT=$(VEC_REPORT)"
	@echo "REF flags:  $(CXXFLAGS_ref)"
	@echo "FULL flags: $(CXXFLAGS_full)"

CXXFLAGS_ref  := $(COMMON) $(ARCH) $(OPT) $(DBGFLAGS) $(VEC_ON)

CXXFLAGS_full := $(COMMON) $(ARCH) $(OPT) $(DBGFLAGS) $(VEC_ON) $(OMP_ON)

$(REF): $(OBJ_REF)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_ref)  -o $@ $^

$(FULL): $(OBJ_FULL)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_full) -o $@ $^

$(OBJDIR_REF)/%.o: %.cpp $(DEPS)
	@mkdir -p $(OBJDIR_REF)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_ref) -c $< -o $@

$(OBJDIR_FULL)/%.o: %.cpp $(DEPS)
	@mkdir -p $(OBJDIR_FULL)
	$(MODULE_CMD) $(CXX) $(CXXFLAGS_full) -c $< -o $@

clean:
	rm -rf build $(REF) $(FULL)