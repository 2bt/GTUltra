//
// gactions - action / keymap layer implementation (M3).
//

#include "gactions.hpp"

#include "goattrk2.hpp"
#include "gorder.hpp"
#include "ginfo.hpp"
#include "gimgui.hpp"
#include "ginput.hpp"
#include "gfiledialog.hpp"
#include "guimodel.hpp"
#include "gpattern.hpp"
#include "gsong.hpp"
#include "gtable.hpp"
#include "gdisplay.hpp"
#include "gsound.hpp"
#include "ginstr.hpp"
#include "log.hpp"

#include <vector>

namespace gtaction {

namespace {

void clear_input() {
    editor_input_clear();
}

static bool instrument_file_context()
{
    return editorInfo.einum &&
           (editorInfo.editmode == EDIT_INSTRUMENT || editorInfo.editmode == EDIT_TABLES);
}

struct ActionMeta {
    Action      action;
    const char* name;
    const char* label;
};

struct Binding {
    Action action;
    Ctx    ctx;
    Chord  chord;
};

const ActionMeta kActionMeta[] = {
    { Action::Save,                     "Save",                     "Save song" },
    { Action::Undo,                     "Undo",                     "Undo" },
    { Action::Quit,                     "Quit",                     "Quit" },
    { Action::Clear,                    "Clear",                    "Clear song" },
    { Action::Help,                     "Help",                     "Help" },
    { Action::EditModeNext,             "EditModeNext",             "Next edit mode" },
    { Action::EditModePrev,             "EditModePrev",             "Previous edit mode" },
    { Action::EditModePattern,          "EditModePattern",          "Pattern editor" },
    { Action::EditModeOrder,            "EditModeOrder",            "Order list" },
    { Action::EditModeInstrument,       "EditModeInstrument",       "Instrument editor" },
    { Action::EditModeTables,           "EditModeTables",           "Table editor" },
    { Action::EditModeNames,            "EditModeNames",            "Song metadata" },
    { Action::PlaySongStart,            "PlaySongStart",            "Play from song start" },
    { Action::PlayPatternStart,         "PlayPatternStart",         "Play from pattern start" },
    { Action::PlayCurrent,              "PlayCurrent",              "Play from cursor" },
    { Action::Stop,                     "Stop",                     "Stop playback" },
    { Action::PlayFromBeginning,        "PlayFromBeginning",        "Play from beginning" },
    { Action::PlayPatternMode,          "PlayPatternMode",          "Play pattern" },
    { Action::ToggleFollow,             "ToggleFollow",             "Toggle follow mode" },
    { Action::ToggleLoop,               "ToggleLoop",               "Toggle pattern loop" },
    { Action::SongPosNext,              "SongPosNext",              "Next song position" },
    { Action::SongPosPrev,              "SongPosPrev",              "Previous song position" },
    { Action::Relocate,                 "Relocate",                 "Open relocator" },
    { Action::LoadSong,                 "LoadSong",                 "Load song" },
    { Action::SaveSong,                 "SaveSong",                 "Save song" },
    { Action::OctaveUp,                 "OctaveUp",                 "Octave up" },
    { Action::OctaveDown,               "OctaveDown",               "Octave down" },
    { Action::PrevInstr,                "PrevInstr",                "Previous instrument" },
    { Action::NextInstr,                "NextInstr",                "Next instrument" },
    { Action::OrderRowUp,               "OrderRowUp",               "Order list: previous row" },
    { Action::OrderRowDown,             "OrderRowDown",             "Order list: next row" },
    { Action::OrderColLeft,             "OrderColLeft",             "Order list: previous nibble" },
    { Action::OrderColRight,            "OrderColRight",            "Order list: next nibble" },
    { Action::OrderPageUp,              "OrderPageUp",              "Order list: page up" },
    { Action::OrderPageDown,            "OrderPageDown",            "Order list: page down" },
    { Action::OrderHome,                "OrderHome",                "Order list: first row" },
    { Action::OrderEnd,                 "OrderEnd",                 "Order list: last row" },
    { Action::OrderInsert,              "OrderInsert",              "Order list: insert row" },
    { Action::OrderDelete,              "OrderDelete",              "Order list: delete row" },
    { Action::OrderGoPattern,           "OrderGoPattern",           "Order list: go to pattern" },
    { Action::OrderSelectPatterns,      "OrderSelectPatterns",      "Order list: sync all channels" },
    { Action::OrderCopy,                "OrderCopy",                "Order list: copy" },
    { Action::OrderCut,                 "OrderCut",                 "Order list: cut" },
    { Action::OrderPaste,               "OrderPaste",               "Order list: paste" },
    { Action::OrderInsertPaste,         "OrderInsertPaste",         "Order list: insert-paste (expanded)" },
    { Action::OrderMarkToggle,          "OrderMarkToggle",          "Order list: mark all/none" },
    { Action::OrderTransposeUp,         "OrderTransposeUp",         "Order list: transpose up" },
    { Action::OrderTransposeDown,       "OrderTransposeDown",       "Order list: transpose down" },
    { Action::OrderInsertRepeat,        "OrderInsertRepeat",        "Order list: insert repeat" },
    { Action::OrderSubtunePrev,         "OrderSubtunePrev",         "Order list: previous subtune" },
    { Action::OrderSubtuneNext,         "OrderSubtuneNext",         "Order list: next subtune" },
    { Action::OrderPlayRangeStart,      "OrderPlayRangeStart",      "Order list: play range start" },
    { Action::OrderPlayRangeEnd,        "OrderPlayRangeEnd",        "Order list: play range end" },
    { Action::PatternRowUp,             "PatternRowUp",             "Pattern: previous row" },
    { Action::PatternRowDown,           "PatternRowDown",           "Pattern: next row" },
    { Action::PatternColLeft,           "PatternColLeft",           "Pattern: previous column" },
    { Action::PatternColRight,          "PatternColRight",          "Pattern: next column" },
    { Action::PatternPageUp,            "PatternPageUp",            "Pattern: page up" },
    { Action::PatternPageDown,          "PatternPageDown",          "Pattern: page down" },
    { Action::PatternHome,              "PatternHome",              "Pattern: first row" },
    { Action::PatternEnd,               "PatternEnd",               "Pattern: last row" },
    { Action::PatternPrev,              "PatternPrev",              "Pattern: previous pattern" },
    { Action::PatternNext,              "PatternNext",              "Pattern: next pattern" },
    { Action::PatternInsert,            "PatternInsert",            "Pattern: insert row" },
    { Action::PatternDelete,            "PatternDelete",            "Pattern: delete row" },
    { Action::PatternCopy,              "PatternCopy",              "Pattern: copy" },
    { Action::PatternCut,               "PatternCut",               "Pattern: cut" },
    { Action::PatternPaste,             "PatternPaste",             "Pattern: paste" },
    { Action::PatternMarkToggle,        "PatternMarkToggle",        "Pattern: mark all/none" },
    { Action::PatternShrink,            "PatternShrink",            "Pattern: shrink" },
    { Action::PatternExpand,            "PatternExpand",            "Pattern: expand" },
    { Action::PatternJoin,              "PatternJoin",              "Pattern: join" },
    { Action::PatternSplit,             "PatternSplit",             "Pattern: split" },
    { Action::PatternToggleJam,         "PatternToggleJam",         "Pattern: toggle jam mode" },
    { Action::PatternPlayFromCursor,    "PatternPlayFromCursor",    "Pattern: play from cursor" },
    { Action::PatternChnNext,           "PatternChnNext",           "Pattern: next channel" },
    { Action::PatternChnPrev,           "PatternChnPrev",           "Pattern: previous channel" },
    { Action::PatternToggleAutoAdvance, "PatternToggleAutoAdvance", "Pattern: cycle autoadvance" },
    { Action::PatternCmdCopy,           "PatternCmdCopy",           "Pattern: copy command" },
    { Action::PatternCmdPaste,          "PatternCmdPaste",          "Pattern: paste command" },
    { Action::PatternInvert,            "PatternInvert",            "Pattern: invert rows" },
    { Action::PatternTransposeUp,       "PatternTransposeUp",       "Pattern: transpose up" },
    { Action::PatternTransposeDown,     "PatternTransposeDown",     "Pattern: transpose down" },
    { Action::PatternOctaveUp,          "PatternOctaveUp",          "Pattern: octave up" },
    { Action::PatternOctaveDown,        "PatternOctaveDown",        "Pattern: octave down" },
    { Action::PatternStepSizeUp,        "PatternStepSizeUp",        "Pattern: increase step size" },
    { Action::PatternStepSizeDown,      "PatternStepSizeDown",      "Pattern: decrease step size" },
    { Action::PatternMarkAll,           "PatternMarkAll",           "Pattern: mark all rows" },
    { Action::PatternAutoPitchbend,     "PatternAutoPitchbend",     "Pattern: auto pitchbend" },
    { Action::PatternPortamentoHelper,  "PatternPortamentoHelper",  "Pattern: portamento helper" },
    { Action::TableRowUp,               "TableRowUp",               "Table: previous row" },
    { Action::TableRowDown,             "TableRowDown",             "Table: next row" },
    { Action::TableColLeft,             "TableColLeft",             "Table: previous nibble" },
    { Action::TableColRight,            "TableColRight",            "Table: next nibble" },
    { Action::TablePageUp,              "TablePageUp",              "Table: page up" },
    { Action::TablePageDown,            "TablePageDown",            "Table: page down" },
    { Action::TableHome,                "TableHome",                "Table: first row" },
    { Action::TableEnd,                 "TableEnd",                 "Table: last row" },
    { Action::TableInsert,              "TableInsert",              "Table: insert row" },
    { Action::TableDelete,              "TableDelete",              "Table: delete row" },
    { Action::TableCopy,                "TableCopy",                "Table: copy" },
    { Action::TableCut,                 "TableCut",                 "Table: cut" },
    { Action::TablePaste,               "TablePaste",               "Table: paste" },
    { Action::TableOptimize,            "TableOptimize",            "Table: optimize" },
    { Action::TableToggleLock,          "TableToggleLock",          "Table: toggle lock" },
    { Action::TableTestNote,            "TableTestNote",            "Table: test note" },
    { Action::TableReleaseNote,         "TableReleaseNote",         "Table: release note" },
    { Action::TableNegate,              "TableNegate",              "Table: negate value" },
    { Action::TableConvertNote,         "TableConvertNote",         "Table: convert note" },
    { Action::InstrRowUp,               "InstrRowUp",               "Instrument: previous" },
    { Action::InstrRowDown,             "InstrRowDown",             "Instrument: next" },
    { Action::InstrColLeft,             "InstrColLeft",             "Instrument: previous field" },
    { Action::InstrColRight,            "InstrColRight",            "Instrument: next field" },
    { Action::InstrPageUp,              "InstrPageUp",              "Instrument: page up" },
    { Action::InstrPageDown,            "InstrPageDown",            "Instrument: page down" },
    { Action::InstrHome,                "InstrHome",                "Instrument: first" },
    { Action::InstrEnd,                 "InstrEnd",                 "Instrument: last" },
    { Action::NamesFieldNext,           "NamesFieldNext",           "Song info: next field" },
    { Action::NamesFieldPrev,           "NamesFieldPrev",           "Song info: previous field" },
    { Action::NamesFieldEdit,           "NamesFieldEdit",           "Song info: edit field" },
    { Action::ToggleSIDTracker64,       "ToggleSIDTracker64",       "Toggle SIDTracker64 mode" },
    { Action::PrevMultiplier,           "PrevMultiplier",           "Previous speed multiplier" },
    { Action::NextMultiplier,           "NextMultiplier",           "Next speed multiplier" },
    { Action::ToggleAdsrOrPan,          "ToggleAdsrOrPan",          "Toggle ADSR / pan edit" },
    { Action::ToggleSidModel,           "ToggleSidModel",           "Toggle SID model" },
    { Action::CycleStereoMode,          "CycleStereoMode",          "Cycle stereo mode" },
    { Action::FastRelocate,             "FastRelocate",             "Fast relocate export" },
    { Action::SaveWav,                  "SaveWav",                  "Save WAV" },
    { Action::SongRewind,               "SongRewind",               "Rewind song position" },
};

// Default keymap. Context-specific entries override Global for the same chord.
const Binding kBindings[] = {
    // Global file / session
    { Action::Save,               Ctx::Global, make_scancode_chord(KEY_S, Ctrl) },
    { Action::Undo,               Ctx::Global, make_scancode_chord(KEY_Z, Ctrl) },
    { Action::Quit,               Ctx::Global, make_scancode_chord(KEY_ESC) },
    { Action::Clear,              Ctx::Global, make_scancode_chord(KEY_ESC, Shift) },
    { Action::Help,               Ctx::Global, make_scancode_chord(KEY_F12) },
    { Action::ToggleSIDTracker64, Ctx::Global, make_scancode_chord(KEY_F12, Shift) },
    { Action::ToggleSIDTracker64, Ctx::Global, make_scancode_chord(KEY_F12, Ctrl) },

    // Edit mode (Tab cycle — rebindable via set_binding() / M7 keymap)
    { Action::EditModeNext,       Ctx::Global, make_scancode_chord(KEY_TAB) },
    { Action::EditModePrev,       Ctx::Global, make_scancode_chord(KEY_TAB, Shift) },
    { Action::EditModePattern,    Ctx::Global, make_scancode_chord(KEY_F5) },
    { Action::EditModeOrder,      Ctx::Global, make_scancode_chord(KEY_F6) },
    { Action::EditModeInstrument, Ctx::Global, make_scancode_chord(KEY_F7) },
    { Action::EditModeNames,      Ctx::Global, make_scancode_chord(KEY_F8) },
    { Action::PrevMultiplier,     Ctx::Global, make_scancode_chord(KEY_F5, Shift) },
    { Action::NextMultiplier,     Ctx::Global, make_scancode_chord(KEY_F6, Shift) },
    { Action::ToggleAdsrOrPan,    Ctx::Global, make_scancode_chord(KEY_F7, Shift) },
    { Action::ToggleSidModel,     Ctx::Global, make_scancode_chord(KEY_F8, Shift) },
    { Action::PrevMultiplier,     Ctx::Global, make_scancode_chord(KEY_F5, Ctrl) },
    { Action::NextMultiplier,     Ctx::Global, make_scancode_chord(KEY_F6, Ctrl) },
    { Action::ToggleAdsrOrPan,    Ctx::Global, make_scancode_chord(KEY_F7, Ctrl) },
    { Action::ToggleSidModel,     Ctx::Global, make_scancode_chord(KEY_F8, Ctrl) },

    // Transport — handler reads shift/ctrl for variant behaviour
    { Action::PlaySongStart,    Ctx::Global, make_scancode_chord(KEY_F1) },
    { Action::PlaySongStart,    Ctx::Global, make_scancode_chord(KEY_F1, Shift) },
    { Action::PlayPatternStart, Ctx::Global, make_scancode_chord(KEY_F2) },
    { Action::PlayPatternStart, Ctx::Global, make_scancode_chord(KEY_F2, Shift) },
    { Action::PlayCurrent,      Ctx::Global, make_scancode_chord(KEY_F3) },
    { Action::PlayCurrent,      Ctx::Global, make_scancode_chord(KEY_F3, Shift) },
    { Action::Stop,             Ctx::Global, make_scancode_chord(KEY_F4) },
    { Action::Stop,             Ctx::Global, make_scancode_chord(KEY_F4, Shift) },

    { Action::Relocate,        Ctx::Global, make_scancode_chord(KEY_F9) },
    { Action::CycleStereoMode, Ctx::Global, make_scancode_chord(KEY_F9, Shift) },
    { Action::FastRelocate,    Ctx::Global, make_scancode_chord(KEY_F9, Ctrl) },
    { Action::LoadSong,        Ctx::Global, make_scancode_chord(KEY_F10) },
    { Action::SaveSong,        Ctx::Global, make_scancode_chord(KEY_F11) },
    { Action::SaveWav,         Ctx::Global, make_scancode_chord(KEY_F11, Shift) },
    { Action::SaveWav,         Ctx::Global, make_scancode_chord(KEY_F11, Ctrl) },

    // Octave / instrument (legacy switch(key) shortcuts)
    { Action::OctaveUp,   Ctx::Global, make_chord('*') },
    { Action::OctaveDown, Ctx::Global, make_chord('/') },
    { Action::OctaveDown, Ctx::Global, make_chord('\'') },
    { Action::OctaveUp,   Ctx::Global, make_scancode_chord(KEY_KPMULTIPLY) },
    { Action::OctaveDown, Ctx::Global, make_scancode_chord(KEY_KPDIVIDE) },
    { Action::PrevInstr,  Ctx::Global, make_chord('?') },
    { Action::PrevInstr,  Ctx::Global, make_chord('-') },
    { Action::PrevInstr,  Ctx::Global, make_chord('<') },
    { Action::NextInstr,  Ctx::Global, make_chord('+') },
    { Action::NextInstr,  Ctx::Global, make_chord('_') },
    { Action::NextInstr,  Ctx::Global, make_chord('>') },

    // Song position (legacy ';' / ':' keys)
    { Action::SongPosPrev, Ctx::Global, make_scancode_chord(KEY_SEMICOLON) },
    { Action::SongPosPrev, Ctx::Global, make_chord(';') },
    { Action::SongPosNext, Ctx::Global, make_scancode_chord(KEY_COLON) },
    { Action::SongPosNext, Ctx::Global, make_chord(':') },

    // Song transport (Ctrl+arrow)
    { Action::SongRewind,  Ctx::Global, make_scancode_chord(KEY_LEFT, Ctrl) },
    { Action::SongPosNext, Ctx::Global, make_scancode_chord(KEY_RIGHT, Ctrl) },

    // Order list — ImGui vertical layout
    { Action::OrderRowUp,          Ctx::Order, make_scancode_chord(KEY_UP) },
    { Action::OrderRowDown,        Ctx::Order, make_scancode_chord(KEY_DOWN) },
    { Action::OrderColLeft,        Ctx::Order, make_scancode_chord(KEY_LEFT) },
    { Action::OrderColRight,       Ctx::Order, make_scancode_chord(KEY_RIGHT) },
    { Action::OrderPageUp,         Ctx::Order, make_scancode_chord(KEY_PGUP) },
    { Action::OrderPageDown,       Ctx::Order, make_scancode_chord(KEY_PGDN) },
    { Action::OrderHome,           Ctx::Order, make_scancode_chord(KEY_HOME) },
    { Action::OrderEnd,            Ctx::Order, make_scancode_chord(KEY_END) },
    { Action::OrderInsert,         Ctx::Order, make_scancode_chord(KEY_INS) },
    { Action::OrderInsert,         Ctx::Order, make_scancode_chord(KEY_DEL, Shift) },
    { Action::OrderDelete,         Ctx::Order, make_scancode_chord(KEY_DEL) },
    { Action::OrderGoPattern,      Ctx::Order, make_scancode_chord(KEY_ENTER) },
    { Action::OrderSelectPatterns, Ctx::Order, make_scancode_chord(KEY_ENTER, Shift) },
    { Action::OrderSelectPatterns, Ctx::Order, make_scancode_chord(KEY_ENTER, Ctrl) },
    { Action::OrderCopy,           Ctx::Order, make_scancode_chord(KEY_C, Shift) },
    { Action::OrderCut,            Ctx::Order, make_scancode_chord(KEY_X, Shift) },
    { Action::OrderPaste,          Ctx::Order, make_scancode_chord(KEY_V, Shift) },
    { Action::OrderInsertPaste,    Ctx::Order, make_chord('i', Ctrl) },
    { Action::OrderMarkToggle,     Ctx::Order, make_scancode_chord(KEY_L, Shift) },
    { Action::OrderTransposeUp,    Ctx::Order, make_chord('+') },
    { Action::OrderTransposeDown,  Ctx::Order, make_chord('-') },
    { Action::OrderInsertRepeat,   Ctx::Order, make_chord('R') },
    { Action::OrderInsertRepeat,   Ctx::Order, make_chord('r') },
    { Action::OrderSubtunePrev,    Ctx::Order, make_chord('<') },
    { Action::OrderSubtunePrev,    Ctx::Order, make_chord('[') },
    { Action::OrderSubtunePrev,    Ctx::Order, make_chord('(') },
    { Action::OrderSubtuneNext,    Ctx::Order, make_chord('>') },
    { Action::OrderSubtuneNext,    Ctx::Order, make_chord(']') },
    { Action::OrderSubtuneNext,    Ctx::Order, make_chord(')') },
    { Action::OrderPlayRangeStart, Ctx::Order, make_scancode_chord(KEY_SPACE) },
    { Action::OrderPlayRangeStart, Ctx::Order, make_scancode_chord(KEY_SPACE, Shift) },
    { Action::OrderPlayRangeEnd,   Ctx::Order, make_scancode_chord(KEY_BACKSPACE) },
    { Action::OrderPlayRangeEnd,   Ctx::Order, make_scancode_chord(KEY_BACKSPACE, Shift) },

    // Pattern editor — unmodified arrow keys
    { Action::PatternRowUp,             Ctx::Pattern, make_scancode_chord(KEY_UP) },
    { Action::PatternRowDown,           Ctx::Pattern, make_scancode_chord(KEY_DOWN) },
    { Action::PatternColLeft,           Ctx::Pattern, make_scancode_chord(KEY_LEFT) },
    { Action::PatternColRight,          Ctx::Pattern, make_scancode_chord(KEY_RIGHT) },
    { Action::PatternPageUp,            Ctx::Pattern, make_scancode_chord(KEY_PGUP) },
    { Action::PatternPageDown,          Ctx::Pattern, make_scancode_chord(KEY_PGDN) },
    { Action::PatternHome,              Ctx::Pattern, make_scancode_chord(KEY_HOME) },
    { Action::PatternEnd,               Ctx::Pattern, make_scancode_chord(KEY_END) },
    { Action::PatternPrev,              Ctx::Pattern, make_scancode_chord(KEY_LEFT, Shift) },
    { Action::PatternNext,              Ctx::Pattern, make_scancode_chord(KEY_RIGHT, Shift) },
    { Action::PatternPrev,              Ctx::Pattern, make_chord('<') },
    { Action::PatternPrev,              Ctx::Pattern, make_chord('[') },
    { Action::PatternPrev,              Ctx::Pattern, make_chord('(') },
    { Action::PatternNext,              Ctx::Pattern, make_chord('>') },
    { Action::PatternNext,              Ctx::Pattern, make_chord(']') },
    { Action::PatternNext,              Ctx::Pattern, make_chord(')') },
    { Action::PatternInsert,            Ctx::Pattern, make_scancode_chord(KEY_INS) },
    { Action::PatternInsert,            Ctx::Pattern, make_scancode_chord(KEY_DEL, Shift) },
    { Action::PatternDelete,            Ctx::Pattern, make_scancode_chord(KEY_DEL) },
    { Action::PatternCopy,              Ctx::Pattern, make_scancode_chord(KEY_C, Shift) },
    { Action::PatternCopy,              Ctx::Pattern, make_scancode_chord(KEY_C, Ctrl) },
    { Action::PatternCut,               Ctx::Pattern, make_scancode_chord(KEY_X, Shift) },
    { Action::PatternCut,               Ctx::Pattern, make_scancode_chord(KEY_X, Ctrl) },
    { Action::PatternPaste,             Ctx::Pattern, make_scancode_chord(KEY_V, Shift) },
    { Action::PatternMarkToggle,        Ctx::Pattern, make_scancode_chord(KEY_L, Shift) },
    { Action::PatternShrink,            Ctx::Pattern, make_scancode_chord(KEY_O, Shift) },
    { Action::PatternExpand,            Ctx::Pattern, make_scancode_chord(KEY_P, Shift) },
    { Action::PatternJoin,              Ctx::Pattern, make_scancode_chord(KEY_J, Shift) },
    { Action::PatternSplit,             Ctx::Pattern, make_scancode_chord(KEY_K, Shift) },
    { Action::PatternToggleJam,         Ctx::Pattern, make_scancode_chord(KEY_SPACE) },
    { Action::PatternPlayFromCursor,    Ctx::Pattern, make_scancode_chord(KEY_SPACE, Shift) },
    { Action::PatternChnNext,           Ctx::Pattern, make_scancode_chord(KEY_APOST2) },
    { Action::PatternChnPrev,           Ctx::Pattern, make_scancode_chord(KEY_APOST2, Shift) },
    { Action::PatternToggleAutoAdvance, Ctx::Pattern, make_scancode_chord(KEY_Z, Shift) },
    { Action::PatternCmdCopy,           Ctx::Pattern, make_scancode_chord(KEY_E, Shift) },
    { Action::PatternCmdPaste,          Ctx::Pattern, make_scancode_chord(KEY_R, Shift) },
    { Action::PatternInvert,            Ctx::Pattern, make_scancode_chord(KEY_I, Shift) },
    { Action::PatternTransposeUp,       Ctx::Pattern, make_scancode_chord(KEY_Q, Shift) },
    { Action::PatternTransposeDown,     Ctx::Pattern, make_scancode_chord(KEY_A, Shift) },
    { Action::PatternOctaveUp,          Ctx::Pattern, make_scancode_chord(KEY_W, Shift) },
    { Action::PatternOctaveDown,        Ctx::Pattern, make_scancode_chord(KEY_S, Shift) },
    { Action::PatternStepSizeUp,        Ctx::Pattern, make_scancode_chord(KEY_M, Shift) },
    { Action::PatternStepSizeDown,      Ctx::Pattern, make_scancode_chord(KEY_N, Shift) },
    { Action::PatternMarkAll,           Ctx::Pattern, make_scancode_chord(KEY_A, Ctrl) },
    { Action::PatternAutoPitchbend,     Ctx::Pattern, make_scancode_chord(KEY_Y, Shift) },
    { Action::PatternPortamentoHelper,  Ctx::Pattern, make_scancode_chord(KEY_H, Shift) },

    // SID tables — ImGui four-column layout
    { Action::TableRowUp,       Ctx::Tables, make_scancode_chord(KEY_UP) },
    { Action::TableRowDown,     Ctx::Tables, make_scancode_chord(KEY_DOWN) },
    { Action::TableColLeft,     Ctx::Tables, make_scancode_chord(KEY_LEFT) },
    { Action::TableColRight,    Ctx::Tables, make_scancode_chord(KEY_RIGHT) },
    { Action::TablePageUp,      Ctx::Tables, make_scancode_chord(KEY_PGUP) },
    { Action::TablePageDown,    Ctx::Tables, make_scancode_chord(KEY_PGDN) },
    { Action::TableHome,        Ctx::Tables, make_scancode_chord(KEY_HOME) },
    { Action::TableEnd,         Ctx::Tables, make_scancode_chord(KEY_END) },
    { Action::TableInsert,      Ctx::Tables, make_scancode_chord(KEY_INS) },
    { Action::TableDelete,      Ctx::Tables, make_scancode_chord(KEY_DEL) },
    { Action::TableCopy,        Ctx::Tables, make_scancode_chord(KEY_C, Shift) },
    { Action::TableCut,         Ctx::Tables, make_scancode_chord(KEY_X, Shift) },
    { Action::TablePaste,       Ctx::Tables, make_scancode_chord(KEY_V, Shift) },
    { Action::TableOptimize,    Ctx::Tables, make_scancode_chord(KEY_O, Shift) },
    { Action::TableToggleLock,  Ctx::Tables, make_scancode_chord(KEY_U, Shift) },
    { Action::TableTestNote,    Ctx::Tables, make_scancode_chord(KEY_SPACE) },
    { Action::TableReleaseNote, Ctx::Tables, make_scancode_chord(KEY_SPACE, Shift) },
    { Action::TableNegate,      Ctx::Tables, make_scancode_chord(KEY_N, Shift) },
    { Action::TableConvertNote, Ctx::Tables, make_scancode_chord(KEY_R, Shift) },

    // Instrument list — ImGui grid layout
    { Action::InstrRowUp,    Ctx::Instrument, make_scancode_chord(KEY_UP) },
    { Action::InstrRowDown,  Ctx::Instrument, make_scancode_chord(KEY_DOWN) },
    { Action::InstrColLeft,  Ctx::Instrument, make_scancode_chord(KEY_LEFT) },
    { Action::InstrColRight, Ctx::Instrument, make_scancode_chord(KEY_RIGHT) },
    { Action::InstrPageUp,   Ctx::Instrument, make_scancode_chord(KEY_PGUP) },
    { Action::InstrPageDown, Ctx::Instrument, make_scancode_chord(KEY_PGDN) },
    { Action::InstrHome,     Ctx::Instrument, make_scancode_chord(KEY_HOME) },
    { Action::InstrEnd,      Ctx::Instrument, make_scancode_chord(KEY_END) },

    // Song metadata (names panel)
    { Action::NamesFieldNext,  Ctx::Names, make_scancode_chord(KEY_DOWN) },
    { Action::NamesFieldPrev,  Ctx::Names, make_scancode_chord(KEY_UP) },
    { Action::NamesFieldEdit,  Ctx::Names, make_scancode_chord(KEY_ENTER) },
};

std::vector<Binding> g_overrides;

Action lookup_ctx_table(Ctx ctx, Chord chord, const Binding* begin, const Binding* end) {
    for (const Binding* p = begin; p != end; ++p) {
        if (p->ctx == ctx && p->chord == chord) return p->action;
    }
    return Action::None;
}

Action lookup_ctx(Ctx ctx, Chord chord) {
    if (chord == kNoChord) return Action::None;

    if (!g_overrides.empty()) {
        Action a = lookup_ctx_table(ctx, chord, g_overrides.data(), g_overrides.data() + g_overrides.size());
        if (a != Action::None) return a;
    }

    return lookup_ctx_table(ctx, chord, kBindings, kBindings + sizeof(kBindings) / sizeof(kBindings[0]));
}

int order_max_channels() {
    int maxCh = 6;
    if ((editorInfo.maxSIDChannels == 3) || (editorInfo.maxSIDChannels == 9 && (editorInfo.esnum & 1))) maxCh = 3;
    return maxCh;
}

void order_clamp_cursor_to_channel() {
    if (editorInfo.escolumn > 1) editorInfo.escolumn = 1;

    if ((editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) ||
        (editorInfo.eseditpos > songlen[editorInfo.esnum][editorInfo.eschn] + 1)) {
        editorInfo.eseditpos = songlen[editorInfo.esnum][editorInfo.eschn] + 1;
        editorInfo.escolumn  = 0;
    }
}

void order_sync_view() {
    if (editorInfo.eseditpos - editorInfo.esview < 0) editorInfo.esview = editorInfo.eseditpos;

    if (editorInfo.expandOrderListView == 0) {
        if (editorInfo.eseditpos - editorInfo.esview >= VISIBLEORDERLIST)
            editorInfo.esview = editorInfo.eseditpos - VISIBLEORDERLIST + 1;
    }
    else {
        if (editorInfo.eseditpos - editorInfo.esview >= EXTENDEDVISIBLEORDERLIST)
            editorInfo.esview = editorInfo.eseditpos - EXTENDEDVISIBLEORDERLIST + 1;
    }
}

static void order_begin_shift_mark() {
    if (editorInfo.esmarkchn == -1 || editorInfo.esmarkchn != editorInfo.eschn ||
        editorInfo.eseditpos != editorInfo.esmarkend) {
        editorInfo.esmarkchn    = editorInfo.eschn;
        editorInfo.esmarkchnend = editorInfo.eschn;
        editorInfo.esmarkstart  = editorInfo.esmarkend = editorInfo.eseditpos;
    }
}

void order_row_up(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        if (shiftOrCtrlPressed) order_begin_shift_mark();
        if (editorInfo.eseditpos > 0) {
            editorInfo.eseditpos--;
            if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;
        }
        order_sync_view();
        return;
    }

