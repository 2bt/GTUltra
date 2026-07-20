#pragma once

#include "gplay.hpp"

#include <cstdint>

struct WAVEFORM_INFO {
    bool     displayOnOff;
    int      value;
    uint8_t* destAddress;
};

// Shared with the editor frame / other panels (force or clear the info line).
extern bool clearInfoLine;
extern int  forceInfoLine;     // countdown: skip that many info-line refreshes
extern int  lastEditWindow;    // last EditMode as int; -1 forces redraw
extern int  lastInfoPatternCh; // channel; -1 forces redraw

// Frame timing written by the main loop; consumed when refreshing pattern info.
extern int      msDelta;
extern uint32_t lastMS;

void displayPatternInfo(GTOBJECT* gt);
void displayInstrumentInfo(GTOBJECT* gt);
void displayTableInfo(GTOBJECT* gt);
void displayWaveTableInfo(GTOBJECT* gt);
void displayPulseTableInfo(GTOBJECT* gt);
void displayFilterTableInfo(GTOBJECT* gt);
void displaySpeedTableInfo(GTOBJECT* gt);
void displayOrderTableInfo(GTOBJECT* gt);
