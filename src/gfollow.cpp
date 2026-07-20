//
// Follow-play cursor sync (M6 Phase 1 — extracted from gdisplay.cpp).
//

#include "gfollow.hpp"

void updateDisplayWhenFollowingAndPlaying(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) updateDisplayWhenFollowingAndPlaying_Expanded(gt);
    else updateDisplayWhenFollowingAndPlaying_Compressed(gt);
}

void updateDisplayWhenFollowingAndPlaying_Compressed(GTOBJECT* gt) {
    if ((followplay) && (isplaying(gt))) {
        for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
            int c2          = getActualChannel(editorInfo.esnum, c);
            int playingSong = getActualSongNumber(editorInfo.esnum, c2);

            int newpos = gt->chn[c2].lastpattptr / 4;
            if (gt->chn[c2].advance) gt->editorUndoInfo.editorInfo[c2].epnum = gt->chn[c2].pattnum;

            if (newpos > pattlen[gt->editorUndoInfo.editorInfo[c2].epnum])
                newpos = pattlen[gt->editorUndoInfo.editorInfo[c2].epnum];

            if (c == gt->masterLoopChannel) {
                editorInfo.eppos  = newpos;
                editorInfo.epview = newpos - VISIBLEPATTROWS / 2;
            }

            newpos = gt->chn[c2].songptr;
            newpos--;
            if (newpos < 0) newpos = 0;
            if (newpos > songlen[playingSong][c2 % 6]) newpos = songlen[playingSong][c2 % 6];

            gt->editorUndoInfo.editorInfo[c2].espos = gt->chn[c2].songptr - 1;
        }
    }
}

void updateDisplayWhenFollowingAndPlaying_Expanded(GTOBJECT* gt) {
    if ((followplay) && (isplaying(gt))) {
        for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
            int c2          = getActualChannel(editorInfo.esnum, c);
            int playingSong = getActualSongNumber(editorInfo.esnum, c2);

            int newpos = gt->chn[c2].lastpattptr / 4;
            if (gt->chn[c2].advance) gt->editorUndoInfo.editorInfo[c2].epnum = gt->chn[c2].pattnum;

            if (newpos > pattlen[gt->editorUndoInfo.editorInfo[c2].epnum])
                newpos = pattlen[gt->editorUndoInfo.editorInfo[c2].epnum];

            if (c == gt->masterLoopChannel) {
                editorInfo.eppos  = newpos;
                editorInfo.epview = newpos - VISIBLEPATTROWS / 2;
            }

            newpos = gt->chn[c2].songptr;
            newpos--;
            if (newpos < 0) newpos = 0;
            if (newpos > songOrderLength[playingSong][c2 % 6]) newpos = songOrderLength[playingSong][c2 % 6];

            gt->editorUndoInfo.editorInfo[c2].espos = gt->chn[c2].songptr - 1;
        }
    }
}
