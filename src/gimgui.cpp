//
// GTUltra Dear ImGui integration layer.
//
// Strategy (see docs/UI_MIGRATION_PLAN.md): reuse gplatform's SDL2 window + renderer.
// Each frame gfx_present() clears and invokes the overlay hook; ImGui draws
// the full UI there.
//
#define IMGUI_DEFINE_MATH_OPERATORS
#include "gimgui.hpp"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "embed.hpp"
#include "gactions.hpp"
#include "gfile.hpp"
#include "ggfx.hpp"
#include "ghelp.hpp"
#include "guicolors.hpp"
#include "guimodel.hpp" // SDL-free bridge to the legacy model
#include "gwin.hpp"
#include "imgui.h"
#include "log.hpp"

#include <SDL.h>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

extern char appFileName[MAX_PATHNAME];
void        createFilename(char* filePath, char* newfileName, const char* filename);

bool g_imgui_ready     = false;
bool g_show_demo       = false; // toggleable ImGui reference/demo window
bool g_show_help       = false;
int  g_help_tab        = 0;
bool g_help_select_tab = false; // one-shot: force tab when opening
int  g_help_tab_bar_id = 0;     // bump on open so ImGui does not restore the old tab
bool g_help_hovered    = false; // pointer over help window (blocks editor wheel)

// Panel under the mouse for wheel → row navigation (-1 = none / non-editor).
int g_hovered_edit_panel = -1;

constexpr float kBaseUIFontPx  = 18.0f;
constexpr float kMinUIFontPx   = 10.0f;
constexpr float kMaxUIFontPx   = 48.0f;
float           g_font_size_px = kBaseUIFontPx;
// Instrument-name editing (per-row InputText, like song metadata fields).
int  g_instr_name_edit        = -1; // instrument index 1..3F, or -1
int  g_instr_name_want_focus  = -1; // one-shot keyboard-focus request
bool g_instr_name_item_active = false;

// Song metadata field focus (0=name, 1=author, 2=copyright).
int  g_song_field_edit        = -1; // field in text-edit mode, or -1 for selection only
int  g_song_field_want_focus  = -1; // one-shot keyboard-focus request (like instr name)
bool g_song_field_item_active = false;

bool gimgui_song_field_editing() { return g_song_field_item_active; }

bool gimgui_instr_name_editing() { return g_instr_name_item_active; }

