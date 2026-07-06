//
// GTUltra Dear ImGui integration layer (milestone M2).
//
// Strategy (see docs/UI_MIGRATION_PLAN.md): rather than open a second window,
// reuse bme's existing SDL2 window + renderer. bme already renders the whole
// legacy editor into a texture and blits it every frame; we hook in just before
// its present to draw ImGui on top, and forward SDL events to ImGui. This lets
// the old UI keep working while native ImGui panels are built on top of it.
//
#define IMGUI_DEFINE_MATH_OPERATORS
#include "gimgui.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "guimodel.h" // SDL-free bridge to the legacy model
#include "imgui.h"

#include <SDL.h>
#include <cstdio>

// bme globals/hooks we bind to. Declared here (with C linkage) instead of
// including bme's headers, so we pull the *system* SDL2 headers that the ImGui
// backends use rather than bme's bundled copy. Both are SDL2, so the opaque
// SDL_Window*/SDL_Renderer* types and the underlying library match.
extern "C" {
extern SDL_Window*   win_window;
extern SDL_Renderer* gfx_renderer;
extern void (*bme_overlay_render_hook)(void);
extern void (*bme_event_hook)(void* sdl_event);
extern int (*bme_input_capture_hook)(void);
}

bool g_imgui_ready = false;
bool g_show_demo   = false; // toggleable ImGui reference/demo window
bool g_show_new_ui = true;  // false = legacy chargen UI visible for comparison

// Instrument-name overlay editor (InputText while active).
int  g_instr_name_edit        = -1;    // instrument index 1..3F, or -1
int  g_instr_name_want_focus  = -1;    // request keyboard focus once when opening
bool g_instr_name_item_active = false; // InputText had focus last frame
bool g_instr_name_had_focus   = false; // InputText had focus on a prior frame
char g_instr_name_buf[gtui::INSTR_NAME_MAX + 1];

static void gimgui_instr_name_commit() {
    if (g_instr_name_edit < gtui::INSTR_FIRST) return;
    gtui::instr_set_name(g_instr_name_edit, g_instr_name_buf);
    g_instr_name_edit        = -1;
    g_instr_name_want_focus  = -1;
    g_instr_name_item_active = false;
    g_instr_name_had_focus   = false;
}

void gimgui_instr_name_begin(int inst) {
    if (inst < gtui::INSTR_FIRST) return;
    gimgui_instr_name_commit();
    g_instr_name_edit       = inst;
    g_instr_name_want_focus = inst;
    snprintf(g_instr_name_buf, sizeof g_instr_name_buf, "%.*s", gtui::INSTR_NAME_MAX, gtui::instr_name(inst));
}

// Commit when the name field had focus and the user left it (not on the first
// idle frame before InputText attaches — that was clearing the edit immediately).
static void gimgui_instr_name_sync_focus() {
    if (g_instr_name_edit < gtui::INSTR_FIRST) return;
    if (g_instr_name_want_focus >= gtui::INSTR_FIRST) return;
    if (g_instr_name_had_focus && !g_instr_name_item_active) gimgui_instr_name_commit();
}

namespace {

constexpr ImGuiWindowFlags kNoNavWindowFlags =
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

constexpr ImGuiWindowFlags kPanelWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                               kNoNavWindowFlags;

constexpr ImGuiWindowFlags kChromeWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                                kNoNavWindowFlags;

// Left/top inset for panel body content (must match gimgui_begin_panel).
constexpr float kPanelBodyPad = 6.0f;

// ImGui resets CursorPos.x to the window edge on newline (Dummy, Separator, …).
void gimgui_snap_body_pad_x() { ImGui::SetCursorPosX(kPanelBodyPad); }

// Monospace grid metrics: use glyph advance, not CalcTextSize("0"), which rounds
// the rendered bbox and drifts when multiplied across columns. For a single
// glyph CalcTextSizeA() is usually close to GetCharAdvance(), but layout math
// should use advance (cursor step), not measured ink width.
float gimgui_mono_advance() {
    ImFontBaked* baked = ImGui::GetFontBaked();
    if (!baked) return ImGui::GetFontSize();
    return baked->GetCharAdvance('0');
}

float gimgui_mono_width(int cols) { return gimgui_mono_advance() * (float)cols; }

// Reusable scaffold for a scrolling, virtualized monospace grid body (used by
// the pattern editor and each SID table). Handles the child window, content
// reservation, row virtualization, no-lag cursor-follow, and click hit-testing.
// The caller supplies the cell/row content and click handling; all colours and
// per-cell layout live there.
//   rows      : total row count
//   rowW      : content width (horizontal scroll reservation)
//   lineH     : row height
//   followRow : row to keep centred when it changes (<0 = don't auto-follow)
//   drawRow(ImDrawList* dl, int row, float x, float y)   x,y = row's top-left
//   onClick(int row, float localX)                       localX = px from row start
//   headerDraw (optional): fixed column titles at the top of the same child,
//     sharing origin.x with data rows. headerBandH = row height of that band (0 = none).
//   widgetRow / placeWidget (optional): when widgetRow >= 0 and the row is visible,
//     placeWidget(screenX, screenY) runs inside the scroll child before drawRow so
//     ImGui widgets (e.g. InputText) share the grid's coordinate space.
template <class HeaderDraw, class DrawRow, class PlaceWidget, class OnClick>
bool gimgui_grid_body(const char* id,
                      int         rows,
                      float       rowW,
                      float       lineH,
                      int         followRow,
                      HeaderDraw  headerDraw,
                      float       headerBandH,
                      int         widgetRow,
                      PlaceWidget placeWidget,
                      DrawRow     drawRow,
                      OnClick     onClick) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(id, ImVec2(0, 0), false, kNoNavWindowFlags);

