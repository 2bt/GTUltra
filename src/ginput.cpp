#include "ginput.hpp"

#include "gimgui.hpp"
#include "gplatform.hpp"

extern int hexnybble;

int      ascii_key             = 0;
int      scancode              = 0;
bool     shift_pressed         = false;
bool     ctrl_pressed          = false;
bool     shift_or_ctrl_pressed = false;
int      cursorflashdelay      = 0;
unsigned mouse_buttons         = 0;
unsigned prev_mouse_buttons    = 0;

EditorInput editor_input_snapshot() {
    EditorInput in;
    in.ascii_key     = ascii_key;
    in.scancode      = scancode;
    in.shift         = shift_pressed;
    in.ctrl          = ctrl_pressed;
    in.shift_or_ctrl = shift_or_ctrl_pressed;
    in.hex_nybble    = hexnybble;
    return in;
}

void editor_input_clear() {
    ascii_key = 0;
    scancode  = 0;
}

void getkey() {
    win_asciikey = 0;
    cursorflashdelay += win_getspeed(50);

    prev_mouse_buttons = mouse_buttons;
    mouse_buttons      = mou_getbuttons();

    // The first non-modifier key held this frame becomes the scancode.
    ascii_key = win_asciikey;
    scancode  = 0;
    for (int sc = 0; sc < SDL_NUM_SCANCODES; sc++) {
        if (win_keytable[sc] && sc != SDL_SCANCODE_LSHIFT && sc != SDL_SCANCODE_RSHIFT &&
            sc != SDL_SCANCODE_LCTRL && sc != SDL_SCANCODE_RCTRL) {
            scancode         = sc;
            win_keytable[sc] = 0;
            break;
        }
    }

    ctrl_pressed          = win_keystate[SDL_SCANCODE_LCTRL] || win_keystate[SDL_SCANCODE_RCTRL];
    shift_pressed         = win_keystate[SDL_SCANCODE_LSHIFT] || win_keystate[SDL_SCANCODE_RSHIFT];
    shift_or_ctrl_pressed = shift_pressed || ctrl_pressed;

    // Keypad Enter acts as Return; keypad digits produce their ASCII digit
    // (KP_1..KP_9 are contiguous in SDL, with KP_0 sitting just after them).
    if (scancode == SDL_SCANCODE_KP_ENTER) {
        ascii_key = SDL_SCANCODE_RETURN;
        scancode  = SDL_SCANCODE_RETURN;
    }
    else if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9) ascii_key = '1' + (scancode - SDL_SCANCODE_KP_1);
    else if (scancode == SDL_SCANCODE_KP_0) ascii_key = '0';

    // ImGui owns the mouse/keyboard while one of its widgets is focused.
    const int cap = gimgui_input_capture();
    if (cap & GimguiCaptureMouse) {
        mouse_buttons      = 0;
        prev_mouse_buttons = 0;
        win_mousewheel     = 0;
    }
    if (cap & GimguiCaptureKeyboard) {
        ascii_key    = 0;
        scancode     = 0;
        win_asciikey = 0;
    }
}