namespace {


void gimgui_draw_help_notes(std::span<const std::string_view> notes) {
    if (notes.empty()) return;
    for (std::string_view line : notes) ImGui::TextWrapped("%.*s", (int)line.size(), line.data());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

// Keycap-style chip so alternate bindings are not joined with "/" (looks like the slash key).
void gimgui_draw_key_chip(const char* label, bool warn = false) {
    const ImVec2 pad(6.0f, 2.0f);
    const ImVec2 text = ImGui::CalcTextSize(label);
    const ImVec2 size(text.x + pad.x * 2.0f, text.y + pad.y * 2.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    ImDrawList*  dl   = ImGui::GetWindowDrawList();
    const ImU32  bg   = warn ? ImGui::GetColorU32(ImGuiCol_HeaderActive) : ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32  edge = warn ? ImGui::GetColorU32(ImGuiCol_CheckMark) : ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32  fg   = ImGui::GetColorU32(ImGuiCol_Text);
    dl->AddRectFilled(p0, p1, bg, 0.0f);
    dl->AddRect(p0, p1, edge, 0.0f);
    dl->AddText(ImVec2(p0.x + pad.x, p0.y + pad.y), fg, label);
    ImGui::Dummy(size);
}

void gimgui_draw_help_keybinds(gtaction::Ctx ctx) {
    const auto rows      = gtaction::binding_rows_for(ctx);
    const auto conflicts = gtaction::conflicts_for(ctx);

    std::unordered_set<gtaction::Chord> conflict_chords;
    conflict_chords.reserve(conflicts.size());
    for (const gtaction::ChordConflict& cf : conflicts) conflict_chords.insert(cf.chord);

    if (!conflicts.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::TextWrapped("Chord conflicts in this context (last binding wins):");
        for (const gtaction::ChordConflict& cf : conflicts) {
            const std::string chord = gtaction::format_chord(cf.chord);
            ImGui::Bullet();
            ImGui::SameLine(0.0f, 4.0f);
            gimgui_draw_key_chip(chord.c_str(), true);
            ImGui::SameLine(0.0f, 8.0f);
            for (std::size_t i = 0; i < cf.claimants.size(); ++i) {
                if (i) {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::TextUnformatted(" → ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                const bool win = (i + 1 == cf.claimants.size());
                if (win) ImGui::Text("%s", gtaction::action_label(cf.claimants[i]));
                else ImGui::TextDisabled("%s", gtaction::action_label(cf.claimants[i]));
                if (i + 1 < cf.claimants.size()) ImGui::SameLine(0.0f, 0.0f);
            }
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    if (rows.empty()) {
        ImGui::TextDisabled("No keybindings in this context.");
        return;
    }

    if (ImGui::BeginTable("##help_keys",
                          2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("desc", ImGuiTableColumnFlags_WidthStretch);
        for (const gtaction::BindingRow& row : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            for (std::size_t i = 0; i < row.chords.size(); ++i) {
                if (i) ImGui::SameLine(0.0f, 6.0f);
                const std::string label = gtaction::format_chord(row.chords[i]);
                const bool        warn  = conflict_chords.contains(row.chords[i]);
                gimgui_draw_key_chip(label.c_str(), warn);
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", gtaction::action_label(row.action));
        }
        ImGui::EndTable();
    }
}

// Split "Label — body" or "Label: body" (short labels only) for reference prose.
bool gimgui_help_split_labeled(std::string_view line, std::string_view* label, std::string_view* body) {
    constexpr std::string_view kEmDash = " \xE2\x80\x94 "; // " — "
    const std::size_t          em      = line.find(kEmDash);
    if (em != std::string_view::npos && em > 0 && em < 40) {
        *label = line.substr(0, em);
        *body  = line.substr(em + kEmDash.size());
        return !body->empty();
    }

    const std::size_t colon = line.find(": ");
    if (colon != std::string_view::npos && colon > 0 && colon < 40) {
        *label = line.substr(0, colon);
        *body  = line.substr(colon + 2);
        return !body->empty();
    }
    return false;
}

void gimgui_draw_help_reference_line(std::string_view line) {
    // "Command 0XY: …" → chip + wrapped description
    constexpr std::string_view kCmd = "Command ";
    if (line.size() > kCmd.size() && line.substr(0, kCmd.size()) == kCmd) {
        const std::size_t colon = line.find(": ");
        if (colon != std::string_view::npos && colon > kCmd.size()) {
            const std::string      code(line.substr(kCmd.size(), colon - kCmd.size()));
            const std::string_view body = line.substr(colon + 2);
            gimgui_draw_key_chip(code.c_str());
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(body.data(), body.data() + body.size());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::Spacing();
            ImGui::Spacing();
            return;
        }
    }

    std::string_view label, body;
    if (gimgui_help_split_labeled(line, &label, &body)) {
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::PopStyleColor();
        ImGui::Indent(10.0f);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(body.data(), body.data() + body.size());
        ImGui::PopTextWrapPos();
        ImGui::Unindent(10.0f);
        ImGui::Spacing();
        ImGui::Spacing();
        return;
    }

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(line.data(), line.data() + line.size());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::Spacing();
}

void gimgui_draw_help_reference(std::span<const std::string_view> lines) {
    for (std::string_view line : lines) gimgui_draw_help_reference_line(line);
}

void gimgui_draw_help_topic_body(const gthelp::Topic& topic) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(topic.title.data(), topic.title.data() + topic.title.size());
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (topic.kind == gthelp::Kind::Keybinds && topic.binds) {
        gimgui_draw_help_notes(topic.notes);
        gimgui_draw_help_keybinds(*topic.binds);
    }
    else {
        gimgui_draw_help_reference(topic.body);
    }
}

void gimgui_draw_help_window() {
    g_help_hovered = false;
    if (g_show_help) ImGui::OpenPopup("Help");

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(780.0f, 560.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(480.0f, 320.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::BeginPopupModal("Help", &g_show_help, ImGuiWindowFlags_NoCollapse)) return;

    g_help_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    const auto topics = gthelp::topics();
    if (g_help_tab < 0 || g_help_tab >= (int)topics.size()) g_help_tab = 0;

    char tab_bar_id[32];
    snprintf(tab_bar_id, sizeof tab_bar_id, "##help_tabs_%d", g_help_tab_bar_id);

    if (ImGui::BeginTabBar(tab_bar_id, ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < (int)topics.size(); ++i) {
            const gthelp::Topic& topic = topics[(std::size_t)i];

            ImGuiTabItemFlags flags = 0;
            if (g_help_select_tab && i == g_help_tab) flags |= ImGuiTabItemFlags_SetSelected;

            const std::string tab(topic.tab);
            if (ImGui::BeginTabItem(tab.c_str(), nullptr, flags)) {
                // While forcing the panel-matched tab, don't let an earlier
                // BeginTabItem (stale selection) overwrite g_help_tab.
                if (!g_help_select_tab) g_help_tab = i;
                if (ImGui::BeginChild("##help_body", ImVec2(0, 0), false)) gimgui_draw_help_topic_body(topic);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    g_help_select_tab = false;

    ImGui::EndPopup();
}


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
constexpr float kSongFormPadY = 6.0f;
constexpr float kSongLabelGap = 10.0f;

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

float gimgui_text_width(int cols) { return gimgui_mono_advance() * (float)cols; }

float gimgui_button_width(int hex_digits) {
    return gimgui_text_width(hex_digits) + ImGui::GetStyle().FramePadding.x * 2.0f;
}


bool gimgui_button(const char* label, int max_chars = 0) {
    if (max_chars <= 0) return ImGui::Button(label);
    return ImGui::Button(label, ImVec2(gimgui_button_width(max_chars), 0.0f));
}

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

// Fixed left-column width shared by Song + Order List (content-driven, not % of window).
float gimgui_order_grid_width() {
    constexpr int kMaxChans  = 6;
    constexpr int kCellChars = 5;
    return gimgui_text_width(4) + (float)kMaxChans * gimgui_text_width(kCellChars) +
           (float)(kMaxChans - 1) * gimgui_mono_advance();
}

float gimgui_song_content_width() {
    const float labelW = gimgui_text_width(9);
    const float inputW = gimgui_text_width(gtui::SONG_STR_MAX) + ImGui::GetStyle().FramePadding.x * 2.0f;
    return labelW + kSongLabelGap + inputW;
}

float gimgui_left_column_width() {
    const float inner = (gimgui_order_grid_width() > gimgui_song_content_width()) ? gimgui_order_grid_width()
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
    return gimgui_text_width(3) + gimgui_text_width(gtui::INSTR_NAME_MAX) + charW +
           (float)gtui::INSTR_FIELDS * gimgui_text_width(3);
}

// One bordered table column: 8-char grid + vertical scrollbar + child chrome.
float gimgui_table_column_width() {
    const ImGuiStyle& style = ImGui::GetStyle();
    return gimgui_text_width(8) + style.ScrollbarSize + 1;
}


float gimgui_tables_row_width() {
    const int   n    = gtui::table_count();
    const float colW = gimgui_table_column_width();
    // const float gap = ImGui::GetStyle().ItemSpacing.x;
    return n * colW;
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
//   onGridMouse (optional): richer mouse handling than onClick (order list).
using GridMouseFn =
    std::function<void(int row, float localX, ImGuiMouseButton button, bool double_click, bool dragging)>;

template <class HeaderDraw, class DrawRow, class OnClick>
void gimgui_grid_body(const char*        id,
                      int                rows,
                      float              rowW,
                      float              lineH,
                      int                followRow,
                      HeaderDraw         headerDraw,
                      float              headerBandH,
                      DrawRow            drawRow,
                      OnClick            onClick,
                      bool               h_scroll      = true,
                      int*               out_view_row  = nullptr,
                      const GridMouseFn* on_grid_mouse = nullptr) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(id, ImVec2(0, 0), false, kNoNavWindowFlags);

    if (headerBandH > 0.0f) {
        ImDrawList*  hdl = ImGui::GetWindowDrawList();
        const ImVec2 hp  = ImGui::GetCursorScreenPos();
        headerDraw(hdl, hp.x, hp.y);
        ImGui::Dummy(ImVec2(rowW, headerBandH));
        ImGui::Separator();
    }

    ImGuiWindowFlags scrollFlags = kNoNavWindowFlags | ImGuiWindowFlags_NoScrollWithMouse;
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

    if (ImGui::IsWindowHovered()) {
        const ImGuiIO&  io            = ImGui::GetIO();
        const ImVec2    m             = io.MousePos;
        const int       r             = (int)((m.y - drawTop) / lineH);
        const float     localX        = m.x - origin.x;
        const bool      inRows        = r >= 0 && r < rows;
        constexpr float kHoldDelaySec = 24.f / 60.f; // legacy HOLDDELAY @ 60 Hz

        if (on_grid_mouse) {
            if (inRows) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Left, true, false);
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Left, false, false);
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.MouseDownDuration[0] >= kHoldDelaySec)
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Left, false, true);
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Right, false, false);
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
                         !ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Right, false, true);
                else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Middle, false, false);
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
                         !ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                    (*on_grid_mouse)(r, localX, ImGuiMouseButton_Middle, false, true);
            }
        }
        else if (inRows && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            onClick(r, localX);
        }
    }

    int firstRow = (int)(drawScroll / lineH);
    int lastRow  = (int)((drawScroll + winH) / lineH) + 1;
    if (firstRow < 0) firstRow = 0;
    if (lastRow > rows) lastRow = rows;
    if (out_view_row) *out_view_row = firstRow;
    for (int r = firstRow; r < lastRow; r++) {
        const float rowY = drawTop + r * lineH;
        drawRow(dl, r, origin.x, rowY);
    }

    ImGui::EndChild(); // ##scroll
    ImGui::EndChild(); // id
    ImGui::PopStyleVar();
}

template <class DrawRow, class OnClick>
void gimgui_grid_body(const char*        id,
                      int                rows,
                      float              rowW,
                      float              lineH,
                      int                followRow,
                      DrawRow            drawRow,
                      OnClick            onClick,
                      bool               h_scroll      = true,
                      int*               out_view_row  = nullptr,
                      const GridMouseFn* on_grid_mouse = nullptr) {
    gimgui_grid_body(id, rows, rowW, lineH, followRow, [](ImDrawList*, float, float) {}, 0.0f, drawRow, onClick, h_scroll, out_view_row, on_grid_mouse);
}

constexpr ImVec2 kChromeWindowPadBase(8.0f, 4.0f);

ImVec2 gimgui_chrome_window_pad() {
    const float s = g_font_size_px / kBaseUIFontPx;
    return kChromeWindowPadBase * s;
    // return ImGui::GetStyle().FramePadding;
}

float gimgui_chrome_row_h() { return ImGui::GetFrameHeight() + gimgui_chrome_window_pad().y * 2.0f; }

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
    const float  colW = gimgui_text_width(cols);
    const ImVec2 p    = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(colW, ImGui::GetFrameHeight()));
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x, p.y + ImGui::GetStyle().FramePadding.y),
                                        ImGui::GetColorU32(ImGuiCol_Text),
                                        text ? text : "");
}

