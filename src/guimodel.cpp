//
// guimodel - implementation of the SDL-free model bridge (see guimodel.h).
// This TU is free to pull in the full legacy headers.
//
#include "guimodel.hpp"
#include "goattrk2.hpp"
#include "gactions.hpp"
#include "ginfo.hpp"
#include "ginstr.hpp"
#include "gmidi.hpp"
#include "gsong.hpp"
#include "gorder.hpp"
#include "gplay.hpp"
#include "gundo.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace gtui {


EditPanel edit_panel() { return static_cast<EditPanel>(editorInfo.editmode); }

void set_edit_panel(EditPanel p) {
    if (p < EditPanelPattern || p > EditPanelNames) return;
    editorInfo.editmode = static_cast<EditMode>(p);
}

int names_field() { return editorInfo.enpos; }

void names_set_field(int field) {
    if (field < 0) field = 0;
    if (field > 2) field = 2;
    editorInfo.editmode  = EditMode::Names;
    editorInfo.enpos     = field;
    editorInfo.nameIndex = field;
}

int table_count() { return MAX_TABLES; }
int table_len() { return MAX_TABLELEN; }
int table_visible_rows() { return VISIBLETABLEROWS; }

int table_view(int t) { return (t >= 0 && t < MAX_TABLES) ? editorInfo.etview[t] : 0; }

int table_cursor_table() { return editorInfo.etnum; }
int table_cursor_pos() { return editorInfo.etpos; }
int table_cursor_col() { return editorInfo.etcolumn; }
int table_mark_table() { return editorInfo.etmarknum; }
int table_mark_start() { return editorInfo.etmarkstart; }
int table_mark_end() { return editorInfo.etmarkend; }

static bool table_row_in_instrument_chain(int t, int row, int ptr) {
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN || ptr < 0) return false;

    if (t == 3) return row == ptr;

    unsigned char visited[MAX_TABLELEN] = {};
    for (int guard = 0; guard < MAX_TABLELEN; guard++) {
        if (ptr < 0 || ptr >= MAX_TABLELEN) break;
        if (visited[ptr]) break;
        visited[ptr] = 1;
        if (ptr == row) return true;
        if (ltable[t][ptr] == 0xff) {
            if (rtable[t][ptr] == 0) break;
            ptr = (int)rtable[t][ptr] - 1;
        }
        else ptr++;
    }
    return false;
}

bool table_row_uses_selected_instrument(int t, int row) {
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return false;
    const int inst = editorInfo.einum;
    if (inst <= 0) return false;
    return table_row_in_instrument_chain(t, row, instr[inst].ptr[t] - 1);
}

void table_set_cursor(int t, int row, int col) {
    if (t < 0) t = 0;
    if (t >= MAX_TABLES) t = MAX_TABLES - 1;
    if (row < 0) row = 0;
    if (row >= MAX_TABLELEN) row = MAX_TABLELEN - 1;
    if (col < 0) col = 0;
    if (col > 3) col = 3;

    editorInfo.editmode      = EditMode::Tables;
    editorInfo.editTableMode = EditTableMode::None; // raw II:LL RR hex (not detailed view)
    editorInfo.etnum         = t;
    editorInfo.etpos         = row;
    editorInfo.etcolumn      = col;
}

void table_set_view(int t, int view_row) {
    if (t < 0 || t >= MAX_TABLES) return;
    if (view_row < 0) view_row = 0;
    if (view_row >= MAX_TABLELEN) view_row = MAX_TABLELEN - 1;
    editorInfo.etview[t] = view_row;
}

unsigned table_left(int t, int row) {
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return 0;
    return ltable[t][row];
}

unsigned table_right(int t, int row) {
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return 0;
    return rtable[t][row];
}

void table_set(int t, int row, int col, unsigned value) {
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return;

    unsigned char  v    = (unsigned char)(value & 0xff);
    unsigned char* cell = (col == 0) ? &ltable[t][row] : &rtable[t][row];
    if (*cell == v) return; // nothing changed -> no undo entry

    // Same bracket the legacy editor uses for table edits (see docommand):
    // snapshot editor + the left/right table areas, mutate, then let the undo
    // system record the diff (or discard the object if nothing changed).
    GTUNDO_OBJECT* ed = undoCreateEditorInfo();
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + t, 0, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + t, 1, UNDO_AREA_DIRTY_CHECK);

    *cell = v;

    if (undoValidateUndoAreas(ed) == 0) undoFreeUndoObject(ed);
}

// ---- pattern editor ----

static int pattern_num_for(int ch) {
    int c2 = getActualChannel(editorInfo.esnum, ch);
    return gtObject.editorUndoInfo.editorInfo[c2].epnum;
}

