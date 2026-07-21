//
// GTUltra orderlist & songname editor
//

#include "goattrk2.hpp"
#include "gimgui.hpp"
#include "gorder.hpp"

namespace {

// gorder-local state.
unsigned char trackcopybuffer[MAX_SONGLEN + 2];
int           trackcopyrows = 0;
int           trackcopywhole;
int           trackcopyrpos;
int           lastSong = -1;
int           tempPatternMin  = 0;
int           tempPatternSec  = 0;
int           tempPatterFrame = 0;
int           pattInstrumentCount[MAX_PATT][MAX_INSTR];
int           instrumentCount[MAX_INSTR];
int           firstInstrumentPattern[MAX_INSTR];
int           patternChecked[MAX_PATT];

int order_expanded_max_channels() {
    if ((editorInfo.maxSIDChannels == 3) || (editorInfo.maxSIDChannels == 9 && (editorInfo.esnum & 1))) return 3;
    return 6;
}

// gorder-local helpers, defined here (above their first use) so no
// forward declarations are needed.

void order_hex_input_original_view(GTOBJECT* gt) {
    if (editorInfo.eseditpos != songlen[editorInfo.esnum][editorInfo.eschn]) {
        switch (editorInfo.escolumn) {
        case 0:
            songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0x0f;
            songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble << 4;
            if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
                if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] >= MAX_PATT)
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = MAX_PATT - 1;
            }
            else {
                if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] >= MAX_SONGLEN)
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = MAX_SONGLEN - 1;
            }
            break;

        case 1:
            songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0xf0;
            if ((songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] & 0xf0) == REPEAT) {
                hexnybble--;
                if (hexnybble < 0) hexnybble = 0xf;
            }
            if ((songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] & 0xf0) == TRANSDOWN) {
                hexnybble = 16 - hexnybble;
                hexnybble &= 0xf;
            }
            songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble;

            if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
                if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] == LOOPSONG)
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = LOOPSONG - 1;
                if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] == TRANSDOWN)
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = TRANSDOWN + 0x0f;
            }
            else {
                if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] >= MAX_SONGLEN)
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = MAX_SONGLEN - 1;
            }
            break;
        }

        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn); // 0-12

        if (editorInfo.eseditpos == gt->editorUndoInfo.editorInfo[c2].espos) {
            if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] <
                MAX_PATT) // remember pattern number for undo
                gt->editorUndoInfo.editorInfo[c2].epnum =
                    songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos];
        }

        editorInfo.escolumn++;
        if (editorInfo.escolumn > 1) {
            editorInfo.escolumn = 0;
            if (editorInfo.eseditpos < (songlen[editorInfo.esnum][editorInfo.eschn] + 1)) {
                editorInfo.eseditpos++;
                if (editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) editorInfo.eseditpos++;
            }
        }
    }
}

void update_transpose_to_playing_song(GTOBJECT* gt) {
    int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);
    if (editorInfo.eseditpos ==
        gt->chn[c2].songptr - 1) // cursor editing row as whats currently playing in this channel?
    {
        int t = songOrderTranspose[editorInfo.esnum][editorInfo.eschn]
                                  [editorInfo.eseditpos]; // Yes. So modify the transpose directly too
        if (t < 0x080) gt->chn[c2].trans = t;
        else gt->chn[c2].trans = -(t & 0x7f);
    }
}

// TO WORK OUT:
// Loop position can be 12 bit in expanded view.
// We can't enter a value longer than 2 digits
// Shift-click on entry to set loop position?
void order_hex_input_expanded_view(GTOBJECT* gt) {
    // songOrderPatterns[editorInfo.esnum][c][p];

    if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] <
        0xff) // editing pattern number or transpose value
    {
        if (editorInfo.escolumn == 3) return; // cursor currently on +/- so we don't want to handle hex input there

        if (editorInfo.escolumn == 4) // transpose value
        {
            if (songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < 0x80) {
                if (hexnybble < 0xf) {
                    songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = hexnybble;
                }
            }
            else {
                songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = 0x80 + hexnybble;
            }

            update_transpose_to_playing_song(gt);

            songCompressedSize[editorInfo.esnum][editorInfo.eschn] =
                generateCompressedSongChannel(editorInfo.esnum, editorInfo.eschn, true);
            return;
        }
    }
    else if (editorInfo.escolumn >= 2) // editing loop position
    {
        switch (editorInfo.escolumn) {
        case 2:
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0x0ff;
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble << 8;
            editorInfo.escolumn = 3;
            break;
        case 3:
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0x0f0f;
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble << 4;
            editorInfo.escolumn = 4;
            break;
        case 4:
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0xff0;
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble;
            editorInfo.escolumn = 2;
            break;
        }
        songCompressedSize[editorInfo.esnum][editorInfo.eschn] =
            generateCompressedSongChannel(editorInfo.esnum, editorInfo.eschn, true);
        return;
    }

    if (editorInfo.escolumn <= 2) // pattern editing
    {
        int temp       = songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos];
        int tempColumn = editorInfo.escolumn;

        switch (editorInfo.escolumn) {
        case 0:

            songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0x0f;
            songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble << 4;
            editorInfo.escolumn = 1;
            break;
        case 1:
            songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0xf0;
            songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= hexnybble;
            editorInfo.escolumn = 0;
            break;
        }

        if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] == 0xff) {
            if (temp == 0xff) editorInfo.escolumn = tempColumn;
            else songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = 0;
        }
        else if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] >= REPEAT) {
            if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < TRANSUP) {
                songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = temp;
                editorInfo.escolumn                                                         = tempColumn;
            }
            else {
                editorInfo.escolumn                                                          = tempColumn;
                songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos]  = 0xff;
                songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = 0;
            }
        }
        else if (temp == 0xff) {
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = 0;
        }
        else {
            int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn); // 0-12

            if (editorInfo.eseditpos == gt->editorUndoInfo.editorInfo[c2].espos) {
                if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] <
                    MAX_PATT) // jpjpjp
                    gt->editorUndoInfo.editorInfo[c2].epnum =
                        songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos];
            }
        }
    }

    songCompressedSize[editorInfo.esnum][editorInfo.eschn] =
        generateCompressedSongChannel(editorInfo.esnum, editorInfo.eschn, true);

    int index = findFirstEndMarkerIndex(editorInfo.esnum, editorInfo.eschn);
    songOrderLength[editorInfo.esnum][editorInfo.eschn] = index + 1;

    // sprintf(textbuffer, "j %x, chn %x, songorderLen %x\n", editorInfo.esnum, editorInfo.eschn,
    // (songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1));

    //	songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos]++;
    return;
}

