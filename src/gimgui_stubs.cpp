// Empty ImGui shims for binaries that link gtcore but have no UI (gt2reloc).
#include "gimgui.hpp"

void gimgui_init() {}
void gimgui_shutdown() {}
void gimgui_process_event(const SDL_Event*) {}
void gimgui_render() {}
int  gimgui_input_capture() { return GimguiCaptureNone; }

bool  gimgui_instr_name_editing() { return false; }
void  gimgui_instr_name_begin(int) {}
void  gimgui_open_help() {}
void  gimgui_close_help() {}
bool  gimgui_help_open() { return false; }
bool  gimgui_song_field_editing() { return false; }
void  gimgui_song_field_begin(int) {}
void  gimgui_song_field_end() {}
float gimgui_font_size() { return 18.0f; }
void  gimgui_set_font_size(float) {}
void  gimgui_adjust_font_size(int) {}
void  gimgui_reset_input_after_modal() {}
