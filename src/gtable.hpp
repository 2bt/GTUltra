#pragma once

#include "gcommon.hpp"
#include "ginput.hpp"
#include "gplay.hpp"

#define MST_NOFINEVIB 0
#define MST_FINEVIB 1
#define MST_FUNKTEMPO 2
#define MST_PORTAMENTO 3
#define MST_RAW 4

void tablecommands(GTOBJECT* gt, const EditorInput* input = nullptr);
bool table_cell_input(GTOBJECT* gt, const EditorInput* input = nullptr);
void tableup();
void tabledown();
int  makespeedtable(unsigned data, int mode, int makenew);
void optimizetable(int num);
void deleteinstrtable(int i);
int  gettablelen(int num);
int  gettablepartlen(int num, int pos);
void gototable(int num, int pos);
void settableview(int num, int pos);
void settableviewfirst(int num, int pos);
void validatetableview();
void exectable(int num, int ptr);
int  findfreespeedtable();
void modifyWaveTableDetailed(int hexnybble);

void allowEnterToReturnToPosition();

void table_list_insert(GTOBJECT* gt);
void table_list_delete(GTOBJECT* gt);
void table_copy_or_cut(int cut);
void table_paste();
void table_optimize();
void table_toggle_lock();
void table_test_note(GTOBJECT* gt);
void table_release_note(GTOBJECT* gt);
void table_negate_value();
void table_convert_note();