int handle_enter_in_compressed_view(GTOBJECT* gt) {
    if (editorInfo.eseditpos >= songlen[editorInfo.esnum][editorInfo.eschn]) return 0;

    if (!shift_or_ctrl_pressed) {
        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn); // 0-12

        if (songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < MAX_PATT)
            gt->editorUndoInfo.editorInfo[c2].epnum =
                songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos];
    }
    else {
        backupPatternDisplayInfo(gt); // V1.2.2 - Preserve pattern edit position
        orderSelectPatternsFromSelected(gt);
        restorePatternDisplayInfo(gt); // V1.2.2
        return 0;
    }
    return 1;
}

int handle_enter_in_expanded_view(GTOBJECT* gt) {

    //	sprintf(textbuffer, "snd %x, chn %x, songorderLen %x\n", editorInfo.esnum,
    // editorInfo.eschn,(songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1));

    if (editorInfo.eseditpos >= songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1) // 1.3.3
        return 0;

    if (!shift_or_ctrl_pressed) {
        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn); // 0-12

        if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < MAX_PATT)
            gt->editorUndoInfo.editorInfo[c2].epnum =
                songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos];
    }
    else {
        backupPatternDisplayInfo(gt); // V1.2.2 - Preserve pattern edit position
        orderSelectPatternsFromSelected(gt);
        restorePatternDisplayInfo(gt); // V1.2.2
        return 0;
    }
    return 1;
}

void get_expanded_selected_area(int* x, int* y, int* w, int* h) {
    int tx, ty, tw, th;

    if (editorInfo.esmarkchn < 0 || editorInfo.esmarkchnend < 0 || editorInfo.esmarkstart < 0 ||
        editorInfo.esmarkend < 0) {
        *x = 0;
        *y = 0;
        *w = 0;
        *h = 0;
        return;
    }

    tx = editorInfo.esmarkchn;
    tw = editorInfo.esmarkchnend - editorInfo.esmarkchn;
    if (tw < 0) {
        tx = editorInfo.esmarkchnend;
        tw = editorInfo.esmarkchn - editorInfo.esmarkchnend;
    }
    tw++;

    ty = editorInfo.esmarkstart;
    th = editorInfo.esmarkend - editorInfo.esmarkstart;
    if (th < 0) {
        ty = editorInfo.esmarkend;
        th = editorInfo.esmarkstart - editorInfo.esmarkend;
    }
    th++;

    *x = tx;
    *y = ty;
    *w = tw;
    *h = th;
}

} // namespace

// Hex-digit entry into the order-list cell under the cursor. Everything else
// the old orderlistcommands() switch did was dead: every navigation/editing
// key is a bound Ctx::Order action consumed by the action layer before this
// fallback runs, so only nibble entry (which has no binding) remains.
bool order_cell_input(GTOBJECT* gt, const EditorInput* input) {
    const EditorInput in = input ? *input : editor_input_snapshot();
    if (in.hex_nybble < 0) return false;

    if (editorInfo.expandOrderListView == 0) order_hex_input_original_view(gt);
    else order_hex_input_expanded_view(gt);
    return true;
}

void namecommands(GTOBJECT* gt, const EditorInput* input) {
    (void)gt;
    (void)input;
    // M6: song name editing is ImGui-only.
}

// Insert single byte into orderlist
void insertorder(uint8_t byte, GTOBJECT* gt) {
    auto& order = songorder[editorInfo.esnum][editorInfo.eschn];
    int&  slen  = songlen[editorInfo.esnum][editorInfo.eschn];
    int&  pos   = editorInfo.eseditpos;

    if ((slen - pos) - 1 >= 0) {
        int len;
        if (slen < MAX_SONGLEN) {
            len            = slen + 1;
            order[len + 1] = order[len];
            order[len]     = LOOPSONG;
            if (len) order[len - 1] = byte;
            countthispattern(gt);
        }
        memmove(&order[pos + 1], &order[pos], (slen - pos) - 1);
        order[pos] = byte;
        len        = slen + 1;
        if ((order[len] > pos) && (order[len] < (len - 2))) order[len]++;
    }
    else {
        if (pos > slen) {
            if (slen < MAX_SONGLEN) {
                order[pos + 1] = order[pos];
                order[pos]     = LOOPSONG;
                if (pos) order[pos - 1] = byte;
                countthispattern(gt);
                pos = slen + 1;
            }
        }
    }
}

void deleteorder(GTOBJECT* gt) {
    auto& order = songorder[editorInfo.esnum][editorInfo.eschn];
    int&  slen  = songlen[editorInfo.esnum][editorInfo.eschn];
    int&  pos   = editorInfo.eseditpos;

    if ((slen - pos) - 1 >= 0) {
        int len;
        memmove(&order[pos], &order[pos + 1], (slen - pos) - 1);
        order[slen - 1] = 0x00;
        if (slen > 0) {
            order[slen - 1] = order[slen];
            order[slen]     = order[slen + 1];
            countthispattern(gt);
        }
        if (pos == slen) pos++;
        len = slen + 1;
        if ((order[len] > pos) && (order[len] > 0)) order[len]--;
    }
    else {
        if (pos > slen) {
            if (slen > 0) {
                order[slen - 1] = order[slen];
                order[slen]     = order[slen + 1];
                countthispattern(gt);
                pos = slen + 1;
            }
        }
    }
}

void nextsong(GTOBJECT* gt) {

    if (editorInfo.expandOrderListView) {
        if (validateAllSongs() > 0xff) return;
        compressSong(editorInfo.esnum);
    }
    editorInfo.esnum++;
    if (editorInfo.esnum >= MAX_SONGS) editorInfo.esnum = MAX_SONGS - 1;
    songchange(gt, true);

    //	if (gt->masterLoopSubSong==-1)
    //		setMasterLoopChannel(gt,"nextsong");
}

void prevsong(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView) {
        if (validateAllSongs() > 0xff) return;
        compressSong(editorInfo.esnum);
    }

    editorInfo.esnum--;
    if (editorInfo.esnum < 0) editorInfo.esnum = 0;
    songchange(gt, true);
    //	if (gt->masterLoopSubSong == -1)
    //		setMasterLoopChannel(gt, "prevsong");
}

