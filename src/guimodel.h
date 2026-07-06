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

// Legacy editorInfo.editmode values (SDL-free mirror of goattrk2.h).
enum EditPanel : int {
    EditPanelPattern    = 0,
    EditPanelOrder      = 1,
    EditPanelInstrument = 2,
    EditPanelTables     = 3,
    EditPanelNames      = 4,
};
EditPanel edit_panel(); // current keyboard-focus edit mode

// ---- SID tables (wave / pulse / filter / speed) ----
int         table_count();          // number of tables
int         table_len();            // rows per table
int         table_visible_rows();   // how many rows the legacy view shows
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

// Mirror ImGui scroll offset for table @p t (first visible row). Used so
// horizontal table switches preserve the on-screen row across columns.
void        table_set_view(int t, int view_row);

// Recompute tableBackgroundColors for the selected instrument (legacy
// setTableBackgroundColours). Call once per tables-panel frame before drawing.
void table_refresh_instr_highlights();
// True when row is part of the selected instrument's table chain (green in legacy).
bool table_row_uses_selected_instrument(int t, int row);

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
int  pattern_play_row(int ch); // currently-playing row in display channel ch, -1 if none
int  pattern_actual_channel(int ch); // legacy channel number for the header
PatCell pattern_cell(int ch, int row);

// Selection (Shift+Up/Down mark). mark_channel is the *actual* channel number
// (compare against pattern_actual_channel(ch)); -1 means no active selection.
int  pattern_mark_channel();
int  pattern_mark_start();
int  pattern_mark_end();
int  pattern_octave();           // note input octave (epoctave, 0..6)
bool pattern_jam_mode();         // true = jam (live keys), false = edit/record-to-pattern

// Move the edit cursor (e.g. from a mouse click in the ImGui grid): switches to
// pattern-edit mode and positions the cursor. col follows the legacy epcolumn
// convention (0 = note, 1..5 = instr hi/lo, cmd, data hi/lo). Keyboard editing
// then flows through the existing legacy pattern editor.
void pattern_set_cursor(int ch, int row, int col);

// ---- order list (vertical view: positions = rows, channels = columns) ----

// One decoded order entry.
struct OrderCell {
    char text[4];  // up to 2 visible chars + NUL: "0A", "+2", "-3", "R4", "=="
    int  kind;     // 0 empty, 1 pattern, 2 command (transpose/repeat), 3 loop marker (==)
    bool valid;    // within this channel's order length (+ loop row)
};

int  order_channels();          // visible channel count (3 or 6)
int  order_subtune();           // current subtune index (esnum)
int  order_rows();              // rows to render (max order length across channels)
int  order_cursor_row();        // edit cursor position (eseditpos)
int  order_cursor_chn();        // edit cursor channel (display index, eschn)
int  order_cursor_col();        // cursor column within the cell (escolumn)
int  order_actual_channel(int ch);
int  order_length(int ch);      // order length of display channel ch
int  order_mark_chn();          // selection channel (display index), -1 if none
int  order_mark_start();
int  order_mark_end();
int  order_selected_row(int ch);   // synced pattern position (espos), -1 if none
int  order_range_end_row(int ch); // F2 range end (esend), -1 if unset
int  order_play_row(int ch);       // playback position in order list, -1 if none
OrderCell order_cell(int ch, int row);
void order_set_cursor(int ch, int row, int col); // click -> place cursor (EDIT_ORDERLIST)

// ---- instruments (table view: one instrument per row) ----
int         instr_count();          // number of instruments
int         instr_current();        // selected instrument (einum)
const char *instr_name(int i);
int         instr_ad(int i);        // attack/decay
int         instr_sr(int i);        // sustain/release
int         instr_ptr(int i, int which); // table pointer: which = WTBL/PTBL/FTBL/STBL
int         instr_vibdelay(int i);
int         instr_gatetimer(int i);
int         instr_firstwave(int i);
int         instr_pan(int i);
void        instr_select(int i);    // click -> select instrument (EDIT_INSTRUMENT)

// Editable instruments are 01..3F; instrument 00 is hidden and not selectable.
enum { INSTR_FIRST = 1 };
enum { INSTR_GRID_ROWS = 63 }; // MAX_INSTR (64) minus instrument 00
enum { INSTR_FIELD_NAME = 10 }; // maps to legacy eipos == LAST_INST

int         instr_rows();           // visible row count (63)
int         instr_grid_row();       // 0-based row for cursor instrument (einum - 1)
int         instr_cursor_field();   // 0..9 hex field, or INSTR_FIELD_NAME
int         instr_cursor_nibble();  // 0 high / 1 low nibble (hex fields only)
bool        instr_cursor_on_name();
void        instr_set_cursor(int inst, int field, int nibble);
void        instr_clamp_selection(); // coerce einum to 1..3F

// Edits routed through the legacy undo system (Ctrl-Z works). field indices:
// 0 AD, 1 SR, 2..5 wave/pulse/filter/vibrato pointers, 6 vib delay, 7 gate
// timer, 8 first-frame wave, 9 pan.
enum { INSTR_FIELDS = 10 };
enum { INSTR_NAME_MAX = 16 }; // == MAX_INSTRNAMELEN (checked in guimodel.cpp)
int         instr_field(int i, int field);   // read field value by index
void        instr_set_field(int i, int field, unsigned value);
void        instr_set_name(int i, const char *name);

// ---- song info + transport ----
enum { SONG_STR_MAX = 31 }; // usable chars (== MAX_STR-1, checked in .cpp)
const char *song_name();
const char *song_author();
const char *song_copyright();
void        song_set_name(const char *s);
void        song_set_author(const char *s);
void        song_set_copyright(const char *s);

void        transport_play_start();   // play from the start of the song
void        transport_play_pattern(); // play the current pattern
void        transport_stop();
bool        transport_playing();
int         transport_time_min();
int         transport_time_sec();

bool        transport_follow();        // follow playback (auto-scroll) on?
void        transport_toggle_follow();
bool        transport_loop();          // loop the current pattern on play?
void        transport_toggle_loop();
void        transport_ff();            // step to next song position
void        transport_rewind();        // step to previous song position

} // namespace gtui

#endif
