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
#include <cstdlib>

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
extern bool g_show_new_ui;   // defined in guiflags.cpp (gtcore)

constexpr float kBaseUIFontPx = 18.0f;
constexpr float kMinUIFontPx  = 10.0f;
constexpr float kMaxUIFontPx  = 48.0f;
float           g_font_size_px = kBaseUIFontPx;

// Instrument-name overlay editor (InputText while active).
int  g_instr_name_edit        = -1;    // instrument index 1..3F, or -1
int  g_instr_name_want_focus  = -1;    // request keyboard focus once when opening
bool g_instr_name_item_active = false; // InputText had focus last frame
bool g_instr_name_had_focus   = false; // InputText had focus on a prior frame
char g_instr_name_buf[gtui::INSTR_NAME_MAX + 1];

// Song metadata field focus (0=name, 1=author, 2=copyright).
int  g_song_field_want_focus  = -1;
bool g_song_field_item_active = false;

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

void gimgui_song_field_begin(int field) {
    if (field < 0 || field > 2) return;
    g_song_field_want_focus = field;
}

bool gimgui_song_field_editing() { return g_song_field_item_active; }

bool gimgui_instr_name_editing() { return g_instr_name_edit >= gtui::INSTR_FIRST; }

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

// Extra inset for the Song metadata form (beyond kPanelBodyPad).
constexpr float kSongFormPadY    = 6.0f;
constexpr float kSongLabelGap    = 10.0f;

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

float gimgui_button_width(int hex_digits) {
    return gimgui_mono_width(hex_digits) + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// Fixed left-column width shared by Song + Order List (content-driven, not % of window).
float gimgui_order_grid_width() {
    constexpr int kMaxChans = 6;
    return gimgui_mono_width(4) + (float)kMaxChans * gimgui_mono_width(2) +
           (float)(kMaxChans - 1) * gimgui_mono_advance(); // 2-char cells + 1-char gaps
}

float gimgui_song_content_width() {
    const float labelW = gimgui_mono_width(9);
    const float inputW = gimgui_mono_width(gtui::SONG_STR_MAX) + ImGui::GetStyle().FramePadding.x * 2.0f;
    return labelW + kSongLabelGap + inputW;
}

float gimgui_left_column_width() {
    const float inner = (gimgui_order_grid_width() > gimgui_song_content_width())
                            ? gimgui_order_grid_width()
                            : gimgui_song_content_width();
    return inner + kPanelBodyPad * 2.0f;
}

float gimgui_song_panel_height() {
    const float panelHeader = ImGui::GetTextLineHeight() + kPanelBodyPad * 2.0f;
    const float frameH      = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
    const float gap         = ImGui::GetStyle().ItemSpacing.y;
    return panelHeader + 4.0f + kPanelBodyPad + kSongFormPadY + 3.0f * frameH + 2.0f * gap + kSongFormPadY +
           kPanelBodyPad;
}

// Fixed right-side panel widths (content-driven, not % of window).
float gimgui_instruments_grid_width() {
    const float charW = gimgui_mono_advance();
    return gimgui_mono_width(3) + gimgui_mono_width(gtui::INSTR_NAME_MAX) + charW +
           (float)gtui::INSTR_FIELDS * gimgui_mono_width(3);
}

// One bordered table column: 8-char grid + vertical scrollbar + child chrome.
float gimgui_table_column_width() {
    const ImGuiStyle& style = ImGui::GetStyle();
    return gimgui_mono_width(8) + style.ScrollbarSize + style.WindowPadding.x * 2.0f +
           style.ChildBorderSize * 2.0f;
}

float gimgui_tables_row_width() {
    const int         n    = gtui::table_count();
    const float       colW = gimgui_table_column_width();
    const float       gap  = ImGui::GetStyle().ItemSpacing.x;
    return (float)n * colW + (float)(n - 1) * gap;
}

float gimgui_instruments_panel_width() { return gimgui_instruments_grid_width() + kPanelBodyPad * 2.0f; }

float gimgui_tables_panel_width() { return gimgui_tables_row_width() + kPanelBodyPad * 2.0f; }

// Reusable scaffold for a scrolling, virtualized monospace grid body (used by
// the pattern editor and each SID table). Handles the child window, content
// reservation, row virtualization, no-lag cursor-follow, and click hit-testing.
// The caller supplies the cell/row content and click handling; all colours and
// per-cell layout live there.
//   rows      : total row count
//   rowW      : content width
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
                      OnClick     onClick,
                      bool        h_scroll = true,
                      int*        out_view_row = nullptr) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(id, ImVec2(0, 0), false, kNoNavWindowFlags);

    if (headerBandH > 0.0f) {
        ImDrawList*  hdl = ImGui::GetWindowDrawList();
        const ImVec2 hp  = ImGui::GetCursorScreenPos();
        headerDraw(hdl, hp.x, hp.y);
        ImGui::Dummy(ImVec2(rowW, headerBandH));
        ImGui::Separator();
    }

    ImGuiWindowFlags scrollFlags = kNoNavWindowFlags;
    if (h_scroll) scrollFlags |= ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("##scroll", ImVec2(0, 0), false, scrollFlags);
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
    if (out_view_row) *out_view_row = firstRow;
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
                      OnClick     onClick,
                      bool        h_scroll = true,
                      int*        out_view_row = nullptr) {
    return gimgui_grid_body(id, rows, rowW, lineH, followRow, [](ImDrawList*, float, float) {}, 0.0f, -1, [](float, float) {}, drawRow, onClick, h_scroll, out_view_row);
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
                      OnClick     onClick,
                      bool        h_scroll = true,
                      int*        out_view_row = nullptr) {
    return gimgui_grid_body(id, rows, rowW, lineH, followRow, headerDraw, headerBandH, -1, [](float, float) {}, drawRow, onClick, h_scroll, out_view_row);
}