void songchange(GTOBJECT* gt, bool reset_editing_positions) {
    int c;
    int s = editorInfo.esnum / 2; // JP 9 or 12 channel song only

    int jc2 =
        getActualChannel(editorInfo.esnum, editorInfo.eschn); // 0-12 for currently selected channel in orderlist
    //	int jsongNum = getActualSongNumber(editorInfo.esnum, jc2);

    editorInfo.highlightLoopChannel = 999; // remove from display
                                           //	gt->interPatternLoopEnabledFlag = 0;		// disable in player
    editorInfo.highlightLoopPatternNumber = -1;
    editorInfo.highlightLoopStart = editorInfo.highlightLoopEnd = 0;

    if (editorInfo.maxSIDChannels <= 6) lastSong = s + 1;
    if (s != lastSong) {
        lastSong = s;

        if (reset_editing_positions) {
            resetSongInfo(gt, jc2);
        }

        if (gt->songinit != PlayMode::Stopped) {
            stopsong(gt);
        }
    }

    if ((editorInfo.maxSIDChannels == 3) || (editorInfo.maxSIDChannels == 9 && (editorInfo.esnum & 1))) {
        if (editorInfo.eschn >= 3) editorInfo.eschn = 2;
    }

    for (c = 0; c < editorInfo.maxSIDChannels; c++) {
        int c2      = getActualChannel(editorInfo.esnum, c); // 0-12
        int songNum = getActualSongNumber(editorInfo.esnum, c2);

        if (gt->editorUndoInfo.editorInfo[c2].espos >= songlen[songNum][c2 % 6] + 1) {
            gt->editorUndoInfo.editorInfo[c2].espos = songlen[songNum][c2 % 6] - 1; // 0;	//
            if (gt->editorUndoInfo.editorInfo[c2].espos < 0) gt->editorUndoInfo.editorInfo[c2].espos = 0;
        }

        if (c2 == jc2 && editorInfo.eseditpos > songlen[songNum][c2 % 6] + 1) // +1 as we have the RPT text
        {
            editorInfo.eseditpos = songlen[songNum][c2 % 6] - 1; // 0;
            if (editorInfo.eseditpos < 0) editorInfo.eseditpos = 0;
        }
    }

    orderSelectPatternsFromSelected(gt);

    editorInfo.eppos  = 0; // pattern pos
    editorInfo.epview = -VISIBLEPATTROWS / 2;

    if (editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) editorInfo.eseditpos++;
    // editorInfo.epmarkchn = -1;	// JP - Removed on 27thAug2022 - Show any existing marked channel area
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;

    updateviewtopos(gt);
}

/*
A cut down version of songchange
initialises editor parameters (espos and epnum..)
*/
void resetSongInfo(GTOBJECT* gt, int jc2) {
    for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
        gt->editorUndoInfo.editorInfo[c].espos = 0; // highlighted (green) position
        gt->editorUndoInfo.editorInfo[c].esend = 0; // end position (length of song)
        gt->editorUndoInfo.editorInfo[c].epnum = c;
    }

    gt->masterLoopSubSong = editorInfo.esnum;
    gt->masterLoopChannel = jc2;

    editorInfo.eseditpos = 0; // Reset cursor position in order list
    editorInfo.esview    = 0; // reset scroll position in order list
}

void updateviewtopos(GTOBJECT* gt) {
    int c, d;

    for (c = 0; c < editorInfo.maxSIDChannels; c++) {
        int c2      = getActualChannel(editorInfo.esnum, c); // 0-12
        int songNum = getActualSongNumber(editorInfo.esnum, c2);
        int c3      = c % 6;

        if (editorInfo.expandOrderListView == 0) {
            for (d = gt->editorUndoInfo.editorInfo[c2].espos; d < songlen[songNum][c3]; d++) {
                if (songorder[songNum][c3][d] < MAX_PATT) {
                    gt->editorUndoInfo.editorInfo[c2].epnum = songorder[songNum][c3][d];
                    break;
                }
                else {
                    if (gt->editorUndoInfo.editorInfo[c2].epnum >=
                        MAX_PATT) // JP added this. Handles 12 channel songs where there's no data in odd song
                                  // number
                        gt->editorUndoInfo.editorInfo[c2].epnum = 0;
                }
            }
            if (songlen[songNum][c3] == 0) {
                gt->editorUndoInfo.editorInfo[c2].epnum = 0;
            }
        }
        else {
            for (d = gt->editorUndoInfo.editorInfo[c2].espos; d < songOrderLength[songNum][c3]; d++) {
                if (songOrderPatterns[songNum][c3][d] < MAX_PATT) {
                    gt->editorUndoInfo.editorInfo[c2].epnum = songOrderPatterns[songNum][c3][d];
                    break;
                }
                else {
                    if (gt->editorUndoInfo.editorInfo[c2].epnum >=
                        MAX_PATT) // JP added this. Handles 12 channel songs where there's no data in odd song
                                  // number
                        gt->editorUndoInfo.editorInfo[c2].epnum = 0;
                }
            }
            if (songOrderLength[songNum][c3] == 0) {
                gt->editorUndoInfo.editorInfo[c2].epnum = 0;
            }
        }
    }
}

int calcStartofInterPatternLoop(int songNum, int channelNum, int startSongPos, GTOBJECT* gtloop) {
    GTOBJECT* gtPlayer = &gtObject;

    int c3 = getActualChannel(songNum, editorInfo.epmarkchn);

    int markStart = editorInfo.epmarkstart;
    int markEnd   = editorInfo.epmarkend;
    if (markEnd < markStart) {
        markStart = markEnd;
        markEnd   = editorInfo.epmarkstart;
    }

    int c2  = channelNum; // getActualChannel(songNum, channelNum);
    int sng = songNum;    // getActualSongNumber(songNum, c2);

    if (c2 >= editorInfo.maxSIDChannels) return -1;

    gtloop->loopEnabledFlag = 0;
    initsong(sng, PlayMode::Beginning, gtloop);

    gtloop->disableLoopSearch = 1;

    do {
        playroutine(gtloop);

        if (gtloop->songinit == PlayMode::Stopped) // Error in song data
            return -1;

    } while (gtloop->chn[c3].songptr <= startSongPos);

    //	int tempMin = gtloop->timemin;
    //	int tempSec = gtloop->timesec;
    //	int tempFrame = gtloop->timeframe;

    // Now get Select Start, Select End and Play Start ..Then get pattern end

    bool findPatternLoopStart = false;
    //	int findPatternLoopEnd = 0;
    int loopPatternNum = 0;

    // Now sync to end of pattern (info used for looping)
    int sptr = gtloop->chn[c3].songptr;

    do {
        playroutine(gtloop);
        if (gtloop->songinit == PlayMode::Stopped) // Error in song data
            return -1;

        int lastpattptr = gtloop->chn[c3].pattptr;
        bool found       = false;
        if (!findPatternLoopStart && gtloop->chn[c3].pattptr == markStart * 4) found = true;
        else if (gtloop->chn[c3].songptr != startSongPos + 1 || gtloop->chn[c3].pattptr < lastpattptr)
            found = true;
        if (found) {
            findPatternLoopStart = true;
            memcpy((char*)&gtPlayer->patternLoopStartChn[0], (char*)&gtloop->chn[0], sizeof(CHN) * MAX_PLAY_CH);
            memcpy((char*)&gtPlayer->looptimemin, (char*)&gtloop->timemin, sizeof(int) * 3);
            tempPatternMin  = gtloop->timemin;
            tempPatternSec  = gtloop->timesec;
            tempPatterFrame = gtloop->timeframe;
            loopPatternNum  = gtloop->chn[c3].pattnum;

            editorInfo.highlightLoopStart         = markStart;
            editorInfo.highlightLoopEnd           = markEnd;
            editorInfo.highlightLoopPatternNumber = loopPatternNum;
            editorInfo.highlightLoopChannel       = c3;

            return 1;
        }
        lastpattptr = gtloop->chn[c3].pattptr;

    } while (gtloop->chn[c3].songptr == sptr);

    return 0;
}

