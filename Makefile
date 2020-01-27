CXXFLAGS=-Wall -O0 -fno-builtin -fno-eliminate-unused-debug-types -ggdb $(shell pkg-config --cflags freetype2)
LIBS=$(shell pkg-config --libs freetype2)

all: vodus Makefile

vodus: main.cpp
	clang++ $(CXXFLAGS) -o vodus main.cpp $(LIBS) -lgif