// Same column width, text centered
void gimgui_chrome_mono_label_centered(const char* text, int cols) {
    const float  colW = gimgui_text_width(cols);
    const float  rowH = ImGui::GetFrameHeight();
    const ImVec2 p    = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(colW, rowH));
    const ImVec2 ts = ImGui::CalcTextSize(text);
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + (colW - ts.x) * 0.5f, p.y + (rowH - ts.y) * 0.5f),
                                        ImGui::GetColorU32(ImGuiCol_Text),
                                        text);
}

void gimgui_chrome_same_line_right(float item_w) {
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - item_w);
}


bool gimgui_stepper(const char* id_str,
                    int&        value,
                    int         min_val,
                    int         max_val,
                    const char* format    = "%d",
                    int         max_chars = 0) {
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

void gimgui_draw_player_status_bar(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_chrome_bar("##player", pos, size)) return;

    {
        float vol = gtui::transport_volume();
        ImGui::SetNextItemWidth(gimgui_text_width(16));
        if (ImGui::SliderFloat("##vol", &vol, 0.0f, gtui::kMasterVolumeMax, "Volume %.1f"))
            gtui::transport_set_volume(vol);
    }
    ImGui::SameLine();
    if (gimgui_button(gtui::player_ntsc() ? "NTSC" : "PAL", 4)) gtui::player_toggle_ntsc();
    ImGui::SameLine();

    if (gimgui_button(gtui::player_sid_model_8580() ? "8580" : "6581")) gtui::player_toggle_sid_model();

    gimgui_vertical_separator();

    {
        float sidComboW = gimgui_text_width(6) + ImGui::GetStyle().FramePadding.x * 2.0f;
        sidComboW += ImGui::GetFrameHeight();

        ImGui::SetNextItemWidth(sidComboW);
        char preview[16];
        gtui::player_sid_chip_combo_label(gtui::player_sid_chip_combo_index(), preview, sizeof preview);
        if (ImGui::BeginCombo("##sid_chips", preview)) {
            const int items = gtui::player_sid_chip_combo_items();
            for (int i = 0; i < items; i++) {
                char label[16];
                gtui::player_sid_chip_combo_label(i, label, sizeof label);
                const bool selected = (i == gtui::player_sid_chip_combo_index());
                if (ImGui::Selectable(label, selected)) gtui::player_set_sid_chip_combo_index(i);
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SameLine();
    if (gimgui_button(gtui::transport_stereo_label(), 3)) gtui::transport_cycle_stereo();

    {
        const int chips = gtui::player_sid_chips();
        for (int c = 0; c < chips; c++) {
            ImGui::SameLine();
            int pan = gtui::player_sid_pan(c);
            ImGui::PushID(c);
            ImGui::SetNextItemWidth(gimgui_text_width(5));
            if (ImGui::SliderInt("##pan", &pan, 0, 14, "%X")) gtui::player_set_sid_pan(c, pan);
            ImGui::PopID();
        }
    }


    gimgui_vertical_separator();
    {
        float speedComboW = gimgui_text_width(4) + ImGui::GetStyle().FramePadding.x * 2.0f;
        speedComboW += ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(speedComboW);
        char preview[8];
        gtui::player_speed_combo_label(gtui::player_speed_combo_index(), preview, sizeof preview);
        if (ImGui::BeginCombo("##speed", preview)) {
            const int items = gtui::player_speed_combo_items();
            for (int i = 0; i < items; i++) {
                char label[8];
                gtui::player_speed_combo_label(i, label, sizeof label);
                const bool selected = (i == gtui::player_speed_combo_index());
                if (ImGui::Selectable(label, selected)) gtui::player_set_speed_combo_index(i);
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    gimgui_vertical_separator();

    if (gimgui_toggle_button("FV", gtui::player_fine_vibrato())) gtui::player_toggle_fine_vibrato();
    ImGui::SameLine();
    if (gimgui_toggle_button("PO", gtui::player_optimize_pulse())) gtui::player_toggle_optimize_pulse();
    ImGui::SameLine();
    if (gimgui_toggle_button("RO", gtui::player_optimize_realtime())) gtui::player_toggle_optimize_realtime();

    gimgui_vertical_separator();

    ImGui::TextUnformatted("HR");
    ImGui::SameLine();
    {
        unsigned hr = (unsigned)gtui::player_hr_adparam();
        ImGui::PushItemWidth(gimgui_button_width(4));
        if (ImGui::InputScalar("##hr",
                               ImGuiDataType_U32,
                               &hr,
                               nullptr,
                               nullptr,
                               "%04X",
                               ImGuiInputTextFlags_CharsHexadecimal))
            gtui::player_set_hr_adparam((int)hr);
        ImGui::PopItemWidth();
    }

    gimgui_vertical_separator();
    if (gimgui_toggle_button("SID64", gtui::player_sidtracker64())) gtui::player_toggle_sidtracker64();


    {
        const char* fname = gtui::player_loaded_filename();
        gimgui_chrome_same_line_right(gimgui_text_width(strlen(fname)));
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
// Returns true when the body should be drawn (caller must still call End()).
bool gimgui_begin_panel(const char* title, ImVec2 pos, ImVec2 size) {
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
    dl->AddRectFilled(wp, ImVec2(wp.x + ww, wp.y + hh), gtui::color(gtui::GuiColorRole::PanelHeaderBg));
    dl->AddText(ImVec2(wp.x + hpad, wp.y + hpad), gtui::color(gtui::GuiColorRole::PanelHeaderText), title);

    // Inset the body below the header, with a small left/top gutter.
    ImGui::SetCursorPos(ImVec2(hpad, hh + 4.0f));
    return true;
}

void gimgui_end_panel() { ImGui::End(); }

// Record which editor panel the pointer is over (for hover-targeted wheel).
void gimgui_note_panel_hover(gtui::EditPanel panel) {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) g_hovered_edit_panel = (int)panel;
}

// The four SID tables (wave/pulse/filter/speed): four independently-scrolling
// columns (matching the legacy per-table scroll), each a custom grid like the
// pattern editor. Keyboard editing flows through the legacy table editor;
// clicking a cell places the cursor.
void gimgui_draw_tables(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Tables", pos, size)) {
        gimgui_end_panel();
        return;
    }

    const ImU32        cCursorRow    = gtui::color(gtui::GuiColorRole::GridCursorRow);
    const ImU32        cSelect       = gtui::color(gtui::GuiColorRole::Selection);
    const ImU32        cInstrSel     = gtui::color(gtui::GuiColorRole::InstrumentHighlight);
    const ImU32        cCursorFill   = gtui::color_a(gtui::GuiColorRole::Cursor, 70);
    const ImU32        cCursorEdge   = gtui::color_a(gtui::GuiColorRole::Cursor, 230);
    const ImU32        cIdx          = gtui::color(gtui::GuiColorRole::GridSecondaryText);
    const ImU32        cVal          = gtui::color(gtui::GuiColorRole::GridPrimaryText);
    static const int   colOff[4]     = { 3, 4, 6, 7 };
    static const char* kTableNames[] = { "Wave", "Pulse", "Filter", "Speed" };

    const float charW       = gimgui_mono_advance();
    const float lineH       = ImGui::GetTextLineHeight();
    const float colW        = gimgui_table_column_width();
    const float cellW8      = gimgui_text_width(8);
    const int   tlen        = gtui::table_len();
    const bool  panelActive = gtui::edit_panel() == gtui::EditPanelTables;
    const int   curTab      = gtui::table_cursor_table();
    const int   curPos      = gtui::table_cursor_pos();
    const int   curCol      = gtui::table_cursor_col();
    const int   markTab     = gtui::table_mark_table();
    int         markLo = gtui::table_mark_start(), markHi = gtui::table_mark_end();
    if (markLo > markHi) {
        int tmp = markLo;
        markLo  = markHi;
        markHi  = tmp;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
    for (int t = 0; t < gtui::table_count(); t++) {
        if (t) ImGui::SameLine();
        ImGui::PushID(t);

        const bool active = (curTab == t);
        ImGui::BeginChild("col", ImVec2(colW, 0), true, kNoNavWindowFlags);
        ImGui::TextUnformatted(kTableNames[t]);
        ImGui::Separator();

        int viewRow = 0;
        gimgui_grid_body(
            "body",
            tlen,
            cellW8,
            lineH,
            (panelActive && active) ? curPos : -1,
            [&](ImDrawList* dl, int r, float x, float y) {
                char buf[16];
                if (gtui::table_row_uses_selected_instrument(t, r))
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cInstrSel);
                if (markTab == t && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cSelect);
                if (active && curPos == r) {
                    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cellW8, y + lineH), cCursorRow);
                    if (panelActive) {
                        float cs = x + colOff[curCol < 0 ? 0 : (curCol > 3 ? 3 : curCol)] * charW;
                        dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                        dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                    }
                }
                snprintf(buf, sizeof buf, "%02X:", r + 1);
                dl->AddText(ImVec2(x, y), cIdx, buf);
                snprintf(buf, sizeof buf, "%02X %02X", gtui::table_left(t, r), gtui::table_right(t, r));
                dl->AddText(ImVec2(x + gimgui_text_width(3), y), cVal, buf);
            },
            [&](int r, float localX) {
                const int off = (int)(localX / charW);
                int       col;
                if (off <= 3) col = 0;
                else if (off == 4) col = 1;
                else if (off <= 6) col = 2;
                else col = 3;
                gtui::table_set_cursor(t, r, col);
            },
            false,
            &viewRow);
        gtui::table_set_view(t, viewRow);
        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    gimgui_note_panel_hover(gtui::EditPanelTables);
    gimgui_end_panel();
}

// Read-only pattern grid (M4 first cut): a custom ImDrawList grid, following
// Furnace's approach - fixed monospace metrics, virtualized to the visible rows,
// per-field coloring. Reads live model state via guimodel; editing comes later.
void gimgui_draw_pattern(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Pattern", pos, size)) {
        gimgui_end_panel();
        return;
    }

    if (gimgui_toggle_button("REC", gtui::pattern_record_mode())) gtui::pattern_toggle_record_mode();

    gimgui_vertical_separator();
    ImGui::TextUnformatted("ADV");
    ImGui::SameLine();
    if (gimgui_button(gtui::pattern_autoadvance_label(), 4)) gtui::pattern_cycle_autoadvance();

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

    // Colors from guicolors (Renoise-inspired default theme).
    const ImU32 cBeat       = gtui::color(gtui::GuiColorRole::BeatLine);
    const ImU32 cCursorRow  = gtui::color(gtui::GuiColorRole::GridCursorRowWarm);
    const ImU32 cPlayRow    = gtui::color(gtui::GuiColorRole::Playhead);
    const ImU32 cSelect     = gtui::color(gtui::GuiColorRole::Selection);
    const ImU32 cInstrSel   = gtui::color(gtui::GuiColorRole::InstrumentHighlight);
    const ImU32 cCursorFill = gtui::color_a(gtui::GuiColorRole::Cursor, 70);
    const ImU32 cCursorEdge = gtui::color_a(gtui::GuiColorRole::Cursor, 230);
    const ImU32 cRowNum     = gtui::color(gtui::GuiColorRole::GridSecondaryText);
    const ImU32 cNote       = gtui::color(gtui::GuiColorRole::GridNoteText);
    const ImU32 cInstr      = gtui::color(gtui::GuiColorRole::GridInstrumentText);
    const ImU32 cCmd        = gtui::color(gtui::GuiColorRole::GridCommandText);
    const ImU32 cDots       = gtui::color(gtui::GuiColorRole::GridDots);
    const ImU32 cMuted      = gtui::color(gtui::GuiColorRole::GridMuted);
    const ImU32 cHeader     = gtui::color(gtui::GuiColorRole::GridHeaderText);
    const ImU32 cMaster     = gtui::color(gtui::GuiColorRole::MasterChannel);
    const ImU32 cEnd        = gtui::color(gtui::GuiColorRole::GridEndMarker);

    const int  chans       = gtui::pattern_channels();
    const int  rows        = gtui::pattern_rows();
    const int  step        = gtui::pattern_step() > 0 ? gtui::pattern_step() : 4;
    const bool panelActive = gtui::edit_panel() == gtui::EditPanelPattern;
    const int  curRow      = gtui::pattern_cursor_row();
    const int  curChn      = gtui::pattern_cursor_chn();
    const int  curCol      = gtui::pattern_cursor_col();
    const int  selInstr    = gtui::instr_current();

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
    const float rowNumW   = gimgui_text_width(4);
    const float chanW     = gimgui_text_width(9);
    const float chanCellW = gimgui_text_width(8);
    const float noteW     = gimgui_text_width(3);
    const float headerH   = lineH * 2.f;
    const float totalW    = rowNumW + chans * chanW;

    gimgui_snap_body_pad_x();
    gimgui_grid_body(
        "patgrid",
        rows,
        totalW,
        lineH,
        panelActive ? curRow : -1,
        [&](ImDrawList* hdl, float x, float y) {
            char hbuf[16];
            for (int c = 0; c < chans; c++) {
                const float cx     = x + rowNumW + c * chanW;
                const bool  master = gtui::order_is_master_channel(c);
                snprintf(hbuf, sizeof hbuf, "%X", gtui::pattern_actual_channel(c));
                hdl->AddText(ImVec2(cx, y), master ? cMaster : cHeader, hbuf);
                snprintf(hbuf, sizeof hbuf, "%02X", gtui::pattern_number(c));
                hdl->AddText(ImVec2(cx, y + lineH), cMuted, hbuf);
            }
        },
        headerH,
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
                if (panelActive && r == curRow && c == curChn) {
                    float cs = (curCol == 0) ? cx : cx + gimgui_text_width(2 + curCol);
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
                    dl->AddRectFilled(ImVec2(ix, y), ImVec2(ix + gimgui_text_width(2), y + lineH), cInstrSel);
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
                    dl->AddText(ImVec2(cx + gimgui_text_width(5), y), cCmd, buf);
                }
                else {
                    dl->AddText(ImVec2(cx + gimgui_text_width(5), y), cDots, "...");
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

    gimgui_note_panel_hover(gtui::EditPanelPattern);
    gimgui_end_panel();
}

// Order list, vertical layout (positions = rows, channels = columns) via the
// shared grid scaffold. Classic view decodes songorder[]; expanded view shows
// songOrderPatterns[] + per-row transpose (GTUltra.pdf §42–47).
void gimgui_draw_orderlist(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Order List", pos, size)) {
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

    gimgui_vertical_separator();
    const bool expanded = gtui::order_expanded_view();
    if (gimgui_toggle_button("Expanded", expanded)) gtui::order_toggle_expanded_view();

    ImGui::Separator();

    const ImU32 cCursorRow  = gtui::color(gtui::GuiColorRole::GridCursorRow);
    const ImU32 cSelect     = gtui::color(gtui::GuiColorRole::Selection);
    const ImU32 cSynced     = gtui::color(gtui::GuiColorRole::InstrumentHighlight);
    const ImU32 cPlayRow    = gtui::color(gtui::GuiColorRole::Playhead);
    const ImU32 cCursorFill = gtui::color_a(gtui::GuiColorRole::Cursor, 70);
    const ImU32 cCursorEdge = gtui::color_a(gtui::GuiColorRole::Cursor, 230);
    const ImU32 cRowNum     = gtui::color(gtui::GuiColorRole::GridSecondaryText);
    const ImU32 cPat        = gtui::color(gtui::GuiColorRole::GridPrimaryText);
    const ImU32 cCmd        = gtui::color(gtui::GuiColorRole::GridCommandText);
    const ImU32 cEnd        = gtui::color(gtui::GuiColorRole::GridEndMarker);
    const ImU32 cMuted      = gtui::color(gtui::GuiColorRole::GridMuted);
    const ImU32 cHeader     = gtui::color(gtui::GuiColorRole::GridHeaderText);
    const ImU32 cMaster     = gtui::color(gtui::GuiColorRole::MasterChannel);
    const ImU32 cSizeBad    = gtui::color(gtui::GuiColorRole::Error);

    const int  chans       = gtui::order_channels();
    const int  rows        = gtui::order_rows();
    const bool panelActive = gtui::edit_panel() == gtui::EditPanelOrder;
    const int  curRow      = gtui::order_cursor_row();
    const int  curChn      = gtui::order_cursor_chn();
    const int  curCol      = gtui::order_cursor_col();
    const int  markChn     = gtui::order_mark_chn();
    int        markChEnd   = gtui::order_mark_chn_end();
    int        markLo = gtui::order_mark_start(), markHi = gtui::order_mark_end();
    if (markLo > markHi) {
        int tmp = markLo;
        markLo  = markHi;
        markHi  = tmp;
    }
    if (markChn >= 0 && markChEnd < 0) markChEnd = markChn;
    int markChLo = markChn, markChHi = markChEnd;
    if (markChLo > markChHi) {
        int tmp  = markChLo;
        markChLo = markChHi;
        markChHi = tmp;
    }

    const float charW     = gimgui_mono_advance();
    const float lineH     = ImGui::GetTextLineHeight();
    const float rowNumW   = gimgui_text_width(4);
    const int   cellChars = 5;
    const float cellW     = gimgui_text_width(cellChars);
    const float colGap    = charW;
    const float colPitch  = cellW + colGap;
    const float totalW    = rowNumW + (float)chans * cellW + (float)(chans > 0 ? chans - 1 : 0) * colGap;
    const float headerH   = lineH * 2.f;

    int selRow[8], endRow[8], playRow[8];
    for (int c = 0; c < chans && c < 8; c++) {
        selRow[c]  = gtui::order_selected_row(c);
        endRow[c]  = gtui::order_range_end_row(c);
        playRow[c] = gtui::order_play_row(c);
    }

    gimgui_snap_body_pad_x();
    const GridMouseFn orderMouse = [&](int r, float localX, ImGuiMouseButton btn, bool dbl, bool drag) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.MouseDown[ImGuiMouseButton_Left] &&
            (io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle])) {
            gtui::order_mouse_mark_cancel();
            return;
        }

        float rx = localX - rowNumW;
        if (rx < 0) return;
        int c = (int)(rx / colPitch);
        if (c < 0 || c >= chans) return;
        float cx = rx - (float)c * colPitch;
        if (cx >= cellW) return;
        int off = (int)(cx / charW);
        if (expanded) {
            if (off > 4) off = 4;
            if (off == 2) {
                gtui::OrderCell cell = gtui::order_cell(c, r);
                if (cell.kind != 4) off = (cx < cellW * 0.5f) ? 1 : 3;
            }
        }
        else if (off > 1) {
            off = 1;
        }

        const bool mod = io.KeyShift || io.KeyCtrl;
        if (btn == ImGuiMouseButton_Left && dbl) gtui::order_mouse_double_click(c, r, off);
        else if (btn == ImGuiMouseButton_Left) gtui::order_mouse_left(c, r, off, mod, drag);
        else if (btn == ImGuiMouseButton_Right || btn == ImGuiMouseButton_Middle) {
            if (drag) gtui::order_mouse_mark_drag(c, r);
            else gtui::order_mouse_mark_begin(c, r);
        }
    };
    gimgui_grid_body(
        "olgrid",
        rows,
        totalW,
        lineH,
        panelActive ? curRow : -1,
        [&](ImDrawList* hdl, float x, float y) {
            char hbuf[16];
            for (int c = 0; c < chans; c++) {
                const float cx     = x + rowNumW + (float)c * colPitch;
                const bool  master = gtui::order_is_master_channel(c);
                snprintf(hbuf, sizeof hbuf, "%X", gtui::order_actual_channel(c));
                hdl->AddText(ImVec2(cx, y), master ? cMaster : cHeader, hbuf);
                {
                    const int total   = expanded ? gtui::order_compressed_size(c) : 0;
                    const int payload = expanded ? gtui::order_compressed_payload_size(c) : gtui::order_length(c);
                    if (expanded && total > 0xff) snprintf(hbuf, sizeof hbuf, "**");
                    else snprintf(hbuf, sizeof hbuf, "%02X", payload);
                    const ImU32 sizeCol = (expanded && total > 0xff)    ? cSizeBad
                                          : (expanded && total >= 0xf0) ? cSizeBad
                                                                        : cMuted;
                    hdl->AddText(ImVec2(cx, y + lineH), sizeCol, hbuf);
                }
            }
        },
        headerH,
        [&](ImDrawList* dl, int r, float x, float y) {
            char buf[8];
            if (r == curRow) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);
            snprintf(buf, sizeof buf, "%02X", r);
            dl->AddText(ImVec2(x, y), cRowNum, buf);

            for (int c = 0; c < chans; c++) {
                const float cx = x + rowNumW + (float)c * colPitch;

                gtui::OrderCell cell = gtui::order_cell(c, r);
                if (!cell.valid) continue;

                if (r == playRow[c]) dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cellW, y + lineH), cPlayRow);
                if (r == selRow[c] || r == endRow[c])
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cellW, y + lineH), cSynced);
                if (markChn >= 0 && c >= markChLo && c <= markChHi && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cellW, y + lineH), cSelect);

                if (panelActive && r == curRow && c == curChn) {
                    float cs;
                    if (expanded) {
                        cs = cx + (float)curCol * charW;
                    }
                    else if (cell.kind != 3) {
                        cs = cx + (curCol < 0 ? 0 : (curCol > 1 ? 1 : curCol)) * charW;
                    }
                    else {
                        cs = cx;
                    }
                    if (cell.kind != 3 || expanded) {
                        dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                        dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                    }
                }

                if (!expanded) {
                    if (cell.kind == 3) {
                        dl->AddText(ImVec2(cx, y), cEnd, "==");
                        continue;
                    }
                    ImU32 col = cell.kind == 2 ? cCmd : cPat;
                    dl->AddText(ImVec2(cx, y), col, cell.text);
                    continue;
                }

                const ImU32 patCol = cell.muted ? cMuted : cPat;
                if (cell.kind == 4) {
                    dl->AddText(ImVec2(cx, y), cCmd, cell.text);
                    dl->AddText(ImVec2(cx + 3.f * charW, y), cCmd, cell.trans);
                    continue;
                }
                dl->AddText(ImVec2(cx, y), patCol, cell.text);
                if (cell.trans[0]) {
                    const bool nz = cell.trans[1] != '0' || (cell.trans[2] && cell.trans[2] != '0');
                    dl->AddText(ImVec2(cx + 3.f * charW, y), nz ? cCmd : patCol, cell.trans);
                }
            }
        },
        [](int, float) {},
        true,
        nullptr,
        &orderMouse);

    gimgui_note_panel_hover(gtui::EditPanelOrder);
    gimgui_end_panel();
}


