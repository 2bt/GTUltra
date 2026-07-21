#pragma once

#include "gcommon.hpp"
#include "ginput.hpp"
#include "gplay.hpp"

#include <cstdint>

extern int instrumentCount[MAX_INSTR];

int  calculateLoopInfo2(int songNum, int channelNum, int startSongPos, GTOBJECT* gtloop);
int  calcStartofInterPatternLoop(int songNum, int channelNum, int startSongPos, GTOBJECT* gtloop);
void setMasterLoopChannel(GTOBJECT* gt, const char* debugText);

void orderPlayFromPosition(GTOBJECT* gt,
                           int       startPatternPos,
                           int       startSongPos,
                           int       focusChannel,
                           bool      enable_sid_writes);
void orderSelectPatternsFromSelected(GTOBJECT* gt);
void updateviewtopos(GTOBJECT* gt);
void orderlistcommands(GTOBJECT* gt, const EditorInput* input = nullptr);
void namecommands(GTOBJECT* gt, const EditorInput* input = nullptr);
void nextsong(GTOBJECT* gt);
void prevsong(GTOBJECT* gt);
void songchange(GTOBJECT* gt, bool reset_editing_positions);
void deleteorder(GTOBJECT* gt);
void insertorder(uint8_t byte, GTOBJECT* gt);
void countInstruments();
void calculateTotalInstrumentsFromAllPatterns();
void countInstrumentsInPattern(int pat);
void resetSongInfo(GTOBJECT* gt, int jc2);
int  findFirstEndMarkerIndex(int sng, int chn);
void orderListCopyMarkedArea();
void orderListCopyMarkedArea_Expanded();
void orderListPasteToCursor(GTOBJECT* gt);
void orderListPasteToCursor_External(GTOBJECT* gt, bool insert, bool transpose_only);
void orderListInsert_External(GTOBJECT* gt);
void orderListInsertRowAtCursor_External(GTOBJECT* gt, int sng, int chn, int row);
void orderListDeleteRowAtCursor_External(int sng, int chn, int row);
void orderListDelete_External();
void order_list_insert(GTOBJECT* gt);
void order_list_delete(GTOBJECT* gt);
void order_list_cut(GTOBJECT* gt);
void order_list_mark_toggle();
void order_list_transpose_up();
void order_list_transpose_down();
void order_list_insert_repeat();
void order_list_swap_channel(GTOBJECT* gt, int targetCh);
void order_col_left_expanded(GTOBJECT* gt);
void order_col_right_expanded(GTOBJECT* gt);
int  order_go_pattern(GTOBJECT* gt);
void order_select_patterns(GTOBJECT* gt);
void order_play_range_start(GTOBJECT* gt);
void order_play_range_end(GTOBJECT* gt);
