#pragma once

#include "goattrk2.hpp"

// Follow-play cursor sync (extracted from gdisplay for M6 Phase 1).
// Updates editorInfo / per-channel pattern & order positions while playing;
// no drawing.

void updateDisplayWhenFollowingAndPlaying(GTOBJECT* gt);
void updateDisplayWhenFollowingAndPlaying_Expanded(GTOBJECT* gt);
void updateDisplayWhenFollowingAndPlaying_Compressed(GTOBJECT* gt);
