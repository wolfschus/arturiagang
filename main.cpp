//============================================================================
// Name        : Arturiagang.cpp
// Author      : Wolfgang Schuster
// Version     : 0.96 06.02.2020
// Copyright   : Wolfgang Schuster
// Description : Sequencer to control BeatStepPro
// License     : GNU General Public License v3.0
//============================================================================

#include <iostream>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rotozoom.h>
#include <string>
#include <vector>
#include <pthread.h>
#include "rtmidi/RtMidi.h"
#include <sqlite3.h>
#include <unistd.h>
#include <algorithm>
#include <sys/sysinfo.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "images/media-playback-start.xpm"
#include "images/media-playback-pause.xpm"
#include "images/media-playback-stop.xpm"
#include "images/media-skip-backward.xpm"
#include "images/media-skip-forward.xpm"
#include "images/media-seek-backward.xpm"
#include "images/media-seek-forward.xpm"
#include "images/gtk-edit.xpm"
#include "images/help-info.xpm"
#include "images/system-shutdown.xpm"
#include "images/dialog-ok-apply.xpm"
#include "images/document-save.xpm"
#include "images/folder-drag-accept.xpm"
#include "images/window-close.xpm"
#include "images/document-properties.xpm"
#include "images/help-about.xpm"
#include "images/go-previous.xpm"
#include "images/go-next.xpm"
#include "images/go-first.xpm"
#include "images/go-last.xpm"
#include "images/go-up.xpm"
#include "images/go-down.xpm"
#include "images/plug.xpm"

using namespace std;

char cstring[512];
char playmode = 0;
int pattern[12][256];

struct arturiasettings{
	string name;
	unsigned int mididevice;
	int midichannel;
	int minprog;
	int maxprog;
	int minbank;
	int maxbank;
	int aktbank;
	bool master;
	bool aktiv=false;
};

arturiasettings asettmp;
vector <arturiasettings> aset;

struct songsettings{
	string name;
	int bspproject;
	int bpm;
};

vector <songsettings> songset;

int aktpos=0;
int startpos=0;
int aktstep=15;
int oldstep=0;
int miditick=0;
int midiintick=0;
int oldmiditick=0;
int timedivision=16;
int maxstep = 15;
int bpm = 60;
float bpmcorrect=1.00;
bool timerrun=false;
bool clockmodeext=false;
bool exttimerrun = false;
bool sequencegang5 = false;

struct cpuwerte{
	float idle;
	float usage;
};

cpuwerte oldcpu;
cpuwerte newcpu;
float cpuusage;
int anzahlcpu;
int cputimer = 0;
struct sysinfo memInfo;

RtMidiOut *midiout = new RtMidiOut();
RtMidiIn *midiin = new RtMidiIn();

class WSMidi
{
public:

	void Clock_Start(uint mididevice)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(0xFA);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void Clock_Cont(uint mididevice)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(0xFB);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void Clock_Stop(uint mididevice)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(0xFC);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void Clock_Tick(uint mididevice)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(0xF8);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void Play()
	{
		playmode=1;
		Clock_Start(aset[3].mididevice);
		Clock_Start(aset[4].mididevice);
		if(clockmodeext==false)
		{
			Clock_Start(aset[5].mididevice);
			Clock_Start(aset[11].mididevice);
		}
		timerrun=true;
		aktstep=15;
	}

	void Pause()
	{
		playmode=2;
		Clock_Stop(aset[3].mididevice);
		Clock_Stop(aset[4].mididevice);
		if(clockmodeext==false)
		{
			Clock_Stop(aset[5].mididevice);
			Clock_Stop(aset[11].mididevice);
		}
		timerrun=false;

	}

	void Stop()
	{
		for(int i=0;i<6;i++)
		{
		  if(pattern[i][0])
		  {
			  ProgramChange(aset[i].mididevice, aset[i].midichannel, pattern[i][0]-1);
		  }
		}
		playmode=0;
		Clock_Stop(aset[3].mididevice);
		Clock_Stop(aset[4].mididevice);
		Clock_Stop(aset[5].mididevice);
		Clock_Stop(aset[11].mididevice);
		timerrun=false;
		aktstep=15;
		aktpos=0;
		startpos=0;
	}

	void ProgramChange(uint mididevice, int midichannel, int program)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(192+midichannel);
			message.push_back(program);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void ArturiaBankChange(uint mididevice, int midichannel, int bank)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(176+midichannel);
			message.push_back(0);
			message.push_back(bank);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void ArturiaSongSelect(uint mididevice, int midichannel, int song)
	{
		vector<unsigned char> message;
		if(mididevice<midiout->getPortCount())
		{
			midiout->openPort(mididevice);
			message.clear();
			message.push_back(243);
			message.push_back(song);
			midiout->sendMessage( &message );
			midiout->closePort();
		}
		return;
	}

	void NextTick()
	{
	  Clock_Tick(aset[3].mididevice);
	  Clock_Tick(aset[4].mididevice);
	  if(clockmodeext==false)
	  {
		  Clock_Tick(aset[5].mididevice);
		  Clock_Tick(aset[11].mididevice);
	  }

	  oldmiditick=miditick;
	  if(miditick<5)
	  {
		  miditick++;
	  }
	  else
	  {
		  miditick=0;
		  oldstep=aktstep;
		  aktstep++;
		  if(aktstep==8)
		  {
			  for(int i=0;i<11;i++)
			  {
				  if(pattern[i][aktpos]>0)
				  {
					  aset[i].aktiv=true;
						if(aset[i].minbank==aset[i].maxbank)
						{
							ProgramChange(aset[i].mididevice, aset[i].midichannel, pattern[i][aktpos]-1);
						}
						else
						{
							ProgramChange(aset[i].mididevice, aset[i].midichannel, pattern[i][aktpos]-1);
						}
				  }
				  else
				  {
					  aset[i].aktiv=false;
				  }
				  if(i==4)
				  {
					  i++;
				  }
			  }
		  }
		  if(aktstep>maxstep)
		  {
			  aktstep=0;
			  if(pattern[5][aktpos]==1)
			  {
				playmode=0;
				Clock_Stop(aset[3].mididevice);
				Clock_Stop(aset[4].mididevice);
				Clock_Stop(aset[5].mididevice);
				Clock_Stop(aset[11].mididevice);
				timerrun=false;
				aktstep=15;
				aktpos=0;
				startpos=0;
			  }
			  if(aktpos<255)
			  {
				  aktpos++;
			  }
			  else
			  {
				  aktpos=0;
			  }
		  }
	  }
	}
};

WSMidi wsmidi;

class ThreadClass
{
public:
   void run()
   {
      pthread_t thread;

      pthread_create(&thread, NULL, entry, this);
   }

   void UpdateMidiTimer()
   {
      while(1)
	  {
    	  usleep(60000000/(24*(bpm*bpmcorrect+60)));
    	  if(timerrun==true and clockmodeext==false)
    	  {
    		  wsmidi.NextTick();
     	  }
	  }
   }

private:
   static void * entry(void *ptr)
   {
      ThreadClass *tc = reinterpret_cast<ThreadClass*>(ptr);
      tc->UpdateMidiTimer();
      return 0;
   }
};

class ThreadCPUClass
{
public:
   void run()
   {
      pthread_t thread;

      pthread_create(&thread, NULL, entry, this);
   }

   void UpdateCPUTimer()
   {
      while(1)
	  {
    	  usleep(200000);
    	  oldcpu.idle=newcpu.idle;
    	  oldcpu.usage=newcpu.usage;
    	  newcpu=get_cpuusage();
    	  cpuusage=((newcpu.usage - oldcpu.usage)/(newcpu.idle + newcpu.usage - oldcpu.idle - oldcpu.usage))*100;
    	  sysinfo (&memInfo);
	  }
   }

   cpuwerte get_cpuusage()
   {
		ifstream fileStat("/proc/stat");
		string line;

		cpuwerte cpuusage;

		while(getline(fileStat,line))
		{
			if(line.find("cpu ")==0)
			{
				istringstream ss(line);
				string token;

				vector<string> result;
				while(std::getline(ss, token, ' '))
				{
					result.push_back(token);
				}
				cpuusage.idle=atof(result[5].c_str()) + atof(result[6].c_str());
				cpuusage.usage=atof(result[2].c_str()) + atof(result[3].c_str()) + atof(result[4].c_str()) + atof(result[7].c_str()) + atof(result[8].c_str()) + atof(result[9].c_str()) + atof(result[10].c_str()) + atof(result[11].c_str());
			}
		}
		return cpuusage;

   }

private:
   static void * entry(void *ptr)
   {
      ThreadCPUClass *tcc = reinterpret_cast<ThreadCPUClass*>(ptr);
      tcc->UpdateCPUTimer();
      return 0;
   }
};

class WSButton
{

public:
	bool aktiv;
	SDL_Rect button_rect;
	SDL_Rect image_rect;
	SDL_Rect text_rect;
	SDL_Surface* button_image;
	string button_text;
	int button_width;
	int button_height;

	WSButton()
	{
		button_text="";
		button_image=NULL;
		button_width=2;
		button_height=2;
		aktiv = false;
		button_rect.x = 0+3;
		button_rect.y = 0+3;
		button_rect.w = 2*48-6;
		button_rect.h = 2*48-6;
	}

	WSButton(int posx, int posy, int width, int height, int scorex, int scorey, SDL_Surface* image, string text)
	{
		button_text=text;
		button_image=image;
		button_width=width;
		button_height=height;
		aktiv = false;
		button_rect.x = posx*scorex+3;
		button_rect.y = posy*scorey+3;
		button_rect.w = button_width*scorex-6;
		button_rect.h = 2*scorey-6;
	}