    if (headerBandH > 0.0f) {
        ImDrawList*  hdl = ImGui::GetWindowDrawList();
        const ImVec2 hp  = ImGui::GetCursorScreenPos();
        headerDraw(hdl, hp.x, hp.y);
        ImGui::Dummy(ImVec2(rowW, headerBandH));
        ImGui::Separator();
    }

    ImGui::BeginChild("##scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar | kNoNavWindowFlags);
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  totalH = rows * lineH;
    ImGui::Dummy(ImVec2(rowW, totalH)); // reserve scroll region

    const float winH       = ImGui::GetWindowHeight();
    const float curScroll  = ImGui::GetScrollY();
    const float contentTop = origin.y + curScroll; // scroll-independent anchor

    // Follow the cursor row when it moves. SetScrollY only takes effect next
    // frame, so we also draw at the target scroll *this* frame (no one-frame
    // jump). Follow state is per-grid (child storage), so grids don't yank each
    // other and manual scrolling sticks until the cursor moves again.
    float drawScroll = curScroll;
    if (followRow >= 0) {
        ImGuiStorage* st  = ImGui::GetStateStorage();
        const ImGuiID key = ImGui::GetID("##gridfollow");
        if (st->GetInt(key, -1) != followRow) {
            st->SetInt(key, followRow);
            float maxScroll = totalH - winH;
            if (maxScroll < 0) maxScroll = 0;
            drawScroll = followRow * lineH - winH * 0.5f;
            if (drawScroll < 0) drawScroll = 0;
            if (drawScroll > maxScroll) drawScroll = maxScroll;
            ImGui::SetScrollY(drawScroll);
        }
    }
    const float drawTop = contentTop - drawScroll;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        int          r = (int)((m.y - drawTop) / lineH);
        if (r >= 0 && r < rows) onClick(r, m.x - origin.x);
    }

    int firstRow = (int)(drawScroll / lineH);
    int lastRow  = (int)((drawScroll + winH) / lineH) + 1;
    if (firstRow < 0) firstRow = 0;
    if (lastRow > rows) lastRow = rows;
    bool widgetPlaced = false;
    for (int r = firstRow; r < lastRow; r++) {
        const float rowY = drawTop + r * lineH;
        if (widgetRow >= 0 && r == widgetRow) {
            placeWidget(origin.x, rowY);
            widgetPlaced = true;
        }
        drawRow(dl, r, origin.x, rowY);
    }

    ImGui::EndChild(); // ##scroll
    ImGui::EndChild(); // id
    ImGui::PopStyleVar();
    return widgetPlaced;
}

template <class DrawRow, class OnClick>
bool gimgui_grid_body(const char* id,
                      int         rows,
                      float       rowW,
                      float       lineH,
                      int         followRow,
                      DrawRow     drawRow,
                      OnClick     onClick) {
    return gimgui_grid_body(id, rows, rowW, lineH, followRow, [](ImDrawList*, float, float) {}, 0.0f, -1, [](float, float) {}, drawRow, onClick);
}

template <class HeaderDraw, class DrawRow, class OnClick>
bool gimgui_grid_body(const char* id,
                      int         rows,
                      float       rowW,
                      float       lineH,
                      int         followRow,
                      HeaderDraw  headerDraw,
                      float       headerBandH,
                      DrawRow     drawRow,
                      OnClick     onClick) {
    return gimgui_grid_body(id, rows, rowW, lineH, followRow, headerDraw, headerBandH, -1, [](float, float) {}, drawRow, onClick);
}

// Draw one SID table as an independently-scrolling column: a fixed header over
// a virtualized scrolling body (fills the available height). Only the active
// table auto-follows its cursor; the others keep their own scroll, matching the
// legacy's per-table independent scrolling.
void gimgui_draw_one_table(int t, float colW, float charW, float lineH) {
    const ImU32      cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32      cSelect     = IM_COL32(48, 96, 200, 110);
    const ImU32      cCursorFill = IM_COL32(235, 225, 120, 70);
    const ImU32      cCursorEdge = IM_COL32(235, 225, 120, 230);
    const ImU32      cIdx        = IM_COL32(120, 140, 160, 255);
    const ImU32      cVal        = IM_COL32(224, 230, 238, 255);
    static const int colOff[4]   = { 3, 4, 6, 7 }; // cursor char within "II:LL RR"

    const int tlen    = gtui::table_len();
    const int curTab  = gtui::table_cursor_table();
    const int curPos  = gtui::table_cursor_pos();
    const int curCol  = gtui::table_cursor_col();
    const int markTab = gtui::table_mark_table();
    int       markLo = gtui::table_mark_start(), markHi = gtui::table_mark_end();
    if (markLo > markHi) {
        int tmp = markLo;
        markLo  = markHi;
        markHi  = tmp;
    }
    const bool active = (curTab == t);

    ImGui::BeginChild("col", ImVec2(colW, 0), true, kNoNavWindowFlags);

    // TODO: clicking a table header should toggle an alternative "detailed"
    // interpreted view (a GTUltra feature; not for the speed table). For now the
    // header is plain text.
    ImGui::TextUnformatted(gtui::table_name(t));
    ImGui::Separator();

    const float cellW8 = gimgui_mono_width(8);
    gimgui_grid_body(
        "body",
        tlen,
        cellW8,
        lineH,
        active ? curPos : -1,
        [&](ImDrawList* dl, int r, float x, float y) {
            char buf[16];
            if (markTab == t && r >= markLo && r <= markHi)
                dl->AddRectFilled(ImVec2(x + gimgui_mono_width(3), y), ImVec2(x + cellW8, y + lineH), cSelect);
            if (active && curPos == r) {
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cCursorRow);
                float cs = x + colOff[curCol < 0 ? 0 : (curCol > 3 ? 3 : curCol)] * charW;
                dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
            }
            snprintf(buf, sizeof buf, "%02X:", r + 1);
            dl->AddText(ImVec2(x, y), cIdx, buf);
            snprintf(buf, sizeof buf, "%02X %02X", gtui::table_left(t, r), gtui::table_right(t, r));
            dl->AddText(ImVec2(x + gimgui_mono_width(3), y), cVal, buf);
        },
        [&](int r, float localX) {
            int off = (int)(localX / charW);
            int col = (off <= 3) ? 0 : (off == 4) ? 1 : (off <= 6) ? 2 : 3;
            gtui::table_set_cursor(t, r, col);
        });

    ImGui::EndChild();
}

