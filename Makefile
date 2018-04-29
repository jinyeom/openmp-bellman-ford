# Makefile for building Bellman-Ford algorithm
CC=g++
CFLAGS=-O3 -std=c++11 -g
LDFLAGS=
SOURCES=./src/main.cpp ./src/graph.cpp
EXECUTABLE=bellman-ford

all: $(EXECUTABLE)

$(EXECUTABLE):
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(INCLUDES)

clean:
	rm $(EXECUTABLE)