int calculateLoopInfo2(int songNum, int channelNum, int startSongPos, GTOBJECT* gtloop) {
    GTOBJECT* gtPlayer = &gtObject;

    int c3  = getActualChannel(songNum, channelNum);
    int sng = songNum;

    if (c3 >= editorInfo.maxSIDChannels) return -1;

    gtloop->loopEnabledFlag = 0;
    initsong(sng, PlayMode::Beginning, gtloop);
    gtloop->disableLoopSearch = 1;
    gtloop->noSIDWrites       = 1;

    do {

        playroutine(gtloop);

        if (gtloop->songinit == PlayMode::Stopped) // Error in song data
            return -1;

    } while (gtloop->chn[c3].songptr <= startSongPos);

    memcpy((char*)&gtPlayer->loopStartChn[0], (char*)&gtloop->chn[0], sizeof(CHN) * MAX_PLAY_CH);

    // Do this manually, in case compiler changes order of data
    //	memcpy((char *)&gtPlayer->looptimemin, (char*)&gtloop->timemin, sizeof(int) * 3);
    gtPlayer->looptimemin   = gtloop->looptimemin;
    gtPlayer->looptimesec   = gtloop->looptimesec;
    gtPlayer->looptimeframe = gtloop->looptimeframe;

    //	int tempMin = gtloop->timemin;
    //	int tempSec = gtloop->timesec;
    //	int tempFrame = gtloop->timeframe;

    // Now get Select Start, Select End and Play Start ..Then get pattern end

    //	int findPatternLoopStart = 0;
    //	int findPatternLoopEnd = 0;
    //	int loopPatternNum = 0;

    // Now sync to end of pattern (info used for looping)
    int sptr = gtloop->chn[c3].songptr;

    bool quitloop = false;
    do {
        playroutine(gtloop);
        if (gtloop->songinit == PlayMode::Stopped) // Error in song data
            return -1;

        if (gtloop->chn[c3].loopCount) // reached end of song and looped?
            quitloop = true;
        else if (gtloop->chn[c3].songptr != sptr) quitloop = true;

    } while (!quitloop); // gtloop->chn[c2].songptr == sptr);

    memcpy((char*)&gtPlayer->loopEndChn[0], (char*)&gtloop->chn[0], sizeof(CHN) * MAX_PLAY_CH);

    return 0;
}

void orderPlayFromPosition(GTOBJECT* gt,
                           int       startPatternPos,
                           int       startSongPos,
                           int       focusChannel,
                           bool      enable_sid_writes) {
    (void)enable_sid_writes;

    //	sprintf(textbuffer, "spp %d ssp %d, fc %d mlc %d", startPatternPos, startSongPos, focusChannel,
    // gt->masterLoopChannel);

    //		int t1 = followplay;
    int t2 = gt->interPatternLoopEnabledFlag;

    if (editorInfo.expandOrderListView == 0) {
        if (startSongPos >= songlen[editorInfo.esnum][focusChannel % 6]) return;
    }
    else {
        if (startSongPos >= songOrderLength[editorInfo.esnum][focusChannel % 6] - 1) // 1.3.3
            return;
    }

    int c2 = getActualChannel(editorInfo.esnum, focusChannel);
    //	int sng = getActualSongNumber(editorInfo.esnum, c2);

    if (c2 >= editorInfo.maxSIDChannels) return;

    // printf("play1\n");
    if (gt->songinit != PlayMode::Stopped) {
        stopsong(gt);
    }

    // printf("play2\n");
    bypassPlayRoutine = 1; // Stop interrupt from updating play routine. We're going to do it manually
    SDL_Delay(50);

    int loopMode = transportLoopPattern; // gt->loopEnabledFlag;

    int ep = startSongPos;

    initsong(editorInfo.esnum, PlayMode::Beginning, gt);
    gt->loopEnabledFlag   = 0;
    gt->disableLoopSearch = 1;

    do {
        playroutine(gt);
        if (gt->songinit == PlayMode::Stopped) // Error in song data
            return;

    } while (gt->chn[c2].songptr <= ep);

    int tempMin   = gt->timemin;
    int tempSec   = gt->timesec;
    int tempFrame = gt->timeframe;

    // Now sync to pattern start position (where cursor was when F3 was pressed)
    if (startPatternPos > 0) {
        while (gt->chn[c2].pattptr < startPatternPos) {
            playroutine(gt);
            if (gt->songinit == PlayMode::Stopped) // Error in song data
                return;
        };

        tempMin   = gt->timemin;
        tempSec   = gt->timesec;
        tempFrame = gt->timeframe;
    }

    if (!gtObject.interPatternLoopEnabledFlag) {
        gt->timemin   = tempMin;
        gt->timesec   = tempSec;
        gt->timeframe = tempFrame;
    }
    else {
        gt->timemin   = tempPatternMin;
        gt->timesec   = tempPatternSec;
        gt->timeframe = tempPatterFrame;
    }

    bypassPlayRoutine = 0;

    gt->loopEnabledFlag             = loopMode;
    gt->disableLoopSearch           = 0;
    gt->interPatternLoopEnabledFlag = t2;
}

