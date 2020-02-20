PKGS=freetype2 libpng libavcodec libavutil
CXXFLAGS=-Wall -O3 -fno-builtin -fno-eliminate-unused-debug-types -std=c++17 -ggdb $(shell pkg-config --cflags $(PKGS)) 
LIBS=$(shell pkg-config --libs $(PKGS)) -lgif -lpthread

.PHONY: all
all: vodus Makefile

vodus: src/vodus.cpp src/vodus_image32.cpp src/vodus_main.cpp
	$(CXX) $(CXXFLAGS) -o vodus src/vodus.cpp $(LIBS)

.PHONY: render
render: output.mp4

output.mp4: vodus
	./vodus 'Chat! Vy r u ded?' phpHop.gif gasm.png
