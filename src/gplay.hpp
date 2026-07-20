#pragma once

#include "gcommon.hpp"

#include <cstdint>

enum class PlayMode : int {
    Playing   = 0x00,
    Beginning = 0x01,
    Pos       = 0x02,
    Pattern   = 0x03,
    Stop      = 0x04,
    Stopped   = 0x80,
};

struct CHN_EDITOR_INFO {
    int espos;
    int esend;
    int epnum;
};

struct EDITOR_UNDO_INFO {
    CHN_EDITOR_INFO editorInfo[MAX_PLAY_CH];
};

struct CHN {
    uint8_t requestKeyOff;

    uint8_t  trans;
    uint8_t  instr;
    uint8_t  note;
    uint8_t  lastnote;
    uint8_t  newnote;
    unsigned pattptr;
    uint8_t  pattnum;
    uint32_t songptr;
    uint32_t nextPatternTriggered;
    uint32_t songLoopPtr;
    uint8_t  repeat;
    uint16_t freq;
    uint8_t  gate;
    uint8_t  wave;
    uint16_t pulse;
    uint8_t  pan;

    uint8_t  ptr[2];
    uint8_t  pulsetime;
    uint8_t  wavetime;
    uint8_t  vibtime;
    uint8_t  vibdelay;
    uint8_t  command;
    uint8_t  cmddata;
    uint8_t  newcommand;
    uint8_t  newcmddata;
    uint8_t  tick;
    uint8_t  tempo;
    uint8_t  mute;
    uint8_t  advance;
    uint8_t  gatetimer;
    uint8_t  loopCount;
    uint16_t freqBackup;

    int releaseTime;

    int portCounter;

    unsigned lastpattptr;
    uint8_t  lastsongptr;
    uint8_t  lastpattnum;
};

struct FILTERINFO {
    uint8_t filterctrl;
    uint8_t filtertype;
    uint8_t filtercutoff;
    uint8_t filtertime;
    uint8_t filterptr;
};

struct GTOBJECT {
    uint8_t*   sidreg[4];
    FILTERINFO filterInfo[4];
    uint8_t    funktable[2];
    uint8_t    masterfader;

    int      psnum;
    int      startpattpos;
    PlayMode songinit;
    PlayMode lastsonginit;
    CHN      chn[MAX_PLAY_CH];

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

    uint8_t controlEditor;
    int     noSIDWrites;
};

extern GTOBJECT gtObject;
extern GTOBJECT gtEditorObject;
extern GTOBJECT gtLoopObject;
extern GTOBJECT gtEditorLoopObject;

extern uint8_t freqtbllo[];
extern uint8_t freqtblhi[];

void initchannels(GTOBJECT* gt);
void initsong(int num, PlayMode mode, GTOBJECT* gt);
void initsongpos(int num, PlayMode playmode, int pattpos, GTOBJECT* gt);
void stopsong(GTOBJECT* gt);
void rewindsong(GTOBJECT* gt);
void playtestnote(int note, int ins, int chnnum, GTOBJECT* gt);
void releasenote(int chnnum, GTOBJECT* gt);
void mutechannel(int chnnum, GTOBJECT* gt);
bool isplaying(GTOBJECT* gt);
void playroutine(GTOBJECT* gt);

int  getActualSongNumber(int currentSong, int channel);
int  getActualChannel(int currentSong, int channel);
void initSID(GTOBJECT* gt);