// Draw one SID table as an independently-scrolling column: a fixed header over
// a virtualized scrolling body (fills the available height). Only the active
// table auto-follows its cursor; the others keep their own scroll, matching the
// legacy's per-table independent scrolling.
void gimgui_draw_one_table(int t, float colW, float charW, float lineH) {
    const ImU32      cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32      cSelect     = IM_COL32(48, 96, 200, 110);
    const ImU32      cInstrSel   = IM_COL32(100, 210, 130, 110); // legacy instrument/table chain
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
    static const char* kTableNames[] = { "Wave", "Pulse", "Filter", "Speed" };
    ImGui::TextUnformatted(kTableNames[t]);
    ImGui::Separator();

    const float cellW8 = gimgui_mono_width(8);
    int         viewRow = 0;
    gimgui_grid_body(
        "body",
        tlen,
        cellW8,
        lineH,
        active ? curPos : -1,
        [&](ImDrawList* dl, int r, float x, float y) {
            char buf[16];
            if (gtui::table_row_uses_selected_instrument(t, r))
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cInstrSel);
            if (markTab == t && r >= markLo && r <= markHi)
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cSelect);
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
            const int off = (int)(localX / charW);
            int       col;
            if (off <= 3)
                col = 0; // index area → left high nibble
            else if (off == 4)
                col = 1;
            else if (off <= 6)
                col = 2;
            else
                col = 3;
            gtui::table_set_cursor(t, r, col);
        },
        false,
        &viewRow);
    gtui::table_set_view(t, viewRow);

    ImGui::EndChild();
}

// Accent/section colours for the fixed tracker layout (theme system is M7).
const ImU32 kAppBg          = IM_COL32(18, 20, 24, 255); // gutter/background
const ImU32 kHeaderBg       = IM_COL32(30, 44, 60, 255);
const ImU32 kHeaderBgActive = IM_COL32(52, 98, 158, 255);
const ImU32 kHeaderTx       = IM_COL32(150, 168, 186, 255);
const ImU32 kHeaderTxActive = IM_COL32(244, 250, 255, 255);

constexpr ImVec2 kChromeWindowPadBase(8.0f, 4.0f);

ImVec2 gimgui_chrome_window_pad() {
    const float s = g_font_size_px / kBaseUIFontPx;
    return ImVec2(kChromeWindowPadBase.x * s, kChromeWindowPadBase.y * s);
}

float gimgui_chrome_row_h() {
    return ImGui::GetFrameHeight() + gimgui_chrome_window_pad().y * 2.0f;
}


