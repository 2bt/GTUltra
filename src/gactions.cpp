//
// gactions - action / keymap layer implementation (M3).
//

#include "gactions.h"

#include "goattrk2.h"
#include "gorder.h"
#include "ginfo.h"
#include "gimgui.h"
#include "gpattern.h"
#include "gdisplay.h"
#include "gsound.h"

namespace gtaction {

namespace {

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
    { Action::Save,              "Save",              "Save song" },
    { Action::Undo,              "Undo",              "Undo" },
    { Action::Quit,              "Quit",              "Quit" },
    { Action::Clear,             "Clear",             "Clear song" },
    { Action::Help,              "Help",              "Help" },
    { Action::EditModeNext,      "EditModeNext",      "Next edit mode" },
    { Action::EditModePrev,      "EditModePrev",      "Previous edit mode" },
    { Action::EditModePattern,   "EditModePattern",   "Pattern editor" },
    { Action::EditModeOrder,     "EditModeOrder",     "Order list" },
    { Action::EditModeInstrument,"EditModeInstrument","Instrument editor" },
    { Action::EditModeTables,    "EditModeTables",    "Table editor" },
    { Action::PlaySongStart,     "PlaySongStart",     "Play from song start" },
    { Action::PlayPatternStart,  "PlayPatternStart",  "Play from pattern start" },
    { Action::PlayCurrent,       "PlayCurrent",       "Play from cursor" },
    { Action::Stop,              "Stop",              "Stop playback" },
    { Action::PlayFromBeginning,"PlayFromBeginning", "Play from beginning" },
    { Action::PlayPatternMode,  "PlayPatternMode",   "Play pattern" },
    { Action::ToggleFollow,      "ToggleFollow",      "Toggle follow mode" },
    { Action::ToggleLoop,        "ToggleLoop",        "Toggle pattern loop" },
    { Action::SongPosNext,       "SongPosNext",       "Next song position" },
    { Action::SongPosPrev,       "SongPosPrev",       "Previous song position" },
    { Action::Relocate,          "Relocate",          "Open relocator" },
    { Action::LoadSong,          "LoadSong",          "Load song" },
    { Action::SaveSong,          "SaveSong",          "Save song" },
    { Action::OctaveUp,          "OctaveUp",          "Octave up" },
    { Action::OctaveDown,        "OctaveDown",        "Octave down" },
    { Action::PrevInstr,         "PrevInstr",         "Previous instrument" },
    { Action::NextInstr,         "NextInstr",         "Next instrument" },
    { Action::OrderRowUp,        "OrderRowUp",        "Order list: previous row" },
    { Action::OrderRowDown,      "OrderRowDown",      "Order list: next row" },
    { Action::OrderColLeft,      "OrderColLeft",      "Order list: previous channel" },
    { Action::OrderColRight,     "OrderColRight",     "Order list: next channel" },
    { Action::PatternRowUp,      "PatternRowUp",      "Pattern: previous row" },
    { Action::PatternRowDown,    "PatternRowDown",    "Pattern: next row" },
    { Action::PatternColLeft,    "PatternColLeft",    "Pattern: previous column" },
    { Action::PatternColRight,   "PatternColRight",   "Pattern: next column" },
    { Action::PatternPageUp,     "PatternPageUp",     "Pattern: page up" },
    { Action::PatternPageDown,   "PatternPageDown",   "Pattern: page down" },
    { Action::PatternHome,       "PatternHome",       "Pattern: first row" },
    { Action::PatternEnd,        "PatternEnd",        "Pattern: last row" },
    { Action::ToggleSIDTracker64,"ToggleSIDTracker64","Toggle SIDTracker64 mode" },
    { Action::PrevMultiplier,    "PrevMultiplier",    "Previous speed multiplier" },
    { Action::NextMultiplier,    "NextMultiplier",    "Next speed multiplier" },
    { Action::ToggleAdsrOrPan,   "ToggleAdsrOrPan",   "Toggle ADSR / pan edit" },
    { Action::ToggleSidModel,    "ToggleSidModel",    "Toggle SID model" },
    { Action::CycleStereoMode,   "CycleStereoMode",   "Cycle stereo mode" },
    { Action::FastRelocate,      "FastRelocate",      "Fast relocate export" },
    { Action::SaveWav,           "SaveWav",           "Save WAV" },
    { Action::SongRewind,        "SongRewind",        "Rewind song position" },
};

// Default keymap. Context-specific entries override Global for the same chord.
const Binding kBindings[] = {
    // Global file / session
    { Action::Save,         Ctx::Global, make_chord(KEY_S, Ctrl) },
    { Action::Undo,         Ctx::Global, make_chord(KEY_Z, Ctrl) },
    { Action::Quit,         Ctx::Global, make_chord(KEY_ESC) },
    { Action::Clear,        Ctx::Global, make_chord(KEY_ESC, Shift) },
    { Action::Help,         Ctx::Global, make_chord(KEY_F12) },
    { Action::ToggleSIDTracker64, Ctx::Global, make_chord(KEY_F12, Shift) },
    { Action::ToggleSIDTracker64, Ctx::Global, make_chord(KEY_F12, Ctrl) },

    // Edit mode
    { Action::EditModeNext, Ctx::Global, make_chord(KEY_TAB) },
    { Action::EditModePrev, Ctx::Global, make_chord(KEY_TAB, Shift) },
    { Action::EditModePattern,    Ctx::Global, make_chord(KEY_F5) },
    { Action::EditModeOrder,      Ctx::Global, make_chord(KEY_F6) },
    { Action::EditModeInstrument, Ctx::Global, make_chord(KEY_F7) },
    { Action::EditModeTables,     Ctx::Global, make_chord(KEY_F8) },
    { Action::PrevMultiplier, Ctx::Global, make_chord(KEY_F5, Shift) },
    { Action::NextMultiplier, Ctx::Global, make_chord(KEY_F6, Shift) },
    { Action::ToggleAdsrOrPan, Ctx::Global, make_chord(KEY_F7, Shift) },
    { Action::ToggleSidModel,  Ctx::Global, make_chord(KEY_F8, Shift) },
    { Action::PrevMultiplier, Ctx::Global, make_chord(KEY_F5, Ctrl) },
    { Action::NextMultiplier, Ctx::Global, make_chord(KEY_F6, Ctrl) },
    { Action::ToggleAdsrOrPan, Ctx::Global, make_chord(KEY_F7, Ctrl) },
    { Action::ToggleSidModel,  Ctx::Global, make_chord(KEY_F8, Ctrl) },

    // Transport — handler reads shift/ctrl for variant behaviour
    { Action::PlaySongStart,    Ctx::Global, make_chord(KEY_F1) },
    { Action::PlaySongStart,    Ctx::Global, make_chord(KEY_F1, Shift) },
    { Action::PlayPatternStart, Ctx::Global, make_chord(KEY_F2) },
    { Action::PlayPatternStart, Ctx::Global, make_chord(KEY_F2, Shift) },
    { Action::PlayCurrent,      Ctx::Global, make_chord(KEY_F3) },
    { Action::PlayCurrent,      Ctx::Global, make_chord(KEY_F3, Shift) },
    { Action::Stop,             Ctx::Global, make_chord(KEY_F4) },
    { Action::Stop,             Ctx::Global, make_chord(KEY_F4, Shift) },

    { Action::Relocate,  Ctx::Global, make_chord(KEY_F9) },
    { Action::CycleStereoMode, Ctx::Global, make_chord(KEY_F9, Shift) },
    { Action::FastRelocate,    Ctx::Global, make_chord(KEY_F9, Ctrl) },
    { Action::LoadSong,  Ctx::Global, make_chord(KEY_F10) },
    { Action::SaveSong,  Ctx::Global, make_chord(KEY_F11) },
    { Action::SaveWav,   Ctx::Global, make_chord(KEY_F11, Shift) },
    { Action::SaveWav,   Ctx::Global, make_chord(KEY_F11, Ctrl) },

    // Octave / instrument (legacy switch(key) shortcuts)
    { Action::OctaveUp,   Ctx::Global, make_chord('*') },
    { Action::OctaveDown, Ctx::Global, make_chord('/') },
    { Action::OctaveDown, Ctx::Global, make_chord('\'') },
    { Action::OctaveUp,   Ctx::Global, make_chord(KEY_KPMULTIPLY) },
    { Action::OctaveDown, Ctx::Global, make_chord(KEY_KPDIVIDE) },
    { Action::PrevInstr,  Ctx::Global, make_chord('?') },
    { Action::PrevInstr,  Ctx::Global, make_chord('-') },
    { Action::PrevInstr,  Ctx::Global, make_chord('<') },
    { Action::NextInstr,  Ctx::Global, make_chord('+') },
    { Action::NextInstr,  Ctx::Global, make_chord('_') },
    { Action::NextInstr,  Ctx::Global, make_chord('>') },

    // Song position (legacy ';' / ':' keys)
    { Action::SongPosPrev, Ctx::Global, make_chord(KEY_SEMICOLON) },
    { Action::SongPosPrev, Ctx::Global, make_chord(';') },
    { Action::SongPosNext, Ctx::Global, make_chord(KEY_COLON) },
    { Action::SongPosNext, Ctx::Global, make_chord(':') },

    // Song transport (Ctrl+arrow)
    { Action::SongRewind,  Ctx::Global, make_chord(KEY_LEFT, Ctrl) },
    { Action::SongPosNext, Ctx::Global, make_chord(KEY_RIGHT, Ctrl) },

    // Order list — vertical ImGui layout (only when legacy horizontal nav is active)
    { Action::OrderRowUp,    Ctx::Order, make_chord(KEY_UP) },
    { Action::OrderRowDown,  Ctx::Order, make_chord(KEY_DOWN) },
    { Action::OrderColLeft,  Ctx::Order, make_chord(KEY_LEFT) },
    { Action::OrderColRight, Ctx::Order, make_chord(KEY_RIGHT) },

    // Pattern editor — unmodified arrow keys
    { Action::PatternRowUp,    Ctx::Pattern, make_chord(KEY_UP) },
    { Action::PatternRowDown,  Ctx::Pattern, make_chord(KEY_DOWN) },
    { Action::PatternColLeft,  Ctx::Pattern, make_chord(KEY_LEFT) },
    { Action::PatternColRight, Ctx::Pattern, make_chord(KEY_RIGHT) },
    { Action::PatternPageUp,   Ctx::Pattern, make_chord(KEY_PGUP) },
    { Action::PatternPageDown, Ctx::Pattern, make_chord(KEY_PGDN) },
    { Action::PatternHome,     Ctx::Pattern, make_chord(KEY_HOME) },
    { Action::PatternEnd,      Ctx::Pattern, make_chord(KEY_END) },
};

Action lookup(Ctx ctx, Chord chord)
{
    if (chord == kNoChord)
        return Action::None;

    for (const Binding& b : kBindings) {
        if (b.ctx == ctx && b.chord == chord)
            return b.action;
    }
    for (const Binding& b : kBindings) {
        if (b.ctx == Ctx::Global && b.chord == chord)
            return b.action;
    }
    return Action::None;
}

int order_max_channels()
{
    int maxCh = 6;
    if ((editorInfo.maxSIDChannels == 3) ||
        (editorInfo.maxSIDChannels == 9 && (editorInfo.esnum & 1)))
        maxCh = 3;
    return maxCh;
}

void order_clamp_cursor_to_channel()
{
    if ((editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) ||
        (editorInfo.eseditpos > songlen[editorInfo.esnum][editorInfo.eschn] + 1))
    {
        editorInfo.eseditpos = songlen[editorInfo.esnum][editorInfo.eschn] + 1;
        editorInfo.escolumn = 0;
    }
}

void order_sync_view()
{
    if (editorInfo.eseditpos - editorInfo.esview < 0)
        editorInfo.esview = editorInfo.eseditpos;

    if (editorInfo.expandOrderListView == 0) {
        if (editorInfo.eseditpos - editorInfo.esview >= VISIBLEORDERLIST)
            editorInfo.esview = editorInfo.eseditpos - VISIBLEORDERLIST + 1;
    } else {
        if (editorInfo.eseditpos - editorInfo.esview >= EXTENDEDVISIBLEORDERLIST)
            editorInfo.esview = editorInfo.eseditpos - EXTENDEDVISIBLEORDERLIST + 1;
    }
}

void order_row_up(GTOBJECT* gt)
{
    if (editorInfo.expandOrderListView) {
        if (shiftOrCtrlPressed) {
            if (editorInfo.esmarkchn == -1) {
                editorInfo.esmarkchn = editorInfo.esmarkchnend = editorInfo.eschn;
                editorInfo.esmarkstart = editorInfo.esmarkend = editorInfo.eseditpos;
            }
        }
        if (editorInfo.eseditpos > 0) {
            editorInfo.eseditpos--;
            if (shiftOrCtrlPressed)
                editorInfo.esmarkend = editorInfo.eseditpos;
        }
        return;
    }

    if (editorInfo.eseditpos > 0) {
        editorInfo.eseditpos--;
        if (shiftOrCtrlPressed)
            editorInfo.esmarkend = editorInfo.eseditpos;
    }
    order_sync_view();
    (void)gt;
}

void order_row_down(GTOBJECT* gt)
{
    if (editorInfo.expandOrderListView) {
        if (shiftOrCtrlPressed) {
            if (editorInfo.esmarkchn == -1) {
                editorInfo.esmarkchn = editorInfo.esmarkchnend = editorInfo.eschn;
                editorInfo.esmarkstart = editorInfo.esmarkend = editorInfo.eseditpos;
            }
        }
        if (editorInfo.eseditpos < 0x7ff) {
            editorInfo.eseditpos++;
            if (shiftOrCtrlPressed)
                editorInfo.esmarkend = editorInfo.eseditpos;
        }
        return;
    }

    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn] + 1) {
        editorInfo.eseditpos++;
        if (shiftOrCtrlPressed)
            editorInfo.esmarkend = editorInfo.eseditpos;
    }
    order_sync_view();
    (void)gt;
}

