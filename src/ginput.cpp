#define GINPUT_C

#include "ginput.hpp"
#include "gconsole.hpp"

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