bool gimgui_button(const char* label, int max_chars = 0) {
    if (max_chars <= 0) return ImGui::Button(label);
    return ImGui::Button(label, ImVec2(gimgui_button_width(max_chars), 0.0f));
}

bool gimgui_begin_chrome_bar(const char* id, ImVec2 pos, ImVec2 size) {
    const ImVec2 pad = gimgui_chrome_window_pad();
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pad);
    if (!ImGui::Begin(id, nullptr, kChromeWindowFlags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return false;
    }
    ImGui::PopStyleVar();
    ImGui::AlignTextToFramePadding();
    return true;
}

// Monospace label in a fixed column width (pairs with chrome buttons on the same row).
void gimgui_chrome_mono_label(const char* text, int cols) {
    const float colW = gimgui_mono_width(cols);
    const ImVec2  p  = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(colW, ImGui::GetFrameHeight()));
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(p.x, p.y + ImGui::GetStyle().FramePadding.y),
        ImGui::GetColorU32(ImGuiCol_Text),
        text ? text : "");
}

// Same column width, text centered
void gimgui_chrome_mono_label_centered(const char* text, int cols) {
    const float colW = gimgui_mono_width(cols);
    const float rowH = ImGui::GetFrameHeight();
    const ImVec2  p  = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(colW, rowH));
    const ImVec2 ts = ImGui::CalcTextSize(text);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(p.x + (colW - ts.x) * 0.5f, p.y + (rowH - ts.y) * 0.5f),
        ImGui::GetColorU32(ImGuiCol_Text),
        text);
}

void gimgui_chrome_same_line_right(float item_w) {
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - item_w);
}

// A button that renders in its "active" colour while toggled on (Follow, Loop).
bool gimgui_toggle_button(const char* label, bool active, int max_chars = 0) {
    if (active) {
        const ImVec4 on = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Button, on);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on);
    }
    bool clicked = gimgui_button(label, max_chars);
    if (active) ImGui::PopStyleColor(2);
    return clicked;
}

bool gimgui_stepper(const char* id_str, int& value, int min_val, int max_val, const char* format = "%d", int max_chars = 0) {
    const int old_value = value;

    ImGui::PushID(id_str);
    ImGui::BeginGroup();

    ImGui::BeginDisabled(value <= min_val);
    if (ImGui::SmallButton("-")) value--;
    ImGui::EndDisabled();
    ImGui::SameLine();

    char buf[16];
    snprintf(buf, sizeof buf, format, value);
    gimgui_chrome_mono_label_centered(buf, max_chars > 0 ? max_chars : (int)strlen(buf));

    ImGui::SameLine();
    ImGui::BeginDisabled(value >= max_val);
    if (ImGui::SmallButton("+")) value++;
    ImGui::EndDisabled();

    ImGui::EndGroup();
    ImGui::PopID();

    return value != old_value;
}

void gimgui_vertical_separator() {
    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "|");
    ImGui::SameLine();
}

