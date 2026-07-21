// Empty ImGui shims for binaries that link gtcore but have no UI (gt2reloc).
#include "gimgui.hpp"
#include "guimodel.hpp"

// gtcore's song/table/undo code reports transient status via gtui::set_status.
// The relocator has no info line, so the messages are dropped.
namespace gtui {
void set_status(const char*, ...) {}
} // namespace gtui

void gimgui_init() {}
void gimgui_process_event(const SDL_Event*) {}
void gimgui_render() {}
int  gimgui_input_capture() { return GimguiCaptureNone; }

bool gimgui_instr_name_editing() { return false; }
void gimgui_instr_name_begin(int) {}
void gimgui_set_help_open(bool) {}
bool gimgui_help_open() { return false; }
bool gimgui_song_field_editing() { return false; }
void gimgui_song_field_begin(int) {}
void gimgui_song_field_end() {}
void gimgui_reset_input_after_modal() {}
