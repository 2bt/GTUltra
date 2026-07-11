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

struct EditorInput;

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
    OrderSelectPatterns,
    OrderCopy,
    OrderCut,
    OrderPaste,
    OrderInsertPaste, // expanded: Ctrl+I insert-paste at cursor
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
    PatternInsert,
    PatternDelete,
    PatternCopy,
    PatternCut,
    PatternPaste,
    PatternMarkToggle,
    PatternShrink,
    PatternExpand,
    PatternJoin,
    PatternSplit,
    PatternToggleJam,
    PatternPlayFromCursor,
    PatternChnNext,
    PatternChnPrev,
    PatternToggleAutoAdvance,
    PatternCmdCopy,
    PatternCmdPaste,
    PatternInvert,
    PatternTransposeUp,
    PatternTransposeDown,
    PatternOctaveUp,
    PatternOctaveDown,
    PatternStepSizeUp,
    PatternStepSizeDown,
    PatternMarkAll,
    PatternAutoPitchbend,
    PatternPortamentoHelper,

    TableRowUp,
    TableRowDown,
    TableColLeft,
    TableColRight,
    TablePageUp,
    TablePageDown,
    TableHome,
    TableEnd,
    TableInsert,
    TableDelete,
    TableCopy,
    TableCut,
    TablePaste,
    TableOptimize,
    TableToggleLock,
    TableTestNote,
    TableReleaseNote,
    TableNegate,
    TableConvertNote,

    InstrRowUp,
    InstrRowDown,
    InstrColLeft,
    InstrColRight,
    InstrPageUp,
    InstrPageDown,
    InstrHome,
    InstrEnd,

    NamesFieldNext,
    NamesFieldPrev,
    NamesFieldEdit,

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
    Shift    = 1u << 16,
    Ctrl     = 1u << 17,
    Alt      = 1u << 18,
    Scancode = 1u << 19, // distinguishes SDL scancodes from ASCII key codes
};

// ASCII / legacy key character (same namespace as old make_chord).
constexpr Chord make_chord(int ascii_key, uint32_t mods = 0) {
    return static_cast<Chord>(static_cast<uint32_t>(ascii_key & 0xffff) | mods);
}

// SDL scancode (KEY_F5, KEY_INS, …) — never collides with ASCII '>' (62) etc.
constexpr Chord make_scancode_chord(int scancode, uint32_t mods = 0) {
    return make_chord(scancode, mods | Scancode);
}

constexpr Chord kNoChord = 0;

Ctx context_from_editmode(int editmode);

const char* action_name(Action a);
const char* action_label(Action a);

// Run an action programmatically (ImGui toolbar, scripts, …).
bool perform(Action act);

// Per-frame input dispatch (gt2stereo.cpp).
bool dispatch_mode_navigation();
bool dispatch_global(Ctx ctx);
bool dispatch_pattern_cell_input(int midiNote, const EditorInput *input = nullptr);
bool dispatch_instrument_cell_input(const EditorInput *input = nullptr);
bool dispatch_table_cell_input(const EditorInput *input = nullptr);
bool consume_legacy_hex_input(int hex_at_frame_start);

// Runtime keymap overrides (M7 TOML / rebind UI).
bool set_binding(Action action, Ctx ctx, Chord chord);
bool clear_binding(Action action, Ctx ctx);
void reset_bindings();

} // namespace gtaction
