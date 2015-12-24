##===- RTLs/hsa/Makefile -----------------------------------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##
#
# Build an RTL for a hsa machine if available
# github: gansong (zhang.guansong@gmail.com)
#
##===----------------------------------------------------------------------===##

#locate hsa libraries

HSA_RUNTIME_PATH := /opt/hsa-nov15
HSA_LLVM_PATH := /opt/amd/cloc/bin

SHELL := /bin/bash

tmachine := HSA
tmachine_name := HSA
tmachine_libname := HSA

hsalibs := 
ifneq ($(wildcard $(HSA_RUNTIME_PATH)/lib),)
hsalibs += -L $(HSA_RUNTIME_PATH)/lib 
endif

hsaincludes := 
ifneq ($(wildcard $(HSA_RUNTIME_PATH)/include),)
hsaincludes += -I$(HSA_RUNTIME_PATH)/include
endif

# hsalib := $(findstring hsa, $(shell ldconfig -p | grep 'libhsa' | grep -o '/.*' ))
hsalib := $(findstring hsa, $(shell find /opt/hsa-nov15/lib -type f | grep 'libhsa' ))

build_HSA :=
ifneq ($(hsalib),)
ifneq ($(hsalibs),)
ifneq ($(hsaincludes),)
	build_HSA := true
endif
endif
endif

CPP_FILES = $(wildcard src/*.cpp)
INC_FILES = $(wildcard src/*.h)
OBJ_FILES = $(subst src/,obj/,$(CPP_FILES:.cpp=.o))

CC := g++
CFLAGS := -c -fPIC -I. -Isrc/ -I../../src/ $(hsaincludes) -DTARGET_NAME=$(tmachine_name)
LDFLAGS := -shared -lelf -lhsa-runtime64 $(hsalibs) 

ifdef OMPTARGET_DEBUG
CFLAGS += -g -DOMPTARGET_DEBUG -DHSA_ERROR_REPORT
endif

ifneq ($(build_HSA),)
../../lib/libomptarget.rtl.hsa.so : $(OBJ_FILES)
	@ echo "Building HSA target RTL" && \
	mkdir -p ../../lib
	$(CC) $(LDFLAGS) -o $@ $(OBJ_FILES)
	
else
all : 
	@ echo "HSA not found in system: not building hsa RTL"
	
endif

obj/%.o: src/%.cpp $(INC_FILES) 
	@ mkdir -p obj
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf obj