void order_col_left(GTOBJECT* gt)
{
    const int maxCh = order_max_channels();

    if (editorInfo.expandOrderListView) {
        // Expanded view: legacy left/right moves within the cell / wraps channel.
        return;
    }

    editorInfo.eschn--;
    if (editorInfo.eschn < 0)
        editorInfo.eschn = maxCh - 1;
    order_clamp_cursor_to_channel();
    setMasterLoopChannel(gt, "action_order_col_left");

    if (shiftOrCtrlPressed) {
        editorInfo.esmarkchn = -1;
        editorInfo.esmarkchnend = -1;
    }
    order_sync_view();
}

void order_col_right(GTOBJECT* gt)
{
    const int maxCh = order_max_channels();

    if (editorInfo.expandOrderListView)
        return;

    editorInfo.eschn++;
    if (editorInfo.eschn >= maxCh)
        editorInfo.eschn = 0;
    order_clamp_cursor_to_channel();
    setMasterLoopChannel(gt, "action_order_col_right");

    if (shiftOrCtrlPressed) {
        editorInfo.esmarkchn = -1;
        editorInfo.esmarkchnend = -1;
    }
    order_sync_view();
}

// ---- transport (migrated from generalcommands KEY_F1..F4) ----

void transport_on_f1(GTOBJECT* gt)
{
    if (editPaletteMode)
        return;

    playUntilEnd(editorInfo.esnum);

    if (useOriginalGTFunctionKeys) {
        transportLoopPattern = 0;
        followplay         = shiftOrCtrlPressed ? 1 : 0;
        orderPlayFromPosition(gt, 0, 0, 0, 1);
    } else {
        if (shiftpressed)
            orderPlayFromPosition(gt, 0, 0, 0, 1);
        else
            playFromCurrentPosition(gt, 0);
    }
}

