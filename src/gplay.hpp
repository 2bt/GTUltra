#pragma once

#include "gcommon.hpp"

#define PLAY_PLAYING 0x00
#define PLAY_BEGINNING 0x01
#define PLAY_POS 0x02
#define PLAY_PATTERN 0x03
#define PLAY_STOP 0x04
#define PLAY_STOPPED 0x80

typedef struct {
    int espos;
    int esend;
    int epnum;
} CHN_EDITOR_INFO;

typedef struct {
    CHN_EDITOR_INFO editorInfo[MAX_PLAY_CH];
} EDITOR_UNDO_INFO;

typedef struct {
    unsigned char requestKeyOff;

    unsigned char  trans;
    unsigned char  instr;
    unsigned char  note;
    unsigned char  lastnote;
    unsigned char  newnote;
    unsigned       pattptr;
    unsigned char  pattnum;
    unsigned int   songptr;
    unsigned int   nextPatternTriggered;
    unsigned int   songLoopPtr;
    unsigned char  repeat;
    unsigned short freq;
    unsigned char  gate;
    unsigned char  wave;
    unsigned short pulse;
    unsigned char  pan;

    unsigned char  ptr[2];
    unsigned char  pulsetime;
    unsigned char  wavetime;
    unsigned char  vibtime;
    unsigned char  vibdelay;
    unsigned char  command;
    unsigned char  cmddata;
    unsigned char  newcommand;
    unsigned char  newcmddata;
    unsigned char  tick;
    unsigned char  tempo;
    unsigned char  mute;
    unsigned char  advance;
    unsigned char  gatetimer;
    unsigned char  loopCount;
    unsigned short freqBackup;

    int releaseTime;

    int portCounter;

    unsigned      lastpattptr;
    unsigned char lastsongptr;
    unsigned char lastpattnum;

} CHN;

typedef struct {
    unsigned char filterctrl;
    unsigned char filtertype;
    unsigned char filtercutoff;
    unsigned char filtertime;
    unsigned char filterptr;
} FILTERINFO;

typedef struct {
    char*         sidreg[4];
    FILTERINFO    filterInfo[4];
    unsigned char funktable[2];
    unsigned char masterfader;

    int psnum;
    int startpattpos;
    int songinit;
    int lastsonginit;
    CHN chn[MAX_PLAY_CH];

    EDITOR_UNDO_INFO editorUndoInfo;

    CHN loopStartChn[MAX_PLAY_CH];
    CHN loopEndChn[MAX_PLAY_CH];
    CHN patternLoopStartChn[MAX_PLAY_CH];
    CHN patternLoopEndChn[MAX_PLAY_CH];
    CHN tempPlayPatternChn[MAX_PLAY_CH];
    int masterLoopChannel;
    int masterLoopSubSong;
    int loopEnabledFlag;
    int interPatternLoopEnabledFlag;
    int disableLoopSearch;
    int debug;

    int looptimemin;
    int looptimesec;
    int looptimeframe;

    int timemin;
    int timesec;
    int timeframe;

    int totalMin;
    int totalSec;
    int totalFrame;

    unsigned char controlEditor;
    int           noSIDWrites;

} GTOBJECT;

extern GTOBJECT gtObject;
extern GTOBJECT gtEditorObject;
extern GTOBJECT gtLoopObject;
extern GTOBJECT gtEditorLoopObject;

extern unsigned char freqtbllo[];
extern unsigned char freqtblhi[];

void initchannels(GTOBJECT* gt);
void initsong(int num, int mode, GTOBJECT* gt);
void initsongpos(int num, int playmode, int pattpos, GTOBJECT* gt);
void stopsong(GTOBJECT* gt);
void rewindsong(GTOBJECT* gt);
void playtestnote(int note, int ins, int chnnum, GTOBJECT* gt);
void releasenote(int chnnum, GTOBJECT* gt);
void mutechannel(int chnnum, GTOBJECT* gt);
int  isplaying(GTOBJECT* gt);
void playroutine(GTOBJECT* gt);

int  getActualSongNumber(int currentSong, int channel);
int  getActualChannel(int currentSong, int channel);
void initSID(GTOBJECT* gt);