/*
Set up the editor pattern length info (gt->editorUndoInfo.editorInfo[c2].epnum ) and song pos (.espos) for each
channel
*/
void orderSelectPatternsFromSelected(GTOBJECT* gt) {

    if (editorInfo.expandOrderListView == 0) {
        if (editorInfo.eseditpos >= songlen[editorInfo.esnum][editorInfo.eschn]) return;
    }
    else {
        if (editorInfo.eseditpos >= songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1) // 1.3.3
            return;
    }
    // V1.2.2. fix - rather than using eschn or epchn, use masterLoopChannel instead. works if you're editing
    // pattern or song.
    //	int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);
    int c2  = gt->masterLoopChannel;
    int sng = getActualSongNumber(editorInfo.esnum, c2);

    if (c2 >= editorInfo.maxSIDChannels) return;
    int ep = editorInfo.eseditpos;

    int ep2;

    if (ep >= 0) {
        GTOBJECT* gte = &gtEditorObject;
        initsong(sng, PlayMode::Beginning, gte); // JP FEB
        do {
            playroutine(gte);
            if (gte->songinit == PlayMode::Stopped) // Error in song data
            {
                return;
            }

        } while (gte->chn[c2].songptr - 1 < ep);

        for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
            c2      = c;
            int sng = getActualSongNumber(editorInfo.esnum, c2);

            ep = gte->chn[c2].songptr - 1; // -1;

            if (editorInfo.expandOrderListView == 0) {
                do {
                    ep2 = ep;
                    if ((songorder[sng][c2 % 6][ep] >= REPEAT) && (songorder[sng][c2 % 6][ep] < TRANSDOWN)) ep++;
                    if ((songorder[sng][c2 % 6][ep] >= TRANSDOWN) && (songorder[sng][c2 % 6][ep] < LOOPSONG)) ep++;
                } while (ep != ep2);
                gt->editorUndoInfo.editorInfo[c2].epnum = songorder[sng][c2 % 6][ep];
            }
            else gt->editorUndoInfo.editorInfo[c2].epnum = songOrderPatterns[sng][c2 % 6][ep];
            gt->editorUndoInfo.editorInfo[c2].espos = ep;
        }
    }
    else {
        for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
            c2      = c;
            int sng = getActualSongNumber(editorInfo.esnum, c2);
            if (editorInfo.expandOrderListView == 0)
                gt->editorUndoInfo.editorInfo[c2].epnum = songorder[sng][c2 % 6][0];
            else gt->editorUndoInfo.editorInfo[c2].epnum = songOrderPatterns[sng][c2 % 6][0];
            gt->editorUndoInfo.editorInfo[c2].espos = 0;
        }
    }

    editorInfo.epview = -VISIBLEPATTROWS / 2;
    editorInfo.eppos  = 0;

    int c3 = getActualChannel(gt->psnum, editorInfo.epmarkchn);

    int plen = (pattlen[gt->editorUndoInfo.editorInfo[c3].epnum] - 1); // *4;

    //		sprintf(textbuffer, "%x markchan ch %x len %x end %x  ", jdebug[15]++, editorInfo.epmarkchn,plen,
    // editorInfo.epmarkend);

    if (editorInfo.epmarkchn >= 0) {

        if (editorInfo.epmarkend > plen || editorInfo.epmarkstart > plen) {
            editorInfo.epmarkstart = editorInfo.epmarkend = 0;
            editorInfo.epmarkchn                          = -1;
        }
    }
}

void countInstruments() {
    for (int p = 0; p < MAX_PATT; p++) {
        patternChecked[p] = 0;

        for (int i = 0; i < MAX_INSTR; i++) {
            pattInstrumentCount[p][i] = 0;
        }
    }

    if (editorInfo.expandOrderListView == 0) {
        for (int s = 0; s < MAX_SONGS; s++) {
            for (int c = 0; c < MAX_CHN; c++) {
                for (int l = 0; l < songlen[s][c]; l++) {
                    int pat = songorder[s][c][l];
                    if (pat >= REPEAT && pat < TRANSDOWN) continue;
                    if (pat >= TRANSDOWN && pat <= LOOPSONG) continue;
                    if (!patternChecked[pat]) {
                        countInstrumentsInPattern(pat);
                        patternChecked[pat]++;
                    }
                }
            }
        }
    }
    else {
        for (int s = 0; s < MAX_SONGS; s++) {

            for (int c = 0; c < MAX_CHN; c++) {
                //	printf("song %x orderlength %x\n", s, songOrderLength[s][c]);

                for (int l = 0; l < songOrderLength[s][c] - 1; l++) {
                    int pat = songOrderPatterns[s][c][l];

                    //	printf("song %x orderlength %x chan %x index %x pat %x\n", s,
                    // songOrderLength[s][c],c,l,pat);

                    if (!patternChecked[pat]) {

                        countInstrumentsInPattern(pat);
                        patternChecked[pat]++;
                    }
                }
            }
        }
    }

    calculateTotalInstrumentsFromAllPatterns();
}

void calculateTotalInstrumentsFromAllPatterns() {
    for (int i = 0; i < MAX_INSTR; i++) {
        instrumentCount[i]        = 0;
        firstInstrumentPattern[i] = -1;
    }

    for (int p = 0; p < MAX_PATT; p++) {
        for (int i = 0; i < MAX_INSTR; i++) {
            instrumentCount[i] += pattInstrumentCount[p][i];
        }
    }
}

void countInstrumentsInPattern(int pat) {

    if (pat >= MAX_PATT) {
        printf("ERROR!  pattern %x \n", pat);
        return;
    }
    for (int i = 0; i < MAX_INSTR; i++) {
        pattInstrumentCount[pat][i] = 0;
    }

    for (int p = 0; p < pattlen[pat]; p++) {
        int instr = pattern[pat][(p * 4) + 1];
        if (instr != 0) {
            if (instr >= MAX_INSTR || pat >= MAX_PATT) {
                printf("ERROR! Instrument %x in pattern %x position %x\n", instr, pat, p);
            }
            else pattInstrumentCount[pat][instr]++;
        }
    }
}

void setMasterLoopChannel(GTOBJECT* gt, const char* debugText) {

    //	sprintf(textbuffer, "%x master %s", jdebug[15]++, debugText);

    int loopChannel = -1;
    if (editorInfo.editmode == EditMode::Pattern) loopChannel = editorInfo.epchn;
    else if (editorInfo.editmode == EditMode::OrderList) loopChannel = editorInfo.eschn;

    if (loopChannel >= 0) {
        int c2 = getActualChannel(editorInfo.esnum, loopChannel);
        if (gt->songinit == PlayMode::Stopped) {
            gt->masterLoopChannel = c2;
            gt->masterLoopSubSong = editorInfo.esnum;
        }
    }
}

int findFirstEndMarkerIndex(int sng, int chn) {
    for (int i = 0; i < MAX_SONGLEN_EXPANDED; i++) {
        if (songOrderPatterns[sng][chn][i] == 0xff) return i;
    }
    return MAX_SONGLEN_EXPANDED - 1;
}

