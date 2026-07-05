//
// gactions - action / keymap layer implementation (M3).
//

#include "gactions.h"

#include "goattrk2.h"
#include "gorder.h"
#include "ginfo.h"
#include "gimgui.h"

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

static const ActionMeta kActionMeta[] = {
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
    { Action::ToggleFollow,      "ToggleFollow",      "Toggle follow mode" },
    { Action::ToggleLoop,        "ToggleLoop",        "Toggle pattern loop" },
    { Action::SongPosNext,       "SongPosNext",       "Next song position" },
    { Action::SongPosPrev,       "SongPosPrev",       "Previous song position" },
    { Action::OrderRowUp,        "OrderRowUp",        "Order list: previous row" },
    { Action::OrderRowDown,      "OrderRowDown",      "Order list: next row" },
    { Action::OrderColLeft,      "OrderColLeft",      "Order list: previous channel" },
    { Action::OrderColRight,     "OrderColRight",     "Order list: next channel" },
};

// Default keymap. Context-specific entries override Global for the same chord.
static const Binding kBindings[] = {
    // Global file / session
    { Action::Save,         Ctx::Global, make_chord(KEY_S, Ctrl) },
    { Action::Undo,         Ctx::Global, make_chord(KEY_Z, Ctrl) },
    { Action::Quit,         Ctx::Global, make_chord(KEY_ESC) },
    { Action::Clear,        Ctx::Global, make_chord(KEY_ESC, Shift) },
    { Action::Help,         Ctx::Global, make_chord(KEY_F12) },

    // Edit mode
    { Action::EditModeNext, Ctx::Global, make_chord(KEY_TAB) },
    { Action::EditModePrev, Ctx::Global, make_chord(KEY_TAB, Shift) },
    { Action::EditModePattern,    Ctx::Global, make_chord(KEY_F5) },
    { Action::EditModeOrder,      Ctx::Global, make_chord(KEY_F6) },
    { Action::EditModeInstrument, Ctx::Global, make_chord(KEY_F7) },
    { Action::EditModeTables,     Ctx::Global, make_chord(KEY_F8) },

    // Transport (F-keys without modifiers — shift variants stay in legacy for now)
    { Action::PlaySongStart,    Ctx::Global, make_chord(KEY_F1) },
    { Action::PlayPatternStart, Ctx::Global, make_chord(KEY_F2) },
    { Action::PlayCurrent,      Ctx::Global, make_chord(KEY_F3) },
    { Action::Stop,             Ctx::Global, make_chord(KEY_F4) },

    // Song position (legacy ';' / ':' keys)
    { Action::SongPosPrev, Ctx::Global, make_chord(KEY_SEMICOLON) },
    { Action::SongPosPrev, Ctx::Global, make_chord(';') },
    { Action::SongPosNext, Ctx::Global, make_chord(KEY_COLON) },
    { Action::SongPosNext, Ctx::Global, make_chord(':') },

    // Order list — vertical ImGui layout (only when legacy horizontal nav is active)
    { Action::OrderRowUp,    Ctx::Order, make_chord(KEY_UP) },
    { Action::OrderRowDown,  Ctx::Order, make_chord(KEY_DOWN) },
    { Action::OrderColLeft,  Ctx::Order, make_chord(KEY_LEFT) },
    { Action::OrderColRight, Ctx::Order, make_chord(KEY_RIGHT) },
};

static Action lookup(Ctx ctx, Chord chord)
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

static int order_max_channels()
{
    int maxCh = 6;
    if ((editorInfo.maxSIDChannels == 3) ||
        (editorInfo.maxSIDChannels == 9 && (editorInfo.esnum & 1)))
        maxCh = 3;
    return maxCh;
}

static void order_clamp_cursor_to_channel()
{
    if ((editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) ||
        (editorInfo.eseditpos > songlen[editorInfo.esnum][editorInfo.eschn] + 1))
    {
        editorInfo.eseditpos = songlen[editorInfo.esnum][editorInfo.eschn] + 1;
        editorInfo.escolumn = 0;
    }
}

static void order_sync_view()
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

static void order_row_up(GTOBJECT* gt)
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

static void order_row_down(GTOBJECT* gt)
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

static void order_col_left(GTOBJECT* gt)
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

static void order_col_right(GTOBJECT* gt)
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

static bool handle_global_action(Action act)
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
        if (shiftOrCtrlPressed)
            onlinehelp(0, 1, gt);
        else
            onlinehelp(0, 0, gt);
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

    // Transport actions with modifier variants are still handled by
    // generalcommands(); only unmodified F-keys are migrated here for now.
    case Action::PlaySongStart:
    case Action::PlayPatternStart:
    case Action::PlayCurrent:
    case Action::Stop:
        return false;

    default:
        return false;
    }
}

static bool handle_order_action(Action act)
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

} // namespace gtaction