    if (shiftOrCtrlPressed) order_begin_shift_mark();
    if (editorInfo.eseditpos > 0) {
        editorInfo.eseditpos--;
        if (shiftOrCtrlPressed) {
            editorInfo.esmarkend = editorInfo.eseditpos;
            if (editorInfo.esmarkend == editorInfo.esmarkstart) editorInfo.esmarkchn = -1;
        }
    }
    order_sync_view();
    (void)gt;
}

void order_row_down(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        if (shiftOrCtrlPressed) order_begin_shift_mark();
        if (editorInfo.eseditpos < 0x7ff) {
            editorInfo.eseditpos++;
            if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;
        }
        order_sync_view();
        return;
    }

    if (shiftOrCtrlPressed) order_begin_shift_mark();
    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn] + 1) {
        editorInfo.eseditpos++;
        if (shiftOrCtrlPressed) {
            editorInfo.esmarkend = editorInfo.eseditpos;
            if (editorInfo.esmarkend == editorInfo.esmarkstart) editorInfo.esmarkchn = -1;
        }
    }
    order_sync_view();
    (void)gt;
}

void order_col_left(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        order_col_left_expanded(gt);
        return;
    }

    const int maxCh = order_max_channels();
    const int oldCh = editorInfo.eschn;

    if (editorInfo.escolumn > 0) {
        editorInfo.escolumn--;
    }
    else {
        editorInfo.eschn--;
        if (editorInfo.eschn < 0) editorInfo.eschn = maxCh - 1;
        editorInfo.escolumn = 1;
        setMasterLoopChannel(gt, "action_order_col_left");
    }

    order_clamp_cursor_to_channel();

    if (shiftOrCtrlPressed) {
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    } else if (editorInfo.eschn != oldCh) {
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    }
    order_sync_view();
}