	void show(SDL_Surface* screen, TTF_Font* font)
	{
		if(aktiv==true)
		{
			boxColor(screen, button_rect.x,button_rect.y,button_rect.x+button_rect.w,button_rect.y+button_rect.h,0x008F00FF);
		}
		else
		{
			boxColor(screen, button_rect.x,button_rect.y,button_rect.x+button_rect.w,button_rect.y+button_rect.h,0x8F8F8FFF);
		}

		if(button_image!=NULL)
		{
			image_rect.x = button_rect.x+2;
			image_rect.y = button_rect.y+1;
			SDL_BlitSurface(button_image, 0, screen, &image_rect);
		}
		if(button_text!="")
		{
			SDL_Surface* button_text_image;
			SDL_Color blackColor = {0, 0, 0};
			button_text_image = TTF_RenderText_Blended(font, button_text.c_str(), blackColor);
			text_rect.x = button_rect.x+button_rect.w/2-button_text_image->w/2;
			text_rect.y = button_rect.y+button_rect.h/2-button_text_image->h/2;
			SDL_BlitSurface(button_text_image, 0, screen, &text_rect);
//			SDL_FreeSurface(button_text_image);
		}
		return;
	}

	~WSButton()
	{
		return;
	}
};

static int settingscallback(void* data, int argc, char** argv, char** azColName)
{
	asettmp.name=argv[1];
	asettmp.mididevice=atoi(argv[2]);
	asettmp.midichannel=atoi(argv[3]);
	asettmp.maxbank=atoi(argv[4]);
	asettmp.maxprog=atoi(argv[5]);
	asettmp.minbank=atoi(argv[6]);
	asettmp.minprog=atoi(argv[7]);
	asettmp.master=atoi(argv[8]);
	aset.push_back(asettmp);

	return 0;
}

static int patterncallback(void* data, int argc, char** argv, char** azColName)
{
	pattern[atoi(argv[2])][atoi(argv[1])]=atoi(argv[3]);
	return 0;
}

static int patternnamecallback(void* data, int argc, char** argv, char** azColName)
{
	songsettings songsettmp;
	songsettmp.name=argv[1];
	songsettmp.bspproject=atoi(argv[2]);
	songsettmp.bpm=atoi(argv[3]);
	songset.push_back(songsettmp);
	return 0;
}

void midiincallback( double deltatime, std::vector< unsigned char > *message, void *userData )
{
	unsigned int nBytes = message->size();
	if(clockmodeext==true)
	{
//		for(int i=0;i<nBytes;i++)
//		{
//			cout << (int)message->at(i) << " ";
//		}
//		cout << endl;

		if((int)message->at(0)==248 and playmode==1)
		{
			if(midiintick>1)
			{
				wsmidi.NextTick();
				midiintick=0;
			}
			midiintick++;
		}

		if((int)message->at(0)==240)
		{
			if((int)message->at(1)==127 and (int)message->at(2)==127 and (int)message->at(3)==6 and (int)message->at(4)==2 and (int)message->at(5)==247)
			{
				wsmidi.Play();
			}
		}
		if((int)message->at(0)==240)
		{
			if((int)message->at(1)==127 and (int)message->at(2)==127 and (int)message->at(3)==6 and (int)message->at(4)==1 and (int)message->at(5)==247)
			{
				wsmidi.Stop();
			}
		}
		if((int)message->at(0)==240)
		{
			if((int)message->at(1)==127 and (int)message->at(2)==127 and (int)message->at(3)==6 and (int)message->at(4)==9 and (int)message->at(5)==247)
			{
				wsmidi.Pause();
			}
		}
	}
	return;
}


