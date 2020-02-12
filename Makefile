PKGS=freetype2 libpng
CXXFLAGS=-Wall -O3 -fno-builtin -fno-eliminate-unused-debug-types -std=c++17 -ggdb $(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lgif -lpthread

all: vodus Makefile

vodus: main.cpp
	clang++ $(CXXFLAGS) -o vodus main.cpp $(LIBS)

.PHONY: render
render: output.mp4

output.mp4: vodus
	rm -rfv output/
	mkdir -p output/
	./vodus 'Chat! Vy r u ded?' phpHop.gif gasm.png > /dev/null
	ffmpeg -y -framerate 60 -i 'output/frame-%04d.png' output.mp4
