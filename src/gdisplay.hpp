#ifndef GDISPLAY_H
#define GDISPLAY_H

// Visible order-list page size (expanded view scroll).
#define EXTENDEDVISIBLEORDERLIST 13

#ifndef GDISPLAY_C
extern const char* notename[];
extern const char* notenameTableView[];
extern char        timechar[];
#endif

void displayupdate(GTOBJECT* gt);
void resettime(GTOBJECT* gt);
void incrementtime(GTOBJECT* gt);
void setSongLengthTime(GTOBJECT* gt);
void setSIDTracker64KeyOnStyle();
int  doDisplay(void* gt);

void updateDisplayWhenFollowingAndPlaying(GTOBJECT* gt);
void updateDisplayWhenFollowingAndPlaying_Expanded(GTOBJECT* gt);
void updateDisplayWhenFollowingAndPlaying_Compressed(GTOBJECT* gt);

#endif