void gimgui_draw_player_status(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_chrome_bar("##player", pos, size)) return;

    {
        float vol = gtui::transport_volume();
        ImGui::SetNextItemWidth(gimgui_mono_width(16));
        if (ImGui::SliderFloat("##vol", &vol, 0.0f, gtui::kMasterVolumeMax, "Volume %.1f"))
            gtui::transport_set_volume(vol);
    }
    ImGui::SameLine();
    if (gimgui_button(gtui::player_ntsc() ? "NTSC" : "PAL", 4))
        gtui::player_toggle_ntsc();
    ImGui::SameLine();

    if (gimgui_button(gtui::player_sid_model_8580() ? "8580" : "6581"))
        gtui::player_toggle_sid_model();

    gimgui_vertical_separator();
    ImGui::Text("SID x%d", gtui::player_sid_chips());
    ImGui::SameLine();
    if (gimgui_button(gtui::transport_stereo_label(), 3)) gtui::transport_cycle_stereo();

    {
        const int chips = gtui::player_sid_chips();
        for (int c = 0; c < chips; c++) {
            ImGui::SameLine();
            int pan = gtui::player_sid_pan(c);
            ImGui::PushID(c);
            ImGui::SetNextItemWidth(gimgui_mono_width(5));
            if (ImGui::SliderInt("##pan", &pan, 0, 15, "%X"))
                gtui::player_set_sid_pan(c, pan);
            ImGui::PopID();
        }
    }

    gimgui_vertical_separator();
    if (gimgui_toggle_button("SID64", gtui::player_sidtracker64()))
        gtui::player_toggle_sidtracker64();

    gimgui_vertical_separator();
    ImGui::TextUnformatted("Speed");
    ImGui::SameLine();
    if (ImGui::SmallButton("-")) gtui::player_multiplier_prev();
    ImGui::SameLine();
    gimgui_chrome_mono_label_centered(gtui::player_speed_label(), 3);
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) gtui::player_multiplier_next();

    gimgui_vertical_separator();

    if (gimgui_toggle_button("FV", gtui::player_fine_vibrato())) gtui::player_toggle_fine_vibrato();
    ImGui::SameLine();
    if (gimgui_toggle_button("PO", gtui::player_optimize_pulse())) gtui::player_toggle_optimize_pulse();
    ImGui::SameLine();
    if (gimgui_toggle_button("RO", gtui::player_optimize_realtime())) gtui::player_toggle_optimize_realtime();

    gimgui_vertical_separator();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("HR");
    ImGui::SameLine();
    {
        unsigned hr = (unsigned)gtui::player_hr_adparam();
        ImGui::PushItemWidth(gimgui_button_width(4));
        if (ImGui::InputScalar("##hr", ImGuiDataType_U32, &hr, nullptr, nullptr, "%04X",
                               ImGuiInputTextFlags_CharsHexadecimal))
            gtui::player_set_hr_adparam((int)hr);
        ImGui::PopItemWidth();
    }

    {
        const char* fname = gtui::player_loaded_filename();
        gimgui_chrome_same_line_right(gimgui_mono_width(strlen(fname)));
        ImGui::TextUnformatted(fname);
    }

    ImGui::End();
}

// Context-sensitive decode of the cell under the cursor (legacy infoTextBuffer).
// Same chrome row height as player/transport bars; room for context buttons later.
void gimgui_draw_context_help(ImVec2 pos, ImVec2 size) {
    gtui::context_help_refresh();
    if (!gimgui_begin_chrome_bar("##context_help", pos, size)) return;

    // ImGui::TextDisabled("Info:");
    // ImGui::SameLine();
    ImGui::TextUnformatted(gtui::context_help());

    ImGui::End();
}

