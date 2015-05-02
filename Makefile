CXX=g++
LIBS?=-lSDL2
CXXFLAGS+=-O2 -Wall -fpermissive -flax-vector-conversions -pthread
SO?=.so

TARGET=mupen64plus-video-neon64$(SO)

$(TARGET):	plugin.cpp rasterize.cpp
	$(CXX) $(CXXFLAGS) -o $@ -dynamiclib $(LIBS) $^

rasterize:	rasterize.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(LIBS) $<

.PHONY:	clean

all: $(TARGET)

clean:
	rm -f mupen64plus-video-neon64$(SO) rasterize