// Accent/section colours for the fixed tracker layout (theme system is M7).
const ImU32 kAppBg          = IM_COL32(18, 20, 24, 255); // gutter/background
const ImU32 kHeaderBg       = IM_COL32(30, 44, 60, 255);
const ImU32 kHeaderBgActive = IM_COL32(52, 98, 158, 255);
const ImU32 kHeaderTx       = IM_COL32(150, 168, 186, 255);
const ImU32 kHeaderTxActive = IM_COL32(244, 250, 255, 255);
const ImU32 kHeaderAccent   = IM_COL32(235, 200, 100, 255);

// A button that renders in its "active" colour while toggled on (Follow, Loop).
bool gimgui_toggle_button(const char* label, bool active) {
    if (active) {
        const ImVec4 on = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Button, on);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on);
    }
    bool clicked = ImGui::Button(label);
    if (active) ImGui::PopStyleColor(2);
    return clicked;
}

// Fixed, non-floating panel. Positioned/sized every frame to tile the app
// window (no title bar, no move/resize) so the UI reads as one cohesive tracker
// rather than a set of ImGui windows. Draws an edge-to-edge section header.
// Returns true when the body should be drawn (caller must still call End()).
bool gimgui_begin_panel(const char* title, ImVec2 pos, ImVec2 size, bool active) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGuiWindowFlags flags = kPanelWindowFlags;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin(title, nullptr, flags);
    ImGui::PopStyleVar();
    if (!open) return false;

    // Edge-to-edge header bar with the section name (window padding is 0, so
    // window coords == content coords).
    ImDrawList*  dl   = ImGui::GetWindowDrawList();
    const ImVec2 wp   = ImGui::GetWindowPos();
    const float  ww   = ImGui::GetWindowSize().x;
    const float  hpad = kPanelBodyPad;
    const float  hh   = ImGui::GetTextLineHeight() + hpad * 2.0f;
    dl->AddRectFilled(wp, ImVec2(wp.x + ww, wp.y + hh), active ? kHeaderBgActive : kHeaderBg);
    if (active) dl->AddRectFilled(ImVec2(wp.x, wp.y + hh - 2.0f), ImVec2(wp.x + ww, wp.y + hh), kHeaderAccent);
    dl->AddText(ImVec2(wp.x + hpad, wp.y + hpad), active ? kHeaderTxActive : kHeaderTx, title);

    // Inset the body below the header, with a small left/top gutter.
    ImGui::SetCursorPos(ImVec2(hpad, hh + 4.0f));
    return true;
}

void gimgui_end_panel(void) { ImGui::End(); }

// The four SID tables (wave/pulse/filter/speed): four independently-scrolling
// columns (matching the legacy per-table scroll), each a custom grid like the
// pattern editor. Keyboard editing flows through the legacy table editor;
// clicking a cell places the cursor.
void gimgui_draw_tables(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Tables", pos, size, gtui::edit_panel() == gtui::EditPanelTables)) {
        gimgui_end_panel();
        return;
    }

    const float charW = gimgui_mono_advance();
    const float lineH = ImGui::GetTextLineHeight();
    const float colW =
        charW * 8.0f + ImGui::GetStyle().ScrollbarSize + ImGui::GetStyle().WindowPadding.x * 2.0f + 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
    for (int t = 0; t < gtui::table_count(); t++) {
        if (t) ImGui::SameLine();
        ImGui::PushID(t);
        gimgui_draw_one_table(t, colW, charW, lineH);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    gimgui_end_panel();
}

