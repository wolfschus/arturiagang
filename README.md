# arturiagang
A Midi Sequence for Linux / SDL to control Arturia BeatStepPro.

Works perfect on Raspberry Pi with touchscreen. To use touchscreen, please run it in fullscreen on X.

short video: https://youtu.be/BszJ_SOvSSw

To use binary:

sudo apt install libsdl-gfx1.2-5

To compile:

sudo apt install libsdl1.2-dev libsdl-ttf2.0-dev libsdl-gfx1.2-dev libsdl-mixer1.2-dev librtmidi-dev librtaudio-dev libsqlite3-dev
make

To install:

make setup
sudo make install