bool CheckMouse(int mousex, int mousey, SDL_Rect Position)
{
	if( ( mousex > Position.x ) && ( mousex < Position.x + Position.w ) && ( mousey > Position.y ) && ( mousey < Position.y + Position.h ) )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

bool ClearPattern()
{
	for(int i=0;i<6;i++)
	{
		for(int j=0;j<256;j++)
		{
			pattern[i][j]=0;
		}
	}
	return true;

}
int main(int argc, char* argv[])
{
	bool debug=false;
	bool fullscreen=false;

// Argumentverarbeitung
	for (int i = 0; i < argc; ++i)
	{
		if(string(argv[i])=="--fullscreen" or string(argv[i])=="-f")
		{
			fullscreen=true;
		}
		if(string(argv[i])=="--debug" or string(argv[i])=="-d")
		{
			debug=true;
		}
		if(string(argv[i])=="--help" or string(argv[i])=="-h")
		{
			cout << "Arturiagang V1.0 (c) 1987-2020 by Wolfgang Schuster" << endl;
			cout << "--debug or -d = Debug" << endl;
			cout << "--fullscreen or -f = fullscreen" << endl;
			cout << "--help or -h = this help" << endl;
			cout << "--midiports or -m = available Midiports" << endl;
			SDL_Quit();
			return 0;
		}
		if(string(argv[i])=="--midiports" or string(argv[i])=="-m")
		{
			// Check available Midi ports.
			int onPorts = midiout->getPortCount();
			cout << "MIDI OUT" << endl;
			if ( onPorts == 0 )
			{
				cout << "No ports available!" << endl;
			}
			else
			{
				for(int i=0;i<onPorts;i++)
				{
					cout << midiout->getPortName(i) << endl;
				}
			}
			int inPorts = midiin->getPortCount();
			cout << "MIDI IN" << endl;
			if ( inPorts == 0 )
			{
				cout << "No ports available!" << endl;
			}
			else
			{
				for(int i=0;i<inPorts;i++)
				{
					cout << midiin->getPortName(i) << endl;
				}
			}
			SDL_Quit();
			return 0;
		}
	}

	ThreadClass tc;
	tc.run();
	ThreadCPUClass tcc;
	tcc.run();

	if(SDL_Init(SDL_INIT_VIDEO) == -1)
	{
		std::cerr << "Konnte SDL nicht initialisieren! Fehler: " << SDL_GetError() << std::endl;
		return -1;
	}
	SDL_Surface* screen;
	if(fullscreen==true)
	{
		screen = SDL_SetVideoMode(1024, 600 , 32, SDL_DOUBLEBUF|SDL_FULLSCREEN);
	}
	else
	{
		screen = SDL_SetVideoMode(1024, 600 , 32, SDL_DOUBLEBUF);
	}
	if(!screen)
	{
	    std::cerr << "Konnte SDL-Fenster nicht erzeugen! Fehler: " << SDL_GetError() << std::endl;
	    return -1;
	}
	int scorex = screen->w/36;
	int scorey = screen->h/21;

	if(TTF_Init() == -1)
	{
	    std::cerr << "Konnte SDL_ttf nicht initialisieren! Fehler: " << TTF_GetError() << std::endl;
	    return -1;
	}
	TTF_Font* fontbold = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22);
	if(!fontbold)
	{
	    std::cerr << "Konnte Schriftart nicht laden! Fehler: " << TTF_GetError() << std::endl;
	    return -1;
	}
	TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 22);
	if(!font)
	{
	    std::cerr << "Konnte Schriftart nicht laden! Fehler: " << TTF_GetError() << std::endl;
	    return -1;
	}
	TTF_Font* fontsmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
	if(!fontsmall)
	{
	    std::cerr << "Konnte Schriftart nicht laden! Fehler: " << TTF_GetError() << std::endl;
	    return -1;
	}
	SDL_WM_SetCaption("Arturiagang", "Arturiagang");

	bool run = true;


	// [vor der Event-Schleife] In diesem Array merken wir uns, welche Tasten gerade gedrückt sind.
	bool keyPressed[SDLK_LAST];
	memset(keyPressed, 0, sizeof(keyPressed));
	SDL_EnableUNICODE(1);
	SDL_Rect textPosition;
	textPosition.x = 5;
	textPosition.y = 0;
	SDL_Surface* text = NULL;
	SDL_Color textColor = {225, 225, 225};
	SDL_Color blackColor = {0, 0, 0};

	SDL_Rect rahmen;
	rahmen.x = 4*scorex;
	rahmen.y = 3*scorey;
	rahmen.w = 32*scorex;
	rahmen.h = 12*scorey;

	SDL_Surface* start_image = IMG_ReadXPMFromArray(media_playback_start_xpm);
	SDL_Surface* stop_image = IMG_ReadXPMFromArray(media_playback_stop_xpm);
	SDL_Surface* pause_image = IMG_ReadXPMFromArray(media_playback_pause_xpm);
	SDL_Surface* next_image = IMG_ReadXPMFromArray(media_skip_forward_xpm);
	SDL_Surface* prev_image = IMG_ReadXPMFromArray(media_skip_backward_xpm);
	SDL_Surface* ff_image = IMG_ReadXPMFromArray(media_seek_forward_xpm);
	SDL_Surface* fb_image = IMG_ReadXPMFromArray(media_seek_backward_xpm);
	SDL_Surface* edit_image = IMG_ReadXPMFromArray(gtk_edit_xpm);
	SDL_Surface* info_image = IMG_ReadXPMFromArray(help_info_xpm);
	SDL_Surface* exit_image = IMG_ReadXPMFromArray(system_shutdown_xpm);
	SDL_Surface* ok_image = IMG_ReadXPMFromArray(dialog_ok_apply_xpm);
	SDL_Surface* save_image = IMG_ReadXPMFromArray(document_save_xpm);
	SDL_Surface* open_image = IMG_ReadXPMFromArray(folder_drag_accept_xpm);
	SDL_Surface* cancel_image = IMG_ReadXPMFromArray(window_close_xpm);
	SDL_Surface* settings_image = IMG_ReadXPMFromArray(document_properties_xpm);
	SDL_Surface* manuell_image = IMG_ReadXPMFromArray(help_about_xpm);
	SDL_Surface* left_image = IMG_ReadXPMFromArray(go_previous_xpm);
	SDL_Surface* right_image = IMG_ReadXPMFromArray(go_next_xpm);
	SDL_Surface* first_image = IMG_ReadXPMFromArray(go_first_xpm);
	SDL_Surface* last_image = IMG_ReadXPMFromArray(go_last_xpm);
	SDL_Surface* up_image = IMG_ReadXPMFromArray(go_up_xpm);
	SDL_Surface* down_image = IMG_ReadXPMFromArray(go_down_xpm);
	SDL_Surface* plug_image = IMG_ReadXPMFromArray(plug_xpm);

	char tmp[256];

	char sql[512];

	// Load SettingsDB
	sqlite3 *settingsdb;
	char dbpath[1024];
	sprintf(dbpath, "%s/.arturiagang/settings.db", getenv("HOME"));

	if(sqlite3_open(dbpath, &settingsdb) != SQLITE_OK)
	{
		cout << "Fehler beim Öffnen: " << sqlite3_errmsg(settingsdb) << endl;
		return 1;
	}
	cout << "Settingsdatenbank erfolgreich geöffnet!" << endl;
	sprintf(sql, "SELECT * FROM settings");
	if( sqlite3_exec(settingsdb,sql,settingscallback,0,0) != SQLITE_OK)
	{
		cout << "Fehler beim SELECT: " << sqlite3_errmsg(settingsdb) << endl;
		return 1;
	}
	sqlite3_close(settingsdb);

	int aktbank[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
	int bspproject = 0;
	int selsetmididevice = 0;
	SDL_Rect selsetmidideviceRect;
	selsetmidideviceRect.x = 5.5*scorex;
	selsetmidideviceRect.y = 4.5*scorey;
	selsetmidideviceRect.w = 24*scorex;
	selsetmidideviceRect.h = 12*scorey;
	int selsetmidichannel = 0;
	SDL_Rect selsetmidichannelRect;
	selsetmidichannelRect.x = 30*scorex;
	selsetmidichannelRect.y = 4.5*scorey;
	selsetmidichannelRect.w = 6*scorex;
	selsetmidichannelRect.h = 12*scorey;
	int selsetminprog = 0;
	SDL_Rect selsetminprogRect;
	selsetminprogRect.x = 7*scorex;
	selsetminprogRect.y = 4.5*scorey;
	selsetminprogRect.w = 4*scorex;
	selsetminprogRect.h = 10*scorey;
	int selsetmaxprog = 0;
	SDL_Rect selsetmaxprogRect;
	selsetmaxprogRect.x = 13*scorex;
	selsetmaxprogRect.y = 4.5*scorey;
	selsetmaxprogRect.w = 4*scorex;
	selsetmaxprogRect.h = 10*scorey;
	int selsetminbank = 0;
	SDL_Rect selsetminbankRect;
	selsetminbankRect.x = 19*scorex;
	selsetminbankRect.y = 4.5*scorey;
	selsetminbankRect.w = 4*scorex;
	selsetminbankRect.h = 10*scorey;
	int selsetmaxbank = 0;
	SDL_Rect selsetmaxbankRect;
	selsetmaxbankRect.x = 25*scorex;
	selsetmaxbankRect.y = 4.5*scorey;
	selsetmaxbankRect.w = 4*scorex;
	selsetmaxbankRect.h = 10*scorey;

	char keyboard_letters[40][2] = {"1","2","3","4","5","6","7","8","9","0","q","w","e","r","t","z","u","i","o","p","a","s","d","f","g","h","j","k","l","+","y","x","c","v","b","n","m",",",".","-"};
	char shkeyboard_letters[40][2] = {"1","2","3","4","5","6","7","8","9","0","Q","W","E","R","T","Z","U","I","O","P","A","S","D","F","G","H","J","K","L","*","Y","X","C","V","B","N","M",";",":","_"};

	anzahlcpu=get_nprocs();
	sysinfo (&memInfo);

	int aktprog[5] = {aset[0].minprog,aset[1].minprog,aset[2].minprog,aset[3].minprog,aset[4].minprog};

	SDL_Rect imagePosition;

	WSButton start(30,19,2,2,scorex,scorey,start_image,"");
	WSButton pause(30,19,2,2,scorex,scorey,pause_image,"");
	WSButton stop(28,19,2,2,scorex,scorey,stop_image,"");
	WSButton ff(32,19,2,2,scorex,scorey,ff_image,"");
	WSButton fb(26,19,2,2,scorex,scorey,fb_image,"");
	WSButton next(34,19,2,2,scorex,scorey,next_image,"");
	WSButton prev(24,19,2,2,scorex,scorey,prev_image,"");
	WSButton exit(0,19,2,2,scorex,scorey,exit_image,"");
	WSButton info(2,19,2,2,scorex,scorey,info_image,"");
	WSButton settings(4,19,2,2,scorex,scorey,settings_image,"");
	WSButton open(6,19,2,2,scorex,scorey,open_image,"");
	WSButton save(8,19,2,2,scorex,scorey,save_image,"");
	WSButton manuell(10,19,2,2,scorex,scorey,manuell_image,"");
	WSButton prog(14,19,2,2,scorex,scorey,NULL,"Prog");
	WSButton bank(16,19,2,2,scorex,scorey,NULL,"Bank");
	WSButton clear(18,19,2,2,scorex,scorey,NULL,"Clear");
	WSButton edit(20,19,2,2,scorex,scorey,edit_image,"");
	WSButton bspff(8,17,2,2,scorex,scorey,right_image,"");
	WSButton bspfb(4,17,2,2,scorex,scorey,left_image,"");
	WSButton bpm10ff(20,17,2,2,scorex,scorey,last_image,"");
	WSButton bpmff(18,17,2,2,scorex,scorey,right_image,"");
	WSButton bpmfb(14,17,2,2,scorex,scorey,left_image,"");
	WSButton bpm10fb(12,17,2,2,scorex,scorey,first_image,"");
	WSButton bpmcor10ff(32,17,2,2,scorex,scorey,last_image,"");
	WSButton bpmcorff(30,17,2,2,scorex,scorey,right_image,"");
	WSButton bpmcorfb(26,17,2,2,scorex,scorey,left_image,"");
	WSButton bpmcor10fb(24,17,2,2,scorex,scorey,first_image,"");
	WSButton ok(16,19,2,2,scorex,scorey,ok_image,"");
	WSButton cancel(18,19,2,2,scorex,scorey,cancel_image,"");
	WSButton settings_next(34,19,2,2,scorex,scorey,next_image,"");
	WSButton settings_prev(34,19,2,2,scorex,scorey,prev_image,"");
	WSButton settings_up(24,19,2,2,scorex,scorey,up_image,"");
	WSButton settings_down(26,19,2,2,scorex,scorey,down_image,"");
	WSButton extmidi(0,17,2,2,scorex,scorey,plug_image,"");
	WSButton sequencegang5up(34,15,2,2,scorex,scorey,up_image,"");
	WSButton sequencegang5down(34,15,2,2,scorex,scorey,down_image,"");
	WSButton setsequencegang5up(34,17,2,2,scorex,scorey,up_image,"");
	WSButton setsequencegang5down(34,17,2,2,scorex,scorey,down_image,"");

	vector <WSButton> manuell_progdown;
	vector <WSButton> manuell_progup;
	vector <WSButton> manuell_progok;
	vector <WSButton> manuell_bankdown;
	vector <WSButton> manuell_bankup;

	for(int i=0;i<5;i++)
	{
		WSButton tmp1(6,5+(2*i),2,2,scorex,scorey,left_image,"");
		manuell_progdown.push_back(tmp1);
		WSButton tmp2(10,5+(2*i),2,2,scorex,scorey,right_image,"");
		manuell_progup.push_back(tmp2);
		WSButton tmp3(33,5+(2*i),2,2,scorex,scorey,ok_image,"");
		manuell_progok.push_back(tmp3);
		WSButton tmp4(14,5+(2*i),2,2,scorex,scorey,left_image,"");
		manuell_bankdown.push_back(tmp4);
		WSButton tmp5(18,5+(2*i),2,2,scorex,scorey,right_image,"");
		manuell_bankup.push_back(tmp5);
	}

	vector <WSButton> load_song;
	for(int i=0;i<4;i++)
	{
		for(int j=0;j<4;j++)
		{
			sprintf(tmp, "%d %s", ((i+1)+4*j), "test");
			WSButton tmp1(1+9*i,5+(2*j),8,2,scorex,scorey,NULL,tmp);
			load_song.push_back(tmp1);
		}
	}

	int save_song=0;
	int fortschritt = 0;

	vector <WSButton> keyboard;
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,8,2,2,scorex,scorey,NULL,keyboard_letters[i]);
		keyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,10,2,2,scorex,scorey,NULL,keyboard_letters[i+10]);
		keyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,12,2,2,scorex,scorey,NULL,keyboard_letters[i+20]);
		keyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,14,2,2,scorex,scorey,NULL,keyboard_letters[i+30]);
		keyboard.push_back(tmp);
	}

	vector <WSButton> shkeyboard;
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,8,2,2,scorex,scorey,NULL,shkeyboard_letters[i]);
		shkeyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,10,2,2,scorex,scorey,NULL,shkeyboard_letters[i+10]);
		shkeyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,12,2,2,scorex,scorey,NULL,shkeyboard_letters[i+20]);
		shkeyboard.push_back(tmp);
	}
	for(int i=0;i<10;i++)
	{
		WSButton tmp(8+2*i,14,2,2,scorex,scorey,NULL,shkeyboard_letters[i+30]);
		shkeyboard.push_back(tmp);
	}

	WSButton key_shift(8,16,4,2,scorex,scorey,NULL,"Shift");
	WSButton key_space(12,16,12,2,scorex,scorey,NULL,"Space");
	WSButton key_backspace(26,16,2,2,scorex,scorey,left_image,"");


	int aktsong=0;

	ClearPattern();

	int mousex = 0;
	int mousey = 0;

	bool anzeige = true;
	bool blinkmode = false;
	int blink = 0;
	int mode = 0;
	int settingsmode = 0;

	bool changesettings = false;
	bool changesong = false;

	string songname = "New Song";
	string songnametmp = "New Song";
	SDL_Rect songnamePosition;

	vector<unsigned char> message;
	vector<unsigned char> inmessage;
	vector<unsigned char> cc;
	message.push_back(0);
	message.push_back(0);
	message.push_back(0);
	vector<string> midioutname;

	// Check available Midi ports.
	unsigned int onPorts = midiout->getPortCount();
	if ( onPorts == 0 )
	{
		cout << "No ports available!" << endl;
	}
	else
	{
		for(unsigned int i=0;i<onPorts;i++)
		{
			midioutname.push_back(midiout->getPortName(i));
			cout << midiout->getPortName(i) << endl;
		}
	}

	// MIDI IN Device -> BestStepPro
	if(aset[5].mididevice<onPorts)
	{
		midiin->openPort( aset[5].mididevice );
		midiin->setCallback( &midiincallback );
		// Don't ignore sysex, timing, or active sensing messages.
		midiin->ignoreTypes( false, false, false );
	}

	while(run)
	{
		if(oldstep!=aktstep)
		{
			anzeige=true;
		}
		if(aktpos>(startpos+1)*16)
		{
			startpos++;
		}
		if(aktpos<startpos*16+1 and aktpos>0)
		{
			startpos--;
		}
		blink++;
		if(blink>200)
		{
			if(blinkmode==true)
			{
				blinkmode=false;
			}
			else
			{
				blinkmode=true;
			}
			anzeige=true;
			blink=0;
		}
		if(anzeige==true)
		{
			SDL_FillRect(screen, NULL, 0x000000);
			boxColor(screen, 0,0,screen->w,1.5*scorey,0x00008FFF);
			SDL_FreeSurface(text);
			text = TTF_RenderText_Blended(fontbold, "*** ARTURIAGANG 1.0 ***", textColor);
			textPosition.x = screen->w/2-text->w/2;
			textPosition.y = 0.75*scorey-text->h/2;
			SDL_BlitSurface(text, 0, screen, &textPosition);
			SDL_FreeSurface(text);
			text = TTF_RenderText_Blended(fontsmall, songname.c_str(), textColor);
			songnamePosition.x = screen->w-text->w-0.5*scorex;
			songnamePosition.y = 0.75*scorey-text->h/2;
			SDL_BlitSurface(text, 0, screen, &songnamePosition);

			if(mode==0)
			{
				for(int i=0;i<16;i++)
				{
					if(aktpos==i+1+startpos*16)
					{
						boxColor(screen, 4*scorex+(2*i)*scorex,2*scorey+3,6*scorex+(2*i)*scorex,15*scorey,0x008F00FF);
					}

					SDL_FreeSurface(text);
					sprintf(tmp, "%d",i+1+startpos*16);
					text = TTF_RenderText_Blended(fontsmall, tmp, textColor);
					textPosition.x = 5*scorex+(2*i)*scorex-text->w/2;
					textPosition.y = 3*scorey-text->h;
					SDL_BlitSurface(text, 0, screen, &textPosition);

// CPU und RAM
					boxColor(screen, 0.2*scorex,0.25*scorey,0.4*scorex,1.25*scorey,0x2F2F2FFF);
					if(cpuusage>90)
					{
						boxColor(screen, 0.2*scorex,(0.25+(100-cpuusage)/100)*scorey,0.4*scorex,1.25*scorey,0xFF0000FF);
					}
					else if(cpuusage>80)
					{
						boxColor(screen, 0.2*scorex,(0.25+(100-cpuusage)/100)*scorey,0.4*scorex,1.25*scorey,0xFFFF00FF);
					}
					else
					{
						boxColor(screen, 0.2*scorex,(0.25+(100-cpuusage)/100)*scorey,0.4*scorex,1.25*scorey,0x00FF00FF);
					}

					boxColor(screen, 0.6*scorex,0.25*scorey,0.8*scorex,1.25*scorey,0x2F2F2FFF);
					if(float(memInfo.freeram+memInfo.bufferram+memInfo.sharedram)/float(memInfo.totalram)<0.1)
					{
						boxColor(screen, 0.6*scorex,(0.25+float(memInfo.freeram+memInfo.bufferram+memInfo.sharedram)/float(memInfo.totalram))*scorey,0.8*scorex,1.25*scorey,0xFF0000FF);
					}
					else if(float(memInfo.freeram+memInfo.bufferram+memInfo.sharedram)/float(memInfo.totalram)<0.2)
					{
						boxColor(screen, 0.6*scorex,(0.25+float(memInfo.freeram+memInfo.bufferram+memInfo.sharedram)/float(memInfo.totalram))*scorey,0.8*scorex,1.25*scorey,0xFFFF00FF);
					}
					else
					{
						boxColor(screen, 0.6*scorex,(0.25+float(memInfo.freeram+memInfo.bufferram+memInfo.sharedram)/float(memInfo.totalram))*scorey,0.8*scorex,1.25*scorey,0x00FF00FF);
					}

// Taktanzeige
					for (int i=0;i<16;i++)
					{
						if(aktstep==i and playmode!=0)
						{
							boxColor(screen, 4*scorex+(2*i)*scorex+3,1.5*scorey+3,6*scorex+(2*i)*scorex-3,2*scorey-3,0x00FF00FF);
						}
						else
						{
							boxColor(screen, 4*scorex+(2*i)*scorex+3,1.5*scorey+3,6*scorex+(2*i)*scorex-3,2*scorey-3,0x8F8F8FFF);
						}
					}


// Raster
					for(int j=0;j<6;j++)
					{
						if(sequencegang5==false)
						{
							if(pattern[j][i+startpos*16]>0)
							{
								if(j==5 and pattern[5][i+startpos*16]==1)
								{
									boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0xFF0000FF);
								}
								else
								{
									boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0x00FF00FF);
								}
								if(aset[j].maxbank>1)
								{
									SDL_FreeSurface(text);
									sprintf(tmp, "%d",int((pattern[j][i+startpos*16]-1)/aset[j].maxprog));
									text = TTF_RenderText_Blended(font, tmp, blackColor);
									textPosition.x = 4*scorex+(2*i)*scorex+3;
									textPosition.y = 3*scorey+(2*j)*scorey;
									SDL_BlitSurface(text, 0, screen, &textPosition);
								}
								SDL_FreeSurface(text);
								{
									sprintf(tmp, "%d",pattern[j][i+startpos*16]-int((pattern[j][i+startpos*16]-1)/aset[j].maxprog)*aset[j].maxprog);
								}
								if(j==5 and pattern[5][i+startpos*16]==1)
								{
									sprintf(tmp, "%s","Stop");
								}
								else
								{
									sprintf(tmp, "%d",pattern[j][i+startpos*16]);
								}
								text = TTF_RenderText_Blended(font, tmp, blackColor);
								textPosition.x = 6*scorex+(2*i)*scorex-3-text->w;
								textPosition.y = 5*scorey+(2*j)*scorey-text->h;
								SDL_BlitSurface(text, 0, screen, &textPosition);
							}
							else
							{
								boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0x8F8F8FFF);
							}
						}
						else
						{
							if(pattern[j+6][i+startpos*16]>0 or (j==5 and pattern[5][i+startpos*16]>0))
							{
								if(j==5 and pattern[5][i+startpos*16]==1)
								{
									boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0xFF0000FF);
								}
								else
								{
									boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0x00FF00FF);
								}
								if(aset[j+6].maxbank>1)
								{
									SDL_FreeSurface(text);
									sprintf(tmp, "%d",int((pattern[j+6][i+startpos*16]-1)/aset[j+6].maxprog));
									text = TTF_RenderText_Blended(font, tmp, blackColor);
									textPosition.x = 4*scorex+(2*i)*scorex+3;
									textPosition.y = 3*scorey+(2*j)*scorey;
									SDL_BlitSurface(text, 0, screen, &textPosition);
								}
								SDL_FreeSurface(text);
								{
									sprintf(tmp, "%d",pattern[j+6][i+startpos*16]-int((pattern[j+6][i+startpos*16]-1)/aset[j+6].maxprog)*aset[j+6].maxprog);
								}
								if(j==5)
								{
									if(pattern[5][i+startpos*16]==1)
									{
										sprintf(tmp, "%s","Stop");
									}
									else
									{
										sprintf(tmp, "%d",pattern[5][i+startpos*16]);
									}
								}
								else
								{
									sprintf(tmp, "%d",pattern[j+6][i+startpos*16]);
								}
								text = TTF_RenderText_Blended(font, tmp, blackColor);
								textPosition.x = 6*scorex+(2*i)*scorex-3-text->w;
								textPosition.y = 5*scorey+(2*j)*scorey-text->h;
								SDL_BlitSurface(text, 0, screen, &textPosition);
							}
							else
							{
								boxColor(screen, 4*scorex+(2*i)*scorex+3,3*scorey+(2*j)*scorey+3,6*scorex+(2*i)*scorex-3,5*scorey+(2*j)*scorey-3,0x8F8F8FFF);
							}
						}
					}
				}
