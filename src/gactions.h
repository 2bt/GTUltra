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

Chord  chord_from_input(int rawkey, int ascii_key, int shift, int ctrl);
Action resolve(Ctx ctx, Chord chord);       // ctx bindings, then Global
Action resolve_ctx(Ctx ctx, Chord chord);   // ctx bindings only
Action resolve_input(Ctx ctx, int raw_scancode, int ascii_key, int shift, int ctrl);

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

// Pattern note/hex cell editing (ImGui new UI). Uses gtObject.
bool dispatch_pattern_cell_input(int midiNote, const EditorInput *input = nullptr);

// Table / instrument cursor navigation (ImGui panels). Uses gtObject.
bool dispatch_table_navigation();
bool dispatch_instrument_navigation();
bool dispatch_names_navigation();

// Global actions (save, undo, quit, edit-mode tab, …). Uses gtObject.
bool dispatch_global(Ctx ctx);

// Debug: log when legacy *commands() handles a key the action layer did not.
void log_legacy_fallback(const char* handler);

// Runtime keymap overrides (M7 TOML will call these).
bool set_binding(Action action, Ctx ctx, Chord chord);
bool clear_binding(Action action, Ctx ctx);
void reset_bindings();

void clear_input();

// After legacy *commands() handled a hex nybble, suppress trailing global dispatch.
bool consume_legacy_hex_input(int hex_at_frame_start);

} // namespace gtaction