void orderListCopyMarkedArea() {
    int c;
    if (editorInfo.esmarkchn == -1) // no table selected. copy single row under cursor
    {
        editorInfo.esmarkchn   = editorInfo.eschn;
        editorInfo.esmarkstart = editorInfo.eseditpos;
        editorInfo.esmarkend   = editorInfo.esmarkstart;
    }

    if (editorInfo.esmarkchn != -1) {
        int d = 0;
        if (editorInfo.esmarkstart <= editorInfo.esmarkend) {
            for (c = editorInfo.esmarkstart; c <= editorInfo.esmarkend; c++)
                trackcopybuffer[d++] = songorder[editorInfo.esnum][editorInfo.eschn][c];
            trackcopyrows = d;
        }
        else {
            for (c = editorInfo.esmarkend; c <= editorInfo.esmarkstart; c++)
                trackcopybuffer[d++] = songorder[editorInfo.esnum][editorInfo.eschn][c];
            trackcopyrows = d;
        }
        if (trackcopyrows == songlen[editorInfo.esnum][editorInfo.eschn]) {
            trackcopywhole = 1;
            trackcopyrpos =
                songorder[editorInfo.esnum][editorInfo.eschn][songlen[editorInfo.esnum][editorInfo.eschn] + 1];
        }
        else trackcopywhole = 0;
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    }
}

void orderListCopyMarkedArea_Expanded() {
    //	int c;
    if (editorInfo.esmarkchn == -1) // no table selected. copy single row under cursor
    {
        editorInfo.esmarkchn    = editorInfo.eschn;
        editorInfo.esmarkchnend = editorInfo.esmarkchn;
        editorInfo.esmarkstart  = editorInfo.eseditpos;
        editorInfo.esmarkend    = editorInfo.esmarkstart;
    }

    if (editorInfo.esmarkchn != -1) {
        int x, y, w, h;
        get_expanded_selected_area(&x, &y, &w, &h);

        int wy = 0;
        for (int i = y; i < (y + h); i++) {
            int wx = 0;
            for (int j = x; j < (x + w); j++) {
                songOrderPatternsCopyPaste[wx][wy]    = songOrderPatterns[editorInfo.esnum][j][i];
                songOrderTransposeCopyPaste[wx++][wy] = songOrderTranspose[editorInfo.esnum][j][i];
            }
            wy++;
        }
        copyPasteW                = w;
        copyPasteH                = h;
        copyExpandedSongValidFlag = 1;

        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    }
}

void orderListPasteToCursor(GTOBJECT* gt) {
    int c;
    int oldlen = songlen[editorInfo.esnum][editorInfo.eschn];

    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
        for (c = trackcopyrows - 1; c >= 0; c--) insertorder(trackcopybuffer[c], gt);
    }
    else {
        for (c = 0; c < trackcopyrows; c++) insertorder(trackcopybuffer[c], gt);
    }
    if ((trackcopywhole) && (!oldlen))
        songorder[editorInfo.esnum][editorInfo.eschn][songlen[editorInfo.esnum][editorInfo.eschn] + 1] =
            trackcopyrpos; // copying whole channel song list? then copy over loop position too
}

void orderListPasteToCursor_External(GTOBJECT* gt, bool insert, bool transpose_only) {
    if (copyExpandedSongValidFlag == 0) return;

    int xd = editorInfo.eschn;
    for (int x = 0; x < copyPasteW; x++) {
        if (insert) {
            if (!transpose_only) {
                for (int y = 0; y < copyPasteH; y++) {
                    orderListInsertRowAtCursor_External(gt, editorInfo.esnum, xd, editorInfo.eseditpos);
                }
            }
        }

        int yd = editorInfo.eseditpos;
        for (int y = 0; y < copyPasteH; y++) {
            if (!transpose_only) songOrderPatterns[editorInfo.esnum][xd][yd] = songOrderPatternsCopyPaste[x][y];
            songOrderTranspose[editorInfo.esnum][xd][yd] = songOrderTransposeCopyPaste[x][y];
            yd++;

            if (transpose_only) {
                if (yd >= songOrderLength[editorInfo.esnum][xd] - 1) break;
            }
            else {
                if (yd == MAX_SONGLEN_EXPANDED - 1) break;
            }
        }
        int index                             = findFirstEndMarkerIndex(editorInfo.esnum, xd);
        songOrderLength[editorInfo.esnum][xd] = index + 1; // 1.3.8

        songCompressedSize[editorInfo.esnum][xd] = generateCompressedSongChannel(editorInfo.esnum, xd, true);

        xd++;
        if (xd == MAX_CHN) break;
    }
}

void orderListInsertRowAtCursor_External(GTOBJECT* gt, int sng, int chn, int row) {
    for (int y = MAX_SONGLEN_EXPANDED - 2; y >= row; y--) // 1.3.4
    {
        songOrderPatterns[sng][chn][y]  = songOrderPatterns[sng][chn][y - 1];
        songOrderTranspose[sng][chn][y] = songOrderTranspose[sng][chn][y - 1];
    }
    songOrderPatterns[sng][chn][row]  = 0;
    songOrderTranspose[sng][chn][row] = 0;

    //	int index = findFirstEndMarkerIndex(sng, chn);
    songOrderLength[sng][chn]++;

    //	sprintf(textbuffer, "sng %x, chn %x songorderLen %x\n", sng,chn, (songOrderLength[sng][chn] - 1));

    int c2 = getActualChannel(sng, chn); // 0-11

    if (gt->editorUndoInfo.editorInfo[c2].espos >= row) // 1.3.8 Was chn
        gt->editorUndoInfo.editorInfo[c2].espos++;

    songCompressedSize[sng][chn] = generateCompressedSongChannel(sng, chn, true);
}

void orderListDeleteRowAtCursor_External(int sng, int chn, int row) {
    for (int y = row; y < MAX_SONGLEN_EXPANDED - 2; y++) // 1.3.4
    {
        songOrderPatterns[sng][chn][y]  = songOrderPatterns[editorInfo.esnum][chn][y + 1];
        songOrderTranspose[sng][chn][y] = songOrderTranspose[editorInfo.esnum][chn][y + 1];
    }
    songOrderPatterns[sng][chn][MAX_SONGLEN_EXPANDED - 2]  = 0; // 1.3.4
    songOrderTranspose[sng][chn][MAX_SONGLEN_EXPANDED - 2] = 0;
    songOrderLength[sng][chn]--;

    songCompressedSize[sng][chn] = generateCompressedSongChannel(sng, chn, true);
}

void orderListInsert_External(GTOBJECT* gt) {
    int x, y, w, h;
    get_expanded_selected_area(&x, &y, &w, &h);
    if (w == 0) // Nothing selected
    {
        w = 1;
        h = 1;
        x = editorInfo.eschn;
        y = editorInfo.eseditpos;
    }

    for (int j = 0; j < h; j++) {
        for (int i = x; i < (x + w); i++) {
            orderListInsertRowAtCursor_External(gt, editorInfo.esnum, i, y);
        }
    }

    //	for (int i = x;i < (x + w);i++)
    //	{
    //	int index = findFirstEndMarkerIndex(editorInfo.esnum, i);
    //	songOrderLength[editorInfo.esnum][i] = index;
    //}
}