int pattern_channels() {
    // Mirror displayPattern6Chn: 3 channels for a 3-SID subtune, else 6.
    if ((editorInfo.esnum & 1 && editorInfo.maxSIDChannels == 9) || editorInfo.maxSIDChannels == 3) return 3;
    return MAX_CHN;
}

int pattern_actual_channel(int ch) { return getActualChannel(editorInfo.esnum, ch); }

int pattern_length(int ch) {
    int pnum = pattern_num_for(ch);
    if (pnum < 0 || pnum >= MAX_PATT) return 0;
    return pattlen[pnum];
}

int pattern_rows() {
    int maxlen = 0;
    int chans  = pattern_channels();
    for (int c = 0; c < chans; c++) {
        int len = pattern_length(c);
        if (len > maxlen) maxlen = len;
    }
    if (maxlen > MAX_PATTROWS) maxlen = MAX_PATTROWS;
    return maxlen + 1; // include the PATT.END row
}

// Currently-playing row for display channel ch, or -1 when not playing or the
// channel is playing a different pattern than the one shown. Mirrors the legacy
// pattern-view playhead (gdisplay.cpp): row = lastpattptr/4, clamped to length.
int pattern_play_row(int ch) {
    if (!isplaying(&gtObject)) return -1;
    int c2 = getActualChannel(editorInfo.esnum, ch);
    if (gtObject.editorUndoInfo.editorInfo[c2].epnum != gtObject.chn[c2].lastpattnum) return -1;
    int chnrow = gtObject.chn[c2].lastpattptr / 4;
    int pnum   = gtObject.chn[c2].lastpattnum;
    if (pnum >= 0 && pnum < MAX_PATT && chnrow > pattlen[pnum]) chnrow = pattlen[pnum];
    return chnrow;
}

int pattern_step() { return stepsize; }
int pattern_cursor_row() { return editorInfo.eppos; }
int pattern_cursor_chn() { return editorInfo.epchn; }
int pattern_cursor_col() { return editorInfo.epcolumn; }
int pattern_number(int ch) { return pattern_num_for(ch); }

PatCell pattern_cell(int ch, int row) {
    PatCell c;
    c.note  = "";
    c.instr = 0;
    c.cmd   = 0;
    c.data  = 0;
    c.end   = false;
    c.valid = false;

    int pnum = pattern_num_for(ch);
    if (pnum < 0 || pnum >= MAX_PATT) return c;
    if (row < 0 || row > pattlen[pnum]) return c;

    const unsigned char* cell = &pattern[pnum][row * 4];
    c.valid                   = true;
    if (cell[0] == ENDPATT) {
        c.end = true;
        return c;
    }

    int n = (int)cell[0] - FIRSTNOTE;
    if (n < 0 || n >= 12 * 8) n = 12 * 8 - 3; // clamp to "..." on odd data
    c.note  = notename[n];
    c.instr = cell[1];
    c.cmd   = cell[2];
    c.data  = cell[3];
    return c;
}

int pattern_mark_channel() { return editorInfo.epmarkchn; }
int pattern_mark_start() { return editorInfo.epmarkstart; }
int pattern_mark_end() { return editorInfo.epmarkend; }

int pattern_octave() { return editorInfo.epoctave; }

void pattern_set_octave(int v) {
    if (v < kPatternOctaveMin) v = kPatternOctaveMin;
    if (v > kPatternOctaveMax) v = kPatternOctaveMax;
    editorInfo.epoctave = v;
}

bool pattern_jam_mode() { return !recordmode; }

bool pattern_record_mode() { return recordmode != 0; }

int pattern_autoadvance() { return autoadvance; }

const char* pattern_autoadvance_label() {
    switch (autoadvance) {
    case 0: return "all";
    case 1: return "note";
    default: return "off";
    }
}

void pattern_cycle_autoadvance() { autoadvance = (autoadvance + 1) % 3; }

void pattern_set_step(int v) {
    if (v < kPatternStepMin) v = kPatternStepMin;
    if (v > kPatternStepMax) v = kPatternStepMax;
    stepsize = v;
}

void pattern_toggle_record_mode() { recordmode = !recordmode; }

// ---- order list ----

int order_channels() { return pattern_channels(); } // same 3/6 rule as patterns
int order_subtune() { return editorInfo.esnum; }
int order_actual_channel(int ch) { return getActualChannel(editorInfo.esnum, ch); }
int order_cursor_row() { return editorInfo.eseditpos; }
int order_cursor_chn() { return editorInfo.eschn; }
int order_cursor_col() { return editorInfo.escolumn; }
int order_mark_chn() { return editorInfo.esmarkchn; }
int order_mark_chn_end() { return editorInfo.esmarkchnend; }
int order_mark_start() { return editorInfo.esmarkstart; }
int order_mark_end() { return editorInfo.esmarkend; }

