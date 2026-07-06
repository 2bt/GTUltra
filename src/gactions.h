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
    EditModeNames,

    PlaySongStart,
    PlayPatternStart,
    PlayCurrent,
    Stop,
    PlayFromBeginning,
    PlayPatternMode,
    ToggleFollow,
    ToggleLoop,
    SongPosNext,
    SongPosPrev,

    Relocate,
    LoadSong,
    SaveSong,

    OctaveUp,
    OctaveDown,
    PrevInstr,
    NextInstr,

    OrderRowUp,
    OrderRowDown,
    OrderColLeft,
    OrderColRight,
    OrderPageUp,
    OrderPageDown,
    OrderHome,
    OrderEnd,
    OrderInsert,
    OrderDelete,
    OrderGoPattern,
    OrderCopy,
    OrderCut,
    OrderPaste,
    OrderMarkToggle,
    OrderTransposeUp,
    OrderTransposeDown,
    OrderInsertRepeat,
    OrderSubtunePrev,
    OrderSubtuneNext,
    OrderPlayRangeStart,
    OrderPlayRangeEnd,

    PatternRowUp,
    PatternRowDown,
    PatternColLeft,
    PatternColRight,
    PatternPageUp,
    PatternPageDown,
    PatternHome,
    PatternEnd,
    PatternPrev,
    PatternNext,

    TableRowUp,
    TableRowDown,
    TableColLeft,
    TableColRight,
    TablePageUp,
    TablePageDown,
    TableHome,
    TableEnd,

    InstrRowUp,
    InstrRowDown,
    InstrColLeft,
    InstrColRight,
    InstrPageUp,
    InstrPageDown,
    InstrHome,
    InstrEnd,

    ToggleSIDTracker64,
    PrevMultiplier,
    NextMultiplier,
    ToggleAdsrOrPan,
    ToggleSidModel,
    CycleStereoMode,
    FastRelocate,
    SaveWav,
    SongRewind,
};

using Chord = uint32_t;

enum Mod : uint32_t {
    Shift = 1u << 16,
    Ctrl  = 1u << 17,
    Alt   = 1u << 18,
};

constexpr Chord make_chord(int scancode, uint32_t mods = 0) {
    return static_cast<Chord>(static_cast<uint32_t>(scancode & 0xffff) | mods);
}

constexpr Chord kNoChord = 0;

Ctx context_from_editmode(int editmode);

Chord  chord_from_input(int rawkey, int ascii_key, int shift, int ctrl);
Action resolve(Ctx ctx, Chord chord);

// Default or overridden chord for an action in a context (kNoChord if unbound).
Chord binding_for(Action action, Ctx ctx);

const char* action_name(Action a);
const char* action_label(Action a);

// Run an action programmatically (ImGui toolbar, scripts, …).
bool perform(Action act);

// Mode-specific navigation (order list, pattern cursor). Uses gtObject.
bool dispatch_mode_navigation();

// Vertical ImGui order-list navigation. Uses the global gtObject.
bool dispatch_order_navigation();

// Pattern cursor navigation (unmodified keys). Uses gtObject.
bool dispatch_pattern_navigation();

// Table / instrument cursor navigation (ImGui panels). Uses gtObject.
bool dispatch_table_navigation();
bool dispatch_instrument_navigation();

// Global actions (save, undo, quit, edit-mode tab, …). Uses gtObject.
bool dispatch_global(Ctx ctx);

// Runtime keymap overrides (M7 TOML will call these).
bool set_binding(Action action, Ctx ctx, Chord chord);
bool clear_binding(Action action, Ctx ctx);
void reset_bindings();

void clear_input();

} // namespace gtaction
