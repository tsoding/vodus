PKGS=freetype2 libpng
CXXFLAGS=-Wall -O0 -fno-builtin -fno-eliminate-unused-debug-types -ggdb $(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lgif

all: vodus Makefile

vodus: main.cpp
	clang++ $(CXXFLAGS) -o vodus main.cpp $(LIBS)