void order_col_right(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        order_col_right_expanded(gt);
        return;
    }

    const int maxCh = order_max_channels();
    const int oldCh = editorInfo.eschn;

    if (editorInfo.escolumn < 1) {
        editorInfo.escolumn++;
    }
    else {
        editorInfo.escolumn = 0;
        editorInfo.eschn++;
        if (editorInfo.eschn >= maxCh) editorInfo.eschn = 0;
        setMasterLoopChannel(gt, "action_order_col_right");
    }

    order_clamp_cursor_to_channel();

    if (shiftOrCtrlPressed) {
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    } else if (editorInfo.eschn != oldCh) {
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    }
    order_sync_view();
}

int order_max_row() {
    if (editorInfo.expandOrderListView)
        return (int)songOrderLength[editorInfo.esnum][editorInfo.eschn];
    return songlen[editorInfo.esnum][editorInfo.eschn] + 1;
}

void order_page_up(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        editorInfo.eseditpos -= EXTENDEDVISIBLEORDERLIST;
        if (editorInfo.eseditpos < 0) editorInfo.eseditpos = 0;
    } else if (editorInfo.eseditpos > VISIBLEORDERLIST) {
        editorInfo.eseditpos -= VISIBLEORDERLIST;
    } else
        editorInfo.eseditpos = 0;

    if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;

    order_sync_view();
    (void)gt;
}

