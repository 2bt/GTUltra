#pragma once
//
// gactions - data-driven action / keymap layer (M3).
//
// Maps keyboard chords to semantic editor actions and dispatches them through
// the existing editing logic. Default bindings mirror the legacy hardcoded
// switches; a TOML keymap (M7) will replace the static table later.
//
// The ImGui UI layer stays SDL-free; only gt2stereo.cpp calls into this module.
//

#include <stdint.h>

namespace gtaction {

enum class Ctx : uint8_t {
    Global     = 0,
    Pattern    = 1,
    Order      = 2,
    Instrument = 3,
    Tables     = 4,
    Names      = 5,
};

enum class Action : uint16_t {
    None = 0,

    Save,
    Undo,
    Quit,
    Clear,
    Help,

    EditModeNext,
    EditModePrev,
    EditModePattern,
    EditModeOrder,
    EditModeInstrument,
    EditModeTables,

    PlaySongStart,
    PlayPatternStart,
    PlayCurrent,
    Stop,
    ToggleFollow,
    ToggleLoop,
    SongPosNext,
    SongPosPrev,

    OctaveUp,
    OctaveDown,
    PrevInstr,
    NextInstr,

    OrderRowUp,
    OrderRowDown,
    OrderColLeft,
    OrderColRight,
};

using Chord = uint32_t;

enum Mod : uint32_t {
    Shift = 1u << 16,
    Ctrl  = 1u << 17,
    Alt   = 1u << 18,
};

constexpr Chord make_chord(int scancode, uint32_t mods = 0)
{
    return static_cast<Chord>(static_cast<uint32_t>(scancode & 0xffff) | mods);
}

constexpr Chord kNoChord = 0;

Ctx context_from_editmode(int editmode);

Chord chord_from_input(int rawkey, int ascii_key, int shift, int ctrl);
Action resolve(Ctx ctx, Chord chord);

const char* action_name(Action a);
const char* action_label(Action a);

// Vertical ImGui order-list navigation. Uses the global gtObject.
bool dispatch_order_navigation();

// Global actions (save, undo, quit, edit-mode tab, …). Uses gtObject.
bool dispatch_global(Ctx ctx);

void clear_input();

} // namespace gtaction
