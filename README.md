# arturiagang
A Midi Sequence for Linux / SDL to control Arturia BeatStepPro

To use binary:

sudo apt install libsdl-gfx1.2-5

To compile:

sudo apt install libsdl1.2-dev libsdl-ttf2.0-dev libsdl-gfx1.2-dev libsdl-mixer1.2-dev librtmidi-dev librtaudio-dev libsqlite3-dev

./make

sudo cp arturigang /usr/local/bin/

rm arturiagang

mkdir ~/.arturiagang

cp -r db/* ~/.arturiagang/