void transport_on_f2(GTOBJECT* gt)
{
    if (editPaletteMode)
        return;

    if (SIDTracker64ForIPadIsAmazing != 0) {
        if (shiftOrCtrlPressed)
            followplay = 1 - followplay;
        else
            playFromCurrentPosition(gt, 0);
    } else if (useOriginalGTFunctionKeys) {
        playFromCurrentPosition(gt, 0);
        transportLoopPattern = 0;
        followplay         = shiftOrCtrlPressed ? 1 : 0;
    } else {
        if (shiftOrCtrlPressed) {
            followplay = 1 - followplay;
        } else {
            transportLoopPattern = 1 - transportLoopPattern;
            if (!transportLoopPattern) {
                editorInfo.highlightLoopChannel       = 999;
                editorInfo.highlightLoopPatternNumber   = -1;
                editorInfo.highlightLoopStart           = 0;
                editorInfo.highlightLoopEnd             = 0;
            }
        }
    }
}

void transport_on_f3(GTOBJECT* gt)
{
    if (editPaletteMode)
        return;

    if (useOriginalGTFunctionKeys && SIDTracker64ForIPadIsAmazing == 0) {
        transportLoopPattern = 1;
        followplay         = shiftOrCtrlPressed ? 1 : 0;
        playFromCurrentPosition(gt, 0);
    } else {
        if (shiftOrCtrlPressed) {
            transportLoopPattern = 1 - transportLoopPattern;
        } else if (editorInfo.editmode == EDIT_ORDERLIST) {
            orderSelectPatternsFromSelected(gt);
            orderPlayFromPosition(gt, 0, editorInfo.eseditpos, editorInfo.eschn, 1);
        } else {
            playFromCurrentPosition(gt, editorInfo.eppos);
        }
    }
}