void order_page_down(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        editorInfo.eseditpos += EXTENDEDVISIBLEORDERLIST;
        if (editorInfo.eseditpos > 0x7ff) editorInfo.eseditpos = 0x7ff;
    } else {
        const int maxRow = order_max_row();
        editorInfo.eseditpos += VISIBLEORDERLIST;
        if (editorInfo.eseditpos > maxRow) editorInfo.eseditpos = maxRow;
    }

    if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;

    order_sync_view();
    (void)gt;
}

void order_nav_home(GTOBJECT* gt) {
    editorInfo.eseditpos = 0;
    if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;
    order_sync_view();
    (void)gt;
}

void order_nav_end(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView)
        editorInfo.eseditpos = (int)songOrderLength[editorInfo.esnum][editorInfo.eschn];
    else
        editorInfo.eseditpos = order_max_row();
    if (shiftOrCtrlPressed) editorInfo.esmarkend = editorInfo.eseditpos;
    order_sync_view();
    (void)gt;
}

static void table_use_raw_hex_mode() {
    if (gimgui_new_ui_active()) editorInfo.editTableMode = EDIT_TABLE_NONE;
}

// Preserve the on-screen row when stepping across table columns (legacy
// etview[] tracks each column's scroll offset).
static void table_step_adjacent(int dir) {
    const int old_t    = editorInfo.etnum;
    const int vis_row  = editorInfo.etpos - editorInfo.etview[old_t];

    if (dir < 0) {
        editorInfo.etcolumn = 3;
        editorInfo.etnum--;
        if (editorInfo.etnum < 0) editorInfo.etnum = MAX_TABLES - 1;
    }
    else {
        editorInfo.etcolumn = 0;
        editorInfo.etnum++;
        if (editorInfo.etnum >= MAX_TABLES) editorInfo.etnum = 0;
    }

    editorInfo.etpos = vis_row + editorInfo.etview[editorInfo.etnum];
    if (editorInfo.etpos < 0) editorInfo.etpos = 0;
    if (editorInfo.etpos >= MAX_TABLELEN) editorInfo.etpos = MAX_TABLELEN - 1;
    validatetableview();
}

static void table_finish_nav() { validatetableview(); }

void table_col_left(GTOBJECT* gt) {
    disableEnterToReturnToLastPos = 1;
    table_use_raw_hex_mode();

    const int old_t = editorInfo.etnum;
    editorInfo.etcolumn--;
    if (editorInfo.etcolumn < 0) table_step_adjacent(-1);
    else table_finish_nav();

    if (shiftpressed && editorInfo.etnum != old_t) editorInfo.etmarknum = -1;
    (void)gt;
}

void table_col_right(GTOBJECT* gt) {
    disableEnterToReturnToLastPos = 1;
    table_use_raw_hex_mode();

    const int old_t = editorInfo.etnum;
    editorInfo.etcolumn++;
    if (editorInfo.etcolumn > 3) table_step_adjacent(+1);
    else table_finish_nav();

    if (shiftpressed && editorInfo.etnum != old_t) editorInfo.etmarknum = -1;
    (void)gt;
}

void table_page_up(GTOBJECT* gt) {
    for (int i = 0; i < PGUPDNREPEAT; i++) tableup();
    (void)gt;
}

void table_page_down(GTOBJECT* gt) {
    for (int i = 0; i < PGUPDNREPEAT; i++) tabledown();
    (void)gt;
}

void table_nav_home(GTOBJECT* gt) {
    editorInfo.etpos = 0;
    (void)gt;
}

