PKGS=freetype2 libpng libavcodec libavutil libcurl
CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb $(shell pkg-config --cflags $(PKGS))
CXXFLAGS_RELEASE=$(CXXFLAGS) -O3
CXXFLAGS_DEBUG=$(CXXFLAGS) -O0 -fno-builtin
LIBS=$(shell pkg-config --libs $(PKGS)) -lgif -lpthread

.PHONY: all
all: vodus.release vodus.debug emote_downloader Makefile

vodus.release: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp)
	$(CXX) $(CXXFLAGS_RELEASE) -o vodus.release src/vodus.cpp $(LIBS)

vodus.debug: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp)
	$(CXX) $(CXXFLAGS_DEBUG) -o vodus.debug src/vodus.cpp $(LIBS)

emote_downloader: src/emote_downloader.cpp $(wildcard src/core*.cpp)
	$(CXX) $(CXXFLAGS) -o emote_downloader src/emote_downloader.cpp $(LIBS)

.PHONY: render
render: output.mpeg

output.mpeg: vodus.release
	./vodus.release --font assets/ComicNeue_Bold.otf --output output.mpeg --limit 20 sample.txt