// Device Names
				for(int i=0+(sequencegang5*6);i<5+(sequencegang5*6);i++)
				{
					SDL_FreeSurface(text);
					sprintf(tmp, "%s",aset[i].name.c_str());
					text = TTF_RenderText_Blended(fontsmall, tmp, textColor);
					textPosition.x = 0.2*scorex;
					textPosition.y = 4*scorey+2*(i-(sequencegang5*6))*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);
				}
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontsmall, "Control", textColor);
				textPosition.x = 0.2*scorex;
				textPosition.y = 14*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

// BeatStep Pro Project
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "BeatStepPro Project", textColor);
				textPosition.x = 7*scorex-text->w/2;
				textPosition.y = 17*scorey-text->h;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				SDL_FreeSurface(text);
				sprintf(tmp, "%d",bspproject+1);
				text = TTF_RenderText_Blended(font, tmp, textColor);
				textPosition.x = 7*scorex-text->w/2;
				textPosition.y = 18*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				bspff.show(screen, fontsmall);
				bspfb.show(screen, fontsmall);

// BPM

				if(clockmodeext==false)
				{
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "BPM", textColor);
					textPosition.x = 17*scorex-text->w/2;
					textPosition.y = 17*scorey-text->h;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					SDL_FreeSurface(text);
					sprintf(tmp, "%d",bpm+60);
					text = TTF_RenderText_Blended(font, tmp, textColor);
					textPosition.x = 17*scorex-text->w/2;
					textPosition.y = 18*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					bpm10ff.show(screen, fontsmall);
					bpmff.show(screen, fontsmall);
					bpmfb.show(screen, fontsmall);
					bpm10fb.show(screen, fontsmall);

					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "BPM correction", textColor);
					textPosition.x = 29*scorex-text->w/2;
					textPosition.y = 17*scorey-text->h;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					SDL_FreeSurface(text);
					sprintf(tmp, "%.2f",bpmcorrect);
					text = TTF_RenderText_Blended(font, tmp, textColor);
					textPosition.x = 29*scorex-text->w/2;
					textPosition.y = 18*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					bpmcor10ff.show(screen, fontsmall);
					bpmcorff.show(screen, fontsmall);
					bpmcorfb.show(screen, fontsmall);
					bpmcor10fb.show(screen, fontsmall);
				}

				extmidi.show(screen, fontsmall);
				
