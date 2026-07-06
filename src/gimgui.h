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
