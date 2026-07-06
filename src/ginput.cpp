#define GINPUT_C

#include "ginput.h"
#include "gconsole.h"

extern int hexnybble;

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
