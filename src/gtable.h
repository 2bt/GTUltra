#ifndef GTABLE_H
#define GTABLE_H

#define MST_NOFINEVIB 0
#define MST_FINEVIB 1
#define MST_FUNKTEMPO 2
#define MST_PORTAMENTO 3
#define MST_RAW 4

void tablecommands(GTOBJECT *gt, const EditorInput *input = nullptr);
bool table_enter_input(GTOBJECT *gt, const EditorInput *input = nullptr);
void tableup(void);
void tabledown(void);
void inserttable(int num, int pos, int mode);
void deletetable(int num, int pos);
int makespeedtable(unsigned data, int mode, int makenew);
void optimizetable(int num);
void deleteinstrtable(int i);
int gettablelen(int num);
int gettablepartlen(int num, int pos);
void gototable(int num, int pos);
void settableview(int num, int pos);
void settableviewfirst(int num, int pos);
void validatetableview(void);
void exectable(int num, int ptr);
int findfreespeedtable(void);
void modifyWaveTableDetailed(int hexnybble);
void modifyWaveTableDetailedLeft(int hexnybble);
void modifyWaveTableDetailedRight(int hexnybble);

void modifyFilterTableDetailed(int hexnybble);
void modifyFilterTableDetailedLeft(int hexnybble);
void modifyFilterTableDetailedRight(int hexnybble);

void modifyPulseTableDetailed(int hexnybble);
void modifyPulseTableDetailedLeft(int hexnybble);
void modifyPulseTableDetailedRight(int hexnybble);

void allowEnterToReturnToPosition();

void table_list_insert(GTOBJECT *gt);
void table_list_delete(GTOBJECT *gt);
void table_copy_or_cut(int cut);
void table_paste(void);
void table_optimize(void);
void table_toggle_lock(void);
void table_test_note(GTOBJECT *gt);
void table_release_note(GTOBJECT *gt);
void table_negate_value(void);
void table_convert_note(void);

#endif
