//
// guimodel - implementation of the SDL-free model bridge (see guimodel.h).
// This TU is free to pull in the full legacy headers.
//
#include "guimodel.h"
#include "goattrk2.h"

namespace gtui {

static const char *kTableNames[MAX_TABLES] = {
    "WAVE TBL", "PULSETBL", "FILT.TBL", "SPEEDTBL"
};

int table_count() { return MAX_TABLES; }
int table_len() { return MAX_TABLELEN; }
int table_visible_rows() { return VISIBLETABLEROWS; }

const char *table_name(int t)
{
    return (t >= 0 && t < MAX_TABLES) ? kTableNames[t] : "";
}

int table_view(int t)
{
    return (t >= 0 && t < MAX_TABLES) ? editorInfo.etview[t] : 0;
}

int table_cursor_table() { return editorInfo.etnum; }
int table_cursor_pos() { return editorInfo.etpos; }
int table_cursor_col() { return editorInfo.etcolumn; }
int table_mark_table() { return editorInfo.etmarknum; }
int table_mark_start() { return editorInfo.etmarkstart; }
int table_mark_end() { return editorInfo.etmarkend; }

void table_set_cursor(int t, int row, int col)
{
    if (t < 0) t = 0;
    if (t >= MAX_TABLES) t = MAX_TABLES - 1;
    if (row < 0) row = 0;
    if (row >= MAX_TABLELEN) row = MAX_TABLELEN - 1;
    if (col < 0) col = 0;
    if (col > 3) col = 3;

    editorInfo.editmode = EDIT_TABLES;
    editorInfo.editTableMode = t + 1; // EDIT_TABLE_WAVE..SPEED
    editorInfo.etnum = t;
    editorInfo.etpos = row;
    editorInfo.etcolumn = col;
}

unsigned table_left(int t, int row)
{
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return 0;
    return ltable[t][row];
}

unsigned table_right(int t, int row)
{
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return 0;
    return rtable[t][row];
}

void table_set(int t, int row, int col, unsigned value)
{
    if (t < 0 || t >= MAX_TABLES || row < 0 || row >= MAX_TABLELEN) return;

    unsigned char v = (unsigned char)(value & 0xff);
    unsigned char *cell = (col == 0) ? &ltable[t][row] : &rtable[t][row];
    if (*cell == v) return; // nothing changed -> no undo entry

    // Same bracket the legacy editor uses for table edits (see docommand):
    // snapshot editor + the left/right table areas, mutate, then let the undo
    // system record the diff (or discard the object if nothing changed).
    GTUNDO_OBJECT *ed = undoCreateEditorInfo();
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + t, 0, UNDO_AREA_DIRTY_CHECK);
    undoAreaSetCheckForChange(UNDO_AREA_TABLES + t, 1, UNDO_AREA_DIRTY_CHECK);

    *cell = v;

    if (undoValidateUndoAreas(ed) == 0)
        undoFreeUndoObject(ed);
}

// ---- pattern editor ----

static int pattern_num_for(int ch)
{
    int c2 = getActualChannel(editorInfo.esnum, ch);
    return gtObject.editorUndoInfo.editorInfo[c2].epnum;
}

int pattern_channels()
{
    // Mirror displayPattern6Chn: 3 channels for a 3-SID subtune, else 6.
    if ((editorInfo.esnum & 1 && editorInfo.maxSIDChannels == 9) ||
        editorInfo.maxSIDChannels == 3)
        return 3;
    return MAX_CHN;
}

int pattern_actual_channel(int ch) { return getActualChannel(editorInfo.esnum, ch); }

int pattern_length(int ch)
{
    int pnum = pattern_num_for(ch);
    if (pnum < 0 || pnum >= MAX_PATT) return 0;
    return pattlen[pnum];
}

int pattern_rows()
{
    int maxlen = 0;
    int chans = pattern_channels();
    for (int c = 0; c < chans; c++)
    {
        int len = pattern_length(c);
        if (len > maxlen) maxlen = len;
    }
    if (maxlen > MAX_PATTROWS) maxlen = MAX_PATTROWS;
    return maxlen + 1; // include the PATT.END row
}

// Currently-playing row for display channel ch, or -1 when not playing or the
// channel is playing a different pattern than the one shown. Mirrors the legacy
// pattern-view playhead (gdisplay.cpp): row = lastpattptr/4, clamped to length.
int pattern_play_row(int ch)
{
    if (!isplaying(&gtObject)) return -1;
    int c2 = getActualChannel(editorInfo.esnum, ch);
    if (gtObject.editorUndoInfo.editorInfo[c2].epnum != gtObject.chn[c2].lastpattnum)
        return -1;
    int chnrow = gtObject.chn[c2].lastpattptr / 4;
    int pnum = gtObject.chn[c2].lastpattnum;
    if (pnum >= 0 && pnum < MAX_PATT && chnrow > pattlen[pnum])
        chnrow = pattlen[pnum];
    return chnrow;
}