// Read-only pattern grid (M4 first cut): a custom ImDrawList grid, following
// Furnace's approach - fixed monospace metrics, virtualized to the visible rows,
// per-field coloring. Reads live model state via guimodel; editing comes later.
void gimgui_draw_pattern(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Pattern", pos, size, gtui::edit_panel() == gtui::EditPanelPattern)) {
        gimgui_end_panel();
        return;
    }

    // Colors (hardcoded for now; the theme system is M7).
    const ImU32 cBeat       = IM_COL32(255, 255, 255, 10);
    const ImU32 cCursorRow  = IM_COL32(255, 255, 100, 20);
    const ImU32 cPlayRow    = IM_COL32(80, 190, 90, 80);   // per-channel playhead
    const ImU32 cSelect     = IM_COL32(48, 96, 200, 110);  // Shift+Up/Down mark
    const ImU32 cCursorFill = IM_COL32(235, 225, 120, 70); // cursor cell
    const ImU32 cCursorEdge = IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum     = IM_COL32(120, 140, 160, 255);
    const ImU32 cNote       = IM_COL32(224, 230, 238, 255);
    const ImU32 cInstr      = IM_COL32(120, 205, 120, 255);
    const ImU32 cCmd        = IM_COL32(235, 180, 90, 255);
    const ImU32 cDots       = IM_COL32(85, 95, 108, 255);
    const ImU32 cHeader     = IM_COL32(180, 200, 220, 255);
    const ImU32 cEnd        = IM_COL32(150, 160, 175, 255);

    const int chans  = gtui::pattern_channels();
    const int rows   = gtui::pattern_rows();
    const int step   = gtui::pattern_step() > 0 ? gtui::pattern_step() : 4;
    const int curRow = gtui::pattern_cursor_row();
    const int curChn = gtui::pattern_cursor_chn();
    const int curCol = gtui::pattern_cursor_col();

    // Active selection (Shift+Up/Down): actual channel + inclusive row range.
    const int markChn = gtui::pattern_mark_channel();
    int       markLo  = gtui::pattern_mark_start();
    int       markHi  = gtui::pattern_mark_end();
    if (markLo > markHi) {
        int t  = markLo;
        markLo = markHi;
        markHi = t;
    }

    // Per-channel playhead rows (-1 when that channel isn't showing its playing
    // pattern). chans <= MAX_CHN (6).
    int playRow[8];
    for (int c = 0; c < chans && c < 8; c++) playRow[c] = gtui::pattern_play_row(c);

    const float charW     = gimgui_mono_advance();
    const float lineH     = ImGui::GetTextLineHeight();
    const float rowNumW   = gimgui_mono_width(4);
    const float chanW     = gimgui_mono_width(9);
    const float chanCellW = gimgui_mono_width(8);
    const float noteW     = gimgui_mono_width(3);

    // Column headers (fixed, above the scrolling body).
    {
        gimgui_snap_body_pad_x();
        ImDrawList* hdl = ImGui::GetWindowDrawList();
        ImVec2      hp  = ImGui::GetCursorScreenPos();
        char        hbuf[32];
        for (int c = 0; c < chans; c++) {
            snprintf(hbuf, sizeof hbuf, "CH%X %02X", gtui::pattern_actual_channel(c), gtui::pattern_number(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + c * chanW, hp.y), cHeader, hbuf);
        }
        gimgui_snap_body_pad_x();
        ImGui::Dummy(ImVec2(rowNumW + chans * chanW, lineH));
        gimgui_snap_body_pad_x();
        ImGui::Separator();
    }

    const float totalW = rowNumW + chans * chanW;

    gimgui_snap_body_pad_x();
    gimgui_grid_body(
        "patgrid",
        rows,
        totalW,
        lineH,
        curRow,
        [&](ImDrawList* dl, int r, float x, float y) {
            char buf[16];

            // Row background: beat highlight + cursor row.
            if (r / step % 2 == 0) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cBeat);
            if (r == curRow) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);

            // Row number.
            snprintf(buf, sizeof buf, "%3d", r);
            dl->AddText(ImVec2(x, y), cRowNum, buf);

            for (int c = 0; c < chans; c++) {
                const float cx = x + rowNumW + c * chanW;

                // Playing-row highlight (per channel; follows playback).
                if (r == playRow[c]) dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + chanCellW, y + lineH), cPlayRow);

                // Selection background for the marked channel + row range.
                if (markChn >= 0 && gtui::pattern_actual_channel(c) == markChn && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + chanCellW, y + lineH), cSelect);

                // Cursor cell: the exact sub-field the cursor is on. epcolumn
                // 0 = note (3 chars); 1..5 = one nibble at cell offset 2+col.
                if (r == curRow && c == curChn) {
                    float cs = (curCol == 0) ? cx : cx + gimgui_mono_width(2 + curCol);
                    float cw = (curCol == 0) ? noteW : charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + cw, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + cw, y + lineH), cCursorEdge);
                }

                gtui::PatCell cell = gtui::pattern_cell(c, r);
                if (!cell.valid) continue;
                if (cell.end) {
                    dl->AddText(ImVec2(cx, y), cEnd, "===");
                    continue;
                }

                // Note (dim an empty/REST note), instrument, command+data.
                const bool emptyNote = (cell.note[0] == '.');
                dl->AddText(ImVec2(cx, y), emptyNote ? cDots : cNote, cell.note);
                if (cell.instr) {
                    snprintf(buf, sizeof buf, "%02X", cell.instr);
                    dl->AddText(ImVec2(cx + noteW, y), cInstr, buf);
                }
                else {
                    dl->AddText(ImVec2(cx + noteW, y), cDots, "..");
                }
                if (cell.cmd) {
                    snprintf(buf, sizeof buf, "%01X%02X", cell.cmd, cell.data);
                    dl->AddText(ImVec2(cx + gimgui_mono_width(5), y), cCmd, buf);
                }
                else {
                    dl->AddText(ImVec2(cx + gimgui_mono_width(5), y), cDots, "...");
                }
            }
        },
        [&](int r, float localX) {
            float rx = localX - rowNumW;
            if (rx < 0) return;
            int c = (int)(rx / chanW);
            if (c < 0 || c >= chans) return;
            int off = (int)((rx - c * chanW) / charW);
            if (off > 7) off = 7;
            int col = (off < 3) ? 0 : (off - 2);
            gtui::pattern_set_cursor(c, r, col);
        });

    gimgui_end_panel();
}

