//
// GTUltra display glue (M6 Phase 2 — chargen draw tree removed).
// Keeps present/follow-play, song timer helpers, and note-name tables.
//

#define GDISPLAY_C

#include "goattrk2.hpp"
#include "gimgui.hpp"
#include "gfollow.hpp"

const char* notename[] = {
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0", "C-1", "C#1", "D-1", "D#1",
    "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1", "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2",
    "G#2", "A-2", "A#2", "B-2", "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4", "C-5", "C#5", "D-5", "D#5",
    "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5", "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6",
    "G#6", "A-6", "A#6", "B-6", "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "...", "---", "+++",
};

const char* notenameTableView[] = {
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0", "C-1", "C#1", "D-1", "D#1",
    "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1", "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2",
    "G#2", "A-2", "A#2", "B-2", "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4", "C-5", "C#5", "D-5", "D#5",
    "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5", "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6",
    "G#6", "A-6", "A#6", "B-6", "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7",
    "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???",
    "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???", "???",
};

char timechar[] = { ':', ' ' };
int  initForST64 = 0;

void setSIDTracker64KeyOnStyle() {
    if (SIDTracker64ForIPadIsAmazing != 0) notename[(12 * 8) - 1] = " | ";
    else
        notename[(12 * 8) - 1] = "+++";
}


void displayupdate(GTOBJECT* gt) {
    if (cursorflashdelay >= 6) {
        cursorflashdelay %= 6;
        cursorflash++;
        cursorflash &= 3;
    }
    doDisplay((void*)gt);
}

int doDisplay(void* gt) {
    GTOBJECT* gto = (GTOBJECT*)gt;
    updateDisplayWhenFollowingAndPlaying(gto);
    gfx_present();
    return 0;
}

void resettime(GTOBJECT* gt) {
    gt->timemin   = 0;
    gt->timesec   = 0;
    gt->timeframe = 0;
}

void setSongLengthTime(GTOBJECT* gt) {
    gt->totalFrame = gt->timeframe;
    gt->totalSec   = gt->timesec;
    gt->totalMin   = gt->timemin;
}

void incrementtime(GTOBJECT* gt) {
    gt->timeframe++;
    if (!editorInfo.ntsc) {
        if (((editorInfo.multiplier) && (gt->timeframe >= PALFRAMERATE * editorInfo.multiplier)) ||
            ((!editorInfo.multiplier) && (gt->timeframe >= PALFRAMERATE / 2))) {
            gt->timeframe = 0;
            gt->timesec++;
        }
    }
    else {
        if (((editorInfo.multiplier) && (gt->timeframe >= NTSCFRAMERATE * editorInfo.multiplier)) ||
            ((!editorInfo.multiplier) && (gt->timeframe >= NTSCFRAMERATE / 2))) {
            gt->timeframe = 0;
            gt->timesec++;
        }
    }
    if (gt->timesec == 60) {
        gt->timesec = 0;
        gt->timemin++;
        gt->timemin %= 60;
    }
}
