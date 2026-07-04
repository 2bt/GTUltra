//
// guimodel - a thin, SDL-free read/query bridge between GTUltra's legacy model
// (globals in gsong / editorInfo) and the new ImGui UI layer.
//
// The ImGui layer (gimgui.cpp) must not include goattrk2.h/bme.h, because those
// drag in bme's bundled SDL headers which would clash with the system SDL2 that
// the ImGui backends use. This header exposes only plain declarations; the
// implementation (guimodel.cpp) is free to include the full legacy headers.
//
// As more panels are ported, extend this with the accessors they need.
//
#ifndef GUIMODEL_H
#define GUIMODEL_H

namespace gtui {

// ---- SID tables (wave / pulse / filter / speed) ----
int         table_count();          // number of tables
int         table_len();            // rows per table
int         table_visible_rows();   // how many rows the legacy view shows
const char *table_name(int t);      // display name, "" if out of range
int         table_view(int t);      // first visible row (scroll offset)
int         table_cursor_table();   // table index the edit cursor is in
int         table_cursor_pos();     // cursor row within its table
unsigned    table_left(int t, int row);   // left/value byte
unsigned    table_right(int t, int row);  // right/arg byte

// Write one table byte (col 0 = left, 1 = right), routed through the legacy
// undo system so it participates in Ctrl-Z like a native edit. No-op if the
// value is unchanged or the indices are out of range.
void        table_set(int t, int row, int col, unsigned value);

} // namespace gtui

#endif