// Order list, vertical layout (positions = rows, channels = columns) via the
// shared grid scaffold. Cells are pattern numbers or commands (+/-/R/RST).
// Keyboard editing flows through the legacy order editor; clicking places the
// cursor (gtui::order_set_cursor).
void gimgui_draw_orderlist(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Order List", pos, size, gtui::edit_panel() == gtui::EditPanelOrder)) {
        gimgui_end_panel();
        return;
    }

    const ImU32 cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32 cSelect     = IM_COL32(48, 96, 200, 110);
    const ImU32 cCursorFill = IM_COL32(235, 225, 120, 70);
    const ImU32 cCursorEdge = IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum     = IM_COL32(120, 140, 160, 255);
    const ImU32 cPat        = IM_COL32(224, 230, 238, 255);
    const ImU32 cCmd        = IM_COL32(235, 180, 90, 255);
    const ImU32 cLoop       = IM_COL32(150, 160, 175, 255);
    const ImU32 cHeader     = IM_COL32(180, 200, 220, 255);

    const int chans   = gtui::order_channels();
    const int rows    = gtui::order_rows();
    const int curRow  = gtui::order_cursor_row();
    const int curChn  = gtui::order_cursor_chn();
    const int curCol  = gtui::order_cursor_col();
    const int markChn = gtui::order_mark_chn();
    int       markLo = gtui::order_mark_start(), markHi = gtui::order_mark_end();
    if (markLo > markHi) {
        int tmp = markLo;
        markLo  = markHi;
        markHi  = tmp;
    }

    const float charW   = gimgui_mono_advance();
    const float lineH   = ImGui::GetTextLineHeight();
    const float rowNumW = gimgui_mono_width(4); // "PP "
    const float colW    = gimgui_mono_width(4); // "PP " cell + gutter
    const float totalW  = rowNumW + chans * colW;

    // Fixed header: POS + channel numbers.
    {
        gimgui_snap_body_pad_x();
        ImDrawList* hdl = ImGui::GetWindowDrawList();
        ImVec2      hp  = ImGui::GetCursorScreenPos();
        char        hbuf[16];
        hdl->AddText(ImVec2(hp.x, hp.y), cHeader, "POS");
        for (int c = 0; c < chans; c++) {
            snprintf(hbuf, sizeof hbuf, "CH%X", gtui::order_actual_channel(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + c * colW, hp.y), cHeader, hbuf);
        }
        gimgui_snap_body_pad_x();
        ImGui::Dummy(ImVec2(totalW, lineH));
        gimgui_snap_body_pad_x();
        ImGui::Separator();
    }

    gimgui_snap_body_pad_x();
    gimgui_grid_body(
        "olgrid",
        rows,
        totalW,
        lineH,
        curRow,
        [&](ImDrawList* dl, int r, float x, float y) {
            char buf[8];
            if (r == curRow) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);
            snprintf(buf, sizeof buf, "%02X", r);
            dl->AddText(ImVec2(x, y), cRowNum, buf);

            for (int c = 0; c < chans; c++) {
                const float cx = x + rowNumW + c * colW;
                if (markChn == c && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + gimgui_mono_width(3), y + lineH), cSelect);
                if (r == curRow && c == curChn) {
                    float cs = cx + (curCol < 0 ? 0 : (curCol > 2 ? 2 : curCol)) * charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                }

                gtui::OrderCell cell = gtui::order_cell(c, r);
                if (!cell.valid) continue;
                ImU32 col = cell.kind == 2 ? cCmd : cell.kind == 3 ? cLoop : cPat;
                dl->AddText(ImVec2(cx, y), col, cell.text);
            }
        },
        [&](int r, float localX) {
            float rx = localX - rowNumW;
            if (rx < 0) return;
            int c = (int)(rx / colW);
            if (c < 0 || c >= chans) return;
            int off = (int)((rx - c * colW) / charW);
            gtui::order_set_cursor(c, r, off > 2 ? 2 : off);
        });

    gimgui_end_panel();
}