// Instrument list (01..3F): custom grid like pattern/order. Hex fields use the
// legacy nibble editor; name cells use always-on InputText like the song panel.
void gimgui_draw_instruments(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Instruments", pos, size)) {
        gimgui_end_panel();
        return;
    }

    gtui::instr_clamp_selection();

    static const char* FIELD_LABELS[] = {
        "AD", "SR", "WP", "PP", "FP", "VP", "VD", "GT", "1W", "PN",
    };

    const ImU32 cCursorRow = gtui::color(gtui::GuiColorRole::GridCursorRow);
    // const ImU32 cInstrSel   = gtui::color(gtui::GuiColorRole::InstrumentHighlight);
    const ImU32 cCursor     = gtui::color(gtui::GuiColorRole::Cursor);
    const ImU32 cCursorFill = gtui::color_a(gtui::GuiColorRole::Cursor, 70);
    const ImU32 cCursorEdge = gtui::color_a(gtui::GuiColorRole::Cursor, 230);
    const ImU32 cText       = gtui::color(gtui::GuiColorRole::GridPrimaryText);
    const ImU32 cTextSel    = gtui::color(gtui::GuiColorRole::GridSecondaryText);
    const ImU32 cInstrText  = gtui::color(gtui::GuiColorRole::GridInstrumentText);
    const ImU32 cHeader     = gtui::color(gtui::GuiColorRole::GridHeaderText);

    const bool panelActive = gtui::edit_panel() == gtui::EditPanelInstrument;
    const int  rows        = gtui::instr_rows();
    const int  curInst     = gtui::instr_current();
    const int  curField    = gtui::instr_cursor_field();
    const int  curNibble   = gtui::instr_cursor_nibble();

    const float charW   = gimgui_mono_advance();
    const float lineH   = ImGui::GetTextLineHeight();
    const float idxW    = gimgui_text_width(3);
    const float nameW   = gimgui_text_width(gtui::INSTR_NAME_MAX) + 4.0f;
    const float nameGap = charW; // one char between name and AD (hex cols use 3 = "XX ")
    const float hexW    = gimgui_text_width(3);
    const float totalW  = gimgui_instruments_grid_width();

    // Cumulative column edges (shared by header + rows — no repeated multiply).
    float colX[3 + gtui::INSTR_FIELDS];
    colX[0] = 0.0f;
    colX[1] = idxW;
    colX[2] = idxW + nameW;
    for (int f = 0; f < gtui::INSTR_FIELDS; f++) colX[3 + f] = colX[2] + nameGap + (float)f * hexW;

    if (!panelActive && g_instr_name_edit >= gtui::INSTR_FIRST) {
        g_instr_name_edit       = -1;
        g_instr_name_want_focus = -1;
    }

    g_instr_name_item_active = false;
    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
    gimgui_snap_body_pad_x();
    gimgui_grid_body(
        "instgrid",
        rows,
        totalW,
        lineH,
        panelActive ? curInst - gtui::INSTR_FIRST : -1,
        [&](ImDrawList* dl, float x, float y) {
            dl->AddText(ImVec2(x + colX[1], y), cHeader, "Name");
            for (int f = 0; f < gtui::INSTR_FIELDS; f++)
                dl->AddText(ImVec2(x + colX[3 + f], y), cHeader, FIELD_LABELS[f]);
        },
        lineH,
        [&](ImDrawList* dl, int r, float x, float y) {
            const int  inst = r + gtui::INSTR_FIRST;
            char       buf[32];
            const bool cur = inst == curInst;

            snprintf(buf, sizeof buf, "%02X", inst);
            if (cur) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);
            dl->AddText(ImVec2(x + colX[0], y), cur ? cInstrText : cTextSel, buf);

            const float nameX = x + colX[1];
            char        nameBuf[gtui::INSTR_NAME_MAX + 1];
            snprintf(nameBuf, sizeof nameBuf, "%.*s", gtui::INSTR_NAME_MAX, gtui::instr_name(inst));

            ImGui::SetCursorScreenPos(ImVec2(nameX, y));
            ImGui::PushID(inst);
            ImGui::PushItemWidth(nameW);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 0.0f));
            const bool nameSelected = panelActive && cur && curField == gtui::INSTR_FIELD_NAME;
            const bool nameEditing  = panelActive && g_instr_name_edit == inst;
            ImGui::PushStyleColor(ImGuiCol_Border, nameSelected ? cCursor : IM_COL32_BLACK_TRANS);
            if (cur) ImGui::PushStyleColor(ImGuiCol_FrameBg, cCursorRow);
            if (g_instr_name_want_focus == inst) ImGui::SetKeyboardFocusHere();

            const ImGuiInputTextFlags inputFlags =
                nameEditing ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly;
            if (ImGui::InputText("##iname", nameBuf, sizeof nameBuf, inputFlags))
                gtui::instr_set_name(inst, nameBuf);
            if (ImGui::IsItemActive()) g_instr_name_item_active = true;
            if (ImGui::IsItemActivated() ||
                (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                gtui::instr_set_cursor(inst, gtui::INSTR_FIELD_NAME, 0);
                g_instr_name_edit       = inst;
                g_instr_name_want_focus = inst;
            }
            if (g_instr_name_want_focus == inst && ImGui::IsItemActive()) g_instr_name_want_focus = -1;
            if (g_instr_name_edit == inst && ImGui::IsItemDeactivated()) g_instr_name_edit = -1;
            if (cur) ImGui::PopStyleColor();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
            ImGui::PopID();

            for (int f = 0; f < gtui::INSTR_FIELDS; f++) {
                const float fx = x + colX[3 + f];
                if (panelActive && cur && curField == f) {
                    const float cs = fx + curNibble * charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                }
                snprintf(buf, sizeof buf, "%02X", gtui::instr_field(inst, f));
                dl->AddText(ImVec2(fx, y), cText, buf);
            }
        },
        [&](int r, float localX) {
            const int inst = r + gtui::INSTR_FIRST;
            if (localX < colX[1]) {
                g_instr_name_edit = -1;
                gtui::instr_set_cursor(inst, 0, 0);
                return;
            }
            if (localX < colX[2]) {
                gtui::instr_set_cursor(inst, gtui::INSTR_FIELD_NAME, 0);
                gimgui_instr_name_begin(inst);
                return;
            }
            g_instr_name_edit = -1;
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
    ImGui::PopItemFlag();

    gimgui_note_panel_hover(gtui::EditPanelInstrument);
    gimgui_end_panel();
}

