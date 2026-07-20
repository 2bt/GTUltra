#pragma once

#include "gcommon.hpp"
#include "gplay.hpp"

struct SNG_INFO {
    char* instrumentData;
    char* editorInfo;
    char* ltable;
    char* rtable;
    char* songorder;
    char* songOrderPatternsExpanded;
    char* songOrderTransposeExpanded;
    char* songOrderLengthExpanded;
    char* songCompressedSizeExpanded;
    char* songOrderPatternsCopyPasteExpanded;
    char* songOrderTransposeCopyPasteExpanded;
    char* pattern;
    char* songName;
    char* loadedSongFileName;
    char* wavfilename;
    char* authorName;
    char* copyrightName;
    char* patternLen;
    char* songLen;

    char* editorUndoInfo;
    int   highestUsedPattern;
    int   highestUsedInstr;
};

extern INSTR instr[MAX_INSTR];

extern int lastValidSongFileIndex;
extern int currentSongFile;

extern unsigned char detailedTableRValue[MAX_TABLELEN];
extern unsigned char detailedTableMaxRValue[MAX_TABLELEN];
extern unsigned char detailedTableMinRValue[MAX_TABLELEN];
extern int           detailedTableBaseRValue[MAX_TABLELEN];

extern unsigned int  detailedTableLValue[MAX_TABLELEN];
extern unsigned char detailedTableMaxLValue[MAX_TABLELEN];
extern unsigned char detailedTableMinLValue[MAX_TABLELEN];
extern int           detailedTableBaseLValue[MAX_TABLELEN];

extern unsigned char ltable[MAX_TABLES][MAX_TABLELEN];
extern unsigned char rtable[MAX_TABLES][MAX_TABLELEN];
extern unsigned char songorder[MAX_SONGS][MAX_CHN][MAX_SONGLEN + 2];

extern unsigned char  songOrderPatterns[MAX_SONGS][MAX_CHN][MAX_SONGLEN_EXPANDED];
extern unsigned short songOrderTranspose[MAX_SONGS][MAX_CHN][MAX_SONGLEN_EXPANDED];
extern unsigned int   songOrderLength[MAX_SONGS][MAX_CHN];
extern unsigned int   songCompressedSize[MAX_SONGS][MAX_CHN];

extern unsigned char  songOrderPatternsCopyPaste[MAX_CHN][MAX_SONGLEN_EXPANDED];
extern unsigned short songOrderTransposeCopyPaste[MAX_CHN][MAX_SONGLEN_EXPANDED];
extern int            copyPasteW;
extern int            copyPasteH;
extern int            copyExpandedSongValidFlag;

extern unsigned char pattern[MAX_PATT][MAX_PATTROWS * 4 + 4];
extern char          songname[MAX_STR];
extern char          authorname[MAX_STR];
extern char          copyrightname[MAX_STR];
extern int           pattlen[MAX_PATT];
extern int           songlen[MAX_SONGS][MAX_CHN];
extern int           highestusedpattern;
extern int           highestusedinstr;

extern SNG_INFO songInfo[16];

int  loadsong(GTOBJECT* gt, int gt2relocMode);
int  mergesong(GTOBJECT* gt);
void loadinstrument(GTOBJECT* gt);
int  savesong();
int  saveinstrument();
void clearsong(int cs, int cp, int ci, int cf, int cn, GTOBJECT* gt);
void countpatternlengths();
void countthispattern(GTOBJECT* gt);
void clearpattern(int p);
int  insertpattern(int p, GTOBJECT* gt);
void deletepattern(int p, GTOBJECT* gt);
void findusedpatterns();
void findduplicatepatterns(GTOBJECT* gt);
void optimizeeverything(int oi, int ot, GTOBJECT* gt);
void expandAllSongs();
void compressAllSongs();
void compressSong(int s);
int  generateCompressedSongChannel(int s, int c, int validateOnly);
int  validateAllSongs();
int  allocateSngMemory(int sngIndex);
int  copyCurrentToSngBuffer(GTOBJECT* gt, int sngIndex);
int  copySngBufferToCurrent(GTOBJECT* gt, int sngIndex);
void initSngMemory();
void debugCheck();