bool order_expanded_view() { return editorInfo.expandOrderListView; }

bool order_toggle_expanded_view() {
    GTOBJECT* gt = &gtObject;
    if (editorInfo.expandOrderListView && validateAllSongs() > 0xff) return false;

    const int jc2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);
    stopsong(gt);
    resetSongInfo(gt, jc2);
    editorInfo.expandOrderListView = !editorInfo.expandOrderListView;
    if (editorInfo.expandOrderListView) expandAllSongs();
    else compressAllSongs();

    editorInfo.esnum = 1;
    songchange(gt, true);
    editorInfo.esnum = 0;
    songchange(gt, true);
    return true;
}

int order_compressed_size(int ch) {
    if (ch < 0 || ch >= MAX_CHN) return 0;
    return (int)songCompressedSize[editorInfo.esnum][ch];
}

int order_compressed_payload_size(int ch) {
    if (ch < 0 || ch >= MAX_CHN) return 0;
    if (order_expanded_view()) {
        const int total = (int)songCompressedSize[editorInfo.esnum][ch];
        if (total > 0xff) return total;
        return (total >= 2) ? total - 2 : 0;
    }
    return (int)songlen[editorInfo.esnum][ch];
}

bool order_is_master_channel(int display_ch) {
    if (display_ch < 0 || display_ch >= MAX_CHN) return false;
    return getActualChannel(editorInfo.esnum, display_ch) == gtObject.masterLoopChannel;
}

int order_selected_row(int ch) {
    if (ch < 0 || ch >= MAX_CHN) return -1;
    int c2  = getActualChannel(editorInfo.esnum, ch);
    int pos = gtObject.editorUndoInfo.editorInfo[c2].espos;
    return pos >= 0 ? pos : -1;
}

int order_range_end_row(int ch) {
    if (ch < 0 || ch >= MAX_CHN) return -1;
    int c2  = getActualChannel(editorInfo.esnum, ch);
    int end = gtObject.editorUndoInfo.editorInfo[c2].esend;
    return end ? end : -1;
}

int order_play_row(int ch) {
    if (!isplaying(&gtObject)) return -1;
    int c2          = getActualChannel(editorInfo.esnum, ch);
    int playingSong = getActualSongNumber(editorInfo.esnum, c2);
    if (editorInfo.esnum != playingSong) return -1;
    if (!gtObject.chn[c2].advance) return -1;
    int pos = gtObject.chn[c2].songptr - 1;
    return pos < 0 ? 0 : pos;
}

int order_length(int ch) {
    if (ch < 0 || ch >= MAX_CHN) return 0;
    if (order_expanded_view()) return (int)songOrderLength[editorInfo.esnum][ch];
    return songlen[editorInfo.esnum][ch];
}

int order_rows() {
    const int chans = order_channels();
    if (order_expanded_view()) {
        int       maxlen = 0;
        const int sn     = editorInfo.esnum;
        for (int c = 0; c < chans; c++) {
            const int len = (int)songOrderLength[sn][c];
            if (len > maxlen) maxlen = len;
        }
        if (maxlen < 16) maxlen = 16;
        if (maxlen > MAX_SONGLEN_EXPANDED) maxlen = MAX_SONGLEN_EXPANDED;
        return maxlen;
    }

    int maxlen = 0;
    for (int c = 0; c < chans; c++)
        if (order_length(c) > maxlen) maxlen = order_length(c);
    if (maxlen > MAX_SONGLEN) maxlen = MAX_SONGLEN;
    return maxlen + 2; // include the RST + loop-position rows
}

OrderCell order_cell(int ch, int row) {
    OrderCell c{};
    if (ch < 0 || ch >= MAX_CHN) return c;

    const int sn = editorInfo.esnum;
    if (order_expanded_view()) {
        if (row < 0 || row >= MAX_SONGLEN_EXPANDED) return c;

        c.valid       = true;
        const int len = (int)songOrderLength[sn][ch];
        c.muted       = row >= len;

        const int pattern   = songOrderPatterns[sn][ch][row];
        const int transpose = songOrderTranspose[sn][ch][row];
        snprintf(c.text, sizeof c.text, "%02X", pattern & 0xff);

        if (pattern == 0xff) {
            snprintf(c.trans, sizeof c.trans, "%03X", transpose & 0xfff);
            c.kind = 4;
            return c;
        }

        const int tv = transpose & 0x7f;
        if (transpose & 0x80) snprintf(c.trans, sizeof c.trans, "-%01X", tv);
        else snprintf(c.trans, sizeof c.trans, "+%01X", tv);
        c.kind = 1;
        return c;
    }

    int len = songlen[sn][ch];
    if (row < 0 || row > len + 1 || row > MAX_SONGLEN + 1) return c;

    c.valid = true;
    int v   = songorder[sn][ch][row];
    if (v == LOOPSONG) {
        c.text[0] = '=';
        c.text[1] = '=';
        c.text[2] = 0;
        c.kind    = 3;
        return c;
    }
    if (v < REPEAT || row >= len) {
        snprintf(c.text, sizeof c.text, "%02X", v);
        c.kind = 1;
        return c;
    }
    if (v >= TRANSUP) snprintf(c.text, sizeof c.text, "+%X", v & 0xf);
    else if (v >= TRANSDOWN) snprintf(c.text, sizeof c.text, "-%X", 16 - (v & 0xf));
    else snprintf(c.text, sizeof c.text, "R%X", (v + 1) & 0xf);
    c.kind = 2;
    return c;
}