void table_nav_end(GTOBJECT* gt) {
    editorInfo.etpos = MAX_TABLELEN - 1;
    (void)gt;
}

void instr_row_up(GTOBJECT* gt) {
    if (editorInfo.einum > gtui::INSTR_FIRST) editorInfo.einum--;
    else
        editorInfo.einum = MAX_INSTR - 1;
    (void)gt;
}

void instr_row_down(GTOBJECT* gt) {
    if (editorInfo.einum < MAX_INSTR - 1) editorInfo.einum++;
    else
        editorInfo.einum = gtui::INSTR_FIRST;
    (void)gt;
}

void instr_page_up(GTOBJECT* gt) {
    int step = VISIBLETABLEROWS;
    if (editorInfo.einum > step + gtui::INSTR_FIRST - 1) editorInfo.einum -= step;
    else
        editorInfo.einum = gtui::INSTR_FIRST;
    (void)gt;
}

void instr_page_down(GTOBJECT* gt) {
    int step = VISIBLETABLEROWS;
    editorInfo.einum += step;
    if (editorInfo.einum >= MAX_INSTR) editorInfo.einum = MAX_INSTR - 1;
    (void)gt;
}

void instr_nav_home(GTOBJECT* gt) {
    editorInfo.einum = gtui::INSTR_FIRST;
    (void)gt;
}

void instr_nav_end(GTOBJECT* gt) {
    editorInfo.einum = MAX_INSTR - 1;
    (void)gt;
}

// Column order matches the grid: Name, AD, SR, …, PN.
static int instr_vis_field(int vis) { return (vis == 0) ? gtui::INSTR_FIELD_NAME : vis - 1; }

static int instr_field_vis(int field) { return (field == gtui::INSTR_FIELD_NAME) ? 0 : field + 1; }

void instr_col_right(GTOBJECT* gt) {
    const int field = (editorInfo.eipos >= LAST_INST) ? gtui::INSTR_FIELD_NAME : editorInfo.eipos;
    if (field == gtui::INSTR_FIELD_NAME) {
        editorInfo.eipos    = 0;
        editorInfo.eicolumn = 0;
        (void)gt;
        return;
    }
    if (editorInfo.eicolumn < 1) {
        editorInfo.eicolumn = 1;
        (void)gt;
        return;
    }
    editorInfo.eicolumn = 0;
    const int vis       = instr_field_vis(field) + 1;
    if (vis > gtui::INSTR_FIELDS) {
        editorInfo.eipos    = LAST_INST;
        editorInfo.eicolumn = 0;
    }
    else {
        editorInfo.eipos = instr_vis_field(vis);
    }
    (void)gt;
}

void instr_col_left(GTOBJECT* gt) {
    const int field = (editorInfo.eipos >= LAST_INST) ? gtui::INSTR_FIELD_NAME : editorInfo.eipos;
    if (field == gtui::INSTR_FIELD_NAME) {
        editorInfo.eipos    = gtui::INSTR_FIELDS - 1;
        editorInfo.eicolumn = 1;
        (void)gt;
        return;
    }
    if (editorInfo.eicolumn > 0) {
        editorInfo.eicolumn = 0;
        (void)gt;
        return;
    }
    const int vis = instr_field_vis(field) - 1;
    if (vis <= 0) {
        editorInfo.eipos    = LAST_INST;
        editorInfo.eicolumn = 0;
    }
    else {
        editorInfo.eipos    = instr_vis_field(vis);
        editorInfo.eicolumn = 1;
    }
    (void)gt;
}

// ---- transport (migrated from generalcommands KEY_F1..F4) ----

void transport_on_f1(GTOBJECT* gt) {
    if (editPaletteMode) return;

    playUntilEnd(editorInfo.esnum);

    if (useOriginalGTFunctionKeys) {
        transportLoopPattern = 0;
        followplay           = shiftOrCtrlPressed ? 1 : 0;
        orderPlayFromPosition(gt, 0, 0, 0, 1);
    }
    else {
        if (shiftpressed) orderPlayFromPosition(gt, 0, 0, 0, 1);
        else
            playFromCurrentPosition(gt, 0);
    }
}

void transport_on_f2(GTOBJECT* gt) {
    if (editPaletteMode) return;

    if (SIDTracker64ForIPadIsAmazing != 0) {
        if (shiftOrCtrlPressed) followplay = 1 - followplay;
        else
            playFromCurrentPosition(gt, 0);
    }
    else if (useOriginalGTFunctionKeys) {
        playFromCurrentPosition(gt, 0);
        transportLoopPattern = 0;
        followplay           = shiftOrCtrlPressed ? 1 : 0;
    }
    else {
        if (shiftOrCtrlPressed) {
            followplay = 1 - followplay;
        }
        else {
            transportLoopPattern = 1 - transportLoopPattern;
            if (!transportLoopPattern) {
                editorInfo.highlightLoopChannel       = 999;
                editorInfo.highlightLoopPatternNumber = -1;
                editorInfo.highlightLoopStart         = 0;
                editorInfo.highlightLoopEnd           = 0;
            }
        }
    }
}

void transport_on_f3(GTOBJECT* gt) {
    if (editPaletteMode) return;

    if (useOriginalGTFunctionKeys && SIDTracker64ForIPadIsAmazing == 0) {
        transportLoopPattern = 1;
        followplay           = shiftOrCtrlPressed ? 1 : 0;
        playFromCurrentPosition(gt, 0);
    }
    else {
        if (shiftOrCtrlPressed) {
            transportLoopPattern = 1 - transportLoopPattern;
        }
        else if (editorInfo.editmode == EDIT_ORDERLIST) {
            orderSelectPatternsFromSelected(gt);
            orderPlayFromPosition(gt, 0, editorInfo.eseditpos, editorInfo.eschn, 1);
        }
        else {
            playFromCurrentPosition(gt, editorInfo.eppos);
        }
    }
}

void transport_on_f4(GTOBJECT* gt) {
    if (shiftOrCtrlPressed) {
        mutechannel(editorInfo.epchn, gt);
        return;
    }
    if (gt->songinit != PLAY_STOPPED) {
        stopsong(gt);
        setMasterLoopChannel(gt, "debug_9");
    }
}

void edit_octave_up() {
    if (editorInfo.editmode == EDIT_NAMES) return;
    if (editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9) return;
    if (editorInfo.epoctave < 7) editorInfo.epoctave++;
}

void edit_octave_down() {
    if (editorInfo.editmode == EDIT_NAMES) return;
    if (editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9) return;
    if (editorInfo.epoctave > 0) editorInfo.epoctave--;
}

static void instr_clamp_after_global_step() {
    if (!gimgui_new_ui_active()) return;
    if (editorInfo.editmode != EDIT_INSTRUMENT) return;
    gtui::instr_clamp_selection();
}

void edit_prev_instr() {
    if ((editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos != 9) || editorInfo.editmode == EDIT_TABLES) {
        previnstr();
        instr_clamp_after_global_step();
        return;
    }
    if (editorInfo.editmode != EDIT_NAMES && editorInfo.editmode != EDIT_ORDERLIST) {
        if (!(editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos == 9)) previnstr();
        instr_clamp_after_global_step();
    }
}

void edit_next_instr() {
    if ((editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos != 9) || editorInfo.editmode == EDIT_TABLES) {
        nextinstr();
        instr_clamp_after_global_step();
        return;
    }
    if (editorInfo.editmode != EDIT_NAMES && editorInfo.editmode != EDIT_ORDERLIST) {
        if (!(editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9)) nextinstr();
        instr_clamp_after_global_step();
    }
}

