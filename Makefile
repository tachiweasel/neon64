CXX=g++
LIBS?=-lSDL2
CXXFLAGS+=-O2 -Wall -fpermissive -flax-vector-conversions -pthread
SO?=.so

TARGET=mupen64plus-video-neon64$(SO)

SRC=displaylist.cpp plugin.cpp rasterize.cpp

$(TARGET):	$(SRC) displaylist.h plugin.h rasterize.h rdp.h
	$(CXX) $(CXXFLAGS) -o $@ -dynamiclib $(LIBS) $(SRC)

rasterize:	rasterize.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(LIBS) $<

.PHONY:	clean

all: $(TARGET)

clean:
	rm -f mupen64plus-video-neon64$(SO) rasterize