int pattern_step() { return stepsize; }
int pattern_cursor_row() { return editorInfo.eppos; }
int pattern_cursor_chn() { return editorInfo.epchn; }
int pattern_cursor_col() { return editorInfo.epcolumn; }
int pattern_number(int ch) { return pattern_num_for(ch); }

PatCell pattern_cell(int ch, int row)
{
    PatCell c;
    c.note = "";
    c.instr = 0;
    c.cmd = 0;
    c.data = 0;
    c.end = false;
    c.valid = false;

    int pnum = pattern_num_for(ch);
    if (pnum < 0 || pnum >= MAX_PATT) return c;
    if (row < 0 || row > pattlen[pnum]) return c;

    const unsigned char *cell = &pattern[pnum][row * 4];
    c.valid = true;
    if (cell[0] == ENDPATT)
    {
        c.end = true;
        return c;
    }

    int n = (int)cell[0] - FIRSTNOTE;
    if (n < 0 || n >= 12 * 8) n = 12 * 8 - 3; // clamp to "..." on odd data
    c.note = notename[n];
    c.instr = cell[1];
    c.cmd = cell[2];
    c.data = cell[3];
    return c;
}

int pattern_mark_channel() { return editorInfo.epmarkchn; }
int pattern_mark_start() { return editorInfo.epmarkstart; }
int pattern_mark_end() { return editorInfo.epmarkend; }

// ---- order list ----

int order_channels() { return pattern_channels(); } // same 3/6 rule as patterns
int order_actual_channel(int ch) { return getActualChannel(editorInfo.esnum, ch); }
int order_cursor_row() { return editorInfo.eseditpos; }
int order_cursor_chn() { return editorInfo.eschn; }
int order_cursor_col() { return editorInfo.escolumn; }
int order_mark_chn() { return editorInfo.esmarkchn; }
int order_mark_start() { return editorInfo.esmarkstart; }
int order_mark_end() { return editorInfo.esmarkend; }

int order_length(int ch)
{
    if (ch < 0 || ch >= MAX_CHN) return 0;
    return songlen[editorInfo.esnum][ch];
}

int order_rows()
{
    int maxlen = 0;
    int chans = order_channels();
    for (int c = 0; c < chans; c++)
        if (order_length(c) > maxlen) maxlen = order_length(c);
    if (maxlen > MAX_SONGLEN) maxlen = MAX_SONGLEN;
    return maxlen + 2; // include the RST + loop-position rows
}

OrderCell order_cell(int ch, int row)
{
    OrderCell c;
    c.text[0] = ' '; c.text[1] = ' '; c.text[2] = ' '; c.text[3] = 0;
    c.kind = 0;
    c.valid = false;
    if (ch < 0 || ch >= MAX_CHN) return c;

    int sn = editorInfo.esnum;
    int len = songlen[sn][ch];
    if (row < 0 || row > len + 1 || row > MAX_SONGLEN + 1) return c;

    c.valid = true;
    int v = songorder[sn][ch][row];
    if (v == LOOPSONG)
    {
        c.text[0] = 'R'; c.text[1] = 'S'; c.text[2] = 'T';
        c.kind = 3;
        return c;
    }
    if (v < REPEAT || row >= len) // pattern number (or a raw value past the end)
    {
        snprintf(c.text, sizeof c.text, "%02X ", v);
        c.kind = 1;
        return c;
    }
    // Command
    if (v >= TRANSUP)        snprintf(c.text, sizeof c.text, "+%X ", v & 0xf);
    else if (v >= TRANSDOWN) snprintf(c.text, sizeof c.text, "-%X ", 16 - (v & 0xf));
    else                     snprintf(c.text, sizeof c.text, "R%X ", (v + 1) & 0xf);
    c.kind = 2;
    return c;
}

void order_set_cursor(int ch, int row, int col)
{
    int chans = order_channels();
    if (ch < 0) ch = 0;
    if (ch >= chans) ch = chans - 1;
    int len = order_length(ch);
    if (row < 0) row = 0;
    if (row > len + 1) row = len + 1;
    if (col < 0) col = 0;
    if (col > 2) col = 2;

    editorInfo.editmode = EDIT_ORDERLIST;
    editorInfo.eschn = ch;
    editorInfo.eseditpos = row;
    editorInfo.escolumn = col;
}

// ---- instruments ----