// Umschalter BSP and sequencegang5

				if(sequencegang5==true)
				{
					sequencegang5up.show(screen, fontsmall);
				}
				else
				{
					sequencegang5down.show(screen, fontsmall);
				}
				
// Exit Info

				exit.show(screen, fontsmall);
				info.show(screen, fontsmall);
				settings.show(screen, fontsmall);
				open.show(screen, fontsmall);
				save.show(screen, fontsmall);
				manuell.show(screen, fontsmall);

// Bank / Prog Umschalter

				prog.show(screen, fontsmall);
				bank.show(screen, fontsmall);
				clear.show(screen, fontsmall);
				edit.show(screen, fontsmall);

// Play Stop Pause FF FR Next Prev Edit

				if(playmode==1)
				{
					pause.show(screen, fontsmall);
				}
				else
				{
					start.show(screen, fontsmall);
				}
				stop.show(screen, fontsmall);
				ff.show(screen, fontsmall);
				fb.show(screen, fontsmall);
				next.show(screen, fontsmall);
				prev.show(screen, fontsmall);

// Debug
				if(debug==true)
				{
					SDL_FreeSurface(text);
					sprintf(tmp, "Aktpos: %d  Startpos: %d Aktstep: %d",aktpos,startpos,aktstep);
					text = TTF_RenderText_Blended(fontsmall, tmp, textColor);
					textPosition.x = 1*scorex;
					textPosition.y = 1*scorey-text->h;
					SDL_BlitSurface(text, 0, screen, &textPosition);
				}
			}

			if(mode==1) // Info
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "(c) 1987-2020 by Wolfgang Schuster", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 3*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontsmall, "GNU General Public License v3.0", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 4*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				int i = 0;
				for(auto &mout: midioutname)
				{
					SDL_FreeSurface(text);
					sprintf(tmp, "%s",mout.c_str());
					text = TTF_RenderText_Blended(fontsmall, tmp, textColor);
					textPosition.x = 2*scorex;
					textPosition.y = (8+i)*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);
					i++;
				}

				ok.show(screen, fontsmall);
			}

			if(mode==2) // Exit
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Really Exit ?", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 10*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);
			}

			if(mode==3) // Settings
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Settings", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 2*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				boxColor(screen, 0,3*scorey,screen->w,4*scorey,0x00008FFF);

				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "Name", textColor);
				textPosition.x = 0.2*scorex;
				textPosition.y = 3*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				if(settingsmode==0)
				{
					if(selsetmididevice>0)
					{
						if(sequencegang5==true)
						{
							boxColor(screen, selsetmidideviceRect.x,selsetmidideviceRect.y+2*(selsetmididevice-7)*scorey,selsetmidideviceRect.x+selsetmidideviceRect.w,selsetmidideviceRect.y+2*(selsetmididevice-6)*scorey,0x4F4F4FFF);
						}
						else
						{
							boxColor(screen, selsetmidideviceRect.x,selsetmidideviceRect.y+2*(selsetmididevice-1)*scorey,selsetmidideviceRect.x+selsetmidideviceRect.w,selsetmidideviceRect.y+2*selsetmididevice*scorey,0x4F4F4FFF);
						}
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "MIDI Device", textColor);
					textPosition.x = 6*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					if(selsetmidichannel>0)
					{
						boxColor(screen, selsetmidichannelRect.x,selsetmidichannelRect.y+2*(selsetmidichannel-1)*scorey,selsetmidichannelRect.x+selsetmidichannelRect.w,selsetmidichannelRect.y+2*selsetmidichannel*scorey,0x4F4F4FFF);
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "MIDI Channel", textColor);
					textPosition.x = 30*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);
				}
				else if (settingsmode==1)
				{
					if(selsetminprog>0)
					{
						boxColor(screen, selsetminprogRect.x,selsetminprogRect.y+2*(selsetminprog-1)*scorey,selsetminprogRect.x+selsetminprogRect.w,selsetminprogRect.y+2*selsetminprog*scorey,0x4F4F4FFF);
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "min Prog", textColor);
					textPosition.x = 7*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					if(selsetmaxprog>0)
					{
						boxColor(screen, selsetmaxprogRect.x,selsetmaxprogRect.y+2*(selsetmaxprog-1)*scorey,selsetmaxprogRect.x+selsetmaxprogRect.w,selsetmaxprogRect.y+2*selsetmaxprog*scorey,0x4F4F4FFF);
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "max Prog", textColor);
					textPosition.x = 13*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					if(selsetminbank>0)
					{
						boxColor(screen, selsetminbankRect.x,selsetminbankRect.y+2*(selsetminbank-1)*scorey,selsetminbankRect.x+selsetminbankRect.w,selsetminbankRect.y+2*selsetminbank*scorey,0x4F4F4FFF);
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "min Bank", textColor);
					textPosition.x = 19*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					if(selsetmaxbank>0)
					{
						boxColor(screen, selsetmaxbankRect.x,selsetmaxbankRect.y+2*(selsetmaxbank-1)*scorey,selsetmaxbankRect.x+selsetmaxbankRect.w,selsetmaxbankRect.y+2*selsetmaxbank*scorey,0x4F4F4FFF);
					}
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, "max Bank", textColor);
					textPosition.x = 25*scorex;
					textPosition.y = 3*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);
				}

				for(int i=0+(sequencegang5*6);i<6+(sequencegang5*6);i++)
				{
					SDL_FreeSurface(text);
					text = TTF_RenderText_Blended(font, aset[i].name.c_str(), textColor);
					textPosition.x = 0.2*scorex;
					textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					if(settingsmode==0)
					{
						SDL_FreeSurface(text);
						if(aset[i].mididevice<onPorts)
						{
							sprintf(tmp, "%s",midioutname[aset[i].mididevice].c_str());
						}
						else
						{
							sprintf(tmp, "%s","Mididevice not available");
						}
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 6*scorex;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);

						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aset[i].midichannel+1);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 33*scorex-text->w;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);
					}
					else if (settingsmode==1 and (i<5 or i>6 or i<11))
					{
						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aset[i].minprog);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x =9*scorex-text->w;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);

						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aset[i].maxprog);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 15*scorex-text->w;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);

						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aset[i].minbank);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 21*scorex-text->w;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);

						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aset[i].maxbank);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 27*scorex-text->w;
						textPosition.y = 5*scorey+2*(i-(sequencegang5*6))*scorey;
						SDL_BlitSurface(text, 0, screen, &textPosition);

					}

				}

				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);

				if(settingsmode==0)
				{
					settings_next.show(screen, fontsmall);
				}
				else
				{
					settings_prev.show(screen, fontsmall);
				}

				settings_up.show(screen, fontsmall);
				settings_down.show(screen, fontsmall);

				// Umschalter BSP and sequencegang5

				if(sequencegang5==true)
				{
					setsequencegang5up.show(screen, fontsmall);
				}
				else
				{
					setsequencegang5down.show(screen, fontsmall);
				}
			}

			if(mode==4)  // Open Song
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Open Song", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 2*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				for(int i=0;i<16;i++)
				{
					load_song[i].show(screen, fontsmall);
				}

				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);
			}

			if(mode==6)  // save Song
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Save Song", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 2*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				for(int i=0;i<16;i++)
				{
					load_song[i].show(screen, fontsmall);
				}

				boxColor(screen, 0,17.5*scorey,screen->w/255*fortschritt,18*scorey,0x008F00FF);

				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);
			}

			if(mode==5) // Manuell
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Manuell", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 2*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				boxColor(screen, 0,3.5*scorey,screen->w,4.5*scorey,0x00008FFF);

				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "Device", textColor);
				textPosition.x = 6;
				textPosition.y = 4*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "Program", textColor);
				textPosition.x = 9*scorex-text->w/2;
				textPosition.y = 4*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "Bank", textColor);
				textPosition.x = 17*scorex-text->w/2;
				textPosition.y = 4*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(font, "Submit", textColor);
				textPosition.x = 34*scorex-text->w/2;
				textPosition.y = 4*scorey-text->h/2;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				for(int i=0;i<5;i++)
				{
					SDL_FreeSurface(text);
					sprintf(tmp, "%s",aset[i].name.c_str());
					text = TTF_RenderText_Blended(font, tmp, textColor);
					textPosition.x = 6;
					textPosition.y = 6*scorey+(2*i)*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					manuell_progdown[i].show(screen, fontsmall);

					SDL_FreeSurface(text);
					sprintf(tmp, "%d",aktprog[i]);
					text = TTF_RenderText_Blended(font, tmp, textColor);
					textPosition.x = 9*scorex-text->w/2;
					textPosition.y = 6*scorey+(i*2)*scorey-text->h/2;
					SDL_BlitSurface(text, 0, screen, &textPosition);

					manuell_progup[i].show(screen, fontsmall);

        			if(aset[i].minbank!=aset[i].maxbank)
        			{
						manuell_bankdown[i].show(screen, fontsmall);

						SDL_FreeSurface(text);
						sprintf(tmp, "%d",aktbank[i]+1);
						text = TTF_RenderText_Blended(font, tmp, textColor);
						textPosition.x = 17*scorex-text->w/2;
						textPosition.y = 6*scorey+(i*2)*scorey-text->h/2;
						SDL_BlitSurface(text, 0, screen, &textPosition);

						manuell_bankup[i].show(screen, fontsmall);
        			}
					manuell_progok[i].show(screen, fontsmall);
				}
				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);
			}

			if(mode==7)  // Change Songname
			{
				SDL_FreeSurface(text);
				text = TTF_RenderText_Blended(fontbold, "Change Songname", textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 2*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				SDL_FreeSurface(text);
				sprintf(tmp, "%s",songnametmp.c_str());
				text = TTF_RenderText_Blended(font, tmp, textColor);
				textPosition.x = screen->w/2-text->w/2;
				textPosition.y = 5*scorey;
				SDL_BlitSurface(text, 0, screen, &textPosition);

				if(blinkmode==true)
				{
					SDL_Surface* texttmp;
					texttmp = TTF_RenderText_Blended(font, "_", textColor);
					textPosition.x = textPosition.x+text->w;
					textPosition.y = 5*scorey;
					SDL_BlitSurface(texttmp, 0, screen, &textPosition);
				}

				for(int i=0;i<40;i++)
        		{
        			if(key_shift.aktiv==false)
        			{
        				keyboard[i].show(screen, font);
        			}
        			else
        			{
        				shkeyboard[i].show(screen, font);
        			}

        		}
        		key_shift.show(screen, font);
        		key_space.show(screen, font);
        		key_backspace.show(screen, font);

				ok.show(screen, fontsmall);
				cancel.show(screen, fontsmall);
			}

			SDL_Flip(screen);
			anzeige=false;
		}

		// Wir holen uns so lange neue Ereignisse, bis es keine mehr gibt.
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			// Was ist passiert?
			switch(event.type)
			{
				case SDL_QUIT:
					run = false;
					break;
				case SDL_KEYDOWN:
					keyPressed[event.key.keysym.sym] = true;
					if(keyPressed[SDLK_ESCAPE])
					{
						run = false;        // Programm beenden.
					}
					anzeige=true;
					break;
				case SDL_MOUSEBUTTONDOWN:
			        if( event.button.button == SDL_BUTTON_LEFT )
			        {
			        	if(mode==0)
			        	{
							if(CheckMouse(mousex, mousey, prog.button_rect)==true and edit.aktiv==true)
							{
								prog.aktiv=true;
								bank.aktiv=false;
								clear.aktiv=false;
							}
							else if(CheckMouse(mousex, mousey, bank.button_rect)==true and edit.aktiv==true)
							{
								prog.aktiv=false;
								bank.aktiv=true;
								clear.aktiv=false;
							}
							else if(CheckMouse(mousex, mousey, clear.button_rect)==true and edit.aktiv==true)
							{
								prog.aktiv=false;
								bank.aktiv=false;
								clear.aktiv=true;
							}
							else if(CheckMouse(mousex, mousey, start.button_rect)==true)
							{
								if(playmode==0)
								{
									wsmidi.Play();
								}
								else if(playmode==2)
								{
									playmode=1;
									wsmidi.Clock_Cont(aset[3].mididevice);
									wsmidi.Clock_Cont(aset[4].mididevice);
									wsmidi.Clock_Cont(aset[5].mididevice);
									timerrun=true;
								}
								else
								{
									playmode=2;
									wsmidi.Clock_Stop(aset[3].mididevice);
									wsmidi.Clock_Stop(aset[4].mididevice);
									wsmidi.Clock_Stop(aset[5].mididevice);
									timerrun=false;
								}
							}
							else if(CheckMouse(mousex, mousey, stop.button_rect)==true)
							{
					        	stop.aktiv = true;
					        	wsmidi.Stop();
							}
							else if(CheckMouse(mousex, mousey, next.button_rect)==true)
							{
								next.aktiv=true;
								if(startpos<15)
								{
									aktpos=aktpos+16;
								}
							}
							else if(CheckMouse(mousex, mousey, prev.button_rect)==true)
							{
								prev.aktiv=true;
								if(startpos>0 and aktpos>16)
								{
									aktpos=aktpos-16;
								}
								else
								{
									aktpos=0;
									startpos=0;
								}
							}
							else if(CheckMouse(mousex, mousey, ff.button_rect)==true)
							{
								ff.aktiv=true;
								if(aktpos<256)
								{
									aktpos++;
								}
							}
							else if(CheckMouse(mousex, mousey, fb.button_rect)==true)
							{
								fb.aktiv=true;
								if(aktpos>0)
								{
									aktpos--;
								}
							}
							else if(CheckMouse(mousex, mousey, bspff.button_rect)==true)
							{
								bspff.aktiv=true;
								if(bspproject<15)
								{
									bspproject++;
								}
								wsmidi.ArturiaBankChange(aset[5].mididevice,  aset[5].midichannel, bspproject);
							}
							else if(CheckMouse(mousex, mousey, bspfb.button_rect)==true)
							{
								bspfb.aktiv=true;
								if(bspproject>0)
								{
									bspproject--;
								}
								wsmidi.ArturiaBankChange(aset[5].mididevice,  aset[5].midichannel, bspproject);
							}
							else if(CheckMouse(mousex, mousey, bpmff.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmff.aktiv=true;
									if(bpm<255)
									{
										bpm++;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpmfb.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmfb.aktiv=true;
									if(bpm>0)
									{
										bpm--;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpm10ff.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpm10ff.aktiv=true;
									if(bpm<245)
									{
										bpm=bpm+10;;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpm10fb.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpm10fb.aktiv=true;
									if(bpm>9)
									{
										bpm=bpm-10;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpmcorff.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmcorff.aktiv=true;
									if(bpmcorrect<2.0)
									{
										bpmcorrect=bpmcorrect+0.01;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpmcorfb.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmcorfb.aktiv=true;
									if(bpm>-2.0)
									{
										bpmcorrect=bpmcorrect-0.01;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpmcor10ff.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmcor10ff.aktiv=true;
									if(bpmcorrect<2.0)
									{
										bpmcorrect=bpmcorrect+0.10;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, bpmcor10fb.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									bpmcor10fb.aktiv=true;
									if(bpmcorrect>-2.0)
									{
										bpmcorrect=bpmcorrect-0.10;
									}
								}
							}
							else if(CheckMouse(mousex, mousey, sequencegang5up.button_rect)==true)
							{
								if(sequencegang5==false)
								{
									sequencegang5=true;
								}
								else
								{
									sequencegang5=false;
								}
							}
							else if(CheckMouse(mousex, mousey, extmidi.button_rect)==true)
							{
								if(clockmodeext==false)
								{
									clockmodeext=true;
									extmidi.aktiv=true;
								}
								else
								{
									clockmodeext=false;
									extmidi.aktiv=false;
								}
							}
							else if(CheckMouse(mousex, mousey, info.button_rect)==true)
							{
								mode=1;
							}
							else if(CheckMouse(mousex, mousey, exit.button_rect)==true)
							{
								mode=2;
							}
							else if(CheckMouse(mousex, mousey, settings.button_rect)==true)
							{
								mode=3;
							}
							else if(CheckMouse(mousex, mousey, open.button_rect)==true)
							{
								// Load SongDB
								songset.clear();
								sqlite3 *songsdb;
								sprintf(dbpath, "%s/.arturiagang/songs.db", getenv("HOME"));
								if(sqlite3_open(dbpath, &songsdb) != SQLITE_OK)
								{
									cout << "Fehler beim Öffnen: " << sqlite3_errmsg(songsdb) << endl;
									return 1;
								}
								cout << "Songsdatenbank erfolgreich geöffnet!" << endl;
								sprintf(sql, "SELECT * FROM settings");
								if( sqlite3_exec(songsdb,sql,patternnamecallback,0,0) != SQLITE_OK)
								{
									cout << "Fehler beim SELECT: " << sqlite3_errmsg(songsdb) << endl;
									return 1;
								}
								sqlite3_close(songsdb);

								for(int i=0;i<16;i++)
								{
									load_song[i].button_text = songset[i].name;
								}

								if(aktsong>0)
								{
									for(int i=0;i<16;i++)
									{
										load_song[i].aktiv=false;
										if(i+1==aktsong)
										{
											load_song[i].aktiv=true;
										}
									}
								}
								mode=4;
							}
							else if(CheckMouse(mousex, mousey, manuell.button_rect)==true)
							{
								mode=5;
							}
							else if(CheckMouse(mousex, mousey, save.button_rect)==true)
							{
								// Load SongDB
								songset.clear();
								sqlite3 *songsdb;
								sprintf(dbpath, "%s/.arturiagang/songs.db", getenv("HOME"));
								if(sqlite3_open(dbpath, &songsdb) != SQLITE_OK)
								{
									cout << "Fehler beim Öffnen: " << sqlite3_errmsg(songsdb) << endl;
									return 1;
								}
								cout << "Songsdatenbank erfolgreich geöffnet!" << endl;
								sprintf(sql, "SELECT * FROM settings");
								if( sqlite3_exec(songsdb,sql,patternnamecallback,0,0) != SQLITE_OK)
								{
									cout << "Fehler beim SELECT: " << sqlite3_errmsg(songsdb) << endl;
									return 1;
								}
								sqlite3_close(songsdb);

								for(int i=0;i<16;i++)
								{
									load_song[i].button_text = songset[i].name;
								}
								save_song=0;
								mode=6;
							}
							else if(CheckMouse(mousex, mousey, songnamePosition)==true)
							{
								songnametmp=songname;
								mode=7;
							}
							else if(CheckMouse(mousex, mousey, edit.button_rect)==true)
							{
								if(edit.aktiv==true)
								{
									edit.aktiv=false;
									prog.aktiv=false;
									bank.aktiv=false;
									clear.aktiv=false;
								}
								else
								{
									edit.aktiv=true;
									prog.aktiv=true;
								}
							}
							else if(CheckMouse(mousex, mousey, rahmen)==true and edit.aktiv==true)
							{
								int i = 5;
								int j = int((mousex/scorex-4)/2)+startpos*16;
								if(sequencegang5==false)
								{
									i = int((mousey/scorey-3)/2);
								}
								else
								{
									if(int((mousey/scorey-3)/2)<5)
									{
										i = int((mousey/scorey-3)/2)+6;
									}
								}
								if(prog.aktiv==true)
								{
									if(aset[i].maxbank>1)
									{
										if(pattern[i][j]==0)
										{
											pattern[i][j]=aset[i].minprog+aktbank[i]*aset[i].maxprog;
										}
										else if(pattern[i][j]==aktbank[i]*aset[i].maxprog+aset[i].maxprog)
										{
											pattern[i][j]=pattern[i][j]-aset[i].maxprog+1;
										}
										else if(pattern[i][j]<aset[i].maxprog+aset[i].maxbank*aset[i].maxprog)
										{
											pattern[i][j]++;
										}
										else
										{
											pattern[i][j]=0;;
										}
									}
									else if(i==5)
									{
										pattern[i][j]=bpm+60;
									}
									else
									{
										if(pattern[i][j]==0)
										{
											pattern[i][j]=aset[i].minprog;
										}
										else if(pattern[i][j]<aset[i].maxprog)
										{
											pattern[i][j]++;
										}
										else
										{
											pattern[i][j]=0;;
										}
									}
								}
								else if(bank.aktiv==true)
								{
									if(i==5)
									{
										pattern[i][j]=1;
									}
									if(aset[i].maxbank>1)
									{
										if(pattern[i][j]<aset[i].maxbank*aset[i].maxprog)
										{
											if(pattern[i][j]==0)
											{
												pattern[i][j]=pattern[i][j]+aset[i].maxprog+1;
											}
											else
											{
												pattern[i][j]=pattern[i][j]+aset[i].maxprog;
											}
											aktbank[i]++;
										}
										else
										{
											pattern[i][j]=pattern[i][j]-aset[i].maxbank*aset[i].maxprog;
											aktbank[i]=aset[i].minbank;
										}
									}
								}
								else if(clear.aktiv==true)
								{
									pattern[i][j]=0;
									aktbank[i]=0;
								}
							}
			        	}
			        	else if(mode==1)
			        	{
							if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
								mode=0;
							}
			        	}
			        	else if(mode==2)
			        	{
							if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
								run = false;        // Programm beenden.
							}
							if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
			        	}
			        	else if(mode==3)  // Settings
			        	{
							if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
								if(changesettings==true)
								{
									cout << "Save Settings" << endl;

									sprintf(dbpath, "%s/.arturiagang/settings.db", getenv("HOME"));
									if(sqlite3_open(dbpath, &settingsdb) != SQLITE_OK)
									{
										cout << "Fehler beim Öffnen: " << sqlite3_errmsg(settingsdb) << endl;
										return 1;
									}
									cout << "Datenbank erfolgreich geöffnet!" << endl;

									// Settings
									int i = 1;
									for(arturiasettings asettemp: aset)
									{
	//									sprintf(sql, "UPDATE settings SET name = \"%s\"  WHERE id = %d",asettemp.name,i);
	//									if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
	//									{
	//										cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
	//										return 1;
	//									}
										sprintf(sql, "UPDATE settings SET mididevice = \"%d\" WHERE id = %d",asettemp.mididevice,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET midichannel = \"%d\" WHERE id = %d",asettemp.midichannel,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET maxbank = \"%d\" WHERE id = %d",asettemp.maxbank,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET maxprog = \"%d\" WHERE id = %d",asettemp.maxprog,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET minbank = \"%d\" WHERE id = %d",asettemp.minbank,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET minprog = \"%d\" WHERE id = %d",asettemp.minprog,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										sprintf(sql, "UPDATE settings SET master = \"%d\" WHERE id = %d",asettemp.master,i);
										if( sqlite3_exec(settingsdb,sql,NULL,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim UPDATE: " << sqlite3_errmsg(settingsdb) << endl;
											return 1;
										}
										i++;

									}
									sqlite3_close(settingsdb);
									changesettings=false;
								}
								mode=0;
							}
							if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
							else if(CheckMouse(mousex, mousey, setsequencegang5up.button_rect)==true)
							{
								if(sequencegang5==false)
								{
									sequencegang5=true;
								}
								else
								{
									sequencegang5=false;
								}
							}

							if(settingsmode==0)
							{
								if(CheckMouse(mousex, mousey, settings_next.button_rect)==true)
								{
									settingsmode=1;
									selsetmidichannel=0;
									selsetmididevice=0;
								}
								else if(CheckMouse(mousex, mousey, selsetmidideviceRect)==true)
								{
									if(sequencegang5==false)
									{
										selsetmididevice=(mousey-selsetmidideviceRect.y)/scorey/2+1;
										if(selsetmididevice>0 and selsetmididevice<4)
										{
											selsetmididevice=6;
										}
									}
									else
									{
										selsetmididevice=12;
									}
									selsetmidichannel=0;
								}
								else if(CheckMouse(mousex, mousey, selsetmidichannelRect)==true)
								{
									selsetmidichannel=(mousey-selsetmidichannelRect.y)/scorey/2+1;
									selsetmididevice=0;
								}
								else if(CheckMouse(mousex, mousey, settings_up.button_rect)==true)
								{
									settings_up.aktiv=true;
									if(selsetmididevice>3)
									{
										if(aset[selsetmididevice-1].mididevice<midiout->getPortCount()-1)
										{
											aset[selsetmididevice-1].mididevice++;
											changesettings=true;
										}
									}
									if(selsetmididevice==6)
									{
										aset[0].mididevice=aset[5].mididevice;
										aset[1].mididevice=aset[5].mididevice;
										aset[2].mididevice=aset[5].mididevice;
									}
									if(selsetmidichannel>0)
									{
										if(aset[selsetmidichannel-1].midichannel<15)
										{
											aset[selsetmidichannel-1].midichannel++;
											changesettings=true;
										}
									}
								}
								else if(CheckMouse(mousex, mousey, settings_down.button_rect)==true)
								{
									settings_down.aktiv=true;
									if(selsetmididevice>3)
									{
										if(aset[selsetmididevice-1].mididevice>0)
										{
											aset[selsetmididevice-1].mididevice--;
											changesettings=true;
										}
									}
									if(selsetmididevice==6)
									{
										aset[0].mididevice=aset[5].mididevice;
										aset[1].mididevice=aset[5].mididevice;
										aset[2].mididevice=aset[5].mididevice;
									}
									if(selsetmidichannel>0)
									{
										if(aset[selsetmidichannel-1].midichannel>0)
										{
											aset[selsetmidichannel-1].midichannel--;
											changesettings=true;
										}
									}
								}
								else
								{
									selsetmididevice=0;
								}
							}
							else if(settingsmode==1)
							{
								if(CheckMouse(mousex, mousey, settings_prev.button_rect)==true)
								{
									settingsmode=0;
								}
								if(CheckMouse(mousex, mousey, settings_prev.button_rect)==true)
								{
									settingsmode=0;
									selsetmidichannel=0;
									selsetmididevice=0;
									selsetminprog=0;
									selsetmaxprog=0;
									selsetminbank=0;
									selsetmaxbank=0;
								}
								else if(CheckMouse(mousex, mousey, selsetminprogRect)==true)
								{
									selsetminprog=(mousey-selsetminprogRect.y)/scorey/2+1;
									selsetmaxprog=0;
									selsetminbank=0;
									selsetmaxbank=0;
								}
								else if(CheckMouse(mousex, mousey, selsetmaxprogRect)==true)
								{
									selsetmaxprog=(mousey-selsetmaxprogRect.y)/scorey/2+1;
									selsetminprog=0;
									selsetminbank=0;
									selsetmaxbank=0;
								}
								else if(CheckMouse(mousex, mousey, selsetminbankRect)==true)
								{
									selsetminbank=(mousey-selsetminbankRect.y)/scorey/2+1;
									selsetminprog=0;
									selsetmaxprog=0;
									selsetmaxbank=0;
								}
								else if(CheckMouse(mousex, mousey, selsetmaxbankRect)==true)
								{
									selsetmaxbank=(mousey-selsetmaxbankRect.y)/scorey/2+1;
									selsetminprog=0;
									selsetmaxprog=0;
									selsetminbank=0;
								}
								else if(CheckMouse(mousex, mousey, settings_up.button_rect)==true)
								{
									settings_up.aktiv=true;
									if(selsetminprog>0) 
									{
										if(aset[selsetminprog-1+sequencegang5*6].minprog<127)
										{
											aset[selsetminprog-1+sequencegang5*6].minprog++;
											changesettings=true;
										}
									}
									if(selsetmaxprog>0) 
									{
										if(aset[selsetmaxprog-1+sequencegang5*6].maxprog<127)
										{
											aset[selsetmaxprog-1+sequencegang5*6].maxprog++;
											changesettings=true;
										}
									}
									if(selsetminbank>0) 
									{
										if(aset[selsetminbank-1+sequencegang5*6].minbank<15)
										{
											aset[selsetminbank-1+sequencegang5*6].minbank++;
											changesettings=true;
										}
									}
									if(selsetmaxbank>0) 
									{
										if(aset[selsetmaxbank-1+sequencegang5*6].maxbank<15)
										{
											aset[selsetmaxbank-1+sequencegang5*6].maxbank++;
											changesettings=true;
										}
									}
								}
								else if(CheckMouse(mousex, mousey, settings_down.button_rect)==true)
								{
									settings_down.aktiv=true;
									if(selsetminprog>0) 
									{
										if(aset[selsetminprog-1+sequencegang5*6].minprog>0)
										{
											aset[selsetminprog-1+sequencegang5*6].minprog--;
											changesettings=true;
										}
									}
									if(selsetmaxprog>0) 
									{
										if(aset[selsetmaxprog-1+sequencegang5*6].maxprog>0)
										{
											aset[selsetmaxprog-1+sequencegang5*6].maxprog--;
											changesettings=true;
										}
									}
									if(selsetminbank>0) 
									{
										if(aset[selsetminbank-1+sequencegang5*6].minbank>0)
										{
											aset[selsetminbank-1+sequencegang5*6].minbank--;
											changesettings=true;
										}
									}
									if(selsetmaxbank>0) 
									{
										if(aset[selsetmaxbank-1+sequencegang5*6].maxbank>0)
										{
											aset[selsetmaxbank-1+sequencegang5*6].maxbank--;
											changesettings=true;
										}
									}
								}
							}
			        	}
			        	else if(mode==4)  // Load
			        	{
							if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
								for(int i=0;i<16;i++)
								{
									if(load_song[i].aktiv==true)
									{
										// Load SongDB
										ClearPattern();
										sqlite3 *songsdb;
									sprintf(dbpath, "%s/.arturiagang/songs.db", getenv("HOME"));
										if(sqlite3_open(dbpath, &songsdb) != SQLITE_OK)
										{
											cout << "Fehler beim Öffnen: " << sqlite3_errmsg(songsdb) << endl;
											return 1;
										}
										cout << "Songsdatenbank erfolgreich geöffnet!" << endl;
										sprintf(sql, "SELECT * FROM Song%d",i);
										if( sqlite3_exec(songsdb,sql,patterncallback,0,0) != SQLITE_OK)
										{
											cout << "Fehler beim SELECT: " << sqlite3_errmsg(songsdb) << endl;
											return 1;
										}
										sqlite3_close(songsdb);
//										cout << "Load Song " << i << endl;
										songname=load_song[i].button_text;
										aktsong=i+1;
										bpm=songset[i].bpm-60;
										bspproject=songset[i].bspproject-1;
										wsmidi.ArturiaBankChange(aset[5].mididevice,  aset[5].midichannel, songset[i].bspproject-1);
									}
								}
								for(int i=0;i<6;i++)
								{
								  if(pattern[i][aktpos])
								  {
									  wsmidi.ProgramChange(aset[i].mididevice, aset[i].midichannel, pattern[i][0]-1);
								  }
								}
								mode=0;
							}
							if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
							for(int i=0;i<16;i++)
							{
								load_song[i].aktiv=false;
								if(CheckMouse(mousex, mousey, load_song[i].button_rect)==true)
								{
									load_song[i].aktiv=true;
								}

							}
			        	}
			        	else if(mode==5)
			        	{
			        		for(int i=0;i<5;i++)
			        		{
			        			if(CheckMouse(mousex, mousey, manuell_progdown[i].button_rect)==true)
			        			{
			        				if(aktprog[i]>aset[i].minprog)
			        				{
			        					manuell_progdown[i].aktiv=true;
			        					aktprog[i]--;
			        				}
			        			}
			        			else if(CheckMouse(mousex, mousey, manuell_progup[i].button_rect)==true)
			        			{
			        				if(aktprog[i]<aset[i].maxprog)
			        				{
			        					manuell_progup[i].aktiv=true;
			        					aktprog[i]++;
			        				}
			        			}
								else if(CheckMouse(mousex, mousey, manuell_progok[i].button_rect)==true)
								{
									manuell_progok[i].aktiv=true;
									wsmidi.ProgramChange(aset[i].mididevice, aset[i].midichannel, aktprog[i]-1);
				        			if(aset[i].minbank!=aset[i].maxbank)
				        			{
				        				wsmidi.ArturiaBankChange(aset[i].mididevice, aset[i].midichannel, aktbank[i]);
				        			}
								}
			        			if(aset[i].minbank!=aset[i].maxbank)
			        			{
									if(CheckMouse(mousex, mousey, manuell_bankdown[i].button_rect)==true)
									{
										if(aktbank[i]>aset[i].minbank)
										{
											manuell_bankdown[i].aktiv=true;
											aktbank[i]--;
										}
									}
									else if(CheckMouse(mousex, mousey, manuell_bankup[i].button_rect)==true)
									{
										if(aktbank[i]<aset[i].maxbank)
										{
											manuell_bankup[i].aktiv=true;
											aktbank[i]++;
										}
									}

			        			}
			        		}
			        		if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
								mode=0;
							}
			        		else if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
			        	}
			        	else if(mode==6 ) //Save
			        	{
			        		if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
			        			for(int i=0;i<16;i++)
			        			{
			        				if(load_song[i].aktiv==true)
			        				{
			        					save_song=i+1;
			        				}
			        			}
			        			if(save_song>0)
			        			{
									// Load SongDB
									sqlite3 *songsdb;
									sprintf(dbpath, "%s/.arturiagang/songs.db", getenv("HOME"));
									if(sqlite3_open(dbpath, &songsdb) != SQLITE_OK)
									{
										cout << "Fehler beim Öffnen: " << sqlite3_errmsg(songsdb) << endl;
										return 1;
									}
									cout << "Songsdatenbank erfolgreich geöffnet!" << endl;

									sprintf(sql, "UPDATE settings SET name = \"%s\" WHERE id = %d",songname.c_str(),save_song);
									if( sqlite3_exec(songsdb,sql,NULL,0,0) != SQLITE_OK)
									{
										cout << "Fehler beim UPDATE: " << sqlite3_errmsg(songsdb) << endl;
										return 1;
									}
									sprintf(sql, "UPDATE settings SET bspproject = \"%d\" WHERE id = %d",bspproject+1,save_song);
									if( sqlite3_exec(songsdb,sql,NULL,0,0) != SQLITE_OK)
									{
										cout << "Fehler beim UPDATE: " << sqlite3_errmsg(songsdb) << endl;
										return 1;
									}
									sprintf(sql, "UPDATE settings SET bpm = \"%d\" WHERE id = %d",bpm+60,save_song);
									if( sqlite3_exec(songsdb,sql,NULL,0,0) != SQLITE_OK)
									{
										cout << "Fehler beim UPDATE: " << sqlite3_errmsg(songsdb) << endl;
										return 1;
									}

									sprintf(sql, "DELETE FROM Song%d",save_song-1);
									if( sqlite3_exec(songsdb,sql,NULL,0,0) != SQLITE_OK)
									{
										cout << "Fehler beim DELETE: " << sqlite3_errmsg(songsdb) << endl;
										return 1;
									}

									for(int i=0;i<6;i++)
									{
										for(int j=0;j<254;j++)
										{
											fortschritt=j;
											if(pattern[i][j]>0)
											{
												sprintf(sql, "INSERT INTO Song%d (step,device,prog) VALUES (%d,%d,%d)",save_song-1,j,i,pattern[i][j]);
												if( sqlite3_exec(songsdb,sql,NULL,0,0) != SQLITE_OK)
												{
													cout << "Fehler beim INSERT: " << sqlite3_errmsg(songsdb) << endl;
													return 1;
												}

											}
										}
									}
									sqlite3_close(songsdb);
			        			}

								mode=0;
							}
			        		else if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
							for(int i=0;i<16;i++)
							{
								load_song[i].aktiv=false;
								if(CheckMouse(mousex, mousey, load_song[i].button_rect)==true)
								{
									load_song[i].aktiv=true;
								}
							}

			        	}
			        	else if(mode==7) // Change Songname
			        	{
			        		for(int i=0;i<40;i++)
			        		{
			        			if(CheckMouse(mousex, mousey, keyboard[i].button_rect)==true)
			        			{
			        				if(key_shift.aktiv==false)
			        				{
										keyboard[i].aktiv=true;
										if(songnametmp==" ")
										{
											songnametmp=keyboard[i].button_text;
										}
										else if(songnametmp.size()<22)
										{
											songnametmp = songnametmp + keyboard[i].button_text;
										}
			        				}
			        			}
			        			if(CheckMouse(mousex, mousey, shkeyboard[i].button_rect)==true)
			        			{
			        				if(key_shift.aktiv==true)
			        				{
										shkeyboard[i].aktiv=true;
										keyboard[i].aktiv=true;
										if(songnametmp==" ")
										{
											songnametmp=shkeyboard[i].button_text;
										}
										else if(songnametmp.size()<22)
										{
											songnametmp = songnametmp + shkeyboard[i].button_text;
										}
			        				}
			        				key_shift.aktiv=false;
			        			}
			        		}
			        		if(CheckMouse(mousex, mousey, key_shift.button_rect)==true)
							{
		        				if(key_shift.aktiv==true)
		        				{
		        					key_shift.aktiv=false;
		        				}
		        				else
		        				{
		        					key_shift.aktiv=true;
		        				}
							}
			        		if(CheckMouse(mousex, mousey, key_space.button_rect)==true)
							{
			        			key_space.aktiv=true;
								songnametmp = songnametmp + " ";
							}
			        		if(CheckMouse(mousex, mousey, key_backspace.button_rect)==true)
							{
			        			key_backspace.aktiv=true;
								if(songnametmp.size()>1)
								{
									songnametmp.erase(songnametmp.end()-1);
								}
								else
								{
									songnametmp=" ";
								}
							}
			        		if(CheckMouse(mousex, mousey, ok.button_rect)==true)
							{
			        			songname=songnametmp;
								mode=0;
							}
			        		else if(CheckMouse(mousex, mousey, cancel.button_rect)==true)
							{
								mode=0;
							}
			        	}
			        }
					anzeige=true;
					break;
				case SDL_MOUSEMOTION:
					mousex = event.button.x;
					mousey = event.button.y;
					anzeige=true;
					break;
				case SDL_MOUSEBUTTONUP:
			        if( event.button.button == SDL_BUTTON_LEFT )
			        {
			        	if(mode==7)
			        	{
							for(int i=0;i<40;i++)
							{
								keyboard[i].aktiv=false;
								shkeyboard[i].aktiv=false;
							}
		        			key_space.aktiv=false;
		        			key_backspace.aktiv=false;
			        	}
			        	stop.aktiv = false;
			        	ff.aktiv = false;
			        	fb.aktiv = false;
			        	bspff.aktiv = false;
			        	bspfb.aktiv = false;
			        	bpmff.aktiv = false;
			        	bpmfb.aktiv = false;
			        	bpm10ff.aktiv = false;
			        	bpm10fb.aktiv = false;
			        	bpmcorff.aktiv = false;
			        	bpmcorfb.aktiv = false;
			        	bpmcor10ff.aktiv = false;
			        	bpmcor10fb.aktiv = false;
			        	next.aktiv = false;
			        	prev.aktiv = false;
						settings_up.aktiv=false;
						settings_down.aktiv=false;
						sequencegang5up.aktiv=false;
						sequencegang5down.aktiv=false;
						setsequencegang5up.aktiv=false;
						setsequencegang5down.aktiv=false;
			        	for(int i=0;i<5;i++)
			        	{
			        		manuell_progdown[i].aktiv = false;
			        		manuell_progup[i].aktiv = false;
			        		manuell_bankdown[i].aktiv = false;
			        		manuell_bankup[i].aktiv = false;
			        		manuell_progok[i].aktiv = false;
			        	}
			        }
					anzeige=true;
					break;
			}
		}
	}
	SDL_Quit();
}
