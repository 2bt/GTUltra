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

} // namespace gtui