// Song info + transport: name/author/copyright text fields and play/stop
// controls. A plain form panel (regular ImGui widgets). Metadata edits write
// directly (not undo-tracked, as in the legacy); transport calls the legacy
// play/stop.
void gimgui_draw_song(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_panel("Song", pos, size)) {
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

    const float inputW    = gimgui_text_width(gtui::SONG_STR_MAX) + ImGui::GetStyle().FramePadding.x * 2.0f;
    const bool  namesMode = (gtui::edit_panel() == gtui::EditPanelNames);
    if (!namesMode && g_song_field_edit >= 0) gimgui_song_field_end();

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
        const bool fieldSelected = namesMode && f == gtui::names_field();
        const bool fieldEditing  = g_song_field_edit == f;
        if (fieldSelected) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border,
                                  ImGui::ColorConvertU32ToFloat4(gtui::color(gtui::GuiColorRole::Cursor)));
        }
        if (g_song_field_want_focus == f) ImGui::SetKeyboardFocusHere();
        const ImGuiInputTextFlags inputFlags =
            fieldEditing ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_ReadOnly;
        if (ImGui::InputText("##v", buf, sizeof buf, inputFlags))
            fields[f].set(buf); // metadata: commit as typed (no undo, like legacy)
        if (ImGui::IsItemActive()) g_song_field_item_active = true;
        if (ImGui::IsItemActivated() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
            gtui::names_set_field(f);
            g_song_field_edit       = f;
            g_song_field_want_focus = f;
        }
        if (g_song_field_want_focus == f && ImGui::IsItemActive()) g_song_field_want_focus = -1;
        if (g_song_field_edit == f && ImGui::IsItemDeactivated()) g_song_field_edit = -1;
        if (fieldSelected) {
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        ImGui::PopItemWidth();
        ImGui::PopID();
    }
    ImGui::PopItemFlag();

    gimgui_note_panel_hover(gtui::EditPanelNames);
    gimgui_end_panel();
}

