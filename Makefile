CXX=g++
LIBS?=-lSDL2
CXXFLAGS+=-Wall -fpermissive -flax-vector-conversions -pthread
SO?=.so
SHARED?=-shared
PIC?=-fPIC

TARGET=mupen64plus-video-neon64$(SO)

SRC=displaylist.cpp plugin.cpp rasterize.cpp textures.cpp

$(TARGET):	$(SRC) displaylist.h plugin.h rasterize.h rdp.h textures.h
	$(CXX) $(CXXFLAGS) -o $@ $(SHARED) $(PIC) $(LIBS) $(SRC)

rasterize:	rasterize.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(LIBS) $<

.PHONY:	clean

all: $(TARGET)

clean:
	rm -f mupen64plus-video-neon64$(SO) rasterize

