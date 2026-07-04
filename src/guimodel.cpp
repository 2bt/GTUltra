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

} // namespace gtui
