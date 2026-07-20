#ifndef GRELOC_H
#define GRELOC_H

#include "embed.hpp"

#include <cstdint>

#define FORMAT_SID 0
#define FORMAT_PRG 1
#define FORMAT_BIN 2

#define PLAYER_BUFFERED 8
#define PLAYER_SOUNDEFFECTS 16
#define PLAYER_VOLUME 32
#define PLAYER_AUTHORINFO 64
#define PLAYER_ZPGHOSTREGS 128
#define PLAYER_NOOPTIMIZATION 256
#define PLAYER_ZPPLAYSID 512
#define PLAYER_FULLBUFFERED 1024

#define TYPE_NONE 0
#define TYPE_OVERFLOW 1
#define TYPE_JUMP 2

#define CAUSE_NONE 0
#define CAUSE_PATTERN 1
#define CAUSE_INSTRUMENT 2
#define CAUSE_WAVECMD 3

#define MAX_BYTES_PER_ROW 16

#ifndef GRELOC_C
extern uint8_t pattused[MAX_PATT];
extern uint8_t instrused[MAX_INSTR];
extern uint8_t tableused[MAX_TABLES][MAX_TABLELEN + 1];
extern uint8_t pattmap[MAX_PATT];
extern uint8_t instrmap[MAX_INSTR];
extern uint8_t tablemap[MAX_TABLES][MAX_TABLELEN + 1];

extern int tableerror;
#endif

void    relocator(GTOBJECT* gt, int gt2relocMode);
int     testoverlap(int area1start, int area1size, int area2start, int area2size);
int     packpattern(uint8_t* dest, uint8_t* src, int rows);
uint8_t swapnybbles(uint8_t n);
void    findtableduplicates(int num);
int     isusedandselfcontained(int num, int start);
void    calcspeedtest(uint8_t pos);

void insert_resource(embed::Id id);
void inserttext(const char* text);
void insertdefine(const char* name, int value);
void insertdefinestring(const char* name, const char* name2);
void insertlabel(const char* name);
void insertbyte(uint8_t byte);
void insertbytes(const uint8_t* bytes, int size);
void insertaddrlo(const char* name);
void insertaddrhi(const char* name);

int findDataPattern(char* tempFirstSIDBuffer, int offset, int maxSize, char* patchInitData);

#endif
