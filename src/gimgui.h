#pragma once

// Initialise ImGui on bme's existing window + renderer and install the bme
// overlay/event hooks. Call once, after the graphics subsystem is up.
void gimgui_init();

// Tear down ImGui. Call once on shutdown.
void gimgui_shutdown();

// True while the native ImGui panels are shown (false = legacy chargen UI visible).
bool gimgui_new_ui_active();

// True while an instrument-name InputText is active (keyboard -> ImGui).
bool gimgui_instr_name_editing();

// Open the on-demand name editor for instrument @p inst (1..3F).
void gimgui_instr_name_begin(int inst);

// Song metadata field focus (names panel, field index 0..2).
bool gimgui_song_field_editing();
void gimgui_song_field_begin(int field);

// UI font size in pixels (default 18). Changing reloads the font atlas.
float gimgui_font_size();
void  gimgui_set_font_size(float px);
void  gimgui_adjust_font_size(int delta_px); // e.g. +/-2 from View menu

// Drop stale mouse/keyboard state after a blocking native file dialog.
void gimgui_reset_input_after_modal();