void order_set_cursor(int ch, int row, int col) {
    const int chans = order_channels();
    if (ch < 0) ch = 0;
    if (ch >= chans) ch = chans - 1;

    if (order_expanded_view()) {
        if (row < 0) row = 0;
        if (row >= MAX_SONGLEN_EXPANDED) row = MAX_SONGLEN_EXPANDED - 1;
        if (col < 0) col = 0;
        if (col > 4) col = 4;

        editorInfo.editmode  = EditMode::OrderList;
        editorInfo.eschn     = ch;
        editorInfo.eseditpos = row;
        editorInfo.escolumn  = col;
        setMasterLoopChannel(&gtObject, "guimodel_order_set_cursor_exp");
        return;
    }

    int len = order_length(ch);
    if (row < 0) row = 0;
    if (row > len + 1) row = len + 1;
    if (row == len) {
        row = len + 1;
        col = 0;
    }
    if (col < 0) col = 0;
    if (col > 1) col = 1;

    editorInfo.editmode  = EditMode::OrderList;
    editorInfo.eschn     = ch;
    editorInfo.eseditpos = row;
    editorInfo.escolumn  = col;
}

// Expanded rows use a spacer column between pattern (0..1) and transpose (3..4).
// Plain left-clicks on that gap are ignored; modifier clicks snap to a real field.
static bool order_expanded_gap_blocks_click(int ch, int row, int col, bool modifier) {
    if (!order_expanded_view() || col != 2) return false;
    if (songOrderPatterns[editorInfo.esnum][ch][row] >= 0xff) return false;
    return !modifier;
}

static int order_expanded_snap_column(int ch, int row, int col) {
    if (!order_expanded_view() || col != 2) return col;
    if (songOrderPatterns[editorInfo.esnum][ch][row] >= 0xff) return col;
    return 1; // default to low pattern nibble when landing in the gap
}

void order_mouse_left(int ch, int row, int col, bool shift_or_ctrl, bool held_drag) {

    col = order_expanded_snap_column(ch, row, col);
    if (order_expanded_gap_blocks_click(ch, row, col, shift_or_ctrl || held_drag)) return;

    if (shift_or_ctrl || held_drag) {
        order_set_cursor(ch, row, col);
        setMasterLoopChannel(&gtObject, "guimodel_order_drag");
        order_select_patterns(&gtObject);
    }
    else {
        order_set_cursor(ch, row, col);
        setMasterLoopChannel(&gtObject, "guimodel_order_click");
    }
}

void order_mouse_double_click(int ch, int row, int col) {

    col = order_expanded_snap_column(ch, row, col);
    if (order_expanded_gap_blocks_click(ch, row, col, false)) return;

    order_set_cursor(ch, row, col);
    setMasterLoopChannel(&gtObject, "guimodel_order_dblclk");
    orderPlayFromPosition(&gtObject, 0, editorInfo.eseditpos, editorInfo.eschn, true);
}

void order_mouse_mark_begin(int ch, int row) {
    editorInfo.editmode = EditMode::OrderList;

    if (order_expanded_view()) {
        if (row >= MAX_SONGLEN_EXPANDED) return;
    }
    else if (row >= order_length(ch)) {
        return;
    }

    if (editorInfo.esmarkchn != ch || row != editorInfo.esmarkend) {
        editorInfo.esmarkchn    = ch;
        editorInfo.esmarkstart  = row;
        editorInfo.esmarkend    = row;
        editorInfo.esmarkchnend = order_expanded_view() ? ch : -1;
    }
}

void order_mouse_mark_drag(int ch, int row) {
    if (editorInfo.esmarkchn < 0) return;

    if (order_expanded_view()) {
        if (row >= MAX_SONGLEN_EXPANDED) return;
        editorInfo.esmarkend    = row;
        editorInfo.esmarkchnend = ch;
    }
    else {
        if (row >= order_length(editorInfo.esmarkchn)) return;
        editorInfo.esmarkend = row;
    }
}

void order_mouse_mark_cancel() {
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
}