void orderListDelete_External() {
    int x, y, w, h;
    get_expanded_selected_area(&x, &y, &w, &h);
    if (w == 0) // Nothing selected
    {
        w = 1;
        h = 1;
        x = editorInfo.eschn;
        y = editorInfo.eseditpos;
    }

    for (int j = 0; j < h; j++) {
        for (int i = x; i < (x + w); i++) {
            orderListDeleteRowAtCursor_External(editorInfo.esnum, i, y);
        }
    }

    for (int i = x; i < (x + w); i++) {
        int index                            = findFirstEndMarkerIndex(editorInfo.esnum, i);
        songOrderLength[editorInfo.esnum][i] = index + 1;
    }

    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
}

void order_list_insert(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView != 0) return;
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
    insertorder(0, gt);
    playUntilEnd(editorInfo.esnum);
    (void)gt;
}

void order_list_delete(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView != 0) return;
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
    deleteorder(gt);
    playUntilEnd(editorInfo.esnum);
}

void order_list_cut(GTOBJECT* gt) {
    int c;

    if (editorInfo.expandOrderListView != 0) return;

    if (editorInfo.esmarkchn == -1) {
        editorInfo.esmarkchn   = editorInfo.eschn;
        editorInfo.esmarkstart = editorInfo.eseditpos;
        editorInfo.esmarkend   = editorInfo.esmarkstart;
    }

    if (editorInfo.esmarkchn == -1) return;

    int d            = 0;
    editorInfo.eschn = editorInfo.esmarkchn;
    if (editorInfo.esmarkstart <= editorInfo.esmarkend) {
        editorInfo.eseditpos = editorInfo.esmarkstart;
        for (c = editorInfo.esmarkstart; c <= editorInfo.esmarkend; c++)
            trackcopybuffer[d++] = songorder[editorInfo.esnum][editorInfo.eschn][c];
        trackcopyrows = d;
    }
    else {
        editorInfo.eseditpos = editorInfo.esmarkend;
        for (c = editorInfo.esmarkend; c <= editorInfo.esmarkstart; c++)
            trackcopybuffer[d++] = songorder[editorInfo.esnum][editorInfo.eschn][c];
        trackcopyrows = d;
    }
    if (trackcopyrows == songlen[editorInfo.esnum][editorInfo.eschn]) {
        trackcopywhole = 1;
        trackcopyrpos =
            songorder[editorInfo.esnum][editorInfo.eschn][songlen[editorInfo.esnum][editorInfo.eschn] + 1];
    }
    else trackcopywhole = 0;
    for (c = 0; c < trackcopyrows; c++) deleteorder(gt);
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
}

void order_list_mark_toggle() {
    if (editorInfo.expandOrderListView) {
        if (editorInfo.esmarkchn == -1) {
            editorInfo.esmarkend    = (int)songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1;
            editorInfo.esmarkchn    = editorInfo.eschn;
            editorInfo.esmarkchnend = editorInfo.eschn;
            editorInfo.esmarkstart  = 0;
        }
        else {
            editorInfo.esmarkchn    = -1;
            editorInfo.esmarkchnend = -1;
        }
        return;
    }

    if (editorInfo.esmarkchn == -1) {
        editorInfo.esmarkend    = songlen[editorInfo.esnum][editorInfo.eschn] - 1;
        editorInfo.esmarkchn    = editorInfo.eschn;
        editorInfo.esmarkchnend = editorInfo.esmarkchn;
        editorInfo.esmarkstart  = 0;
    }
    else {
        editorInfo.esmarkchn    = -1;
        editorInfo.esmarkchnend = -1;
    }
}

void order_list_transpose_up() {
    if (editorInfo.expandOrderListView) {
        if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < 0xff &&
            editorInfo.escolumn == 3) {
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] &= 0x7f;
            if ((songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] & 0x7f) == 0xf)
                songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos]--;
            update_transpose_to_playing_song(&gtObject);
            songCompressedSize[editorInfo.esnum][editorInfo.eschn] =
                generateCompressedSongChannel(editorInfo.esnum, editorInfo.eschn, true);
        }
        return;
    }
    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
        songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = TRANSUP;
        editorInfo.escolumn                                                 = 1;
    }
}

void order_list_transpose_down() {
    if (editorInfo.expandOrderListView) {
        if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < 0xff &&
            editorInfo.escolumn == 3) {
            songOrderTranspose[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] |= 0x80;
            update_transpose_to_playing_song(&gtObject);
            songCompressedSize[editorInfo.esnum][editorInfo.eschn] =
                generateCompressedSongChannel(editorInfo.esnum, editorInfo.eschn, true);
        }
        return;
    }
    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
        songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = TRANSDOWN + 0x0F;
        editorInfo.escolumn                                                 = 1;
    }
}

void order_list_insert_repeat() {
    if (editorInfo.expandOrderListView != 0) return;
    if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn]) {
        songorder[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] = REPEAT + 0x01;
        editorInfo.escolumn                                                 = 1;
    }
}

void order_list_swap_channel(GTOBJECT* gt, int tchn) {
    int       c;
    const int schn = editorInfo.eschn;

    if (tchn < 0 || tchn > 5 || schn == tchn) return;

    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;

    int lentemp                     = songlen[editorInfo.esnum][schn];
    songlen[editorInfo.esnum][schn] = songlen[editorInfo.esnum][tchn];
    songlen[editorInfo.esnum][tchn] = lentemp;

    for (c = 0; c < MAX_SONGLEN + 2; c++) {
        unsigned char temp                   = songorder[editorInfo.esnum][schn][c];
        songorder[editorInfo.esnum][schn][c] = songorder[editorInfo.esnum][tchn][c];
        songorder[editorInfo.esnum][tchn][c] = temp;
    }

    lentemp                                 = songOrderLength[editorInfo.esnum][schn];
    songOrderLength[editorInfo.esnum][schn] = songOrderLength[editorInfo.esnum][tchn];
    songOrderLength[editorInfo.esnum][tchn] = lentemp;

    for (c = 0; c < MAX_SONGLEN_EXPANDED; c++) {
        unsigned char temp                           = songOrderPatterns[editorInfo.esnum][schn][c];
        songOrderPatterns[editorInfo.esnum][schn][c] = songOrderPatterns[editorInfo.esnum][tchn][c];
        songOrderPatterns[editorInfo.esnum][tchn][c] = temp;

        short stemp                                   = songOrderTranspose[editorInfo.esnum][schn][c];
        songOrderTranspose[editorInfo.esnum][schn][c] = songOrderTranspose[editorInfo.esnum][tchn][c];
        songOrderTranspose[editorInfo.esnum][tchn][c] = stemp;
    }

    (void)gt;
}