bool handle_global_action(Action act) {
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::Save: {
        int validSize = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff) validSize = 0;
        }
        if (validSize) {
            if (gimgui_new_ui_active() && instrument_file_context()) {
                char path[MAX_PATHNAME];
                if (gtfile::save_instrument(path, sizeof path)) saveinstrument();
            } else {
                int s = quickSave();
                if (s) sprintf(infoTextBuffer, "quick save: %d", s);
                else if (gimgui_new_ui_active()) {
                    char path[MAX_PATHNAME];
                    if (gtfile::save_song(path, sizeof path)) saveSongAtPath(gt, path);
                } else
                    save(gt, 0);
            }
        }
        return true;
    }

    case Action::Undo:
        if (!editPaletteMode) undoPerform(gt);
        return true;

    case Action::Quit:
        if (!shiftOrCtrlPressed) quit(gt);
        return true;

    case Action::Clear:
        if (shiftOrCtrlPressed) clear(gt);
        return true;

    case Action::Help:
        stopScreenDisplay();
        onlinehelp(0, shiftOrCtrlPressed ? 1 : 0, gt);
        restartScreenDisplay();
        return true;

    case Action::EditModeNext:
        if (!shiftOrCtrlPressed) {
            editorInfo.editmode++;
            if (editorInfo.editmode > EDIT_NAMES) editorInfo.editmode = EDIT_PATTERN;
            setMasterLoopChannel(gt, "action_editmode_next");
        }
        return true;

    case Action::EditModePrev:
        if (shiftOrCtrlPressed) {
            editorInfo.editmode--;
            if (editorInfo.editmode < EDIT_PATTERN) editorInfo.editmode = EDIT_NAMES;
            setMasterLoopChannel(gt, "action_editmode_prev");
        }
        return true;

    case Action::EditModePattern:
        if (!shiftOrCtrlPressed) {
            if (editorInfo.editmode == EDIT_ORDERLIST) {
                if (!order_go_pattern(gt))
                    editorInfo.editmode = EDIT_PATTERN;
            }
            else
                editorInfo.editmode = EDIT_PATTERN;
        }
        return true;

    case Action::EditModeOrder:
        if (!shiftOrCtrlPressed) editorInfo.editmode = EDIT_ORDERLIST;
        return true;

    case Action::EditModeInstrument:
        if (!shiftOrCtrlPressed) {
            if (editorInfo.editmode == EDIT_INSTRUMENT) {
                editorInfo.editmode = EDIT_TABLES;
                table_use_raw_hex_mode();
            }
            else
                editorInfo.editmode = EDIT_INSTRUMENT;
            disableEnterToReturnToLastPos = 1;
        }
        return true;

    case Action::EditModeTables:
        if (!shiftOrCtrlPressed) {
            editorInfo.editmode           = EDIT_TABLES;
            table_use_raw_hex_mode();
            disableEnterToReturnToLastPos = 1;
        }
        return true;

    case Action::EditModeNames:
        if (!shiftOrCtrlPressed) {
            editorInfo.editmode = EDIT_NAMES;
            if (gimgui_new_ui_active()) gimgui_song_field_end();
        }
        return true;

    case Action::SongPosNext: nextSongPos(gt); return true;

    case Action::SongPosPrev: previousSongPos(gt, 1); return true;

    case Action::PlaySongStart: transport_on_f1(gt); return true;

    case Action::PlayPatternStart: transport_on_f2(gt); return true;

    case Action::PlayCurrent: transport_on_f3(gt); return true;

    case Action::Stop: transport_on_f4(gt); return true;

    case Action::PlayFromBeginning: initsong(editorInfo.esnum, PLAY_BEGINNING, gt); return true;

    case Action::PlayPatternMode: initsong(editorInfo.esnum, PLAY_PATTERN, gt); return true;

    case Action::ToggleFollow: followplay = 1 - followplay; return true;

    case Action::ToggleLoop: transportLoopPattern = 1 - transportLoopPattern; return true;

    case Action::Relocate: {
        int ok = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff) ok = 0;
            else
                compressAllSongs();
        }
        if (ok) {
            if (gimgui_new_ui_active()) {
                LOG_DEBUG("Relocate: new UI export");
                char path[MAX_PATHNAME];
                if (gtfile::save_relocated(path, sizeof path)) {
                    relocator(gt, 0, 1);
                    sprintf(infoTextBuffer, "Song Exported:%s", packedsongname);
                    LOG_INFO("exported to {}", packedsongname);
                }
            } else {
                stopScreenDisplay();
                relocator(gt, 0, 0);
                restartScreenDisplay();
                printmainscreen(gt);
                sprintf(infoTextBuffer, " ");
            }
        }
        return true;
    }

    case Action::LoadSong:
        LOG_DEBUG("LoadSong new_ui={} instrument_ctx={}", gimgui_new_ui_active(),
                  instrument_file_context());
        if (gimgui_new_ui_active()) {
            char path[MAX_PATHNAME];
            if (instrument_file_context()) {
                if (gtfile::open_instrument(path, sizeof path)) loadinstrument(gt);
            } else {
                const bool merge = shiftOrCtrlPressed != 0;
                if (gtfile::open_song(path, sizeof path, merge)) handleLoadPath(gt, path, merge);
            }
        } else
            handleLoad(gt, NULL);
        return true;

    case Action::SaveSong: {
        int ok = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff) ok = 0;
        }
        if (ok) {
            if (gimgui_new_ui_active()) {
                char path[MAX_PATHNAME];
                if (instrument_file_context()) {
                    if (gtfile::save_instrument(path, sizeof path)) saveinstrument();
                } else if (gtfile::save_song(path, sizeof path)) {
                    saveSongAtPath(gt, path);
                }
            } else
                save(gt, 0);
        }
        return true;
    }

    case Action::OctaveUp: edit_octave_up(); return true;

    case Action::OctaveDown: edit_octave_down(); return true;

    case Action::PrevInstr: edit_prev_instr(); return true;

    case Action::NextInstr: edit_next_instr(); return true;

    case Action::ToggleSIDTracker64:
        SIDTracker64ForIPadIsAmazing = 1 - SIDTracker64ForIPadIsAmazing;
        setSIDTracker64KeyOnStyle();
        if (!SIDTracker64ForIPadIsAmazing) sprintf(infoTextBuffer, "SIDTracker64 Mode: Disabled");
        else
            sprintf(infoTextBuffer, "SIDTracker64 Mode: Enabled");
        forceInfoLine = 1;
        return true;

    case Action::PrevMultiplier: prevmultiplier(); return true;

    case Action::NextMultiplier: nextmultiplier(); return true;

    case Action::ToggleAdsrOrPan:
        if (!editPan) editadsr(gt);
        else
            editSIDPan(gt);
        return true;

    case Action::ToggleSidModel:
        editorInfo.sidmodel ^= 1;
        sound_init(b,
                   mr,
                   writer,
                   hardsid,
                   editorInfo.sidmodel,
                   editorInfo.ntsc,
                   editorInfo.multiplier,
                   catweasel,
                   interpolate,
                   customclockrate);
        return true;

    case Action::CycleStereoMode:
        stereoMode++;
        stereoMode %= 3;
        validateStereoMode();
        return true;

    case Action::FastRelocate:
        if (songExported) {
            LOG_DEBUG("FastRelocate to {}", packedsongname);
            relocator(gt, 0, 1);
            sprintf(infoTextBuffer, "Song Exported:%s", packedsongname);
            LOG_INFO("re-exported to {}", packedsongname);
        }
        return true;

    case Action::SaveWav:
        LOG_DEBUG("SaveWav new_ui={}", gimgui_new_ui_active());
        if (gimgui_new_ui_active()) {
            char path[MAX_PATHNAME];
            if (gtfile::export_wav(path, sizeof path)) {
                doExportToWAV = 1;
            }
        } else
            save(gt, 1);
        return true;

    case Action::SongRewind: {
        leftKeyTicksDelta = SDL_GetTicks() - leftKeyTicks;
        leftKeyTicks      = SDL_GetTicks();
        handlePressRewind(leftKeyTicksDelta < 300 ? 1 : 0, gt);
        return true;
    }

    default: return false;
    }
}

bool handle_pattern_action(Action act) {
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::PatternRowUp: pattern_nav_up(gt); return true;
    case Action::PatternRowDown: pattern_nav_down(gt); return true;
    case Action::PatternColLeft: pattern_col_left(gt); return true;
    case Action::PatternColRight: pattern_col_right(gt); return true;
    case Action::PatternPageUp: pattern_nav_page_up(gt); return true;
    case Action::PatternPageDown: pattern_nav_page_down(gt); return true;
    case Action::PatternHome: pattern_nav_home(gt); return true;
    case Action::PatternEnd: pattern_nav_end(gt); return true;
    case Action::PatternPrev: prevpattern(gt); return true;
    case Action::PatternNext: nextpattern(gt); return true;
    case Action::PatternInsert:
        pattern_list_insert(gt);
        return true;
    case Action::PatternDelete:
        pattern_list_delete(gt);
        return true;
    case Action::PatternCopy:
        pattern_copy_or_cut(gt, 0);
        return true;
    case Action::PatternCut:
        pattern_copy_or_cut(gt, 1);
        return true;
    case Action::PatternPaste:
        pattern_paste(gt);
        return true;
    case Action::PatternMarkToggle:
        pattern_mark_toggle();
        return true;
    case Action::PatternShrink:
        if (shiftOrCtrlPressed) shrinkpattern(gt);
        return true;
    case Action::PatternExpand:
        if (shiftOrCtrlPressed) expandpattern(gt);
        return true;
    case Action::PatternJoin:
        if (shiftOrCtrlPressed) joinpattern(gt);
        return true;
    case Action::PatternSplit:
        if (shiftOrCtrlPressed) splitpattern(gt);
        return true;
    case Action::PatternToggleJam:
        pattern_toggle_jam();
        return true;
    case Action::PatternPlayFromCursor:
        pattern_play_from_cursor(gt);
        return true;
    case Action::PatternChnNext:
        pattern_chn_next(gt);
        return true;
    case Action::PatternChnPrev:
        pattern_chn_prev(gt);
        return true;
    case Action::PatternToggleAutoAdvance:
        pattern_toggle_autoadvance();
        return true;
    case Action::PatternCmdCopy:
        pattern_cmd_copy(gt);
        return true;
    case Action::PatternCmdPaste:
        pattern_cmd_paste(gt);
        return true;
    case Action::PatternInvert:
        pattern_invert(gt);
        return true;
    case Action::PatternTransposeUp:
        pattern_transpose_up(gt);
        return true;
    case Action::PatternTransposeDown:
        pattern_transpose_down(gt);
        return true;
    case Action::PatternOctaveUp:
        pattern_octave_up(gt);
        return true;
    case Action::PatternOctaveDown:
        pattern_octave_down(gt);
        return true;
    case Action::PatternStepSizeUp:
        pattern_step_size_up();
        return true;
    case Action::PatternStepSizeDown:
        pattern_step_size_down();
        return true;
    case Action::PatternMarkAll:
        pattern_mark_all(gt);
        return true;
    case Action::PatternAutoPitchbend:
        pattern_auto_pitchbend(gt);
        return true;
    case Action::PatternPortamentoHelper:
        pattern_portamento_helper(gt);
        return true;
    default: return false;
    }
}

bool pattern_action_needs_imgui(Action act) {
    switch (act) {
    case Action::PatternInsert:
    case Action::PatternDelete:
    case Action::PatternCopy:
    case Action::PatternCut:
    case Action::PatternPaste:
    case Action::PatternMarkToggle:
    case Action::PatternShrink:
    case Action::PatternExpand:
    case Action::PatternJoin:
    case Action::PatternSplit:
    case Action::PatternToggleJam:
    case Action::PatternPlayFromCursor:
    case Action::PatternChnNext:
    case Action::PatternChnPrev:
    case Action::PatternToggleAutoAdvance:
    case Action::PatternCmdCopy:
    case Action::PatternCmdPaste:
    case Action::PatternInvert:
    case Action::PatternTransposeUp:
    case Action::PatternTransposeDown:
    case Action::PatternOctaveUp:
    case Action::PatternOctaveDown:
    case Action::PatternStepSizeUp:
    case Action::PatternStepSizeDown:
    case Action::PatternMarkAll:
    case Action::PatternAutoPitchbend:
    case Action::PatternPortamentoHelper: return true;
    default: return false;
    }
}