// Fixed, non-floating panel. Positioned/sized every frame to tile the app
// window (no title bar, no move/resize) so the UI reads as one cohesive tracker
// rather than a set of ImGui windows. Draws an edge-to-edge section header.
// Optional @p header_right is drawn right-aligned (subtune, octave, etc.).
// Returns true when the body should be drawn (caller must still call End()).
bool gimgui_begin_panel(const char* title,
                        ImVec2      pos,
                        ImVec2      size,
                        bool        active,
                        const char* header_right = nullptr) {
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
    dl->AddText(ImVec2(wp.x + hpad, wp.y + hpad), active ? kHeaderTxActive : kHeaderTx, title);

    if (header_right && header_right[0]) {
        const ImVec2 ts  = ImGui::CalcTextSize(header_right);
        const ImU32  col = active ? kHeaderTxActive : kHeaderTx;
        dl->AddText(ImVec2(wp.x + ww - hpad - ts.x, wp.y + hpad), col, header_right);
    }

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
    const float colW  = gimgui_table_column_width();

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
    if (!gimgui_begin_panel("Pattern",
                            pos,
                            size,
                            gtui::edit_panel() == gtui::EditPanelPattern)) {
        gimgui_end_panel();
        return;
    }

    if (gimgui_toggle_button("REC", gtui::pattern_record_mode()))
        gtui::pattern_toggle_record_mode();

    gimgui_vertical_separator();
    ImGui::TextUnformatted("ADV");
    ImGui::SameLine();
    if (gimgui_button(gtui::pattern_autoadvance_label(), 4))
        gtui::pattern_cycle_autoadvance();

    gimgui_vertical_separator();
    ImGui::TextUnformatted("OCT");
    ImGui::SameLine();
    {
        int oct = gtui::pattern_octave();
        if (gimgui_stepper("##oct", oct, gtui::kPatternOctaveMin, gtui::kPatternOctaveMax, "%d", 1))
            gtui::pattern_set_octave(oct);
    }

    gimgui_vertical_separator();
    ImGui::TextUnformatted("STEP");
    ImGui::SameLine();
    {
        int step = gtui::pattern_step();
        if (gimgui_stepper("##step", step, gtui::kPatternStepMin, gtui::kPatternStepMax, "%02d", 2))
            gtui::pattern_set_step(step);
    }


    // new line
    ImGui::Separator();

    // Colors (hardcoded for now; the theme system is M7).
    const ImU32 cBeat       = IM_COL32(255, 255, 255, 10);
    const ImU32 cCursorRow  = IM_COL32(255, 255, 100, 20);
    const ImU32 cPlayRow    = IM_COL32(80, 190, 90, 80);   // per-channel playhead
    const ImU32 cSelect     = IM_COL32(48, 96, 200, 110);  // Shift+Up/Down mark
    const ImU32 cInstrSel   = IM_COL32(100, 210, 130, 110); // matches selected instrument
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
    const int curRow  = gtui::pattern_cursor_row();
    const int curChn  = gtui::pattern_cursor_chn();
    const int curCol  = gtui::pattern_cursor_col();
    const int selInstr = gtui::instr_current();

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
            snprintf(hbuf, sizeof hbuf, "%X:%02X", gtui::pattern_actual_channel(c), gtui::pattern_number(c));
            // snprintf(hbuf, sizeof hbuf, "%02X", gtui::pattern_number(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + c * chanW, hp.y), cHeader, hbuf);
        }
        ImGui::Dummy(ImVec2(rowNumW + chans * chanW, lineH));
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

                // Highlight instrument IDs that match the selected instrument.
                if (selInstr >= gtui::INSTR_FIRST && cell.instr == selInstr) {
                    const float ix = cx + noteW;
                    dl->AddRectFilled(ImVec2(ix, y), ImVec2(ix + gimgui_mono_width(2), y + lineH), cInstrSel);
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
// shared grid scaffold. Cells are pattern numbers or commands (+/-/R); loop
// marker row shows "==". Keyboard editing flows through the legacy order editor; clicking places the
// cursor (gtui::order_set_cursor).
void gimgui_draw_orderlist(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Order List",
                            pos,
                            size,
                            gtui::edit_panel() == gtui::EditPanelOrder)) {
        gimgui_end_panel();
        return;
    }
    ImGui::AlignTextToFramePadding();

    ImGui::TextUnformatted("SUB");
    ImGui::SameLine();
    int sub = gtui::order_subtune();
    if (gimgui_stepper("##sub", sub, gtui::kOrderSubtuneMin, gtui::kOrderSubtuneMax, "%02X", 2))
        gtui::order_set_subtune(sub);

    gimgui_vertical_separator();
    ImGui::TextUnformatted("BNK");
    ImGui::SameLine();
        int bnk = gtui::order_song_bank() + 1;
        if (gimgui_stepper("##bnk", bnk, 1, gtui::order_song_bank_count(), "%d", 2))
            gtui::order_set_song_bank(bnk - 1);

    // new line
    ImGui::Separator();

    const ImU32 cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32 cSelect     = IM_COL32(48, 96, 200, 110);
    const ImU32 cSynced     = IM_COL32(100, 210, 130, 110); // legacy CORDER_INST_TABLE_EDITING
    const ImU32 cPlayRow    = IM_COL32(80, 190, 90, 80);
    const ImU32 cCursorFill = IM_COL32(235, 225, 120, 70);
    const ImU32 cCursorEdge = IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum     = IM_COL32(120, 140, 160, 255);
    const ImU32 cPat        = IM_COL32(224, 230, 238, 255);
    const ImU32 cCmd        = IM_COL32(235, 180, 90, 255);
    const ImU32 cEnd        = IM_COL32(150, 160, 175, 255);
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
    const float rowNumW = gimgui_mono_width(4); // "POS "
    const float cellW   = gimgui_mono_width(2); // two nibbles per channel
    const float colGap  = charW;                // one char between columns
    const float colPitch = cellW + colGap;
    const float totalW  = rowNumW + (float)chans * cellW + (float)(chans - 1) * colGap;

    int selRow[8], endRow[8], playRow[8];
    for (int c = 0; c < chans && c < 8; c++) {
        selRow[c]  = gtui::order_selected_row(c);
        endRow[c]  = gtui::order_range_end_row(c);
        playRow[c] = gtui::order_play_row(c);
    }

    // Fixed header: empty column + channel numbers.
    {
        gimgui_snap_body_pad_x();
        ImDrawList* hdl = ImGui::GetWindowDrawList();
        ImVec2      hp  = ImGui::GetCursorScreenPos();
        char        hbuf[16];
        for (int c = 0; c < chans; c++) {
            snprintf(hbuf, sizeof hbuf, "%X", gtui::order_actual_channel(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + (float)c * colPitch, hp.y), cHeader, hbuf);
        }
        ImGui::Dummy(ImVec2(totalW, lineH));
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
                const float cx = x + rowNumW + (float)c * colPitch;

                gtui::OrderCell cell = gtui::order_cell(c, r);
                const float cw = cellW;

                if (r == playRow[c]) dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cw, y + lineH), cPlayRow);
                if (r == selRow[c] || r == endRow[c])
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cw, y + lineH), cSynced);
                if (markChn == c && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cw, y + lineH), cSelect);
                if (r == curRow && c == curChn && cell.kind != 3) {
                    float cs = cx + (curCol < 0 ? 0 : (curCol > 1 ? 1 : curCol)) * charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                }

                if (!cell.valid) continue;
                if (cell.kind == 3) {
                    dl->AddText(ImVec2(cx, y), cEnd, "==");
                    continue;
                }
                ImU32 col = cell.kind == 2 ? cCmd : cPat;
                dl->AddText(ImVec2(cx, y), col, cell.text);
            }
        },
        [&](int r, float localX) {
            float rx = localX - rowNumW;
            if (rx < 0) return;
            int c = (int)(rx / colPitch);
            if (c < 0 || c >= chans) return;
            float cx = rx - (float)c * colPitch;
            if (cx >= cellW) return;
            int off = (int)(cx / charW);
            if (off > 1) off = 1;
            gtui::order_set_cursor(c, r, off);
        });

    gimgui_end_panel();
}


