#!/bin/bash

g++ -v -O0 -g3 -w -Wall -Wconversion -c -fmessage-length=0 -MMD -MP -MF"main.d" -MT"main.o" -o "main.o" "main.cpp"
g++ -v -o "arturiagang" ./main.o -lSDL -lSDL_image -lSDL_ttf -lSDL_gfx -lSDL_mixer -lrtmidi -lpthread -lsqlite3
rm main.d main.o
