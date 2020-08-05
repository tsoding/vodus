VODUS_EXTRA_CXXFLAGS=

ifdef VODUS_SSE
VODUS_EXTRA_CXXFLAGS += -DVODUS_SSE -msse4
endif

# TODO(#87): we need an option to build with system libraries
VODUS_PKGS=freetype2
VODUS_CXXFLAGS=-Wall -fno-exceptions -std=c++17 $(VODUS_EXTRA_CXXFLAGS) -ggdb `pkg-config --cflags $(VODUS_PKGS)` -I./third_party/ffmpeg-4.3-dist/usr/local/include/ -I./third_party/giflib-5.2.1-dist/usr/local/include/
VODUS_LIBS=`pkg-config --libs $(VODUS_PKGS)` -L./third_party/giflib-5.2.1-dist/usr/local/lib/ -l:libgif.a -L./third_party/ffmpeg-4.3-dist/usr/local/lib/ -lavcodec -lavutil -lswresample -pthread -lm -llzma -lz

EMOTE_DOWNLOADER_PKGS=libcurl
EMOTE_DOWNLOADER_CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb `pkg-config --cflags $(EMOTE_DOWNLOADER_PKGS)`
EMOTE_DOWNLOADER_LIBS=`pkg-config --libs $(EMOTE_DOWNLOADER_PKGS)`

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
	./vodus.debug sample.txt output.mpeg --config video.conf

.PHONY: render.release
render.release: vodus.release
	./vodus.release sample.txt output.mpeg --config video.conf
