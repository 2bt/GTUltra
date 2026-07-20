#pragma once

#include "gcommon.hpp"
#include "gplay.hpp"

#include <cstdint>

// Packer output format (fileformat / file dialogs).
constexpr int FORMAT_SID = 0;
constexpr int FORMAT_PRG = 1;
constexpr int FORMAT_BIN = 2;

// Player feature flags (playerversion bitfield).
constexpr unsigned PLAYER_BUFFERED       = 8;
constexpr unsigned PLAYER_SOUNDEFFECTS   = 16;
constexpr unsigned PLAYER_VOLUME         = 32;
constexpr unsigned PLAYER_AUTHORINFO     = 64;
constexpr unsigned PLAYER_ZPGHOSTREGS    = 128;
constexpr unsigned PLAYER_NOOPTIMIZATION = 256;
constexpr unsigned PLAYER_ZPPLAYSID      = 512;
constexpr unsigned PLAYER_FULLBUFFERED   = 1024;

// table_error codes shared with gtable (marktable / relocator).
constexpr int TYPE_NONE     = 0;
constexpr int TYPE_OVERFLOW = 1;
constexpr int TYPE_JUMP     = 2;

// Usage maps filled by relocator; also read by song/pattern/table editors.
extern uint8_t patt_used[MAX_PATT];
extern uint8_t instr_used[MAX_INSTR];
extern uint8_t table_used[MAX_TABLES][MAX_TABLELEN + 1];
extern int     table_error;

void relocator(GTOBJECT* gt, int gt2reloc_mode);
