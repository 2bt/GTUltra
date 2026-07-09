//
// guicolors - semantic UI color roles for the ImGui layer.
//
// All tracker UI colors are defined once here (defaults table in guicolors.cpp).
// Panels call color() / color_a() instead of scattering IM_COL32 literals.
// M7 will load overrides from config.toml by role name.
//
#ifndef GUICOLORS_H
#define GUICOLORS_H

#include "imgui.h"

namespace gtui {

enum class GuiColorRole : unsigned char {
    // Chrome / fixed layout
    AppBackground,
    PanelHeaderBg,
    PanelHeaderBgActive,
    PanelHeaderText,
    PanelHeaderTextActive,

    // ImGui widget surfaces (derived into ImGuiCol_* in gui_colors_apply_imgui_style)
    WidgetSurface,
    WidgetSurfaceDeep,
    WidgetFrameBg,
    WidgetBorder,
    WidgetAccent,

    // Grid text
    GridPrimaryText,
    GridNoteText,
    GridSecondaryText,
    GridHeaderText,
    GridCommandText,
    GridInstrumentText,
    GridMuted,
    GridDots,
    GridEndMarker,

    // Grid highlights
    GridCursorRow,
    GridCursorRowWarm,
    Cursor,
    Selection,
    InstrumentHighlight,
    Playhead,
    BeatLine,
    MasterChannel,
    Error,

    Count
};

void gui_colors_init();
void gui_colors_reset_defaults();

ImU32 color(GuiColorRole role);
ImU32 color_a(GuiColorRole role, unsigned char alpha);

const char* color_role_name(GuiColorRole role);
const char* color_role_label(GuiColorRole role);

// Apply ImGuiCol_* from the semantic palette (call after spacing setup).
void gui_colors_apply_imgui_style();

} // namespace gtui

#endif
