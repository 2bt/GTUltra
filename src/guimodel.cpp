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