// Instrument list (01..3F): custom grid like pattern/order. Hex fields use the
// legacy nibble editor; the name column uses an on-demand InputText in the grid.
void gimgui_draw_instruments(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Instruments", pos, size, gtui::edit_panel() == gtui::EditPanelInstrument)) {
        gimgui_end_panel();
        return;
    }

    gtui::instr_clamp_selection();

    static const char* fieldLabel[gtui::INSTR_FIELDS] = { "AD", "SR", "WP", "PP", "FP",
                                                          "VP", "VD", "GT", "1W", "PN" };

    const ImU32 cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32 cCursorFill = IM_COL32(235, 225, 120, 70);
    const ImU32 cCursorEdge = IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum     = IM_COL32(120, 140, 160, 255);
    const ImU32 cText       = IM_COL32(224, 230, 238, 255);
    const ImU32 cHeader     = IM_COL32(180, 200, 220, 255);

    const int rows      = gtui::instr_rows();
    const int curRow    = gtui::instr_grid_row();
    const int curField  = gtui::instr_cursor_field();
    const int curNibble = gtui::instr_cursor_nibble();

    const float charW  = gimgui_mono_advance();
    const float lineH  = ImGui::GetTextLineHeight();
    const float idxW   = gimgui_mono_width(3);
    const float nameW  = gimgui_mono_width(gtui::INSTR_NAME_MAX);
    const float hexW   = gimgui_mono_width(3);
    const float totalW = idxW + nameW + gtui::INSTR_FIELDS * hexW;

    // Cumulative column edges (shared by header + rows — no repeated multiply).
    float colX[3 + gtui::INSTR_FIELDS];
    colX[0] = 0.0f;
    colX[1] = idxW;
    colX[2] = idxW + nameW;
    for (int f = 0; f < gtui::INSTR_FIELDS; f++) colX[3 + f] = colX[2] + (float)f * hexW;

    const int nameEditRow =
        (g_instr_name_edit >= gtui::INSTR_FIRST) ? (g_instr_name_edit - gtui::INSTR_FIRST) : -1;

    gimgui_snap_body_pad_x();
    const bool nameEditVisible = gimgui_grid_body(
        "instgrid",
        rows,
        totalW,
        lineH,
        curRow,
        [&](ImDrawList* dl, float x, float y) {
            dl->AddText(ImVec2(x + colX[0], y), cHeader, "##");
            dl->AddText(ImVec2(x + colX[1], y), cHeader, "Name");
            for (int f = 0; f < gtui::INSTR_FIELDS; f++)
                dl->AddText(ImVec2(x + colX[3 + f], y), cHeader, fieldLabel[f]);
        },
        lineH,
        nameEditRow,
        [&](float gridX, float rowY) {
            const int inst = g_instr_name_edit;
            ImGui::SetCursorScreenPos(ImVec2(gridX + colX[1], rowY));
            ImGui::PushID(inst);
            ImGui::PushItemWidth(-1);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
            if (g_instr_name_want_focus == inst) ImGui::SetKeyboardFocusHere();
            ImGui::InputText("##iname", g_instr_name_buf, sizeof g_instr_name_buf);
            if (ImGui::IsItemDeactivatedAfterEdit()) gimgui_instr_name_commit();
            g_instr_name_item_active = ImGui::IsItemActive();
            if (g_instr_name_item_active) g_instr_name_had_focus = true;
            if (g_instr_name_want_focus == inst && g_instr_name_item_active) g_instr_name_want_focus = -1;
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
            ImGui::PopID();
        },
        [&](ImDrawList* dl, int r, float x, float y) {
            const int inst = r + gtui::INSTR_FIRST;
            char      buf[32];

            if (r == curRow) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);

            snprintf(buf, sizeof buf, "%02X", inst);
            dl->AddText(ImVec2(x + colX[0], y), cRowNum, buf);

            const float nameX = x + colX[1];
            if (r == curRow && curField == gtui::INSTR_FIELD_NAME) {
                dl->AddRectFilled(ImVec2(nameX, y), ImVec2(nameX + nameW, y + lineH), cCursorFill);
                dl->AddRect(ImVec2(nameX, y), ImVec2(nameX + nameW, y + lineH), cCursorEdge);
            }
            if (inst != g_instr_name_edit) {
                snprintf(buf, sizeof buf, "%-*s", gtui::INSTR_NAME_MAX, gtui::instr_name(inst));
                dl->AddText(ImVec2(nameX, y), cText, buf);
            }

            for (int f = 0; f < gtui::INSTR_FIELDS; f++) {
                const float fx = x + colX[3 + f];
                if (r == curRow && curField == f) {
                    const float cs = fx + curNibble * charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                }
                snprintf(buf, sizeof buf, "%02X", gtui::instr_field(inst, f));
                dl->AddText(ImVec2(fx, y), cText, buf);
            }
        },
        [&](int r, float localX) {
            gimgui_instr_name_commit();
            const int inst = r + gtui::INSTR_FIRST;
            if (localX < colX[1]) {
                gtui::instr_set_cursor(inst, 0, 0);
                return;
            }
            if (localX < colX[2]) {
                gtui::instr_set_cursor(inst, gtui::INSTR_FIELD_NAME, 0);
                gimgui_instr_name_begin(inst);
                return;
            }
            for (int f = 0; f < gtui::INSTR_FIELDS; f++) {
                const float x0 = colX[3 + f];
                const float x1 = x0 + hexW;
                if (localX >= x0 && localX < x1) {
                    const int nib = (int)((localX - x0) / charW);
                    gtui::instr_set_cursor(inst, f, nib > 0 ? 1 : 0);
                    return;
                }
            }
        });

    if (g_instr_name_edit >= gtui::INSTR_FIRST) {
        if (!nameEditVisible) gimgui_instr_name_commit();
    }
    else {
        g_instr_name_item_active = false;
    }

    gimgui_end_panel();
}

// Song info + transport: name/author/copyright text fields and play/stop
// controls. A plain form panel (regular ImGui widgets). Metadata edits write
// directly (not undo-tracked, as in the legacy); transport calls the legacy
// play/stop.
void gimgui_draw_song(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Song", pos, size, gtui::edit_panel() == gtui::EditPanelNames)) {
        gimgui_end_panel();
        return;
    }

    struct Field {
        const char* label;
        const char* (*get)();
        void (*set)(const char*);
    };
    static const Field fields[3] = {
        { "Name", gtui::song_name, gtui::song_set_name },
        { "Author", gtui::song_author, gtui::song_set_author },
        { "Copyright", gtui::song_copyright, gtui::song_set_copyright },
    };

    float input_width = gimgui_mono_width(gtui::SONG_STR_MAX) + ImGui::GetStyle().FramePadding.x * 2.0f;

    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
    for (int f = 0; f < 3; f++) {
        char buf[gtui::SONG_STR_MAX + 1];
        snprintf(buf, sizeof buf, "%.*s", (int)gtui::SONG_STR_MAX, fields[f].get());
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(fields[f].label);
        ImGui::SameLine(72.0f);
        ImGui::PushID(f);
        ImGui::PushItemWidth(input_width);
        if (ImGui::InputText("##v", buf, sizeof buf))
            fields[f].set(buf); // metadata: commit as typed (no undo, like legacy)
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::PopItemFlag();

    gimgui_end_panel();
}

