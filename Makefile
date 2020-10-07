UNAME:=$(shell uname)

VODUS_EXTRA_CXXFLAGS=

ifdef VODUS_SSE
VODUS_EXTRA_CXXFLAGS += -DVODUS_SSE -msse4
endif

# TODO(#87): we need an option to build with system libraries
VODUS_PKGS=freetype2 libpcre2-8
VODUS_CXXFLAGS=-Wall -fno-exceptions -std=c++17 $(VODUS_EXTRA_CXXFLAGS) -ggdb `pkg-config --cflags $(VODUS_PKGS)` -I./third_party/ffmpeg-4.3-dist/usr/local/include/ -I./third_party/giflib-5.2.1-dist/usr/local/include/
VODUS_LIBS=`pkg-config --libs $(VODUS_PKGS)` -L./third_party/giflib-5.2.1-dist/usr/local/lib/ ./third_party/giflib-5.2.1-dist/usr/local/lib/libgif.a -L./third_party/ffmpeg-4.3-dist/usr/local/lib/ -lavcodec -lavutil -lswresample -pthread -lm -llzma -lz

ifeq ($(UNAME), Darwin)
VODUS_LIBS += -framework AVFoundation -framework VideoToolbox -framework CoreVideo -framework AudioToolbox -framework CoreMedia -framework CoreFoundation -liconv
endif

EMOTE_DOWNLOADER_PKGS=libcurl
EMOTE_DOWNLOADER_CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb `pkg-config --cflags $(EMOTE_DOWNLOADER_PKGS)`
EMOTE_DOWNLOADER_LIBS=`pkg-config --libs $(EMOTE_DOWNLOADER_PKGS)`

DIFFIMG_CXXFLAGS=-Wall -fno-exceptions -std=c++17 -ggdb

.PHONY: all
all: vodus.release vodus.debug emote_downloader diffimg Makefile

vodus.release: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp) $(wildcard src/stb_image*.h)
	$(CXX) $(VODUS_CXXFLAGS) -DVODUS_RELEASE -O3 -o vodus.release src/vodus.cpp $(VODUS_LIBS)

vodus.debug: $(wildcard src/vodus*.cpp) $(wildcard src/core*.cpp) stb_image.o stb_image_resize.o stb_image_write.o
	$(CXX) $(VODUS_CXXFLAGS) -O0 -fno-builtin -o vodus.debug src/vodus.cpp $(VODUS_LIBS) stb_image.o stb_image_resize.o stb_image_write.o

stb_image.o: src/stb_image.h
	$(CC) $(CFLAGS) -x c -ggdb -DSTBI_ONLY_PNG -DSTB_IMAGE_IMPLEMENTATION -c -o stb_image.o src/stb_image.h

stb_image_resize.o: src/stb_image_resize.h
	$(CC) $(CFLAGS) -x c -ggdb -DSTB_IMAGE_RESIZE_IMPLEMENTATION -c -o stb_image_resize.o src/stb_image_resize.h

stb_image_write.o: src/stb_image_write.h
	$(CC) $(CFLAGS) -x c -ggdb -DSTB_IMAGE_WRITE_IMPLEMENTATION -c -o stb_image_write.o src/stb_image_write.h

emote_downloader: src/emote_downloader.cpp $(wildcard src/core*.cpp)
	$(CXX) $(EMOTE_DOWNLOADER_CXXFLAGS) -o emote_downloader src/emote_downloader.cpp $(EMOTE_DOWNLOADER_LIBS)

.PHONY: render
render: vodus.debug
	./vodus.debug sample.txt output.mpeg --config video.conf

.PHONY: render.release
render.release: vodus.release
	./vodus.release sample.txt output.mpeg --config video.conf

diffimg: src/diffimg.cpp
	$(CXX) $(DIFFIMG_CXXFLAGS) -o diffimg src/diffimg.cpp
