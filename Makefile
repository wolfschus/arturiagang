CC       = gcc
CXX      = g++
CCFLAGS  = -Wall
CXXFLAGS = -Wall -Wno-unused-variable -Wno-write-strings 
LDFLAGS  = -lSDL -lSDL_image -lSDL_ttf -lSDL_gfx -lSDL_mixer -lrtmidi -lpthread -lsqlite3

TARGET  = arturiagang
OBJECTS = main.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $(OBJECTS)

.cpp.o:
	$(CXX) -c $< $(CXXFLAGS)

clean:
	rm $(OBJECTS)
	rm $(TARGET)

setup:
	if test -d ~/.$(TARGET); \
	then echo .$(TARGET) exists; \
	else mkdir ~/.$(TARGET); \
	cp -r db/* ~/.$(TARGET); \
	fi

install:
	cp $(TARGET) /usr/local/bin/