static bool instr_ok(int i) { return i >= 0 && i < MAX_INSTR; }

int instr_count() { return MAX_INSTR; }
int instr_current() { return editorInfo.einum; }
const char *instr_name(int i) { return instr_ok(i) ? instr[i].name : ""; }
int instr_ad(int i) { return instr_ok(i) ? instr[i].ad : 0; }
int instr_sr(int i) { return instr_ok(i) ? instr[i].sr : 0; }
int instr_ptr(int i, int which)
{
    if (!instr_ok(i) || which < 0 || which >= MAX_TABLES) return 0;
    return instr[i].ptr[which];
}
int instr_vibdelay(int i) { return instr_ok(i) ? instr[i].vibdelay : 0; }
int instr_gatetimer(int i) { return instr_ok(i) ? instr[i].gatetimer : 0; }
int instr_firstwave(int i) { return instr_ok(i) ? instr[i].firstwave : 0; }
int instr_pan(int i) { return instr_ok(i) ? instr[i].pan : 0; }

void instr_select(int i)
{
    if (!instr_ok(i)) return;
    editorInfo.editmode = EDIT_INSTRUMENT;
    editorInfo.einum = i;
}

static unsigned char *instr_field_ptr(int i, int field)
{
    INSTR &in = instr[i];
    switch (field)
    {
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
template <class Apply>
static void instr_edit(int i, Apply apply)
{
    GTUNDO_OBJECT *ed = undoCreateEditorInfo();
    undoAreaSetCheckForChange(UNDO_AREA_INSTRUMENTS, i, UNDO_AREA_DIRTY_CHECK);
    apply();
    if (undoValidateUndoAreas(ed) == 0)
        undoFreeUndoObject(ed);
}

static_assert(INSTR_NAME_MAX == MAX_INSTRNAMELEN, "instrument name length mismatch");

int instr_field(int i, int field)
{
    if (!instr_ok(i)) return 0;
    unsigned char *p = instr_field_ptr(i, field);
    return p ? *p : 0;
}

void instr_set_field(int i, int field, unsigned value)
{
    if (!instr_ok(i)) return;
    unsigned char *p = instr_field_ptr(i, field);
    if (!p) return;
    unsigned char v = (unsigned char)(value & 0xff);
    if (*p == v) return;
    instr_edit(i, [&] { *p = v; });
}

void instr_set_name(int i, const char *name)
{
    if (!instr_ok(i) || !name) return;
    char clean[MAX_INSTRNAMELEN];
    strncpy(clean, name, MAX_INSTRNAMELEN); // truncate/pad to the fixed field width
    if (memcmp(instr[i].name, clean, MAX_INSTRNAMELEN) == 0) return;
    instr_edit(i, [&] { memcpy(instr[i].name, clean, MAX_INSTRNAMELEN); });
}

// ---- song info + transport ----

static_assert(SONG_STR_MAX == MAX_STR - 1, "song string length mismatch");

const char *song_name() { return songname; }
const char *song_author() { return authorname; }
const char *song_copyright() { return copyrightname; }

static void song_set_str(char *dst, const char *s)
{
    if (!s) return;
    strncpy(dst, s, MAX_STR - 1);
    dst[MAX_STR - 1] = 0;
}
void song_set_name(const char *s) { song_set_str(songname, s); }
void song_set_author(const char *s) { song_set_str(authorname, s); }
void song_set_copyright(const char *s) { song_set_str(copyrightname, s); }

void transport_play_start() { initsong(editorInfo.esnum, PLAY_BEGINNING, &gtObject); }
void transport_play_pattern() { initsong(editorInfo.esnum, PLAY_PATTERN, &gtObject); }
void transport_stop() { stopsong(&gtObject); }
bool transport_playing() { return isplaying(&gtObject) != 0; }
int transport_time_min() { return gtObject.timemin; }
int transport_time_sec() { return gtObject.timesec; }

void pattern_set_cursor(int ch, int row, int col)
{
    int chans = pattern_channels();
    if (ch < 0) ch = 0;
    if (ch >= chans) ch = chans - 1;

    int len = pattern_length(ch);
    if (row < 0) row = 0;
    if (row > len) row = len; // legacy allows the cursor on the PATT.END row

    if (col < 0) col = 0;
    if (col > 5) col = 5;

    editorInfo.editmode = EDIT_PATTERN;
    editorInfo.epchn = ch;
    editorInfo.eppos = row;
    editorInfo.epcolumn = col;
    // Keep the master-loop / mark channel in sync, as the legacy click does, so
    // play-from-here and Shift-select act on the clicked channel.
    setMasterLoopChannel(&gtObject, (char *)"imgui");
}

} // namespace gtui
