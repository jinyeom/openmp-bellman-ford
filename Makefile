# Makefile for building Bellman-Ford algorithm
CC=icc
CFLAGS=-O3 -std=c++11 -g
LDFLAGS=-lpthread
SOURCES=./src/main.cpp ./src/graph.cpp /opt/intel/vtune_amplifier/lib64/libittnotify.a
INCLUDES=-I/opt/intel/vtune_amplifier/include
EXECUTABLE=bellman-ford

all: $(EXECUTABLE)

$(EXECUTABLE):
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(INCLUDES)

clean:
	rm $(EXECUTABLE)
