#ifndef GINSTR_H
#define GINSTR_H

#define LAST_INST 10

#ifndef GINSTR_C
extern INSTR instrcopybuffer;
#endif

void instrumentcommands(GTOBJECT *gt, const EditorInput *input = nullptr);
bool instrument_cell_input(GTOBJECT *gt, const EditorInput *input = nullptr);
void nextinstr(void);
void previnstr(void);
void clearinstr(int num);
void gotoinstr(int i);
void showinstrtable(void);

#endif