bool handle_table_action(Action act) {
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::TableRowUp:
        tableup();
        table_finish_nav();
        return true;
    case Action::TableRowDown:
        tabledown();
        table_finish_nav();
        return true;
    case Action::TableColLeft:
        table_col_left(gt);
        return true;
    case Action::TableColRight:
        table_col_right(gt);
        return true;
    case Action::TablePageUp:
        table_page_up(gt);
        table_finish_nav();
        return true;
    case Action::TablePageDown:
        table_page_down(gt);
        table_finish_nav();
        return true;
    case Action::TableHome:
        table_nav_home(gt);
        table_finish_nav();
        return true;
    case Action::TableEnd:
        table_nav_end(gt);
        table_finish_nav();
        return true;
    case Action::TableInsert:
        table_list_insert(gt);
        return true;
    case Action::TableDelete:
        table_list_delete(gt);
        return true;
    case Action::TableCopy:
        table_copy_or_cut(0);
        return true;
    case Action::TableCut:
        table_copy_or_cut(1);
        return true;
    case Action::TablePaste:
        table_paste();
        return true;
    case Action::TableOptimize:
        table_optimize();
        return true;
    case Action::TableToggleLock:
        table_toggle_lock();
        return true;
    case Action::TableTestNote:
        table_test_note(gt);
        return true;
    case Action::TableReleaseNote:
        table_release_note(gt);
        return true;
    case Action::TableNegate:
        table_negate_value();
        return true;
    case Action::TableConvertNote:
        table_convert_note();
        return true;
    default: return false;
    }
}

bool handle_instrument_action(Action act) {
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::InstrRowUp: instr_row_up(gt); return true;
    case Action::InstrRowDown: instr_row_down(gt); return true;
    case Action::InstrColLeft: instr_col_left(gt); return true;
    case Action::InstrColRight: instr_col_right(gt); return true;
    case Action::InstrPageUp: instr_page_up(gt); return true;
    case Action::InstrPageDown: instr_page_down(gt); return true;
    case Action::InstrHome: instr_nav_home(gt); return true;
    case Action::InstrEnd: instr_nav_end(gt); return true;
    default: return false;
    }
}

static void names_field_step(int delta) {
    if (gimgui_new_ui_active()) gimgui_song_field_end();
    int f = editorInfo.enpos + delta;
    if (f > 2) f = 0;
    if (f < 0) f = 2;
    editorInfo.enpos     = f;
    editorInfo.nameIndex = f;
}

bool handle_names_action(Action act) {
    switch (act) {
    case Action::NamesFieldNext:
        names_field_step(1);
        return true;
    case Action::NamesFieldPrev:
        names_field_step(-1);
        return true;
    case Action::NamesFieldEdit:
        if (gimgui_new_ui_active()) gimgui_song_field_begin(editorInfo.enpos);
        return true;
    default: return false;
    }
}

bool handle_order_action(Action act) {
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::OrderRowUp: order_row_up(gt); return true;
    case Action::OrderRowDown: order_row_down(gt); return true;
    case Action::OrderColLeft:
        order_col_left(gt);
        return true;
    case Action::OrderColRight:
        order_col_right(gt);
        return true;
    case Action::OrderPageUp: order_page_up(gt); return true;
    case Action::OrderPageDown: order_page_down(gt); return true;
    case Action::OrderHome: order_nav_home(gt); return true;
    case Action::OrderEnd: order_nav_end(gt); return true;
    case Action::OrderInsert:
        if (editorInfo.expandOrderListView) {
            orderListInsert_External(gt);
            playUntilEnd(editorInfo.esnum);
        } else
            order_list_insert(gt);
        return true;
    case Action::OrderDelete:
        if (editorInfo.expandOrderListView) {
            orderListDelete_External();
            playUntilEnd(editorInfo.esnum);
        } else
            order_list_delete(gt);
        return true;
    case Action::OrderGoPattern:
        order_go_pattern(gt);
        return true;
    case Action::OrderSelectPatterns:
        order_select_patterns(gt);
        return true;
    case Action::OrderCopy:
        if (editorInfo.expandOrderListView == 0)
            orderListCopyMarkedArea();
        else
            orderListCopyMarkedArea_Expanded();
        return true;
    case Action::OrderCut:
        if (editorInfo.expandOrderListView) {
            orderListCopyMarkedArea_Expanded();
            orderListDelete_External();
            playUntilEnd(editorInfo.esnum);
        } else
            order_list_cut(gt);
        return true;
    case Action::OrderPaste:
        if (editorInfo.expandOrderListView == 0)
            orderListPasteToCursor(gt);
        else {
            int transposeOnly = editorInfo.escolumn > 2 ? 1 : 0;
            orderListPasteToCursor_External(gt, 0, transposeOnly);
        }
        return true;
    case Action::OrderInsertPaste:
        if (editorInfo.expandOrderListView)
            orderListPasteToCursor_External(gt, 1, 0);
        return true;
    case Action::OrderMarkToggle:
        order_list_mark_toggle();
        return true;
    case Action::OrderTransposeUp:
        order_list_transpose_up();
        return true;
    case Action::OrderTransposeDown:
        order_list_transpose_down();
        return true;
    case Action::OrderInsertRepeat:
        order_list_insert_repeat();
        return true;
    case Action::OrderSubtunePrev:
        prevsong(gt);
        return true;
    case Action::OrderSubtuneNext:
        nextsong(gt);
        return true;
    case Action::OrderPlayRangeStart:
        order_play_range_start(gt);
        return true;
    case Action::OrderPlayRangeEnd:
        order_play_range_end(gt);
        return true;
    default: return false;
    }
}

Action resolve_ctx(Ctx ctx, Chord chord) { return lookup_ctx(ctx, chord); }

Action resolve_input(Ctx ctx, int raw_scancode, int ascii_key, int shift, int ctrl) {
    uint32_t mods = 0;
    if (shift) mods |= Shift;
    if (ctrl) mods |= Ctrl;

    if (raw_scancode) {
        const Chord sc = make_scancode_chord(raw_scancode, mods);
        Action      a  = resolve_ctx(ctx, sc);
        if (a != Action::None) return a;
        a = resolve_ctx(Ctx::Global, sc);
        if (a != Action::None) return a;
    }
    if (ascii_key > 0 && ascii_key < 256) {
        const Chord ac = make_chord(ascii_key, mods);
        Action      a  = resolve_ctx(ctx, ac);
        if (a != Action::None) return a;
        a = resolve_ctx(Ctx::Global, ac);
        if (a != Action::None) return a;
    }
    return Action::None;
}

Action resolve_input_ctx(Ctx ctx, int raw_scancode, int ascii_key, int shift, int ctrl) {
    uint32_t mods = 0;
    if (shift) mods |= Shift;
    if (ctrl) mods |= Ctrl;

    if (raw_scancode) {
        Action a = resolve_ctx(ctx, make_scancode_chord(raw_scancode, mods));
        if (a != Action::None) return a;
    }
    if (ascii_key > 0 && ascii_key < 256) {
        Action a = resolve_ctx(ctx, make_chord(ascii_key, mods));
        if (a != Action::None) return a;
    }
    return Action::None;
}

static bool is_nav_arrow_key(int raw_scancode) {
    switch (raw_scancode) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN:
    case KEY_HOME:
    case KEY_END: return true;
    default: return false;
    }
}

Action resolve_input_ctx_nav(Ctx ctx, int raw_scancode, int ascii_key, int shift, int ctrl) {
    Action a = resolve_input_ctx(ctx, raw_scancode, ascii_key, shift, ctrl);
    if (a != Action::None) return a;
    if (shift && !ctrl && is_nav_arrow_key(raw_scancode))
        return resolve_input_ctx(ctx, raw_scancode, ascii_key, 0, 0);
    return Action::None;
}

bool dispatch_order_navigation() {
    if (editorInfo.editmode != EDIT_ORDERLIST) return false;

    // Vertical layout and editing actions apply to the ImGui order panel.
    if (!gimgui_new_ui_active()) return false;

    if (shiftpressed && !ctrlpressed && rawkey >= KEY_1 && rawkey <= KEY_6) {
        order_list_swap_channel(&gtObject, rawkey - KEY_1);
        clear_input();
        return true;
    }

    const Action act = resolve_input_ctx_nav(Ctx::Order, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    switch (rawkey) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN: win_enableKeyRepeat(); break;
    default: break;
    }

    if (!handle_order_action(act)) return false;

    clear_input();
    return true;
}

bool dispatch_pattern_navigation() {
    if (editorInfo.editmode != EDIT_PATTERN) return false;

    // Ctrl+arrow is global song transport.
    if (ctrlpressed) {
        const Action act = resolve_input_ctx(Ctx::Pattern, rawkey, key, shiftpressed, ctrlpressed);
        if (act != Action::PatternCopy && act != Action::PatternCut) return false;
    }

    if (gimgui_new_ui_active() && shiftpressed && !ctrlpressed && rawkey >= KEY_1 && rawkey <= KEY_6) {
        pattern_mute_channel(&gtObject, rawkey - KEY_1);
        clear_input();
        return true;
    }

    const Action act = resolve_input_ctx_nav(Ctx::Pattern, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    if (pattern_action_needs_imgui(act) && !gimgui_new_ui_active()) return false;

    switch (rawkey) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN:
    case KEY_INS:
    case KEY_DEL: win_enableKeyRepeat(); break;
    default: break;
    }

    if (!handle_pattern_action(act)) return false;

    clear_input();
    return true;
}