// Full-width transport toolbar (no section header): rewind / play / pattern /
// fast-forward / stop, then the Follow and Loop toggles, then a play indicator
// and elapsed time. Sits at the top of the window, below the menu bar.
void gimgui_draw_transport_bar(ImVec2 pos, ImVec2 size) {
    if (!gimgui_begin_chrome_bar("##transport", pos, size)) return;


    if (gimgui_button("<<", 3)) gtui::transport_rewind();
    ImGui::SameLine();
    if (gimgui_toggle_button(">", gtui::transport_playing(), 3)) gtui::transport_toggle_play();
    ImGui::SameLine();
    if (gimgui_button("Pat")) gtui::transport_play_pattern();
    ImGui::SameLine();
    if (gimgui_button(">>", 3)) gtui::transport_ff();
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

    {
        const float midiComboW = gimgui_text_width(22) + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float midiLabelW = gimgui_text_width(4);
        const float midiBlockW = midiLabelW + ImGui::GetStyle().ItemSpacing.x + midiComboW;
        gimgui_chrome_same_line_right(midiBlockW);

        ImGui::TextUnformatted("MIDI");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(midiComboW);

        char preview[128];
        gtui::midi_combo_label(gtui::midi_combo_index(), preview, sizeof preview);

        if (ImGui::BeginCombo("##midi_port", preview)) {
            const int items = gtui::midi_combo_items();
            for (int i = 0; i < items; i++) {
                char label[128];
                gtui::midi_combo_label(i, label, sizeof label);
                const bool selected = (i == gtui::midi_combo_index());
                if (ImGui::Selectable(label, selected)) gtui::midi_set_combo_index(i);
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::End();
}


// Load the bundled monospace font from the embed catalog. Falls back to ImGui default.
void gimgui_load_font_at(float sizePx) {
    ImGuiIO&     io   = ImGui::GetIO();
    auto const&  font = embed::get(embed::Id::font);
    ImFontConfig cfg;
    // Embedded bytes live in the binary for the process lifetime.
    cfg.FontDataOwnedByAtlas = false;
    if (io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(font.data)),
                                       static_cast<int>(font.size),
                                       sizePx,
                                       &cfg)) {
        io.FontDefault = io.Fonts->Fonts.back();
        return;
    }
    io.Fonts->AddFontDefault();
    io.FontDefault = io.Fonts->Fonts.back();
}

// Flat dark theme: square chrome; colors from guicolors.
void gimgui_apply_style() {
    ImGuiStyle& s     = ImGui::GetStyle();
    const float scale = g_font_size_px / kBaseUIFontPx;
    // ImGui 1.92+ rasterizes glyphs at FontSizeBase, not AddFontFromFileTTF()'s size.
    s.FontSizeBase           = g_font_size_px;
    s._NextFrameFontSizeBase = g_font_size_px;
    s.WindowRounding         = 0.0f;
    s.ChildRounding          = 0.0f;
    s.FrameRounding          = 0.0f;
    s.PopupRounding          = 0.0f;
    s.ScrollbarRounding      = 0.0f;
    s.GrabRounding           = 0.0f;
    s.TabRounding            = 0.0f;
    s.WindowBorderSize       = 1.0f;
    s.ChildBorderSize        = 1.0f;
    s.FrameBorderSize        = 1.0f;

    s.WindowPadding    = ImVec2(8.0f * scale, 6.0f * scale);
    s.FramePadding     = ImVec2(6.0f * scale, 3.0f * scale);
    s.ItemSpacing      = ImVec2(6.0f * scale, 4.0f * scale);
    s.ItemInnerSpacing = ImVec2(4.0f * scale, 4.0f * scale);

    s.ScrollbarSize = 8.0f * scale;

    gtui::gui_colors_apply_imgui_style();
}

void gimgui_reload_font() {
    if (!g_imgui_ready) return;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->ClearFonts();
    gimgui_load_font_at(g_font_size_px);
    gimgui_apply_style();
}

// Mouse wheel → cursor row/field for the panel under the pointer.
void gimgui_dispatch_mouse_wheel() {
    if (!g_imgui_ready) return;
    if (g_show_help) return; // modal Help owns input

    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseWheel == 0.f) return;
    if (!io.WantCaptureMouse) return;

    // Scrollable ImGui windows keep the wheel; don't move editor rows.
    if (g_help_hovered) return;
    if (g_hovered_edit_panel < 0) return;

    if (gimgui_song_field_editing() || gimgui_instr_name_editing()) return;
    if (ImGui::IsAnyItemActive()) return;

    const auto       panel = (gtui::EditPanel)g_hovered_edit_panel;
    gtaction::Action act   = gtaction::Action::None;
    if (io.MouseWheel > 0.f) {
        switch (panel) {
        case gtui::EditPanelOrder: act = gtaction::Action::OrderRowUp; break;
        case gtui::EditPanelPattern: act = gtaction::Action::PatternRowUp; break;
        case gtui::EditPanelInstrument: act = gtaction::Action::InstrRowUp; break;
        case gtui::EditPanelTables: act = gtaction::Action::TableRowUp; break;
        case gtui::EditPanelNames: act = gtaction::Action::NamesFieldPrev; break;
        }
    }
    else {
        switch (panel) {
        case gtui::EditPanelOrder: act = gtaction::Action::OrderRowDown; break;
        case gtui::EditPanelPattern: act = gtaction::Action::PatternRowDown; break;
        case gtui::EditPanelInstrument: act = gtaction::Action::InstrRowDown; break;
        case gtui::EditPanelTables: act = gtaction::Action::TableRowDown; break;
        case gtui::EditPanelNames: act = gtaction::Action::NamesFieldNext; break;
        }
    }

    if (act != gtaction::Action::None) gtaction::perform(act);

    io.MouseWheel  = 0.f;
    io.MouseWheelH = 0.f;
}


} // namespace