int order_song_bank() { return currentSongFile; }

int order_song_bank_count() { return lastValidSongFileIndex + 1; }

static void order_song_bank_switch(int next) {
    GTOBJECT* gt = &gtObject;
    stopsong(gt);
    undoCreateEditorInfoBackup();
    copyCurrentToSngBuffer(gt, currentSongFile);
    currentSongFile = next;
    copySngBufferToCurrent(gt, currentSongFile);
    undoInvalidateUndoAreas();
    editorInfo.currentSongFile = currentSongFile;
    undoAddEditorSettingsToList();
}

void order_song_bank_next() {
    if (currentSongFile >= lastValidSongFileIndex) return;
    order_song_bank_switch(currentSongFile + 1);
}

void order_song_bank_prev() {
    if (currentSongFile <= 0) return;
    order_song_bank_switch(currentSongFile - 1);
}

void order_set_subtune(int v) {
    if (v < kOrderSubtuneMin) v = kOrderSubtuneMin;
    if (v > kOrderSubtuneMax) v = kOrderSubtuneMax;
    if (editorInfo.esnum == v) return;
    editorInfo.esnum = v;
    songchange(&gtObject, true);
}

void order_set_song_bank(int bank) {
    if (bank < 0) bank = 0;
    if (bank > lastValidSongFileIndex) bank = lastValidSongFileIndex;
    if (bank == currentSongFile) return;
    order_song_bank_switch(bank);
}

// ---- instruments ----

static bool instr_ok(int i) { return i >= 0 && i < MAX_INSTR; }
static bool instr_editable(int i) { return i >= INSTR_FIRST && i < MAX_INSTR; }

int instr_count() { return MAX_INSTR; }
int instr_rows() { return INSTR_GRID_ROWS; }

int instr_current() { return editorInfo.einum; }

int instr_cursor_field() {
    if (editorInfo.eipos >= LAST_INST) return INSTR_FIELD_NAME;
    if (editorInfo.eipos >= 0 && editorInfo.eipos < INSTR_FIELDS) return editorInfo.eipos;
    return 0;
}

int instr_cursor_nibble() {
    if (editorInfo.eicolumn < 0) return 0;
    if (editorInfo.eicolumn > 1) return 1;
    return editorInfo.eicolumn;
}

bool instr_cursor_on_name() { return editorInfo.eipos >= LAST_INST; }

void instr_clamp_selection() {
    if (editorInfo.einum < INSTR_FIRST) editorInfo.einum = INSTR_FIRST;
    if (editorInfo.einum >= MAX_INSTR) editorInfo.einum = MAX_INSTR - 1;
}

void instr_set_cursor(int inst, int field, int nibble) {
    if (!instr_editable(inst)) return;
    editorInfo.editmode = EditMode::Instrument;
    editorInfo.einum    = inst;
    if (field == INSTR_FIELD_NAME) {
        editorInfo.eipos    = LAST_INST;
        editorInfo.eicolumn = 0;
        return;
    }
    if (field < 0 || field >= INSTR_FIELDS) return;
    editorInfo.eipos = field;
    if (nibble < 0) nibble = 0;
    if (nibble > 1) nibble = 1;
    editorInfo.eicolumn = nibble;
}

const char* instr_name(int i) { return instr_ok(i) ? instr[i].name : ""; }
int         instr_ad(int i) { return instr_ok(i) ? instr[i].ad : 0; }
int         instr_sr(int i) { return instr_ok(i) ? instr[i].sr : 0; }
int         instr_ptr(int i, int which) {
    if (!instr_ok(i) || which < 0 || which >= MAX_TABLES) return 0;
    return instr[i].ptr[which];
}
int instr_vibdelay(int i) { return instr_ok(i) ? instr[i].vibdelay : 0; }
int instr_gatetimer(int i) { return instr_ok(i) ? instr[i].gatetimer : 0; }
int instr_firstwave(int i) { return instr_ok(i) ? instr[i].firstwave : 0; }
int instr_pan(int i) { return instr_ok(i) ? instr[i].pan : 0; }

void instr_select(int i) {
    if (!instr_editable(i)) return;
    editorInfo.editmode = EditMode::Instrument;
    editorInfo.einum    = i;
}

static unsigned char* instr_field_ptr(int i, int field) {
    INSTR& in = instr[i];
    switch (field) {
    case 0: return &in.ad;
    case 1: return &in.sr;
    case 2: return &in.ptr[0]; // WTBL
    case 3: return &in.ptr[1]; // PTBL
    case 4: return &in.ptr[2]; // FTBL
    case 5: return &in.ptr[3]; // STBL (vibrato)
    case 6: return &in.vibdelay;
    case 7: return &in.gatetimer;
    case 8: return &in.firstwave;
    case 9: return &in.pan;
    default: return 0;
    }
}