// Full-width transport toolbar (no section header): rewind / play / pattern /
// fast-forward / stop, then the Follow and Loop toggles, then a play indicator
// and elapsed time. Sits at the top of the window, below the menu bar.
void gimgui_draw_transport(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    const ImGuiWindowFlags flags = kChromeWindowFlags;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    bool open = ImGui::Begin("##transport", nullptr, flags);
    ImGui::PopStyleVar();
    if (!open) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Legacy")) g_show_new_ui = false;
    ImGui::SameLine(0.0f, 16.0f);


    if (ImGui::Button("<<")) // fast-backward: previous song position
        gtui::transport_rewind();
    ImGui::SameLine();
    if (ImGui::Button(">")) // play from start of song
        gtui::transport_play_start();
    ImGui::SameLine();
    if (ImGui::Button("Pat")) // play current pattern
        gtui::transport_play_pattern();
    ImGui::SameLine();
    if (ImGui::Button(">>")) // fast-forward: next song position
        gtui::transport_ff();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) gtui::transport_stop();

    ImGui::SameLine(0.0f, 16.0f);
    if (gimgui_toggle_button("Follow", gtui::transport_follow())) gtui::transport_toggle_follow();
    ImGui::SameLine();
    if (gimgui_toggle_button("Loop", gtui::transport_loop())) gtui::transport_toggle_loop();

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s %02d:%02d",
                gtui::transport_playing() ? "|>" : "[]",
                gtui::transport_time_min(),
                gtui::transport_time_sec());

    ImGui::End();
}

// Small always-on-top bar shown when the legacy UI is visible — the only ImGui
// chrome in that mode, so the chargen renderer underneath stays fully usable.
void gimgui_draw_legacy_mode_bar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float          w  = 100.0f;
    const float          h  = ImGui::GetFrameHeight() + 12.0f;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - w - 8.0f, vp->Pos.y + 8.0f));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    const ImGuiWindowFlags flags = kChromeWindowFlags;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    if (ImGui::Begin("##legacy_mode", nullptr, flags)) {
        if (ImGui::Button("New UI")) g_show_new_ui = true;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// Load the bundled monospace font at a legible size so the UI doesn't use the
// tiny default ImGui bitmap font. Searches a few locations (next to the binary,
// the build-time source assets dir, then the cwd); falls back to the default
// font if none are found.
void gimgui_load_font() {
    ImGuiIO&    io     = ImGui::GetIO();
    const float sizePx = 18.0f;
    const char* fname  = "IBMPlexMono-Regular.otf";

    const char* dirs[3];
    int         nd = 0;
    char        baseDir[1024];
    baseDir[0] = 0;
    if (char* base = SDL_GetBasePath()) {
        snprintf(baseDir, sizeof baseDir, "%sassets/fonts", base);
        SDL_free(base);
        dirs[nd++] = baseDir;
    }
#ifdef GTULTRA_ASSETS_DIR
    dirs[nd++] = GTULTRA_ASSETS_DIR "/fonts";
#endif
    dirs[nd++] = "assets/fonts";

    char path[1152];
    for (int i = 0; i < nd; i++) {
        snprintf(path, sizeof path, "%s/%s", dirs[i], fname);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fclose(f);
        if (io.Fonts->AddFontFromFileTTF(path, sizePx)) return; // loaded
    }
    io.Fonts->AddFontDefault(); // last resort
}

// Flat, professional dark theme: square windows, minimal borders, a blue accent.
// Deliberately unlike StyleColorsDark so the UI doesn't read as a default ImGui
// app. A full theme/config system arrives with M7.
void gimgui_apply_style() {
    ImGuiStyle& s       = ImGui::GetStyle();
    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 0.0f;
    s.FrameRounding     = 2.0f;
    s.PopupRounding     = 2.0f;
    s.ScrollbarRounding = 2.0f;
    s.GrabRounding      = 2.0f;
    s.TabRounding       = 0.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = ImVec2(8, 6);
    s.FramePadding      = ImVec2(6, 3);
    s.ItemSpacing       = ImVec2(6, 4);
    s.ItemInnerSpacing  = ImVec2(4, 4);
    s.ScrollbarSize     = 12.0f;

    ImVec4* c                        = s.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.86f, 0.89f, 0.93f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.48f, 0.52f, 1.00f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_Border]               = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.30f, 0.36f, 0.44f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.26f, 0.41f, 1.00f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.31f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.40f, 0.46f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.22f, 0.30f, 0.42f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.42f, 0.58f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.36f, 0.52f, 0.72f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.20f, 0.34f, 0.52f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.26f, 0.42f, 0.62f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.30f, 0.48f, 0.70f, 1.00f);
    c[ImGuiCol_Separator]            = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.30f, 0.35f, 1.00f);
}

} // namespace