void gimgui_open_help() {
    if (g_show_help) {
        g_show_help = false;
        return;
    }
    g_show_help       = true;
    g_help_tab        = (int)gthelp::topic_index_for_edit_panel((int)gtui::edit_panel());
    g_help_select_tab = true;
    ++g_help_tab_bar_id;
}

void gimgui_close_help() { g_show_help = false; }

bool gimgui_help_open() { return g_show_help; }

void gimgui_instr_name_begin(int inst) {
    if (inst < gtui::INSTR_FIRST) return;
    g_instr_name_edit       = inst;
    g_instr_name_want_focus = inst;
}

void gimgui_song_field_begin(int field) {
    if (field < 0 || field > 2) return;
    g_song_field_edit       = field;
    g_song_field_want_focus = field;
}

void gimgui_song_field_end() {
    g_song_field_edit       = -1;
    g_song_field_want_focus = -1;
}


void gimgui_render() {
    if (!g_imgui_ready) return;

    // Draws the entire ImGui frame into gfx_renderer (caller clears/presents).
    g_hovered_edit_panel = -1;

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            const char* help_shortcut = "F12";
            std::string help_shortcut_owned;
            for (const gtaction::BindingRow& row : gtaction::binding_rows_for(gtaction::Ctx::Global)) {
                if (row.action == gtaction::Action::Help && !row.chords.empty()) {
                    help_shortcut_owned = gtaction::format_chord(row.chords[0]);
                    help_shortcut       = help_shortcut_owned.c_str();
                    break;
                }
            }
            if (ImGui::MenuItem("Help", help_shortcut)) gimgui_open_help();
            ImGui::Separator();
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


    // Fixed tiled layout filling the whole window. Five columns:
    //   left       : Song (top) + Order List (bottom), fixed width
    //   centre     : Pattern (fills remaining width)
    //   right pair : Instruments + Tables side by side, each fixed width
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::GetBackgroundDrawList()->AddRectFilled(vp->Pos,
                                                  ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                                                  gtui::color(gtui::GuiColorRole::AppBackground));

    const ImVec2 vo = vp->WorkPos;  // origin below the menu bar
    const ImVec2 vs = vp->WorkSize; // area excluding the menu bar
    const float  g  = 3.0f;         // gutter between panels

    const float chromeH = gimgui_chrome_row_h();

    gimgui_draw_player_status_bar(vo, ImVec2(vs.x, chromeH));
    gimgui_draw_transport_bar(ImVec2(vo.x, vo.y + chromeH + g), ImVec2(vs.x, chromeH));

    const ImVec2 o = ImVec2(vo.x, vo.y + chromeH * 2.0f + 2 * g);
    const ImVec2 s = ImVec2(vs.x, vs.y - chromeH * 3.0f - 3 * g);

    const float leftW = gimgui_left_column_width();
    const float songH = gimgui_song_panel_height();
    const float insW  = gimgui_instruments_panel_width();
    const float tblW  = gimgui_tables_panel_width();
    const float patW  = s.x - leftW - insW - tblW - 4 * g;
    const float leftX = o.x;
    const float patX  = o.x + leftW + g;
    const float insX  = o.x + leftW + patW + 2 * g;
    const float tblX  = insX + insW + g;

    gimgui_draw_song(ImVec2(leftX, o.y), ImVec2(leftW, songH));
    gimgui_draw_orderlist(ImVec2(leftX, o.y + songH + g), ImVec2(leftW, s.y - songH - g));
    gimgui_draw_pattern(ImVec2(patX, o.y), ImVec2(patW, s.y));
    gimgui_draw_instruments(ImVec2(insX, o.y), ImVec2(insW, s.y));
    gimgui_draw_tables(ImVec2(tblX, o.y), ImVec2(tblW, s.y));

    gimgui_draw_context_help(ImVec2(vo.x, vo.y + vs.y - chromeH), ImVec2(vs.x, chromeH));

    gimgui_draw_help_window();

    if (g_show_demo) ImGui::ShowDemoWindow(&g_show_demo);

    gimgui_dispatch_mouse_wheel();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gfx_renderer);
}