void transport_on_f4(GTOBJECT* gt)
{
    if (shiftOrCtrlPressed) {
        mutechannel(editorInfo.epchn, gt);
        return;
    }
    if (gt->songinit != PLAY_STOPPED) {
        stopsong(gt);
        setMasterLoopChannel(gt, "debug_9");
    }
}

void edit_octave_up()
{
    if (editorInfo.editmode == EDIT_NAMES)
        return;
    if (editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9)
        return;
    if (editorInfo.epoctave < 7)
        editorInfo.epoctave++;
}

void edit_octave_down()
{
    if (editorInfo.editmode == EDIT_NAMES)
        return;
    if (editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9)
        return;
    if (editorInfo.epoctave > 0)
        editorInfo.epoctave--;
}

void edit_prev_instr()
{
    if ((editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos != 9) ||
        editorInfo.editmode == EDIT_TABLES) {
        previnstr();
        return;
    }
    if (editorInfo.editmode != EDIT_NAMES && editorInfo.editmode != EDIT_ORDERLIST) {
        if (!(editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos == 9))
            previnstr();
    }
}

void edit_next_instr()
{
    if ((editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos != 9) ||
        editorInfo.editmode == EDIT_TABLES) {
        nextinstr();
        return;
    }
    if (editorInfo.editmode != EDIT_NAMES && editorInfo.editmode != EDIT_ORDERLIST) {
        if (!(editorInfo.editmode == EDIT_INSTRUMENT && editorInfo.eipos >= 9))
            nextinstr();
    }
}