// Called by bme (via bme_overlay_render_hook) between its RenderCopy and its
// RenderPresent, i.e. on top of the freshly-drawn legacy frame.
extern "C" void gimgui_overlay_render(void) {
    if (!g_imgui_ready) return;

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (!g_show_new_ui) {
        gimgui_draw_legacy_mode_bar();
        if (g_show_demo) ImGui::ShowDemoWindow(&g_show_demo);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gfx_renderer);
        return;
    }

    // if (ImGui::BeginMainMenuBar()) {
    //     if (ImGui::BeginMenu("View")) {
    //         ImGui::MenuItem("ImGui Demo", nullptr, &g_show_demo);
    //         ImGui::EndMenu();
    //     }
    //     ImGui::EndMainMenuBar();
    // }


    // Fixed tiled layout filling the whole window. Panels are opaque and cover the legacy screen; a full-window
    // background fill hides the legacy in the gutters between panels. Two columns:
    //   left  : Pattern (top) + SID Tables (bottom)
    //   right : Order List (top) + Instruments (mid) + Song (bottom)
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos,
                                                  ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                  kAppBg);

    const ImVec2 vo = vp->WorkPos;  // origin below the menu bar
    const ImVec2 vs = vp->WorkSize; // area excluding the menu bar
    const float  g  = 3.0f;         // gutter between panels

    // Full-width transport toolbar across the top; the two columns fill below it.
    const float transportH = ImGui::GetFrameHeight() + 12.0f;
    gimgui_draw_transport(vo, ImVec2(vs.x, transportH));

    const ImVec2 o = ImVec2(vo.x, vo.y + transportH + g);
    const ImVec2 s = ImVec2(vs.x, vs.y - transportH - g);

    // Three columns: left = Order List (narrow, leftmost); centre = Pattern
    // (widest); right = Instruments / SID Tables / Song stacked.
    const float orderW = floorf(s.x * 0.22f);
    const float rightW = floorf(s.x * 0.31f);
    const float patW   = s.x - orderW - rightW - 2 * g;
    const float orderX = o.x;
    const float patX   = o.x + orderW + g;
    const float rightX = o.x + orderW + patW + 2 * g;

    // Left and centre columns, full height.
    gimgui_draw_orderlist(ImVec2(orderX, o.y), ImVec2(orderW, s.y));
    gimgui_draw_pattern(ImVec2(patX, o.y), ImVec2(patW, s.y));

    // Right column split: Instruments / Tables / Song.
    const float insH  = floorf(s.y * 0.40f);
    const float tblH  = floorf(s.y * 0.34f);
    const float songH = s.y - insH - tblH - 2 * g;
    gimgui_draw_instruments(ImVec2(rightX, o.y), ImVec2(rightW, insH));
    gimgui_draw_tables(ImVec2(rightX, o.y + insH + g), ImVec2(rightW, tblH));
    gimgui_draw_song(ImVec2(rightX, o.y + insH + tblH + 2 * g), ImVec2(rightW, songH));

    if (g_show_demo) ImGui::ShowDemoWindow(&g_show_demo);

    gimgui_instr_name_sync_focus();

    ImGui::Render();

    // The legacy frame is letterboxed via an explicit destination rect (no
    // renderer logical size), so ImGui already draws across the full window in
    // output pixels here.
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gfx_renderer);
}

// Called by bme (via bme_event_hook) for every polled SDL event.
extern "C" void gimgui_event_process(void* sdl_event) {
    if (!g_imgui_ready) return;
    const SDL_Event* e = static_cast<const SDL_Event*>(sdl_event);
    // Tab is reserved for edit-mode cycling (action layer / future keymap).
    if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
        if (e->key.keysym.scancode == SDL_SCANCODE_TAB) return;
    }
    ImGui_ImplSDL2_ProcessEvent(e);
}

// Called by bme (via bme_input_capture_hook): tells the legacy editor to ignore
// input that ImGui is consuming. bit0 = mouse, bit1 = keyboard.
extern "C" int gimgui_input_capture(void) {
    if (!g_imgui_ready) return 0;
    ImGuiIO& io    = ImGui::GetIO();
    int      flags = 0;
    if (io.WantCaptureMouse) flags |= 1;
    if (io.WantCaptureKeyboard) {
        // Let Tab / Shift+Tab through to the action layer even when a text field
        // is focused (NoTabStop on widgets; Tab events are not fed to ImGui).
        if (!ImGui::IsKeyDown(ImGuiKey_Tab)) flags |= 2;
    }
    return flags;
}

bool gimgui_new_ui_active() { return g_show_new_ui; }

bool gimgui_instr_name_editing() { return g_instr_name_edit >= gtui::INSTR_FIRST; }

void gimgui_init() {
    if (g_imgui_ready) return;
    // gfx_renderer can be null under headless/unsupported video drivers.
    if (!win_window || !gfx_renderer) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);

    gimgui_load_font();
    gimgui_apply_style();

    ImGui_ImplSDL2_InitForSDLRenderer(win_window, gfx_renderer);
    ImGui_ImplSDLRenderer2_Init(gfx_renderer);

    g_imgui_ready = true;

    bme_overlay_render_hook = gimgui_overlay_render;
    bme_event_hook          = gimgui_event_process;
    bme_input_capture_hook  = gimgui_input_capture;
}

void gimgui_shutdown() {
    if (!g_imgui_ready) return;

    bme_overlay_render_hook = nullptr;
    bme_event_hook          = nullptr;
    bme_input_capture_hook  = nullptr;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    g_imgui_ready = false;
}
