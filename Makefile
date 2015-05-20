CXX=g++
LIBS?=-lSDL2
CXXFLAGS+=-Wall -fpermissive -flax-vector-conversions -pthread
SO?=.so
SHARED?=-shared
PIC?=-fPIC

TARGET=mupen64plus-video-neon64$(SO)

SRC=displaylist.cpp drawgl.cpp plugin.cpp textures.cpp

$(TARGET):	$(SRC) displaylist.h drawgl.h plugin.h rdp.h textures.h
	$(CXX) $(CXXFLAGS) -o $@ $(SHARED) $(PIC) $(LIBS) $(SRC)

.PHONY:	clean

all: $(TARGET)

clean:
	rm -f mupen64plus-video-neon64$(SO)