void gimgui_process_event(const SDL_Event* event) {
    if (!g_imgui_ready) return;
    // Tab is reserved for edit-mode cycling (action layer / future keymap).
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        if (event->key.keysym.scancode == SDL_SCANCODE_TAB) return;
    }
    ImGui_ImplSDL2_ProcessEvent(event);
}

int gimgui_input_capture() {
    if (!g_imgui_ready) return GimguiCaptureNone;
    ImGuiIO& io = ImGui::GetIO();
    // Modal Help: always block mouse. Leave keyboard free for gactions (Cancel/Help).
    if (g_show_help) return GimguiCaptureMouse;
    int flags = GimguiCaptureNone;
    if (io.WantCaptureMouse) flags |= GimguiCaptureMouse;
    if (io.WantCaptureKeyboard) {
        // Let Tab / Shift+Tab through to the action layer even when a text field
        // is focused (NoTabStop on widgets; Tab events are not fed to ImGui).
        if (!ImGui::IsKeyDown(ImGuiKey_Tab)) flags |= GimguiCaptureKeyboard;
    }
    return flags;
}

void gimgui_init() {
    if (g_imgui_ready) return;
    // gfx_renderer can be null under headless/unsupported video drivers.
    if (!win_window || !gfx_renderer) return;

    if (const char* env = std::getenv("GTULTRA_UI_FONT_SIZE")) {
        float px = (float)std::atof(env);
        if (px >= kMinUIFontPx && px <= kMaxUIFontPx) g_font_size_px = px;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    gtui::gui_colors_init();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);

    // Keep layout next to gtultra.cfg (not the process cwd, which chdir's on export).
    // ImGui stores the pointer; this buffer must outlive the context.
    static char imgui_ini_path[MAX_PATHNAME];
    createFilename(appFileName, imgui_ini_path, "imgui.ini");
#if !defined(__WIN32__) && !defined(__amigaos__)
    {
        char dir[MAX_PATHNAME];
        std::snprintf(dir, sizeof(dir), "%s", imgui_ini_path);
        if (char* slash = std::strrchr(dir, '/')) {
            *slash = '\0';
            mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
        }
    }
#endif
    io.IniFilename = imgui_ini_path;

    gimgui_load_font_at(g_font_size_px);
    gimgui_apply_style();

    ImGui_ImplSDL2_InitForSDLRenderer(win_window, gfx_renderer);
    ImGui_ImplSDLRenderer2_Init(gfx_renderer);

    g_imgui_ready = true;
    LOG_INFO("ImGui UI initialized (font {}px)", static_cast<int>(g_font_size_px));
}

void gimgui_shutdown() {
    if (!g_imgui_ready) return;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    g_imgui_ready = false;
    LOG_INFO("ImGui UI shutdown");
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

void gimgui_reset_input_after_modal() {
    if (!g_imgui_ready) return;

    ImGuiIO& io = ImGui::GetIO();
    io.ClearInputMouse();
    io.ClearInputKeys();
    io.KeyCtrl = io.KeyShift = io.KeyAlt = io.KeySuper = false;

    if (win_window) {
        int    mx = 0, my = 0;
        Uint32 buttons  = SDL_GetMouseState(&mx, &my);
        io.MousePos     = ImVec2((float)mx, (float)my);
        io.MouseDown[0] = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        io.MouseDown[1] = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        io.MouseDown[2] = (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    }
}
