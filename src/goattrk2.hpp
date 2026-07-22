#pragma once

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "gplatform.hpp"

#include "gcommon.hpp"
#include "ginput.hpp"
#include "gimgui.hpp"
#include "gplay.hpp"
#include "gsound.hpp"
#include "gsid.hpp"
#include "gsong.hpp"

#include "gdisplay.hpp"
#include "greloc.hpp"
#include "gfile.hpp"
#include "gpattern.hpp"
#include "gorder.hpp"
#include "ginstr.hpp"
#include "gtable.hpp"
#include "ginfo.hpp"
#include "gundo.hpp"

#include <cstdint>

constexpr int REMOVE_UNDO = 0;

enum class KeyPreset : unsigned {
    Tracker = 0,
    Dmc     = 1,
    Janko   = 2,
};

constexpr int VISIBLEPATTROWS  = 29; // 31
constexpr int VISIBLEORDERLIST = 11; // 11
constexpr int VISIBLETABLEROWS = 14;
constexpr int VISIBLEFILES     = 24;

constexpr int PGUPDNREPEAT = 8;

#ifndef GOATTRK2_C

extern char        packedsongname[MAX_PATHNAME];
extern int         SIDTracker64ForIPadIsAmazing;
extern bool        autoNextPattern;
extern char        appFileName[MAX_PATHNAME];
extern int         menu;
extern bool        doExportToWAV;
extern bool        recordmode;
extern bool        followplay;
extern int         hexnybble;
extern int         stepsize;
extern int         autoadvance;
extern int         defaultpatternlength;
extern int         cursorflash;
extern int         cursorcolortable[];
extern bool        exitprogram;
extern int         eamode;
extern KeyPreset   keypreset;
extern unsigned    playerversion;
extern PackFormat  fileformat;
extern int         zeropageadr;
extern int         playeradr;
extern bool        debugEnabled;
extern unsigned    patterndispmode;
extern unsigned    sidaddress;
extern unsigned    b;
extern unsigned    mr;
extern unsigned    writer;
extern unsigned    hardsid;
extern unsigned    catweasel;
extern unsigned    interpolate;
extern unsigned    hardsidbufinteractive;
extern unsigned    hardsidbufplayback;
extern unsigned    customclockrate;
extern int         leftKeyTicks;
extern int         leftKeyTicksDelta;
extern unsigned    monomode;
extern unsigned    stereoMode;
extern float       basepitch;
extern char        configbuf[MAX_PATHNAME];
extern char        loadedsongfilename[MAX_PATHNAME];
extern char        wavfilename[MAX_PATHNAME];
extern char        songfilename[MAX_PATHNAME];
extern char        songpath[MAX_PATHNAME];
extern char        instrfilename[MAX_FILENAME];
extern char        instrpath[MAX_PATHNAME];
extern char        packedpath[MAX_PATHNAME];
extern const char* programname;
extern const char* notename[];
extern const char* notenameTableView[];
extern char        textbuffer[MAX_PATHNAME];
extern char        debugTextbuffer[MAX_PATHNAME];
extern uint8_t     hexkeytbl[16];
extern int         jdebug[16];
extern char        backupFolderName[MAX_PATHNAME];
extern char        backupSngFilename[MAX_PATHNAME];

extern int patternOrderArray[256];
extern int patternOrderList[256];
extern int patternRemapOrderIndex;

extern char sourceBackupFolderName[MAX_FILENAME];
extern char destBackupFolderName[MAX_FILENAME];

extern int debugTicks;

extern bool forceSave3ChannelSng;
extern bool normalizeWAV;

extern float        masterVolume;
extern unsigned int lmanMode;

extern int  SID_StereoPanPositions[4][4];
extern char editPan;

extern char transportPolySIDEnabled[4];
extern char transportLoopPattern;
extern char transportLoopPatternSelectArea;
extern char transportRecord;
extern char transportPlay;
extern char transportFollowPlay;

extern unsigned int enablekeyrepeat;

extern int          selectedMIDIPort;
extern bool         midiEnabled;
extern unsigned int enableAntiAlias;

extern bool useOriginalGTFunctionKeys;

extern float detuneCent;
extern int   displayingPanel;
extern int   displayStopped;

extern bool useRepeatsWhenCompressing;
extern bool songExported;
extern bool songExportSuccessFlag;
extern int  sidAddr1;
extern int  sidAddr2;
extern int  sidAddr3;
extern int  sidAddr4;

#endif

void getparam(FILE* handle, unsigned int* value);
void getfloatparam(FILE* handle, float* value);
void getstringparam(FILE* handle, char* value);
void waitkey(GTOBJECT* gt);
void editor_frame_update(GTOBJECT* gt);
void waitkeymouse(GTOBJECT* gt);
void converthex();
void docommand();
void generalcommands(GTOBJECT* gt);
int  load(GTOBJECT* gt, char* dragDropFileName);
void clear(GTOBJECT* gt);
int  prevmultiplier();
int  nextmultiplier();
void editadsr(GTOBJECT* gt);
void calculatefreqtable();
void setspecialnotenames();
void readscalatuningfile();
int  quickSave();
void playUntilEnd(int songNumber);
void playUntilEnd2(int songNumber);
void initRemapArrays();

void handleSIDChannelCountChange(GTOBJECT* gt);
void nextSongPos(GTOBJECT* gt);
void previousSongPos(GTOBJECT* gt, int songDffset);
void playFromCurrentPosition(GTOBJECT* gt, int currentPos);
void handlePressRewind(int doubleClick, GTOBJECT* gt);
void createFilename(char* filePath, char* newfileName, const char* filename);
void backupPatternDisplayInfo(GTOBJECT* gt);
void restorePatternDisplayInfo(GTOBJECT* gt);
void reInitSID();
void validateStereoMode();
void editSIDPan(GTOBJECT* gt);
void convertInsToPans(int sidChips);
void convertPansToInts(int sidChips);
void saveBackupSong();
int  createBackupFolder();
int  replacechar(char* str, char orig, char rep);
void handleLoadPath(GTOBJECT* gt, const char* path, int merge);
int  saveSongAtPath(GTOBJECT* gt, const char* path);
void stopScreenDisplay();
void restartScreenDisplay();
void ExportAsPCM(int songNumber, int doNormalize, GTOBJECT* gt);
