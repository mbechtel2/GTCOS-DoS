CC=gcc
CXX=g++
PGMS=gpuwrite oclVersion

CFLAGS=-O2 -Wall -g
CXXFLAGS=$(CFLAGS)
LDFLAGS=-Wl,--no-as-needed -lrt

LIB=-lOpenCL -lm

all: $(PGMS)

gpuwrite: 
	g++ --std=gnu++11 -O3 ${INCLUDE} ${LIBPATH} gpuwrite.cpp -o gpuwrite ${LIB}
	
oclVersion: 
	g++ --std=gnu++11 -O3 ${INCLUDE} ${LIBPATH} oclVersion.cpp -o oclVersion ${LIB}

install:
	cp -v $(PGMS) /usr/local/bin
clean:
	rm $(PGMS)