bool handle_global_action(Action act)
{
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::Save: {
        int validSize = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff)
                validSize = 0;
        }
        if (validSize) {
            int s = quickSave();
            if (s)
                sprintf(infoTextBuffer, "quick save: %d", s);
            else
                save(gt, 0);
        }
        return true;
    }

    case Action::Undo:
        if (!editPaletteMode)
            undoPerform(gt);
        return true;

    case Action::Quit:
        if (!shiftOrCtrlPressed)
            quit(gt);
        return true;

    case Action::Clear:
        if (shiftOrCtrlPressed)
            clear(gt);
        return true;

    case Action::Help:
        stopScreenDisplay();
        onlinehelp(0, shiftOrCtrlPressed ? 1 : 0, gt);
        restartScreenDisplay();
        return true;

    case Action::EditModeNext:
        if (!shiftOrCtrlPressed) {
            editorInfo.editmode++;
            if (editorInfo.editmode > EDIT_NAMES)
                editorInfo.editmode = EDIT_PATTERN;
            setMasterLoopChannel(gt, "action_editmode_next");
        }
        return true;

    case Action::EditModePrev:
        if (shiftOrCtrlPressed) {
            editorInfo.editmode--;
            if (editorInfo.editmode < EDIT_PATTERN)
                editorInfo.editmode = EDIT_NAMES;
            setMasterLoopChannel(gt, "action_editmode_prev");
        }
        return true;

    case Action::EditModePattern:
        if (!shiftOrCtrlPressed)
            editorInfo.editmode = EDIT_PATTERN;
        return true;

    case Action::EditModeOrder:
        if (!shiftOrCtrlPressed)
            editorInfo.editmode = EDIT_ORDERLIST;
        return true;

    case Action::EditModeInstrument:
        if (!shiftOrCtrlPressed) {
            if (editorInfo.editmode == EDIT_INSTRUMENT)
                editorInfo.editmode = EDIT_TABLES;
            else
                editorInfo.editmode = EDIT_INSTRUMENT;
            disableEnterToReturnToLastPos = 1;
        }
        return true;

    case Action::EditModeTables:
        if (!shiftOrCtrlPressed) {
            editorInfo.editmode = EDIT_TABLES;
            disableEnterToReturnToLastPos = 1;
        }
        return true;

    case Action::SongPosNext:
        nextSongPos(gt);
        return true;

    case Action::SongPosPrev:
        previousSongPos(gt, 1);
        return true;

    case Action::PlaySongStart:
        transport_on_f1(gt);
        return true;

    case Action::PlayPatternStart:
        transport_on_f2(gt);
        return true;

    case Action::PlayCurrent:
        transport_on_f3(gt);
        return true;

    case Action::Stop:
        transport_on_f4(gt);
        return true;

    case Action::PlayFromBeginning:
        initsong(editorInfo.esnum, PLAY_BEGINNING, gt);
        return true;

    case Action::PlayPatternMode:
        initsong(editorInfo.esnum, PLAY_PATTERN, gt);
        return true;

    case Action::ToggleFollow:
        followplay = 1 - followplay;
        return true;

    case Action::ToggleLoop:
        transportLoopPattern = 1 - transportLoopPattern;
        return true;

    case Action::Relocate: {
        int ok = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff)
                ok = 0;
            else
                compressAllSongs();
        }
        if (ok) {
            stopScreenDisplay();
            relocator(gt, 0, 0);
            restartScreenDisplay();
            printmainscreen(gt);
            sprintf(infoTextBuffer, " ");
        }
        return true;
    }

    case Action::LoadSong:
        handleLoad(gt, NULL);
        return true;

    case Action::SaveSong: {
        int ok = 1;
        if (editorInfo.expandOrderListView) {
            int maxSize = validateAllSongs();
            if (maxSize > 0xff)
                ok = 0;
        }
        if (ok)
            save(gt, 0);
        return true;
    }

    case Action::OctaveUp:
        edit_octave_up();
        return true;

    case Action::OctaveDown:
        edit_octave_down();
        return true;

    case Action::PrevInstr:
        edit_prev_instr();
        return true;

    case Action::NextInstr:
        edit_next_instr();
        return true;

    case Action::ToggleSIDTracker64:
        SIDTracker64ForIPadIsAmazing = 1 - SIDTracker64ForIPadIsAmazing;
        setSIDTracker64KeyOnStyle();
        if (!SIDTracker64ForIPadIsAmazing)
            sprintf(infoTextBuffer, "SIDTracker64 Mode: Disabled");
        else
            sprintf(infoTextBuffer, "SIDTracker64 Mode: Enabled");
        forceInfoLine = 1;
        return true;

    case Action::PrevMultiplier:
        prevmultiplier();
        return true;

    case Action::NextMultiplier:
        nextmultiplier();
        return true;

    case Action::ToggleAdsrOrPan:
        if (!editPan)
            editadsr(gt);
        else
            editSIDPan(gt);
        return true;

    case Action::ToggleSidModel:
        editorInfo.sidmodel ^= 1;
        sound_init(b, mr, writer, hardsid, editorInfo.sidmodel, editorInfo.ntsc,
                   editorInfo.multiplier, catweasel, interpolate, customclockrate);
        return true;

    case Action::CycleStereoMode:
        stereoMode++;
        stereoMode %= 3;
        validateStereoMode();
        return true;

    case Action::FastRelocate:
        if (songExported) {
            relocator(gt, 0, 1);
            sprintf(infoTextBuffer, "Song Exported:%s", packedsongname);
        }
        return true;

    case Action::SaveWav:
        save(gt, 1);
        return true;

    case Action::SongRewind: {
        leftKeyTicksDelta = SDL_GetTicks() - leftKeyTicks;
        leftKeyTicks = SDL_GetTicks();
        handlePressRewind(leftKeyTicksDelta < 300 ? 1 : 0, gt);
        return true;
    }

    default:
        return false;
    }
}

