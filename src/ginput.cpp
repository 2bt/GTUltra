#include "ginput.hpp"

#include "gimgui.hpp"
#include "gplatform.hpp"

extern int hexnybble;

int      key                = 0;
int      rawkey             = 0;
bool     shiftpressed       = false;
bool     shiftOrCtrlPressed = false;
bool     ctrlpressed        = false;
int      cursorflashdelay   = 0;
unsigned mouseb             = 0;
unsigned prevmouseb         = 0;

EditorInput editor_input_snapshot() {
    EditorInput in;
    in.key           = key;
    in.rawkey        = rawkey;
    in.shift         = shiftpressed;
    in.ctrl          = ctrlpressed;
    in.shift_or_ctrl = shiftOrCtrlPressed;
    in.hex_nybble    = hexnybble;
    return in;
}

void editor_input_clear() {
    key    = 0;
    rawkey = 0;
}

void getkey() {
    win_asciikey = 0;
    cursorflashdelay += win_getspeed(50);

    prevmouseb = mouseb;
    mouseb     = mou_getbuttons();

    // The first non-modifier key held this frame becomes the raw key.
    key    = win_asciikey;
    rawkey = 0;
    for (int scancode = 0; scancode < SDL_NUM_SCANCODES; scancode++) {
        if (win_keytable[scancode] && scancode != SDL_SCANCODE_LSHIFT && scancode != SDL_SCANCODE_RSHIFT &&
            scancode != SDL_SCANCODE_LCTRL && scancode != SDL_SCANCODE_RCTRL) {
            rawkey                 = scancode;
            win_keytable[scancode] = 0;
            break;
        }
    }

    ctrlpressed        = win_keystate[SDL_SCANCODE_LCTRL] || win_keystate[SDL_SCANCODE_RCTRL];
    shiftpressed       = win_keystate[SDL_SCANCODE_LSHIFT] || win_keystate[SDL_SCANCODE_RSHIFT];
    shiftOrCtrlPressed = shiftpressed || ctrlpressed;

    // Keypad Enter acts as Return; keypad digits produce their ASCII digit
    // (KP_1..KP_9 are contiguous in SDL, with KP_0 sitting just after them).
    if (rawkey == SDL_SCANCODE_KP_ENTER) {
        key    = SDL_SCANCODE_RETURN;
        rawkey = SDL_SCANCODE_RETURN;
    }
    else if (rawkey >= SDL_SCANCODE_KP_1 && rawkey <= SDL_SCANCODE_KP_9) key = '1' + (rawkey - SDL_SCANCODE_KP_1);
    else if (rawkey == SDL_SCANCODE_KP_0) key = '0';

    // ImGui owns the mouse/keyboard while one of its widgets is focused.
    const int cap = gimgui_input_capture();
    if (cap & GimguiCaptureMouse) {
        mouseb         = 0;
        prevmouseb     = 0;
        win_mousewheel = 0;
    }
    if (cap & GimguiCaptureKeyboard) {
        key          = 0;
        rawkey       = 0;
        win_asciikey = 0;
    }
}
