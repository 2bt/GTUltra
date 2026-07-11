//
// guicolors - default palette + ImGui style derivation.
//
#include "guicolors.hpp"

namespace gtui {
namespace {

struct GuiColorDef {
    GuiColorRole  role;
    const char*   name;
    const char*   label;
    unsigned char r, g, b, a;
};

static ImU32 g_colors[(unsigned)GuiColorRole::Count];

static const GuiColorDef kDefaultColors[] = {
    // Renoise-inspired: charcoal base, orange reserved for focus/playhead/notes.
    { GuiColorRole::AppBackground,         "app_background",           "App background",             30, 30, 30, 255 },
    { GuiColorRole::PanelHeaderBg,         "panel_header_bg",          "Panel header",               46, 46, 46, 255 },
    { GuiColorRole::PanelHeaderBgActive,   "panel_header_bg_active",   "Panel header (active)",      58, 50, 44, 255 },
    { GuiColorRole::PanelHeaderText,       "panel_header_text",        "Panel header text",          195, 200, 205, 255 },
    { GuiColorRole::PanelHeaderTextActive, "panel_header_text_active", "Panel header text (active)", 255, 155, 60, 255 },

    { GuiColorRole::WidgetSurface,         "widget_surface",           "Widget surface",             40, 40, 40, 255 },
    { GuiColorRole::WidgetSurfaceDeep,     "widget_surface_deep",      "Widget surface (deep)",      32, 32, 32, 255 },
    { GuiColorRole::WidgetFrameBg,         "widget_frame_bg",          "Widget frame",               54, 54, 54, 255 },
    { GuiColorRole::WidgetBorder,          "widget_border",            "Widget border",              22, 22, 22, 255 },
    { GuiColorRole::WidgetAccent,          "widget_accent",            "Widget accent",              220, 100, 25, 255 },

    { GuiColorRole::GridPrimaryText,       "grid_primary_text",        "Grid primary text",          224, 230, 238, 255 },
    { GuiColorRole::GridNoteText,          "grid_note_text",           "Grid note text",             255, 255, 255, 255 },
    { GuiColorRole::GridSecondaryText,     "grid_secondary_text",      "Grid secondary text",        130, 145, 160, 255 },
    { GuiColorRole::GridHeaderText,        "grid_header_text",         "Grid header text",           200, 210, 220, 255 },
    { GuiColorRole::GridCommandText,       "grid_command_text",        "Grid command text",          235, 195, 110, 255 },
    { GuiColorRole::GridInstrumentText,    "grid_instrument_text",     "Grid instrument text",       155, 205, 135, 255 },
    { GuiColorRole::GridMuted,             "grid_muted",               "Grid muted text",            115, 125, 140, 255 },
    { GuiColorRole::GridDots,              "grid_dots",                "Grid dots",                  70, 78, 88, 255 },
    { GuiColorRole::GridEndMarker,         "grid_end_marker",          "Grid end marker",            155, 165, 180, 255 },

    { GuiColorRole::GridCursorRow,         "grid_cursor_row",          "Grid cursor row",            220, 100, 25, 22 },
    { GuiColorRole::GridCursorRowWarm,     "grid_cursor_row_warm",     "Grid cursor row (warm)",     220, 100, 25, 32 },
    { GuiColorRole::Cursor,                "cursor",                   "Cursor",                     255, 145, 50, 255 },
    { GuiColorRole::Selection,             "selection",                "Selection",                  220, 100, 25, 85 },
    { GuiColorRole::InstrumentHighlight,   "instrument_highlight",     "Instrument highlight",       220, 100, 25, 32 },
    { GuiColorRole::Playhead,              "playhead",                 "Playhead",                   220, 100, 25, 70 },
    { GuiColorRole::BeatLine,              "beat_line",                "Beat line",                  68, 68, 68, 55 },
    { GuiColorRole::MasterChannel,         "master_channel",           "Master channel",             225, 155, 75, 255 },
    { GuiColorRole::Error,                 "error",                    "Error",                      215, 85, 85, 255 },
};

static ImVec4 to_vec4(GuiColorRole role) {
    const ImU32 c = color(role);
    return ImVec4(((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                  ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
}

static ImVec4 lerp_rgb(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static_assert(sizeof(kDefaultColors) / sizeof(kDefaultColors[0]) == (unsigned)GuiColorRole::Count,
              "kDefaultColors must match GuiColorRole enum order");

} // namespace

void gui_colors_reset_defaults() {
    for (const GuiColorDef& d : kDefaultColors) g_colors[(unsigned)d.role] = IM_COL32(d.r, d.g, d.b, d.a);
}

void gui_colors_init() { gui_colors_reset_defaults(); }

ImU32 color(GuiColorRole role) {
    const unsigned i = (unsigned)role;
    if (i >= (unsigned)GuiColorRole::Count) return IM_COL32(255, 0, 255, 255);
    return g_colors[i];
}

ImU32 color_a(GuiColorRole role, unsigned char alpha) {
    return (color(role) & 0x00FFFFFFu) | ((ImU32)alpha << IM_COL32_A_SHIFT);
}

const char* color_role_name(GuiColorRole role) {
    const unsigned i = (unsigned)role;
    if (i >= (unsigned)GuiColorRole::Count) return "";
    return kDefaultColors[i].name;
}

const char* color_role_label(GuiColorRole role) {
    const unsigned i = (unsigned)role;
    if (i >= (unsigned)GuiColorRole::Count) return "";
    return kDefaultColors[i].label;
}

void gui_colors_apply_imgui_style() {
    ImVec4* c = ImGui::GetStyle().Colors;

    const ImVec4 text    = to_vec4(GuiColorRole::GridPrimaryText);
    const ImVec4 textDim = to_vec4(GuiColorRole::GridMuted);
    const ImVec4 surface = to_vec4(GuiColorRole::WidgetSurface);
    const ImVec4 deep    = to_vec4(GuiColorRole::WidgetSurfaceDeep);
    const ImVec4 frame   = to_vec4(GuiColorRole::WidgetFrameBg);
    const ImVec4 border  = to_vec4(GuiColorRole::WidgetBorder);
    const ImVec4 accent  = to_vec4(GuiColorRole::WidgetAccent);
    const ImVec4 white   = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    const ImVec4 frameHover  = lerp_rgb(frame, white, 0.10f);
    const ImVec4 frameActive = lerp_rgb(frame, white, 0.16f);
    const ImVec4 btnIdle     = lerp_rgb(frame, deep, 0.35f);
    const ImVec4 btnHover    = lerp_rgb(btnIdle, accent, 0.22f);
    const ImVec4 btnActive   = lerp_rgb(btnIdle, accent, 0.42f);
    const ImVec4 scrollGrab  = lerp_rgb(deep, white, 0.14f);
    const ImVec4 scrollHover = lerp_rgb(deep, white, 0.22f);
    const ImVec4 rowAlt      = lerp_rgb(surface, white, 0.03f);
    const ImVec4 tableHdr    = lerp_rgb(surface, white, 0.06f);

    c[ImGuiCol_Text]                 = text;
    c[ImGuiCol_TextDisabled]         = textDim;
    c[ImGuiCol_WindowBg]             = surface;
    c[ImGuiCol_ChildBg]              = surface;
    c[ImGuiCol_PopupBg]              = deep;
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_FrameBg]              = frame;
    c[ImGuiCol_FrameBgHovered]       = frameHover;
    c[ImGuiCol_FrameBgActive]        = frameActive;
    c[ImGuiCol_TitleBg]              = deep;
    c[ImGuiCol_TitleBgActive]        = lerp_rgb(surface, accent, 0.18f);
    c[ImGuiCol_MenuBarBg]            = deep;
    c[ImGuiCol_ScrollbarBg]          = deep;
    c[ImGuiCol_ScrollbarGrab]        = scrollGrab;
    c[ImGuiCol_ScrollbarGrabHovered] = scrollHover;
    c[ImGuiCol_ScrollbarGrabActive]  = lerp_rgb(scrollGrab, accent, 0.20f);
    c[ImGuiCol_Button]               = btnIdle;
    c[ImGuiCol_ButtonHovered]        = btnHover;
    c[ImGuiCol_ButtonActive]         = btnActive;
    c[ImGuiCol_Header]               = lerp_rgb(btnIdle, accent, 0.12f);
    c[ImGuiCol_HeaderHovered]        = lerp_rgb(btnHover, accent, 0.18f);
    c[ImGuiCol_HeaderActive]         = lerp_rgb(btnActive, accent, 0.22f);
    c[ImGuiCol_Separator]            = border;

    c[ImGuiCol_SliderGrab]           = btnActive;
    c[ImGuiCol_SliderGrabActive]     = btnActive;

    c[ImGuiCol_CheckMark]            = accent;
    c[ImGuiCol_TableHeaderBg]        = tableHdr;
    c[ImGuiCol_TableRowBg]           = lerp_rgb(surface, deep, 0.35f);
    c[ImGuiCol_TableRowBgAlt]        = rowAlt;
    c[ImGuiCol_TableBorderLight]     = lerp_rgb(border, surface, 0.35f);
    c[ImGuiCol_TableBorderStrong]    = border;
}

} // namespace gtui