bool handle_pattern_action(Action act)
{
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::PatternRowUp:
        pattern_nav_up(gt);
        return true;
    case Action::PatternRowDown:
        pattern_nav_down(gt);
        return true;
    case Action::PatternColLeft:
        pattern_col_left(gt);
        return true;
    case Action::PatternColRight:
        pattern_col_right(gt);
        return true;
    case Action::PatternPageUp:
        pattern_nav_page_up(gt);
        return true;
    case Action::PatternPageDown:
        pattern_nav_page_down(gt);
        return true;
    case Action::PatternHome:
        pattern_nav_home(gt);
        return true;
    case Action::PatternEnd:
        pattern_nav_end(gt);
        return true;
    default:
        return false;
    }
}

bool handle_order_action(Action act)
{
    GTOBJECT* gt = &gtObject;
    switch (act) {
    case Action::OrderRowUp:
        order_row_up(gt);
        return true;
    case Action::OrderRowDown:
        order_row_down(gt);
        return true;
    case Action::OrderColLeft:
        if (editorInfo.expandOrderListView)
            return false;
        order_col_left(gt);
        return true;
    case Action::OrderColRight:
        if (editorInfo.expandOrderListView)
            return false;
        order_col_right(gt);
        return true;
    default:
        return false;
    }
}

} // namespace

