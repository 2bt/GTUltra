#pragma once

#include <SDL.h>

// ImGui layer. Real implementation in gimgui.cpp (gtultra).
// Empty stubs in gimgui_stubs.cpp (gt2reloc — no UI).

void gimgui_init();

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

void gimgui_set_help_open(bool open);
bool gimgui_help_open();

bool gimgui_song_field_editing();
void gimgui_song_field_begin(int field);
void gimgui_song_field_end();

void gimgui_reset_input_after_modal();
