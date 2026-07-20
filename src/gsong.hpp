#pragma once

#include "gcommon.hpp"
#include "gplay.hpp"

#include <cstdint>

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

extern uint8_t detailedTableRValue[MAX_TABLELEN];
extern uint8_t detailedTableMaxRValue[MAX_TABLELEN];
extern uint8_t detailedTableMinRValue[MAX_TABLELEN];
extern int     detailedTableBaseRValue[MAX_TABLELEN];

extern uint32_t detailedTableLValue[MAX_TABLELEN];
extern uint8_t  detailedTableMaxLValue[MAX_TABLELEN];
extern uint8_t  detailedTableMinLValue[MAX_TABLELEN];
extern int      detailedTableBaseLValue[MAX_TABLELEN];

extern uint8_t ltable[MAX_TABLES][MAX_TABLELEN];
extern uint8_t rtable[MAX_TABLES][MAX_TABLELEN];
extern uint8_t songorder[MAX_SONGS][MAX_CHN][MAX_SONGLEN + 2];

extern uint8_t  songOrderPatterns[MAX_SONGS][MAX_CHN][MAX_SONGLEN_EXPANDED];
extern uint16_t songOrderTranspose[MAX_SONGS][MAX_CHN][MAX_SONGLEN_EXPANDED];
extern uint32_t songOrderLength[MAX_SONGS][MAX_CHN];
extern uint32_t songCompressedSize[MAX_SONGS][MAX_CHN];

extern uint8_t  songOrderPatternsCopyPaste[MAX_CHN][MAX_SONGLEN_EXPANDED];
extern uint16_t songOrderTransposeCopyPaste[MAX_CHN][MAX_SONGLEN_EXPANDED];
extern int      copyPasteW;
extern int      copyPasteH;
extern int      copyExpandedSongValidFlag;

extern uint8_t pattern[MAX_PATT][MAX_PATTROWS * 4 + 4];
extern char    songname[MAX_STR];
extern char    authorname[MAX_STR];
extern char    copyrightname[MAX_STR];
extern int     pattlen[MAX_PATT];
extern int     songlen[MAX_SONGS][MAX_CHN];
extern int     highestusedpattern;
extern int     highestusedinstr;

extern SNG_INFO songInfo[16];

bool loadsong(GTOBJECT* gt, bool gt2reloc_mode);
bool mergesong(GTOBJECT* gt);
void loadinstrument(GTOBJECT* gt);
bool savesong();
bool saveinstrument();
void clearsong(bool      clear_songs,
               bool      clear_patterns,
               bool      clear_instruments,
               bool      clear_tables,
               bool      clear_names,
               GTOBJECT* gt);
void countpatternlengths();
void countthispattern(GTOBJECT* gt);
void clearpattern(int p);
bool insertpattern(int p, GTOBJECT* gt);
void deletepattern(int p, GTOBJECT* gt);
void findusedpatterns();
void findduplicatepatterns(GTOBJECT* gt);
void optimizeeverything(bool optimize_instruments, bool optimize_tables, GTOBJECT* gt);
void expandAllSongs();
void compressAllSongs();
void compressSong(int s);
int  generateCompressedSongChannel(int s, int c, bool validate_only);
int  validateAllSongs();
bool allocateSngMemory(int sngIndex);
bool copyCurrentToSngBuffer(GTOBJECT* gt, int sngIndex);
bool copySngBufferToCurrent(GTOBJECT* gt, int sngIndex);
void initSngMemory();
void debugCheck();
