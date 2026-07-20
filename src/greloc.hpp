#pragma once

#include "gcommon.hpp"
#include "gplay.hpp"

#include <cstdint>

enum class PackFormat : int {
    Sid = 0,
    Prg = 1,
    Bin = 2,
};

// Bitflags stored in the unsigned playerversion field.
namespace player_feature {
constexpr unsigned buffered        = 8;
constexpr unsigned sound_effects   = 16;
constexpr unsigned volume          = 32;
constexpr unsigned author_info     = 64;
constexpr unsigned zp_ghost_regs   = 128;
constexpr unsigned no_optimization = 256;
constexpr unsigned zp_play_sid     = 512;
constexpr unsigned full_buffered   = 1024;
} // namespace player_feature

enum class TableError : int {
    None     = 0,
    Overflow = 1,
    Jump     = 2,
};

// Usage maps filled by relocator; also read by song/pattern/table editors.
extern uint8_t    patt_used[MAX_PATT];
extern uint8_t    instr_used[MAX_INSTR];
extern uint8_t    table_used[MAX_TABLES][MAX_TABLELEN + 1];
extern TableError table_error;

void relocator(GTOBJECT* gt, bool gt2reloc_mode);
