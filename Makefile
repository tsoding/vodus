VODUS_PKGS=freetype2 libavcodec libavutil
VODUS_CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb `pkg-config --cflags $(VODUS_PKGS)`
VODUS_LIBS=`pkg-config --libs $(VODUS_PKGS)` -lgif

EMOTE_DOWNLOADER_PKGS=libcurl
EMOTE_DOWNLOADER_CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb `pkg-config --cflags $(EMOTE_DOWNLOADER_PKGS)`
EMOTE_DOWNLOADER_LIBS=`pkg-config --libs $(EMOTE_DOWNLOADER_PKGS)`

RENDER_ARGS=--font assets/ComicNeue_Bold.otf --output output.mpeg --font-size 46 --fps 30 --width 704 --height 576 --limit 20 sample.txt --bitrate 6000000

.PHONY: all
all: vodus.release vodus.debug emote_downloader Makefile

vodus.release: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp)
	$(CXX) $(VODUS_CXXFLAGS) -O3 -o vodus.release src/vodus.cpp $(VODUS_LIBS)

vodus.debug: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp)
	$(CXX) $(VODUS_CXXFLAGS) -O0 -fno-builtin -o vodus.debug src/vodus.cpp $(VODUS_LIBS)

emote_downloader: src/emote_downloader.cpp $(wildcard src/core*.cpp)
	$(CXX) $(EMOTE_DOWNLOADER_CXXFLAGS) -o emote_downloader src/emote_downloader.cpp $(EMOTE_DOWNLOADER_LIBS)

.PHONY: render
render: vodus.debug
	./vodus.debug $(RENDER_ARGS)

.PHONY: render.release
render.release: vodus.release
	./vodus.release $(RENDER_ARGS)
