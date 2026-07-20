#pragma once

#include "gcommon.hpp"
#include "ginput.hpp"
#include "gplay.hpp"

#define LAST_INST 10

extern INSTR instrcopybuffer;

void instrumentcommands(GTOBJECT* gt, const EditorInput* input = nullptr);
bool instrument_cell_input(GTOBJECT* gt, const EditorInput* input = nullptr);
void nextinstr();
void previnstr();
void clearinstr(int num);
void gotoinstr(int i);
