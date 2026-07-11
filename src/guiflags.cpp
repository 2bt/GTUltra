//
// Shared ImGui UI toggle (lives in gtcore so legacy *commands() can query it).
//

#include "gimgui.hpp"

bool g_show_new_ui = false;

bool gimgui_new_ui_active() { return g_show_new_ui; }