// Bracket a mutation of instrument i in the legacy undo system (same areas the
// legacy instrument editor marks). The mutation happens in `apply`.
template <class Apply> static void instr_edit(int i, Apply apply) {
    GTUNDO_OBJECT* ed = undoCreateEditorInfo();
    undoAreaSetCheckForChange(UNDO_AREA_INSTRUMENTS, i, UNDO_AREA_DIRTY_CHECK);
    apply();
    if (undoValidateUndoAreas(ed) == 0) undoFreeUndoObject(ed);
}

static_assert(INSTR_NAME_MAX == MAX_INSTRNAMELEN, "instrument name length mismatch");

int instr_field(int i, int field) {
    if (!instr_ok(i)) return 0;
    unsigned char* p = instr_field_ptr(i, field);
    return p ? *p : 0;
}

void instr_set_field(int i, int field, unsigned value) {
    if (!instr_ok(i)) return;
    unsigned char* p = instr_field_ptr(i, field);
    if (!p) return;
    unsigned char v = (unsigned char)(value & 0xff);
    if (*p == v) return;
    instr_edit(i, [&] { *p = v; });
}

void instr_set_name(int i, const char* name) {
    if (!instr_ok(i) || !name) return;
    char clean[MAX_INSTRNAMELEN];
    strncpy(clean, name, MAX_INSTRNAMELEN); // truncate/pad to the fixed field width
    if (memcmp(instr[i].name, clean, MAX_INSTRNAMELEN) == 0) return;
    instr_edit(i, [&] { memcpy(instr[i].name, clean, MAX_INSTRNAMELEN); });
}

// ---- song info + transport ----

static_assert(SONG_STR_MAX == MAX_STR - 1, "song string length mismatch");

const char* song_name() { return songname; }
const char* song_author() { return authorname; }
const char* song_copyright() { return copyrightname; }

static void song_set_str(char* dst, const char* s) {
    if (!s) return;
    strncpy(dst, s, MAX_STR - 1);
    dst[MAX_STR - 1] = 0;
}
void song_set_name(const char* s) { song_set_str(songname, s); }
void song_set_author(const char* s) { song_set_str(authorname, s); }
void song_set_copyright(const char* s) { song_set_str(copyrightname, s); }

void transport_play_start() { gtaction::perform(gtaction::Action::PlayFromBeginning); }

void transport_toggle_play() {
    if (isplaying(&gtObject)) stopsong(&gtObject);
    else playFromCurrentPosition(&gtObject, editorInfo.eppos);
}

void transport_play_pattern() { gtaction::perform(gtaction::Action::PlayPatternMode); }
void transport_stop() { gtaction::perform(gtaction::Action::Stop); }
bool transport_playing() { return isplaying(&gtObject); }
int  transport_time_min() { return gtObject.timemin; }
int  transport_time_sec() { return gtObject.timesec; }
int  transport_total_min() { return gtEditorObject.totalMin; }
int  transport_total_sec() { return gtEditorObject.totalSec; }

float transport_volume() { return masterVolume; }

void transport_set_volume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > kMasterVolumeMax) v = kMasterVolumeMax;
    masterVolume = v;
}

const char* transport_stereo_label() {
    if (monomode || (editorInfo.maxSIDChannels == 3 && stereoMode == 1)) return "MON";
    if (stereoMode == 1) return "STE";
    return "PAN";
}

void transport_cycle_stereo() { gtaction::perform(gtaction::Action::CycleStereoMode); }

bool transport_follow() { return followplay != 0; }
void transport_toggle_follow() { gtaction::perform(gtaction::Action::ToggleFollow); }
bool transport_loop() { return transportLoopPattern != 0; }
void transport_toggle_loop() { gtaction::perform(gtaction::Action::ToggleLoop); }
void transport_ff() { gtaction::perform(gtaction::Action::SongPosNext); }
void transport_rewind() { gtaction::perform(gtaction::Action::SongPosPrev); }

// ---- player / chip settings ----

template <class Apply> static void player_settings_edit(Apply apply) {
    undoCreateEditorInfoBackup();
    apply();
    undoAddEditorSettingsToList();
}

const char* player_loaded_filename() {
    if (!loadedsongfilename[0]) return "(unsaved)";
    const char* base = strrchr(loadedsongfilename, '/');
    if (!base) base = strrchr(loadedsongfilename, '\\');
    return base ? base + 1 : loadedsongfilename;
}

int player_sid_chips() { return editorInfo.maxSIDChannels / 3; }

int player_sid_chip_combo_items() { return 4; }

int player_sid_chip_combo_index() {
    switch (editorInfo.maxSIDChannels) {
    case 3: return 0;
    case 6: return 1;
    case 9: return 2;
    default: return 3; // 12
    }
}