bool dispatch_table_navigation() {
    if (editorInfo.editmode != EDIT_TABLES) return false;

    if (!gimgui_new_ui_active()) return false;

    table_use_raw_hex_mode();

    // Ctrl+arrow is global song transport; leave to dispatch_global.
    if (ctrlpressed) return false;

    const Action act = resolve_input_ctx_nav(Ctx::Tables, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    switch (rawkey) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN:
    case KEY_INS:
    case KEY_DEL: win_enableKeyRepeat(); break;
    default: break;
    }

    if (!handle_table_action(act)) return false;

    clear_input();
    return true;
}

bool dispatch_instrument_navigation() {
    if (editorInfo.editmode != EDIT_INSTRUMENT) return false;

    if (!gimgui_new_ui_active()) return false;

    if (gimgui_instr_name_editing()) return false;

    if (ctrlpressed) return false;

    // Enter on the name field opens the ImGui editor (replaces legacy editstring).
    if (rawkey == KEY_ENTER && editorInfo.einum >= gtui::INSTR_FIRST && editorInfo.eipos >= LAST_INST) {
        gimgui_instr_name_begin(editorInfo.einum);
        clear_input();
        return true;
    }

    const Action act = resolve_input_ctx(Ctx::Instrument, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    switch (rawkey) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN: win_enableKeyRepeat(); break;
    default: break;
    }

    if (!handle_instrument_action(act)) return false;

    clear_input();
    return true;
}

bool dispatch_names_navigation() {
    if (editorInfo.editmode != EDIT_NAMES) return false;

    if (!gimgui_new_ui_active()) return false;

    const Action act = resolve_input_ctx(Ctx::Names, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    if (gimgui_song_field_editing()) {
        if (act == Action::NamesFieldNext || act == Action::NamesFieldPrev)
            gimgui_song_field_end();
        else
            return false;
    }

    if (!handle_names_action(act)) return false;

    clear_input();
    return true;
}


} // namespace

Ctx context_from_editmode(int editmode) {
    switch (editmode) {
    case EDIT_PATTERN: return Ctx::Pattern;
    case EDIT_ORDERLIST: return Ctx::Order;
    case EDIT_INSTRUMENT: return Ctx::Instrument;
    case EDIT_TABLES: return Ctx::Tables;
    case EDIT_NAMES: return Ctx::Names;
    default: return Ctx::Global;
    }
}

bool set_binding(Action action, Ctx ctx, Chord chord) {
    if (action == Action::None || chord == kNoChord) return false;

    for (auto it = g_overrides.begin(); it != g_overrides.end();) {
        if ((it->action == action && it->ctx == ctx) || (it->ctx == ctx && it->chord == chord))
            it = g_overrides.erase(it);
        else
            ++it;
    }

    g_overrides.push_back({ action, ctx, chord });
    return true;
}

bool clear_binding(Action action, Ctx ctx) {
    bool removed = false;
    for (auto it = g_overrides.begin(); it != g_overrides.end();) {
        if (it->action == action && it->ctx == ctx) {
            it      = g_overrides.erase(it);
            removed = true;
        }
        else {
            ++it;
        }
    }
    return removed;
}

void reset_bindings() { g_overrides.clear(); }

const char* action_name(Action a) {
    for (const ActionMeta& m : kActionMeta) {
        if (m.action == a) return m.name;
    }
    return "";
}

const char* action_label(Action a) {
    for (const ActionMeta& m : kActionMeta) {
        if (m.action == a) return m.label;
    }
    return "";
}

bool dispatch_mode_navigation() {
    switch (editorInfo.editmode) {
    case EDIT_ORDERLIST: return dispatch_order_navigation();
    case EDIT_PATTERN: return dispatch_pattern_navigation();
    case EDIT_TABLES: return dispatch_table_navigation();
    case EDIT_INSTRUMENT: return dispatch_instrument_navigation();
    case EDIT_NAMES: return dispatch_names_navigation();
    default: return false;
    }
}

bool dispatch_global(Ctx ctx) {
    if (editPaletteMode) return false;

    const Action act = resolve_input(ctx, rawkey, key, shiftpressed, ctrlpressed);
    if (act == Action::None) return false;

    if (!handle_global_action(act)) return false;

    clear_input();
    return true;
}

bool dispatch_pattern_cell_input(int midiNote, const EditorInput *input) {
    if (editorInfo.editmode != EDIT_PATTERN) return false;
    if (!gimgui_new_ui_active()) return false;

    const EditorInput in = input ? *input : editor_input_snapshot();
    if (!pattern_cell_input(&gtObject, midiNote, &in)) return false;

    clear_input();
    return true;
}

bool dispatch_instrument_cell_input(const EditorInput *input) {
    if (!gimgui_new_ui_active()) return false;
    if (editorInfo.editmode != EDIT_INSTRUMENT) return false;
    if (gimgui_instr_name_editing()) return false;

    const EditorInput in = input ? *input : editor_input_snapshot();
    if (!instrument_cell_input(&gtObject, &in)) return false;

    clear_input();
    return true;
}

bool dispatch_table_cell_input(const EditorInput *input) {
    if (!gimgui_new_ui_active()) return false;
    if (editorInfo.editmode != EDIT_TABLES) return false;

    const EditorInput in = input ? *input : editor_input_snapshot();
    if (!table_cell_input(&gtObject, &in)) return false;

    clear_input();
    return true;
}

bool consume_legacy_hex_input(int hex_at_frame_start) {
    if (hex_at_frame_start < 0) return false;
    if (!gimgui_new_ui_active()) return false;
    switch (editorInfo.editmode) {
    case EDIT_ORDERLIST:
    case EDIT_PATTERN:
    case EDIT_TABLES:
    case EDIT_INSTRUMENT:
        hexnybble = -1;
        clear_input();
        return true;
    default: return false;
    }
}

bool perform(Action act) {
    if (act == Action::None) return false;

    bool ok;
    switch (act) {
    case Action::OrderRowUp:
    case Action::OrderRowDown:
    case Action::OrderColLeft:
    case Action::OrderColRight:
    case Action::OrderPageUp:
    case Action::OrderPageDown:
    case Action::OrderHome:
    case Action::OrderEnd:
    case Action::OrderInsert:
    case Action::OrderDelete:
    case Action::OrderGoPattern:
    case Action::OrderSelectPatterns:
    case Action::OrderCopy:
    case Action::OrderCut:
    case Action::OrderPaste:
    case Action::OrderInsertPaste:
    case Action::OrderMarkToggle:
    case Action::OrderTransposeUp:
    case Action::OrderTransposeDown:
    case Action::OrderInsertRepeat:
    case Action::OrderSubtunePrev:
    case Action::OrderSubtuneNext:
    case Action::OrderPlayRangeStart:
    case Action::OrderPlayRangeEnd:
        ok = handle_order_action(act);
        break;
    case Action::PatternRowUp:
    case Action::PatternRowDown:
    case Action::PatternColLeft:
    case Action::PatternColRight:
    case Action::PatternPageUp:
    case Action::PatternPageDown:
    case Action::PatternHome:
    case Action::PatternEnd:
    case Action::PatternPrev:
    case Action::PatternNext:
    case Action::PatternInsert:
    case Action::PatternDelete:
    case Action::PatternCopy:
    case Action::PatternCut:
    case Action::PatternPaste:
    case Action::PatternMarkToggle:
    case Action::PatternShrink:
    case Action::PatternExpand:
    case Action::PatternJoin:
    case Action::PatternSplit:
    case Action::PatternToggleJam:
    case Action::PatternPlayFromCursor:
    case Action::PatternChnNext:
    case Action::PatternChnPrev:
    case Action::PatternToggleAutoAdvance:
    case Action::PatternCmdCopy:
    case Action::PatternCmdPaste:
    case Action::PatternInvert:
    case Action::PatternTransposeUp:
    case Action::PatternTransposeDown:
    case Action::PatternOctaveUp:
    case Action::PatternOctaveDown:
    case Action::PatternStepSizeUp:
    case Action::PatternStepSizeDown:
    case Action::PatternMarkAll:
    case Action::PatternAutoPitchbend:
    case Action::PatternPortamentoHelper:
        ok = handle_pattern_action(act);
        break;
    case Action::TableRowUp:
    case Action::TableRowDown:
    case Action::TableColLeft:
    case Action::TableColRight:
    case Action::TablePageUp:
    case Action::TablePageDown:
    case Action::TableHome:
    case Action::TableEnd:
    case Action::TableInsert:
    case Action::TableDelete:
    case Action::TableCopy:
    case Action::TableCut:
    case Action::TablePaste:
    case Action::TableOptimize:
    case Action::TableToggleLock:
    case Action::TableTestNote:
    case Action::TableReleaseNote:
    case Action::TableNegate:
    case Action::TableConvertNote:
        ok = handle_table_action(act);
        break;
    case Action::InstrRowUp:
    case Action::InstrRowDown:
    case Action::InstrColLeft:
    case Action::InstrColRight:
    case Action::InstrPageUp:
    case Action::InstrPageDown:
    case Action::InstrHome:
    case Action::InstrEnd:
        ok = handle_instrument_action(act);
        break;
    case Action::NamesFieldNext:
    case Action::NamesFieldPrev:
    case Action::NamesFieldEdit:
        ok = handle_names_action(act);
        break;
    default:
        ok = handle_global_action(act);
        break;
    }

    return ok;
}

} // namespace gtaction