// Instrument list (01..3F): custom grid like pattern/order. Hex fields use the
// legacy nibble editor; the name column uses an on-demand InputText in the grid.
void gimgui_draw_instruments(ImVec2 pos, ImVec2 size) {
    char hdr[16];
    snprintf(hdr, sizeof hdr, "INS %02X", gtui::instr_current());
    if (!gimgui_begin_panel("Instruments", pos, size, gtui::edit_panel() == gtui::EditPanelInstrument, hdr)) {
        gimgui_end_panel();
        return;
    }

    gtui::instr_clamp_selection();

    static const char* fieldLabel[gtui::INSTR_FIELDS] = { "AD", "SR", "WP", "PP", "FP",
                                                          "VP", "VD", "GT", "1W", "PN" };

    const ImU32 cCursorRow  = IM_COL32(255, 255, 255, 20);
    const ImU32 cInstrSel   = IM_COL32(100, 210, 130, 110); // selected instrument id
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
    const float idxW    = gimgui_mono_width(3);
    const float nameW   = gimgui_mono_width(gtui::INSTR_NAME_MAX);
    const float nameGap = charW; // one char between name and AD (hex cols use 3 = "XX ")
    const float hexW    = gimgui_mono_width(3);
    const float totalW  = gimgui_instruments_grid_width();

    // Cumulative column edges (shared by header + rows — no repeated multiply).
    float colX[3 + gtui::INSTR_FIELDS];
    colX[0] = 0.0f;
    colX[1] = idxW;
    colX[2] = idxW + nameW;
    for (int f = 0; f < gtui::INSTR_FIELDS; f++) colX[3 + f] = colX[2] + nameGap + (float)f * hexW;

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
            ImGui::PushItemWidth(nameW);
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

            if (r == curRow) {
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);

                dl->AddRectFilled(ImVec2(x + colX[0], y),
                                  ImVec2(x + colX[0] + gimgui_mono_width(2), y + lineH),
                                  cInstrSel);
            }

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
        },
        false);

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
        { "Name",      gtui::song_name,      gtui::song_set_name },
        { "Author",    gtui::song_author,    gtui::song_set_author },
        { "Copyright", gtui::song_copyright, gtui::song_set_copyright },
    };

    const float inputW    = gimgui_mono_width(gtui::SONG_STR_MAX) + ImGui::GetStyle().FramePadding.x * 2.0f;
    const bool  namesMode = (gtui::edit_panel() == gtui::EditPanelNames);

    g_song_field_item_active = false;
    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
    for (int f = 0; f < 3; f++) {
        char buf[gtui::SONG_STR_MAX + 1];
        snprintf(buf, sizeof buf, "%.*s", (int)gtui::SONG_STR_MAX, fields[f].get());

        gimgui_snap_body_pad_x();
        gimgui_chrome_mono_label(fields[f].label, 9);
        ImGui::SameLine();

        ImGui::PushID(f);
        ImGui::PushItemWidth(inputW);
        if (namesMode && f == gtui::names_field()) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (g_song_field_want_focus == f) ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##v", buf, sizeof buf)) fields[f].set(buf); // metadata: commit as typed (no undo, like legacy)
        if (ImGui::IsItemActive()) g_song_field_item_active = true;
        if (ImGui::IsItemActivated()) gtui::names_set_field(f);
        if (g_song_field_want_focus == f && ImGui::IsItemActive()) g_song_field_want_focus = -1;
        if (namesMode && f == gtui::names_field()) ImGui::PopStyleVar();
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
    if (!gimgui_begin_chrome_bar("##transport", pos, size)) return;


    if (gimgui_button("<<")) gtui::transport_rewind();
    ImGui::SameLine();
    if (gimgui_button(">")) gtui::transport_play_start();
    ImGui::SameLine();
    if (gimgui_button("Pat")) gtui::transport_play_pattern();
    ImGui::SameLine();
    if (gimgui_button(">>")) gtui::transport_ff();
    ImGui::SameLine();
    if (gimgui_button("Stop")) gtui::transport_stop();

    ImGui::SameLine();
    if (gimgui_toggle_button("Follow", gtui::transport_follow(), 6)) gtui::transport_toggle_follow();
    ImGui::SameLine();
    if (gimgui_toggle_button("Loop", gtui::transport_loop(), 6)) gtui::transport_toggle_loop();

    ImGui::SameLine();
    {
        char tbuf[32];
        snprintf(tbuf,
                 sizeof tbuf,
                 "%s %02d:%02d / %02d:%02d",
                 gtui::transport_playing() ? "|>" : "||",
                 gtui::transport_time_min(),
                 gtui::transport_time_sec(),
                 gtui::transport_total_min(),
                 gtui::transport_total_sec());
        gimgui_chrome_mono_label(tbuf, 20);
    }


    gimgui_chrome_same_line_right(gimgui_button_width(6));
    if (gimgui_button("Legacy")) g_show_new_ui = false;

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

} // namespace