void player_sid_chip_combo_label(int index, char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return;
    if (index < 0) index = 0;
    if (index > 3) index = 3;
    snprintf(buf, (size_t)bufSize, "SID x%d", index + 1);
}

bool player_set_sid_chip_combo_index(int index) {
    if (index < 0) index = 0;
    if (index > 3) index = 3;
    static const int kChannels[] = { 3, 6, 9, 12 };
    const int        newCh       = kChannels[index];
    if (newCh == editorInfo.maxSIDChannels) return false;
    undoCreateEditorInfoBackup();
    editorInfo.maxSIDChannels = newCh;
    undoAddEditorSettingsToList();
    handleSIDChannelCountChange(&gtObject);
    return true;
}

bool player_sid_model_8580() { return editorInfo.sidmodel != 0; }
bool player_ntsc() { return editorInfo.ntsc != 0; }

const char* player_speed_label() {
    static char buf[8];
    if (editorInfo.multiplier == 0) snprintf(buf, sizeof buf, "25Hz");
    else snprintf(buf, sizeof buf, "%dX", (int)editorInfo.multiplier);
    return buf;
}

int player_speed_combo_items() { return 17; } // 0 = 25Hz, 1..16 = 1X..16X

int player_speed_combo_index() { return (int)editorInfo.multiplier; }

void player_speed_combo_label(int index, char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return;
    if (index < 0) index = 0;
    if (index > 16) index = 16;
    if (index == 0) snprintf(buf, (size_t)bufSize, "25Hz");
    else snprintf(buf, (size_t)bufSize, "%dX", index);
}

bool player_set_speed_combo_index(int index) {
    if (index < 0) index = 0;
    if (index > 16) index = 16;
    if ((int)editorInfo.multiplier == index) return false;
    player_settings_edit([index] {
        editorInfo.multiplier = (unsigned)index;
        if ((editorInfo.finevibrato == 1) && (editorInfo.multiplier < 2)) editorInfo.usefinevib = 1;
        reInitSID();
        playUntilEnd(editorInfo.esnum);
    });
    return true;
}

int player_hr_adparam() { return (int)editorInfo.adparam; }

void player_set_hr_adparam(int v) {
    v &= 0xffff;
    if ((int)editorInfo.adparam == v) return;
    player_settings_edit([v] { editorInfo.adparam = (unsigned)v; });
}

int player_sid_pan(int chip) {
    int sidChips = editorInfo.maxSIDChannels / 3;
    if (chip < 0 || chip >= sidChips) return 0;
    return SID_StereoPanPositions[sidChips - 1][chip];
}

void player_set_sid_pan(int chip, int pan) {
    int sidChips = editorInfo.maxSIDChannels / 3;
    if (chip < 0 || chip >= sidChips) return;
    pan &= 0xf;
    if (SID_StereoPanPositions[sidChips - 1][chip] == pan) return;
    player_settings_edit([sidChips, chip, pan] {
        SID_StereoPanPositions[sidChips - 1][chip] = pan;
        convertPansToInts(sidChips);
    });
}

const char* player_pan_summary() {
    static char buf[16];
    unsigned    v        = 0;
    int         sidChips = editorInfo.maxSIDChannels / 3;
    for (int i = 0; i < sidChips; i++) {
        v <<= 4;
        v |= SID_StereoPanPositions[sidChips - 1][i];
    }
    if (sidChips == 1) snprintf(buf, sizeof buf, "P1:%01X", v);
    else if (sidChips == 2) snprintf(buf, sizeof buf, "P2:%02X", v);
    else if (sidChips == 3) snprintf(buf, sizeof buf, "P3:%03X", v);
    else snprintf(buf, sizeof buf, "P4:%04X", v);
    return buf;
}

bool player_fine_vibrato() { return editorInfo.finevibrato != 0; }
bool player_optimize_pulse() { return editorInfo.optimizepulse != 0; }
bool player_optimize_realtime() { return editorInfo.optimizerealtime != 0; }
bool player_sidtracker64() { return SIDTracker64ForIPadIsAmazing != 0; }

void player_toggle_fine_vibrato() {
    player_settings_edit([] {
        editorInfo.finevibrato = 1 - editorInfo.finevibrato;
        if ((editorInfo.finevibrato == 1) && (editorInfo.multiplier < 2)) editorInfo.usefinevib = 1;
        if (editorInfo.finevibrato > 1) editorInfo.usefinevib = 1;
    });
}

void player_toggle_optimize_pulse() {
    player_settings_edit([] { editorInfo.optimizepulse ^= 1; });
}

void player_toggle_optimize_realtime() {
    player_settings_edit([] { editorInfo.optimizerealtime ^= 1; });
}

