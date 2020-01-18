CXXFLAGS=-Wall -O0 -fno-builtin -ggdb $(shell pkg-config --cflags freetype2)
LIBS=$(shell pkg-config --libs freetype2)

all: vodus Makefile

vodus: main.cpp
	clang++ $(CXXFLAGS) -o vodus main.cpp $(LIBS)
