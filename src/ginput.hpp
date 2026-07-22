#pragma once

// Per-frame input snapshot for editor command handlers (M3).
// docommand() captures the live input globals into an EditorInput before
// calling the *commands() handlers, so they don't depend on ascii_key /
// scancode being cleared by action dispatch mid-frame.
struct EditorInput {
    int  ascii_key     = 0;  // ASCII of the character typed this frame, or 0
    int  scancode      = 0;  // SDL scancode pressed this frame, or 0
    bool shift         = false;
    bool ctrl          = false;
    bool shift_or_ctrl = false;
    int  hex_nybble    = -1; // 0..15 when a hex digit is pressed, else -1
};

EditorInput editor_input_snapshot();
void        editor_input_clear();

void getkey();

// Live per-frame input state, refreshed by getkey().
extern int      ascii_key;             // ASCII of the character typed this frame
extern int      scancode;              // SDL scancode pressed this frame
extern bool     shift_pressed;
extern bool     ctrl_pressed;
extern bool     shift_or_ctrl_pressed;
extern int      cursorflashdelay;
extern unsigned mouse_buttons;         // current mouse-button bitmask (MOUSEB_*)
extern unsigned prev_mouse_buttons;    // previous frame's button bitmask