void order_col_left_expanded(GTOBJECT* gt) {
    const int maxCh = order_expanded_max_channels();

    if (ctrl_pressed) return;

    if (editorInfo.escolumn > 0) {
        editorInfo.escolumn--;
        if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < 0xff) {
            if (editorInfo.escolumn == 2) editorInfo.escolumn--;
        }
    }
    else {
        editorInfo.escolumn = 4;
        editorInfo.eschn--;
        if (editorInfo.eschn < 0) editorInfo.eschn = maxCh - 1;
        setMasterLoopChannel(gt, "action_order_col_left_exp");
    }

    if (shift_or_ctrl_pressed) {
        if (editorInfo.esmarkchn == -1) {
            editorInfo.esmarkchn = editorInfo.esmarkchnend = editorInfo.eschn;
            editorInfo.esmarkstart = editorInfo.esmarkend = editorInfo.eseditpos;
        }
        else editorInfo.esmarkchnend = editorInfo.eschn;
    }
}

void order_col_right_expanded(GTOBJECT* gt) {
    const int maxCh = order_expanded_max_channels();

    if (ctrl_pressed) return;

    editorInfo.escolumn++;
    if (songOrderPatterns[editorInfo.esnum][editorInfo.eschn][editorInfo.eseditpos] < 0xff) {
        if (editorInfo.escolumn == 2) editorInfo.escolumn++;
    }
    editorInfo.escolumn %= 5;
    if (!editorInfo.escolumn) {
        editorInfo.eschn++;
        if (editorInfo.eschn >= maxCh) editorInfo.eschn = 0;
        setMasterLoopChannel(gt, "action_order_col_right_exp");
    }

    if (shift_or_ctrl_pressed) {
        if (editorInfo.esmarkchn == -1) {
            editorInfo.esmarkchn = editorInfo.esmarkchnend = editorInfo.eschn;
            editorInfo.esmarkstart = editorInfo.esmarkend = editorInfo.eseditpos;
        }
        else editorInfo.esmarkchnend = editorInfo.eschn;
    }
}

int order_go_pattern(GTOBJECT* gt) {
    int ret;

    if (editorInfo.expandOrderListView == 0) ret = handle_enter_in_compressed_view(gt);
    else ret = handle_enter_in_expanded_view(gt);

    if (ret) {
        editorInfo.epmarkchn = -1;
        editorInfo.epchn     = editorInfo.eschn;
        editorInfo.epcolumn  = 0;
        editorInfo.eppos     = 0;
        editorInfo.epview    = -VISIBLEPATTROWS / 2;
        editorInfo.editmode  = EditMode::Pattern;
        if (editorInfo.epchn == editorInfo.epmarkchn) editorInfo.epmarkchn = -1;
    }
    return ret;
}

void order_select_patterns(GTOBJECT* gt) {
    if (editorInfo.expandOrderListView == 0) {
        if (editorInfo.eseditpos >= songlen[editorInfo.esnum][editorInfo.eschn]) return;
    }
    else {
        if (editorInfo.eseditpos >= songOrderLength[editorInfo.esnum][editorInfo.eschn] - 1) return;
    }

    backupPatternDisplayInfo(gt);
    orderSelectPatternsFromSelected(gt);
    restorePatternDisplayInfo(gt);
}

void order_play_range_start(GTOBJECT* gt) {
    if (!shift_or_ctrl_pressed) {
        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);

        if (editorInfo.expandOrderListView == 0) {
            if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn])
                gt->editorUndoInfo.editorInfo[c2].espos = editorInfo.eseditpos;
        }
        else {
            if (editorInfo.eseditpos < songOrderLength[editorInfo.esnum][editorInfo.eschn])
                gt->editorUndoInfo.editorInfo[c2].espos = editorInfo.eseditpos;
        }
        if (gt->editorUndoInfo.editorInfo[c2].esend < gt->editorUndoInfo.editorInfo[c2].espos)
            gt->editorUndoInfo.editorInfo[c2].esend = 0;
    }
    else {
        for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
            int c2      = getActualChannel(editorInfo.esnum, c);
            int songNum = getActualSongNumber(editorInfo.esnum, c2);
            int c3      = c2 % 6;

            if (editorInfo.expandOrderListView == 0) {
                if (editorInfo.eseditpos < songlen[songNum][c3])
                    gt->editorUndoInfo.editorInfo[c2].espos = editorInfo.eseditpos;
            }
            else {
                if (editorInfo.eseditpos < songOrderLength[songNum][c3])
                    gt->editorUndoInfo.editorInfo[c2].espos = editorInfo.eseditpos;
            }
            if (gt->editorUndoInfo.editorInfo[c2].esend < gt->editorUndoInfo.editorInfo[c2].espos)
                gt->editorUndoInfo.editorInfo[c2].esend = 0;
        }
    }
}

void order_play_range_end(GTOBJECT* gt) {
    if (!shift_or_ctrl_pressed) {
        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);

        if ((gt->editorUndoInfo.editorInfo[c2].esend != editorInfo.eseditpos) &&
            (editorInfo.eseditpos > gt->editorUndoInfo.editorInfo[c2].espos)) {
            if (editorInfo.expandOrderListView == 0) {
                if (editorInfo.eseditpos < songlen[editorInfo.esnum][editorInfo.eschn])
                    gt->editorUndoInfo.editorInfo[c2].esend = editorInfo.eseditpos;
            }
            else {
                if (editorInfo.eseditpos < songOrderLength[editorInfo.esnum][editorInfo.eschn])
                    gt->editorUndoInfo.editorInfo[c2].esend = editorInfo.eseditpos;
            }
        }
        else gt->editorUndoInfo.editorInfo[c2].esend = 0;
    }
    else {
        int c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);

        if ((gt->editorUndoInfo.editorInfo[c2].esend != editorInfo.eseditpos) &&
            (editorInfo.eseditpos > gt->editorUndoInfo.editorInfo[c2].espos)) {
            for (int c = 0; c < editorInfo.maxSIDChannels; c++) {
                int c3          = c % 6;
                int playingSong = getActualSongNumber(editorInfo.esnum, c);
                c2              = getActualChannel(editorInfo.esnum, c);

                if (editorInfo.expandOrderListView == 0) {
                    if (editorInfo.eseditpos < songlen[playingSong][c3])
                        gt->editorUndoInfo.editorInfo[c2].esend = editorInfo.eseditpos;
                }
                else {
                    if (editorInfo.eseditpos < songOrderLength[playingSong][c3])
                        gt->editorUndoInfo.editorInfo[c2].esend = editorInfo.eseditpos;
                }
            }
        }
        else {
            for (int c = 0; c < editorInfo.maxSIDChannels; c++) gt->editorUndoInfo.editorInfo[c].esend = 0;
        }
    }
}