void player_toggle_ntsc() {
    player_settings_edit([] {
        editorInfo.ntsc ^= 1;
        reInitSID();
    });
}

void player_toggle_sid_model() { gtaction::perform(gtaction::Action::ToggleSidModel); }

void player_toggle_sidtracker64() { gtaction::perform(gtaction::Action::ToggleSIDTracker64); }

void player_multiplier_prev() {
    player_settings_edit([] { prevmultiplier(); });
}

void player_multiplier_next() {
    player_settings_edit([] { nextmultiplier(); });
}

namespace {

// Transient status message ("" = none). Set by editor actions (save, export,
// mode toggles) via set_status(); shown by context_help() in place of the
// cursor-cell description until the user navigates away.
std::string g_help;          // cursor-cell description, rebuilt each refresh
std::string g_status;
long long   g_status_sig = 0; // cursor signature captured when the status was set

// Small hash of the edit cursor. When it changes, the user has navigated, so
// the transient status is dropped and normal context help resumes. This
// replaces the legacy forceInfoLine skip-counter that lived inside ginfo.
long long cursor_signature() {
    long long h    = static_cast<int>(editorInfo.editmode);
    auto      mix  = [&](int v) { h = h * 131 + v; };
    mix(editorInfo.eppos);
    mix(editorInfo.epchn);
    mix(editorInfo.esnum);
    mix(editorInfo.eseditpos);
    mix(editorInfo.eschn);
    mix(editorInfo.einum);
    mix(editorInfo.eipos);
    mix(editorInfo.etnum);
    mix(editorInfo.etpos);
    mix(editorInfo.etcolumn);
    return h;
}

} // namespace

void set_status(const char* fmt, ...) {
    char    buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_status     = buf;
    g_status_sig = cursor_signature();
}

void context_help_refresh() {
    // While the poly keyboard is being played, the info line shows the live
    // note offsets (keyOffsetText) instead of cursor help or status.
    if (checkAnyPolyPlaying()) {
        calculateNoteOffsets();
        return;
    }

    if (!g_status.empty() && cursor_signature() != g_status_sig) g_status.clear();

    if (editorInfo.editmode == EditMode::Names) g_help = "Song metadata (name, author, copyright)";
    else g_help = ginfo::describe(gtObject);
}

const char* context_help() {
    if (checkAnyPolyPlaying()) return keyOffsetText;
    if (!g_status.empty()) return g_status.c_str();
    return g_help.c_str();
}

void pattern_set_cursor(int ch, int row, int col) {
    int chans = pattern_channels();
    if (ch < 0) ch = 0;
    if (ch >= chans) ch = chans - 1;

    int len = pattern_length(ch);
    if (row < 0) row = 0;
    if (row > len) row = len; // legacy allows the cursor on the PATT.END row

    if (col < 0) col = 0;
    if (col > 5) col = 5;

    editorInfo.editmode = EditMode::Pattern;
    editorInfo.epchn    = ch;
    editorInfo.eppos    = row;
    editorInfo.epcolumn = col;
    // Keep the master-loop / mark channel in sync, as the legacy click does, so
    // play-from-here and Shift-select act on the clicked channel.
    setMasterLoopChannel(&gtObject, "imgui");
}

// ---- MIDI input ----

int midi_port_count() { return (int)getPortCount(); }

bool midi_input_enabled() { return midiEnabled != 0; }

int midi_combo_items() {
    const int ports = midi_port_count();
    return 1 + (ports > 0 ? ports : 0);
}

int midi_combo_index() {
    if (!midi_input_enabled() || selectedMIDIPort == MIDI_PORT_DISABLED) return 0;
    return selectedMIDIPort + 1;
}

void midi_combo_label(int index, char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return;

    if (index <= 0) {
        if (midi_port_count() == 0) snprintf(buf, (size_t)bufSize, "Off (no ports)");
        else snprintf(buf, (size_t)bufSize, "Off");
        return;
    }

    const int port = index - 1;
    char      name[96];
    if (getMidiPortNameInto(port, name, (int)sizeof name)) snprintf(buf, (size_t)bufSize, "%d: %s", port, name);
    else snprintf(buf, (size_t)bufSize, "%d", port);
}

bool midi_set_combo_index(int index) {
    if (index <= 0) {
        setMidiPort(MIDI_PORT_DISABLED);
        selectedMIDIPort = MIDI_PORT_DISABLED;
        midiEnabled      = false;
        return true;
    }

    const int port   = index - 1;
    const int opened = setMidiPort(port);
    if (opened == MIDI_PORT_DISABLED) {
        selectedMIDIPort = MIDI_PORT_DISABLED;
        midiEnabled      = false;
        return false;
    }

    selectedMIDIPort = opened;
    midiEnabled      = true;
    return true;
}

} // namespace gtui
