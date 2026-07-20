#include "ginput.hpp"

#include "gimgui.hpp"
#include "gplatform.hpp"

extern int hexnybble;

int key                = 0;
int rawkey             = 0;
int shiftpressed       = 0;
int shiftOrCtrlPressed = 0;
int ctrlpressed        = 0;
int cursorflashdelay   = 0;
int mouseb             = 0;
int prevmouseb         = 0;

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
    mouseb     = (int)mou_getbuttons();

    key    = win_asciikey;
    rawkey = 0;
    for (int c = 0; c < SDL_NUM_SCANCODES; c++) {
        if (win_keytable[c]) {
            if ((c != SDL_SCANCODE_LSHIFT) && (c != SDL_SCANCODE_RSHIFT) && (c != SDL_SCANCODE_LCTRL) &&
                (c != SDL_SCANCODE_RCTRL)) {
                rawkey          = c;
                win_keytable[c] = 0;
                break;
            }
        }
    }

    ctrlpressed  = 0;
    shiftpressed = 0;

    if (win_keystate[SDL_SCANCODE_LCTRL] || win_keystate[SDL_SCANCODE_RCTRL]) ctrlpressed = 1;

    if (win_keystate[SDL_SCANCODE_LSHIFT] || win_keystate[SDL_SCANCODE_RSHIFT]) shiftpressed = 1;

    shiftOrCtrlPressed = shiftpressed | ctrlpressed;

    if (rawkey == SDL_SCANCODE_KP_ENTER) {
        key    = KEY_ENTER;
        rawkey = SDL_SCANCODE_RETURN;
    }

    if (rawkey == SDL_SCANCODE_KP_0) key = '0';
    if (rawkey == SDL_SCANCODE_KP_1) key = '1';
    if (rawkey == SDL_SCANCODE_KP_2) key = '2';
    if (rawkey == SDL_SCANCODE_KP_3) key = '3';
    if (rawkey == SDL_SCANCODE_KP_4) key = '4';
    if (rawkey == SDL_SCANCODE_KP_5) key = '5';
    if (rawkey == SDL_SCANCODE_KP_6) key = '6';
    if (rawkey == SDL_SCANCODE_KP_7) key = '7';
    if (rawkey == SDL_SCANCODE_KP_8) key = '8';
    if (rawkey == SDL_SCANCODE_KP_9) key = '9';

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