// Load the bundled monospace font at @p sizePx. Searches a few locations (next to
// the binary, the build-time source assets dir, then the cwd); falls back to the
// default font if none are found.
void gimgui_load_font_at(float sizePx) {
    ImGuiIO&    io     = ImGui::GetIO();
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
        if (io.Fonts->AddFontFromFileTTF(path, sizePx)) {
            io.FontDefault = io.Fonts->Fonts.back();
            return;
        }
    }
    io.Fonts->AddFontDefault();
    io.FontDefault = io.Fonts->Fonts.back();
}

// Flat, professional dark theme: square windows, minimal borders, a blue accent.
// Spacing scales with UI font size so chrome bars stay proportional.
void gimgui_apply_style() {
    ImGuiStyle& s       = ImGui::GetStyle();
    const float scale   = g_font_size_px / kBaseUIFontPx;
    // ImGui 1.92+ rasterizes glyphs at FontSizeBase, not AddFontFromFileTTF()'s size.
    s.FontSizeBase            = g_font_size_px;
    s._NextFrameFontSizeBase  = g_font_size_px;
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
    s.WindowPadding     = ImVec2(8.0f * scale, 6.0f * scale);
    s.FramePadding      = ImVec2(6.0f * scale, 3.0f * scale);
    s.ItemSpacing       = ImVec2(6.0f * scale, 4.0f * scale);
    s.ItemInnerSpacing  = ImVec2(4.0f * scale, 4.0f * scale);
    s.ScrollbarSize     = 12.0f * scale;

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

void gimgui_reload_font() {
    if (!g_imgui_ready) return;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->ClearFonts();
    gimgui_load_font_at(g_font_size_px);
    gimgui_apply_style();
}

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

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Smaller font", nullptr, false, g_font_size_px > kMinUIFontPx))
                gimgui_adjust_font_size(-2);
            if (ImGui::MenuItem("Larger font", nullptr, false, g_font_size_px < kMaxUIFontPx))
                gimgui_adjust_font_size(+2);
            ImGui::Separator();
            char szbuf[32];
            snprintf(szbuf, sizeof szbuf, "Font size: %.0f px", g_font_size_px);
            ImGui::MenuItem(szbuf, nullptr, false, false);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }


    // Fixed tiled layout filling the whole window. Panels are opaque and cover the legacy screen; a full-window
    // background fill hides the legacy in the gutters between panels. Five columns:
    //   left       : Song (top) + Order List (bottom), fixed width
    //   centre     : Pattern (fills remaining width)
    //   right pair : Instruments + Tables side by side, each fixed width
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos,
                                                  ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                  kAppBg);

    const ImVec2 vo = vp->WorkPos;  // origin below the menu bar
    const ImVec2 vs = vp->WorkSize; // area excluding the menu bar
    const float  g  = 3.0f;         // gutter between panels

    const float chromeH = gimgui_chrome_row_h();

    gimgui_draw_player_status(vo, ImVec2(vs.x, chromeH));
    gimgui_draw_transport(ImVec2(vo.x, vo.y + chromeH + g), ImVec2(vs.x, chromeH));

    const ImVec2 o = ImVec2(vo.x, vo.y + chromeH * 2.0f + 2 * g);
    const ImVec2 s = ImVec2(vs.x, vs.y - chromeH * 3.0f - 3 * g);

    const float leftW  = gimgui_left_column_width();
    const float songH  = gimgui_song_panel_height();
    const float insW   = gimgui_instruments_panel_width();
    const float tblW   = gimgui_tables_panel_width();
    const float patW   = s.x - leftW - insW - tblW - 4 * g;
    const float leftX  = o.x;
    const float patX   = o.x + leftW + g;
    const float insX   = o.x + leftW + patW + 2 * g;
    const float tblX   = insX + insW + g;

    gimgui_draw_song(ImVec2(leftX, o.y), ImVec2(leftW, songH));
    gimgui_draw_orderlist(ImVec2(leftX, o.y + songH + g), ImVec2(leftW, s.y - songH - g));
    gimgui_draw_pattern(ImVec2(patX, o.y), ImVec2(patW, s.y));
    gimgui_draw_instruments(ImVec2(insX, o.y), ImVec2(insW, s.y));
    gimgui_draw_tables(ImVec2(tblX, o.y), ImVec2(tblW, s.y));

    gimgui_draw_context_help(ImVec2(vo.x, vo.y + vs.y - chromeH), ImVec2(vs.x, chromeH));

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

void gimgui_init() {
    g_show_new_ui = true;
    if (g_imgui_ready) return;
    // gfx_renderer can be null under headless/unsupported video drivers.
    if (!win_window || !gfx_renderer) return;

    if (const char* env = std::getenv("GTULTRA_UI_FONT_SIZE")) {
        float px = (float)std::atof(env);
        if (px >= kMinUIFontPx && px <= kMaxUIFontPx) g_font_size_px = px;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);

    gimgui_load_font_at(g_font_size_px);
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

float gimgui_font_size() { return g_font_size_px; }

void gimgui_set_font_size(float px) {
    if (px < kMinUIFontPx) px = kMinUIFontPx;
    if (px > kMaxUIFontPx) px = kMaxUIFontPx;
    if (px == g_font_size_px) return;
    g_font_size_px = px;
    gimgui_reload_font();
}

void gimgui_adjust_font_size(int delta_px) { gimgui_set_font_size(g_font_size_px + (float)delta_px); }
