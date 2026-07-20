#pragma once

#include <SDL.h>

// Per-frame keyboard snapshot for editor command handlers (M3).
// docommand() captures globals into EditorInput before calling *commands()
// so handlers do not depend on key/rawkey being cleared by action dispatch.
struct EditorInput {
    int key           = 0;
    int rawkey        = 0;
    int shift         = 0;
    int ctrl          = 0;
    int shift_or_ctrl = 0;
    int hex_nybble    = -1;
};

EditorInput editor_input_snapshot();
void        editor_input_clear();

void getkey();

extern int key;
extern int rawkey;
extern int shiftpressed;
extern int ctrlpressed;
extern int shiftOrCtrlPressed;
extern int cursorflashdelay;
extern int mouseb;
extern int prevmouseb;
