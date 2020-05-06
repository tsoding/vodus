PKGS=freetype2 libpng libavcodec libavutil libcurl
CXXFLAGS=-Wall -O3 -fno-exceptions -std=c++17 -ggdb $(shell pkg-config --cflags $(PKGS))
LIBS=$(shell pkg-config --libs $(PKGS)) -lgif -lpthread

.PHONY: all
all: vodus bttv_downloader Makefile

vodus: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp)
	$(CXX) $(CXXFLAGS) -o vodus src/vodus.cpp $(LIBS)

bttv_downloader: src/bttv_downloader.cpp $(wildcard src/core*.cpp)
	$(CXX) $(CXXFLAGS) -o bttv_downloader src/bttv_downloader.cpp $(LIBS)

.PHONY: render
render: output.mpeg

output.mpeg: vodus
	./vodus --font assets/ComicNeue_Bold.otf --output output.mpeg --limit 20 sample.txt
