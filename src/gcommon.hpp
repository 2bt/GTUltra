#pragma once

#include <cstdint>

constexpr uint8_t CMD_DONOTHING       = 0;
constexpr uint8_t CMD_PORTAUP         = 1;
constexpr uint8_t CMD_PORTADOWN       = 2;
constexpr uint8_t CMD_TONEPORTA       = 3;
constexpr uint8_t CMD_VIBRATO         = 4;
constexpr uint8_t CMD_SETAD           = 5;
constexpr uint8_t CMD_SETSR           = 6;
constexpr uint8_t CMD_SETWAVE         = 7;
constexpr uint8_t CMD_SETWAVEPTR      = 8;
constexpr uint8_t CMD_SETPULSEPTR     = 9;
constexpr uint8_t CMD_SETFILTERPTR    = 10;
constexpr uint8_t CMD_SETFILTERCTRL   = 11;
constexpr uint8_t CMD_SETFILTERCUTOFF = 12;
constexpr uint8_t CMD_SETMASTERVOL    = 13;
constexpr uint8_t CMD_FUNKTEMPO       = 14;
constexpr uint8_t CMD_SETTEMPO        = 15;

constexpr uint8_t WTBL = 0;
constexpr uint8_t PTBL = 1;
constexpr uint8_t FTBL = 2;
constexpr uint8_t STBL = 3;

constexpr int MAX_FILT             = 64;
constexpr int MAX_STR              = 32;
constexpr int MAX_INSTR            = 64;
constexpr int MAX_CHN              = 6;
constexpr int MAX_PLAY_CH          = 12;
constexpr int MAX_PATT             = 208;
constexpr int MAX_TABLES           = 4;
constexpr int MAX_TABLELEN         = 255;
constexpr int MAX_INSTRNAMELEN     = 16;
constexpr int MAX_PATTROWS         = 128;
constexpr int MAX_SONGLEN          = 254;
constexpr int MAX_SONGLEN_EXPANDED = 0x800;
constexpr int MAX_SONGS            = 32;
constexpr int MAX_NOTES            = 96;
constexpr int MAX_SONG_FILES       = 16;

constexpr uint8_t REPEAT    = 0xd0;
constexpr uint8_t TRANSDOWN = 0xe0;
constexpr uint8_t TRANSUP   = 0xf0;
constexpr uint8_t LOOPSONG  = 0xff;

constexpr uint8_t ENDPATT   = 0xff;
constexpr uint8_t INSTRCHG  = 0x00;
constexpr uint8_t FX        = 0x40;
constexpr uint8_t FXONLY    = 0x50;
constexpr uint8_t FIRSTNOTE = 0x60;
constexpr uint8_t LASTNOTE  = 0xbc;
constexpr uint8_t REST      = 0xbd;
constexpr uint8_t KEYOFF    = 0xbe;
constexpr uint8_t KEYON     = 0xbf;
constexpr uint8_t OLDKEYOFF = 0x5e;
constexpr uint8_t OLDREST   = 0x5f;

constexpr uint8_t WAVEDELAY      = 0x1;
constexpr uint8_t WAVELASTDELAY  = 0xf;
constexpr uint8_t WAVESILENT     = 0xe0;
constexpr uint8_t WAVELASTSILENT = 0xef;
constexpr uint8_t WAVECMD        = 0xf0;
constexpr uint8_t WAVELASTCMD    = 0xfe;

struct INSTR {
    uint8_t ad;
    uint8_t sr;
    uint8_t ptr[MAX_TABLES];
    uint8_t vibdelay;
    uint8_t gatetimer;
    uint8_t firstwave;
    uint8_t pan;
    char    name[MAX_INSTRNAMELEN];
};