Ctx context_from_editmode(int editmode)
{
    switch (editmode) {
    case EDIT_PATTERN:     return Ctx::Pattern;
    case EDIT_ORDERLIST:   return Ctx::Order;
    case EDIT_INSTRUMENT:  return Ctx::Instrument;
    case EDIT_TABLES:      return Ctx::Tables;
    case EDIT_NAMES:       return Ctx::Names;
    default:               return Ctx::Global;
    }
}

Chord chord_from_input(int raw_scancode, int ascii_key, int shift, int ctrl)
{
    uint32_t mods = 0;
    if (shift)
        mods |= Shift;
    if (ctrl)
        mods |= Ctrl;

    if (raw_scancode)
        return make_chord(raw_scancode, mods);

    if (ascii_key > 0 && ascii_key < 256)
        return make_chord(ascii_key, mods);

    return kNoChord;
}

Action resolve(Ctx ctx, Chord chord)
{
    return lookup(ctx, chord);
}

const char* action_name(Action a)
{
    for (const ActionMeta& m : kActionMeta) {
        if (m.action == a)
            return m.name;
    }
    return "";
}

const char* action_label(Action a)
{
    for (const ActionMeta& m : kActionMeta) {
        if (m.action == a)
            return m.label;
    }
    return "";
}

bool dispatch_order_navigation()
{
    if (editorInfo.editmode != EDIT_ORDERLIST)
        return false;

    // Vertical arrow remapping only applies to the ImGui order panel.
    if (!gimgui_new_ui_active())
        return false;

    const Chord chord = chord_from_input(rawkey, key, shiftpressed, ctrlpressed);
    const Action act  = resolve(Ctx::Order, chord);
    if (act == Action::None)
        return false;

    if (!handle_order_action(act))
        return false;

    clear_input();
    return true;
}

bool dispatch_pattern_navigation()
{
    if (editorInfo.editmode != EDIT_PATTERN)
        return false;

    // Shift/Ctrl variants stay in patterncommands (prev/next pattern, etc.).
    if (shiftOrCtrlPressed)
        return false;

    const Chord chord = chord_from_input(rawkey, key, shiftpressed, ctrlpressed);
    const Action act  = resolve(Ctx::Pattern, chord);
    if (act == Action::None)
        return false;

    switch (rawkey) {
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_PGUP:
    case KEY_PGDN:
        win_enableKeyRepeat();
        break;
    default:
        break;
    }

    if (!handle_pattern_action(act))
        return false;

    clear_input();
    return true;
}

bool dispatch_mode_navigation()
{
    switch (editorInfo.editmode) {
    case EDIT_ORDERLIST:
        return dispatch_order_navigation();
    case EDIT_PATTERN:
        return dispatch_pattern_navigation();
    default:
        return false;
    }
}

bool dispatch_global(Ctx ctx)
{
    if (editPaletteMode)
        return false;

    const Chord chord = chord_from_input(rawkey, key, shiftpressed, ctrlpressed);
    Action act = resolve(ctx, chord);
    if (act == Action::None)
        act = resolve(Ctx::Global, chord);
    if (act == Action::None)
        return false;

    if (!handle_global_action(act))
        return false;

    clear_input();
    return true;
}

void clear_input()
{
    key    = 0;
    rawkey = 0;
}

bool perform(Action act)
{
    if (act == Action::None)
        return false;

    switch (act) {
    case Action::OrderRowUp:
    case Action::OrderRowDown:
    case Action::OrderColLeft:
    case Action::OrderColRight:
        return handle_order_action(act);
    case Action::PatternRowUp:
    case Action::PatternRowDown:
    case Action::PatternColLeft:
    case Action::PatternColRight:
    case Action::PatternPageUp:
    case Action::PatternPageDown:
    case Action::PatternHome:
    case Action::PatternEnd:
        return handle_pattern_action(act);
    default:
        return handle_global_action(act);
    }
}

} // namespace gtaction
