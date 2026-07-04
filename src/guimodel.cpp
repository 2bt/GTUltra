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

} // namespace gtui
