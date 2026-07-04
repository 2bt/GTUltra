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
int         table_cursor_col();     // cursor column (0..3 = L hi/lo, R hi/lo)
unsigned    table_left(int t, int row);   // left/value byte
unsigned    table_right(int t, int row);  // right/arg byte

// Selection (Shift-marked range in a table). mark_table is the table index the
// mark is in, -1 if none.
int         table_mark_table();
int         table_mark_start();
int         table_mark_end();

// Write one table byte (col 0 = left, 1 = right), routed through the legacy
// undo system so it participates in Ctrl-Z like a native edit. No-op if the
// value is unchanged or the indices are out of range.
void        table_set(int t, int row, int col, unsigned value);

// Move the table edit cursor from a click: switches to table-edit mode and
// positions the cursor. col follows the legacy etcolumn convention (0..3 =
// left hi/lo, right hi/lo). Keyboard editing then flows through the legacy
// table editor.
void        table_set_cursor(int t, int row, int col);

// ---- pattern editor (read-only for now) ----

// One decoded pattern cell. note points into a static name table (valid until
// process exit); "" when the row is outside the pattern.
struct PatCell {
    const char *note;   // 3-char note name ("C-4", "...", "---", "+++"), "" if invalid
    int  instr;         // instrument byte, 0 = empty
    int  cmd;           // command nibble, 0 = empty
    int  data;          // data byte
    bool end;           // true = ENDPATT marker row
    bool valid;         // row within this channel's pattern
};

int  pattern_channels();       // visible channel count (3 or 6)
int  pattern_rows();           // rows to render (max pattern length across channels)
int  pattern_step();           // beat-highlight step
int  pattern_cursor_row();     // edit cursor row (eppos)
int  pattern_cursor_chn();     // edit cursor channel (display index, epchn)
int  pattern_cursor_col();     // edit cursor column within the cell (epcolumn)
int  pattern_number(int ch);   // pattern index shown in display channel ch
int  pattern_length(int ch);   // pattern length of that pattern
int  pattern_actual_channel(int ch); // legacy channel number for the header
PatCell pattern_cell(int ch, int row);

// Selection (Shift+Up/Down mark). mark_channel is the *actual* channel number
// (compare against pattern_actual_channel(ch)); -1 means no active selection.
int  pattern_mark_channel();
int  pattern_mark_start();
int  pattern_mark_end();

// Move the edit cursor (e.g. from a mouse click in the ImGui grid): switches to
// pattern-edit mode and positions the cursor. col follows the legacy epcolumn
// convention (0 = note, 1..5 = instr hi/lo, cmd, data hi/lo). Keyboard editing
// then flows through the existing legacy pattern editor.
void pattern_set_cursor(int ch, int row, int col);

// ---- order list (vertical view: positions = rows, channels = columns) ----

// One decoded order entry.
struct OrderCell {
    char text[4];  // 3 visible chars + NUL: "0A ", "+2 ", "-3 ", "R4 ", "RST", "   "
    int  kind;     // 0 empty, 1 pattern, 2 command (transpose/repeat), 3 loop (RST)
    bool valid;    // within this channel's order length (+ loop row)
};

int  order_channels();          // visible channel count (3 or 6)
int  order_rows();              // rows to render (max order length across channels)
int  order_cursor_row();        // edit cursor position (eseditpos)
int  order_cursor_chn();        // edit cursor channel (display index, eschn)
int  order_cursor_col();        // cursor column within the cell (escolumn)
int  order_actual_channel(int ch);
int  order_length(int ch);      // order length of display channel ch
int  order_mark_chn();          // selection channel (display index), -1 if none
int  order_mark_start();
int  order_mark_end();
OrderCell order_cell(int ch, int row);
void order_set_cursor(int ch, int row, int col); // click -> place cursor (EDIT_ORDERLIST)

} // namespace gtui

#endif
