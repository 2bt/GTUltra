#pragma once

#include <SDL.h>

// ImGui layer. Real implementation in gimgui.cpp (gtultra).
// Empty stubs in gimgui_stubs.cpp (gt2reloc — no UI).

void gimgui_init();
void gimgui_shutdown();

void gimgui_process_event(const SDL_Event* event);
void gimgui_render(); // draw frame into gfx_renderer (call between clear and present)

// Bitflags: legacy editor should ignore mouse / keyboard while ImGui wants them.
enum GimguiCapture : int {
    GimguiCaptureNone     = 0,
    GimguiCaptureMouse    = 1,
    GimguiCaptureKeyboard = 2,
};
int gimgui_input_capture();

bool gimgui_instr_name_editing();
void gimgui_instr_name_begin(int inst);

void gimgui_open_help();
void gimgui_close_help();
bool gimgui_help_open();

bool gimgui_song_field_editing();
void gimgui_song_field_begin(int field);
void gimgui_song_field_end();

float gimgui_font_size();
void  gimgui_set_font_size(float px);
void  gimgui_adjust_font_size(int delta_px);

void gimgui_reset_input_after_modal();
