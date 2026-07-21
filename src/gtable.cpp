//
// GTUltra table editor
//

// GTABLE_C removed (pragma once headers)

#include "goattrk2.hpp"
#include "gtable.hpp"
#include "gimgui.hpp"
#include "guimodel.hpp"

unsigned char ltablecopybuffer[MAX_TABLELEN];
unsigned char rtablecopybuffer[MAX_TABLELEN];
int           tablecopyrows = 0;

namespace {

void modify_wave_table_detailed_left(int hexnybble) {

    if (detailedTableBaseLValue[editorInfo.etpos] == -1) return;

    unsigned char v = detailedTableLValue[editorInfo.etpos];
    //	unsigned char o = v;

    switch (editorInfo.etcolumn) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }

    if ((unsigned char)v > detailedTableMaxLValue[editorInfo.etpos]) v = detailedTableMaxLValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinLValue[editorInfo.etpos]) v = detailedTableMinLValue[editorInfo.etpos];

    detailedTableLValue[editorInfo.etpos] = v;

    // Convert detailed value back to original table value
    v += detailedTableBaseLValue[editorInfo.etpos];

    if (ltable[0][editorInfo.etpos] == 0xff) rtable[0][editorInfo.etpos] = v;
    else {
        if (ltable[0][editorInfo.etpos] >= 0x10 &&
            ltable[0][editorInfo.etpos] <= 0xef) // waveform (not delay or command)
        {
            if (v < 0x10) v += 0xe0; // remap 0-0xf to 0xe0-0xef
        }

        ltable[0][editorInfo.etpos] = v;
    }
}

void modify_wave_table_detailed_right(int hexnybble) {
    unsigned char v = detailedTableRValue[editorInfo.etpos];
    //	unsigned char o = v;


    if (detailedTableBaseRValue[editorInfo.etpos] == -1) return;

    switch (editorInfo.etcolumn - 2) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }

    if ((unsigned char)v > detailedTableMaxRValue[editorInfo.etpos]) v = detailedTableMaxRValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinRValue[editorInfo.etpos]) v = detailedTableMinRValue[editorInfo.etpos];

    detailedTableRValue[editorInfo.etpos] = v;

    // Convert detailed R value back to original table value


    if (rtable[0][editorInfo.etpos] >= 0x60 && rtable[0][editorInfo.etpos] <= 0x7f) // negative relative notes
    {
        int v2 = -v;
        v2 &= 0xff;

        //	int v3 = v2;

        v2 += 0x20;


        v = v2;
    }

    v += detailedTableBaseRValue[editorInfo.etpos];
    rtable[0][editorInfo.etpos] = v;
}

void modify_pulse_table_detailed_left(int hexnybble) {
    if (detailedTableBaseLValue[editorInfo.etpos] == -1) return;

    unsigned char v = detailedTableLValue[editorInfo.etpos];
    //	unsigned char o = v;

    switch (editorInfo.etcolumn - 1) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }


    if ((unsigned char)v > detailedTableMaxLValue[editorInfo.etpos]) v = detailedTableMaxLValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinLValue[editorInfo.etpos]) v = detailedTableMinLValue[editorInfo.etpos];

    detailedTableLValue[editorInfo.etpos] = v;

    // Convert detailed value back to original table value
    v += detailedTableBaseLValue[editorInfo.etpos];


    int lv = ltable[PTBL][editorInfo.etpos];
    if (lv == 0xff) {
        rtable[PTBL][editorInfo.etpos] = v;
    }
    else if (lv >= 1 && lv <= 0x7f) {
        ltable[PTBL][editorInfo.etpos] = v;
    }
}

void modify_pulse_table_detailed_right(int hexnybble) {
    unsigned char v = detailedTableRValue[editorInfo.etpos];
    //	unsigned char o = v;

    if (detailedTableBaseRValue[editorInfo.etpos] == -1) return;

    int lv = ltable[PTBL][editorInfo.etpos];


    switch (editorInfo.etcolumn - 3) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }

    if ((unsigned char)v > detailedTableMaxRValue[editorInfo.etpos]) v = detailedTableMaxRValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinRValue[editorInfo.etpos]) v = detailedTableMinRValue[editorInfo.etpos];

    detailedTableRValue[editorInfo.etpos] = v;


    // Convert detailed R value back to original table value

    if (lv >= 1 && lv <= 0x7f) // Modify pulse
    {
        if (rtable[PTBL][editorInfo.etpos] >= 0x80) // currently holds negative value
        {
            v = 0x100 - v;
        }
    }

    v += detailedTableBaseRValue[editorInfo.etpos];
    rtable[PTBL][editorInfo.etpos] = v;
}

void modify_pulse_table_detailed(int hexnybble) {
    if (ltable[PTBL][editorInfo.etpos] >= 0x80 && ltable[PTBL][editorInfo.etpos] <= 0xfe) {
        int v = detailedTableLValue[editorInfo.etpos];

        switch (editorInfo.etcolumn) {
        case 0:
            v &= 0x0ff;
            v |= hexnybble << 8;
            break;
        case 1:
            v &= 0xf0f;
            v |= hexnybble << 4;
            break;
        case 2:
            v &= 0xff0;
            v |= hexnybble;
            break;
        }

        ltable[PTBL][editorInfo.etpos] = ((v >> 8) & 0xf) | 0x80;
        rtable[PTBL][editorInfo.etpos] = v & 0xff;
    }
    else if (editorInfo.etcolumn >= 3) modify_pulse_table_detailed_right(hexnybble);
    else modify_pulse_table_detailed_left(hexnybble);
}

void modify_filter_table_detailed_left(int hexnybble) {
    if (detailedTableBaseLValue[editorInfo.etpos] == -1) return;


    unsigned char v = detailedTableLValue[editorInfo.etpos];
    //	unsigned char o = v;

    switch (editorInfo.etcolumn) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }


    if ((unsigned char)v > detailedTableMaxLValue[editorInfo.etpos]) v = detailedTableMaxLValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinLValue[editorInfo.etpos]) v = detailedTableMinLValue[editorInfo.etpos];

    detailedTableLValue[editorInfo.etpos] = v;

    // Convert detailed value back to original table value
    v += detailedTableBaseLValue[editorInfo.etpos];


    int lv = ltable[FTBL][editorInfo.etpos];
    if (lv == 0 || lv == 0xff) {
        rtable[FTBL][editorInfo.etpos] = v;
    }
    else if (lv >= 1 && lv <= 0x7f) // modify filter cutoff
    {
        ltable[FTBL][editorInfo.etpos] = v;
    }
    else if (lv >= 0x80 && lv <= 0x0f0) // modify filter resonance?
    {
        int rv = rtable[FTBL][editorInfo.etpos];
        rv &= 0xf;
        rv |= v << 4;
        rtable[FTBL][editorInfo.etpos] = rv;
    }
}

void modify_filter_table_detailed_right(int hexnybble) {
    unsigned char v = detailedTableRValue[editorInfo.etpos];
    //	unsigned char o = v;

    if (detailedTableBaseRValue[editorInfo.etpos] == -1) return;

    int lv = ltable[FTBL][editorInfo.etpos];


    switch (editorInfo.etcolumn - 2) {
    case 0:
        v &= 0x0f;
        v |= hexnybble << 4;
        break;
    case 1:
        v &= 0xf0;
        v |= hexnybble;
        break;
    }

    if ((unsigned char)v > detailedTableMaxRValue[editorInfo.etpos]) v = detailedTableMaxRValue[editorInfo.etpos];

    if ((unsigned char)v < detailedTableMinRValue[editorInfo.etpos]) v = detailedTableMinRValue[editorInfo.etpos];

    detailedTableRValue[editorInfo.etpos] = v;


    // Convert detailed R value back to original table value

    if (lv >= 1 && lv <= 0x7f) // modify filter cutoff
    {
        if (rtable[FTBL][editorInfo.etpos] >= 0x80) // currently holds negative value
        {
            v = 0x100 - v;
        }
    }

    v += detailedTableBaseRValue[editorInfo.etpos];
    rtable[FTBL][editorInfo.etpos] = v;
}

void modify_filter_table_detailed(int hexnybble) {
    if (editorInfo.etcolumn >= 2) modify_filter_table_detailed_right(hexnybble);
    else modify_filter_table_detailed_left(hexnybble);
}

} // namespace

bool table_enter_input(GTOBJECT* gt, const EditorInput* input) {
    (void)gt;
    const EditorInput in = input ? *input : editor_input_snapshot();
    if (in.rawkey != SDL_SCANCODE_RETURN) return false;

    if (editorInfo.etnum == WTBL) {
        int table   = -1;
        int mstmode = MST_PORTAMENTO;

        switch (ltable[editorInfo.etnum][editorInfo.etpos]) {
        case WAVECMD + CMD_PORTAUP:
        case WAVECMD + CMD_PORTADOWN:
        case WAVECMD + CMD_TONEPORTA: table = STBL; break;

        case WAVECMD + CMD_VIBRATO:
            table   = STBL;
            mstmode = editorInfo.finevibrato;
            break;

        case WAVECMD + CMD_FUNKTEMPO:
            table   = STBL;
            mstmode = MST_FUNKTEMPO;
            break;

        case WAVECMD + CMD_SETPULSEPTR: table = PTBL; break;

        case WAVECMD + CMD_SETFILTERPTR: table = FTBL; break;
        }
        switch (table) {
        default:
            editorInfo.editmode = EditMode::Instrument;
            editorInfo.eipos    = editorInfo.etnum + 2;
            return true;

        case STBL:
            if (rtable[editorInfo.etnum][editorInfo.etpos]) {
                if (!in.shift_or_ctrl) {
                    allowEnterToReturnToPosition();
                    gototable(STBL, rtable[editorInfo.etnum][editorInfo.etpos] - 1);
                    return true;
                }
                {
                    int oldeditpos    = editorInfo.etpos;
                    int oldeditcolumn = editorInfo.etcolumn;
                    int pos           = makespeedtable(rtable[editorInfo.etnum][editorInfo.etpos], mstmode, 1);
                    allowEnterToReturnToPosition();
                    gototable(WTBL, oldeditpos);
                    editorInfo.etcolumn = oldeditcolumn;

                    rtable[editorInfo.etnum][editorInfo.etpos] = pos + 1;
                    return true;
                }
            }
            {
                int pos = findfreespeedtable();
                if (pos >= 0) {
                    rtable[editorInfo.etnum][editorInfo.etpos] = pos + 1;
                    allowEnterToReturnToPosition();
                    gototable(STBL, pos);
                    return true;
                }
            }
            break;

        case PTBL:
        case FTBL:
            if (rtable[editorInfo.etnum][editorInfo.etpos]) {
                allowEnterToReturnToPosition();
                gototable(table, rtable[editorInfo.etnum][editorInfo.etpos] - 1);
                return true;
            }
            if (in.shift_or_ctrl) {
                int pos = gettablelen(table);
                if (pos >= MAX_TABLELEN - 1) pos = MAX_TABLELEN - 1;
                rtable[editorInfo.etnum][editorInfo.etpos] = pos + 1;
                allowEnterToReturnToPosition();
                gototable(table, pos);
                return true;
            }
            break;
        }
    }
    else {
        if (!disableEnterToReturnToLastPos)
            memcpy((char*)&editorInfo, (char*)&editorInfoBackup, sizeof(EDITOR_INFO));
        return true;
    }

    return false;
}

static void table_cycle_type(int direction) {
    editorInfo.etpos -= editorInfo.etview[editorInfo.etnum];
    editorInfo.etnum += direction;
    if (editorInfo.etnum < 0) editorInfo.etnum = MAX_TABLES - 1;
    if (editorInfo.etnum >= MAX_TABLES) editorInfo.etnum = 0;
    editorInfo.etpos += editorInfo.etview[editorInfo.etnum];
}

bool table_cell_input(GTOBJECT* gt, const EditorInput* input) {
    const EditorInput in = input ? *input : editor_input_snapshot();

    if (table_enter_input(gt, &in)) return true;

    switch (in.rawkey) {
    case SDL_SCANCODE_Q:
        if (in.shift_or_ctrl && editorInfo.etnum == STBL) {
            int speed =
                (ltable[editorInfo.etnum][editorInfo.etpos] << 8) | rtable[editorInfo.etnum][editorInfo.etpos];
            speed *= 34716;
            speed /= 32768;
            if (speed > 65535) speed = 65535;

            ltable[editorInfo.etnum][editorInfo.etpos] = speed >> 8;
            rtable[editorInfo.etnum][editorInfo.etpos] = speed & 0xff;
            return true;
        }
        break;

    case SDL_SCANCODE_A:
        if (in.shift_or_ctrl && editorInfo.etnum == STBL) {
            int speed =
                (ltable[editorInfo.etnum][editorInfo.etpos] << 8) | rtable[editorInfo.etnum][editorInfo.etpos];
            speed *= 30929;
            speed /= 32768;

            ltable[editorInfo.etnum][editorInfo.etpos] = speed >> 8;
            rtable[editorInfo.etnum][editorInfo.etpos] = speed & 0xff;
            return true;
        }
        break;

    case SDL_SCANCODE_W:
        if (in.shift_or_ctrl && editorInfo.etnum == STBL) {
            int speed =
                (ltable[editorInfo.etnum][editorInfo.etpos] << 8) | rtable[editorInfo.etnum][editorInfo.etpos];
            speed *= 2;
            if (speed > 65535) speed = 65535;

            ltable[editorInfo.etnum][editorInfo.etpos] = speed >> 8;
            rtable[editorInfo.etnum][editorInfo.etpos] = speed & 0xff;
            return true;
        }
        if (in.shift_or_ctrl && ((editorInfo.etnum == PTBL) || (editorInfo.etnum == FTBL)) &&
            (ltable[editorInfo.etnum][editorInfo.etpos] < 0x80)) {
            int speed = (signed char)(rtable[editorInfo.etnum][editorInfo.etpos]);
            speed *= 2;

            if (speed > 127) speed = 127;
            if (speed < -128) speed = -128;
            rtable[editorInfo.etnum][editorInfo.etpos] = speed;
            return true;
        }
        break;

    case SDL_SCANCODE_S:
        if (!in.ctrl) {
            if (in.shift_or_ctrl && editorInfo.etnum == STBL) {
                int speed =
                    (ltable[editorInfo.etnum][editorInfo.etpos] << 8) | rtable[editorInfo.etnum][editorInfo.etpos];
                speed /= 2;

                ltable[editorInfo.etnum][editorInfo.etpos] = speed >> 8;
                rtable[editorInfo.etnum][editorInfo.etpos] = speed & 0xff;
                return true;
            }
            if (in.shift_or_ctrl && ((editorInfo.etnum == PTBL) || (editorInfo.etnum == FTBL)) &&
                (ltable[editorInfo.etnum][editorInfo.etpos] < 0x80)) {
                int speed = (signed char)(rtable[editorInfo.etnum][editorInfo.etpos]);
                speed /= 2;

                rtable[editorInfo.etnum][editorInfo.etpos] = speed;
                return true;
            }
        }
        break;

    case SDL_SCANCODE_GRAVE: table_cycle_type(in.shift_or_ctrl ? -1 : 1); return true;
    }

    return false;
}

void tablecommands(GTOBJECT* gt, const EditorInput* input) {
    (void)input;
    // Hex nibble entry only; navigation/edits go through the ImGui action layer.

    if (hexnybble >= 0) {
        if (editorInfo.editTableMode == EditTableMode::Wave && editorInfo.etnum == 0) {
            modifyWaveTableDetailed(hexnybble);

            editorInfo.etcolumn++;
            if (editorInfo.etcolumn > 3) {
                editorInfo.etcolumn = 2;
            }
            else if (editorInfo.etcolumn == 2) editorInfo.etcolumn = 0;
        }
        else if (editorInfo.editTableMode == EditTableMode::Filter) {
            modify_filter_table_detailed(hexnybble);

            editorInfo.etcolumn++;
            if (editorInfo.etcolumn > 3) {
                editorInfo.etcolumn = 2;
            }
            else if (editorInfo.etcolumn == 2) editorInfo.etcolumn = 0;
        }
        else if (editorInfo.editTableMode == EditTableMode::Pulse) {
            modify_pulse_table_detailed(hexnybble);

            editorInfo.etcolumn++;
            if (editorInfo.etcolumn > 4) {
                editorInfo.etcolumn = 3;
            }
            else if (editorInfo.etcolumn == 3) editorInfo.etcolumn = 0;
        }
        else {
            switch (editorInfo.etcolumn) {
            case 0:
                ltable[editorInfo.etnum][editorInfo.etpos] &= 0x0f;
                ltable[editorInfo.etnum][editorInfo.etpos] |= hexnybble << 4;
                break;
            case 1:
                ltable[editorInfo.etnum][editorInfo.etpos] &= 0xf0;
                ltable[editorInfo.etnum][editorInfo.etpos] |= hexnybble;
                break;
            case 2:
                rtable[editorInfo.etnum][editorInfo.etpos] &= 0x0f;
                rtable[editorInfo.etnum][editorInfo.etpos] |= hexnybble << 4;
                break;
            case 3:
                rtable[editorInfo.etnum][editorInfo.etpos] &= 0xf0;
                rtable[editorInfo.etnum][editorInfo.etpos] |= hexnybble;
                break;
            }

            editorInfo.etcolumn++;
            if (editorInfo.etcolumn > 3) {
                editorInfo.etcolumn = 0;
                editorInfo.etpos++;
                if (editorInfo.etpos >= MAX_TABLELEN) editorInfo.etpos = MAX_TABLELEN - 1;
            }
        }
    }

    validatetableview();
}

void delete_table(int num, int pos) {
    int c, d;

    // Shift tablepointers in instruments
    for (c = 1; c < MAX_INSTR; c++) {
        if ((instr[c].ptr[num] - 1) > pos) instr[c].ptr[num]--;
    }

    // Shift tablepointers in wavetable commands
    for (c = 0; c < MAX_TABLELEN; c++) {
        if ((ltable[WTBL][c] >= WAVECMD) && (ltable[WTBL][c] <= WAVELASTCMD)) {
            int cmd = ltable[WTBL][c] & 0xf;

            if (num < STBL) {
                if (cmd == CMD_SETWAVEPTR + num) {
                    if ((rtable[WTBL][c] - 1) > pos) rtable[WTBL][c]--;
                }
            }
            else {
                if ((cmd == CMD_FUNKTEMPO) || ((cmd >= CMD_PORTAUP) && (cmd <= CMD_VIBRATO))) {
                    if ((rtable[WTBL][c] - 1) > pos) rtable[WTBL][c]--;
                }
            }
        }
    }

    // Shift tablepointers in patterns
    for (c = 0; c < MAX_PATT; c++) {
        for (d = 0; d <= MAX_PATTROWS; d++) {
            if (num < STBL) {
                if (pattern[c][d * 4 + 2] == CMD_SETWAVEPTR + num) {
                    if ((pattern[c][d * 4 + 3] - 1) > pos) pattern[c][d * 4 + 3]--;
                }
            }
            else {
                if ((pattern[c][d * 4 + 2] == CMD_FUNKTEMPO) ||
                    ((pattern[c][d * 4 + 2] >= CMD_PORTAUP) && (pattern[c][d * 4 + 2] <= CMD_VIBRATO))) {
                    if ((pattern[c][d * 4 + 3] - 1) > pos) pattern[c][d * 4 + 3]--;
                }
            }
        }
    }

    // Shift jumppointers in the table itself
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (num != STBL) {
            if (ltable[num][c] == 0xff)
                if ((rtable[num][c] - 1) > pos) rtable[num][c]--;
        }
    }

    for (c = pos; c < MAX_TABLELEN; c++) {
        if (c + 1 < MAX_TABLELEN) {
            ltable[num][c] = ltable[num][c + 1];
            rtable[num][c] = rtable[num][c + 1];
        }
        else {
            ltable[num][c] = 0;
            rtable[num][c] = 0;
        }
    }
}

void insert_table(int num, int pos, int mode) {
    int c, d;

    // Shift tablepointers in instruments
    for (c = 1; c < MAX_INSTR; c++) {
        if (!mode) {
            if ((instr[c].ptr[num] - 1) >= pos) instr[c].ptr[num]++;
        }
        else {
            if ((instr[c].ptr[num] - 1) > pos) instr[c].ptr[num]++;
        }
    }

    // Shift tablepointers in wavetable commands
    for (c = 0; c < MAX_TABLELEN; c++) {
        if ((ltable[WTBL][c] >= WAVECMD) && (ltable[WTBL][c] <= WAVELASTCMD)) {
            int cmd = ltable[WTBL][c] & 0xf;

            if (num < STBL) {
                if (cmd == CMD_SETWAVEPTR + num) {
                    if (!mode) {
                        if ((rtable[WTBL][c] - 1) >= pos) rtable[WTBL][c]++;
                    }
                    else {
                        if ((rtable[WTBL][c] - 1) > pos) rtable[WTBL][c]++;
                    }
                }
            }
            else {
                if ((cmd == CMD_FUNKTEMPO) || ((cmd >= CMD_PORTAUP) && (cmd <= CMD_VIBRATO))) {
                    if (!mode) {
                        if ((rtable[WTBL][c] - 1) >= pos) rtable[WTBL][c]++;
                    }
                    else {
                        if ((rtable[WTBL][c] - 1) > pos) rtable[WTBL][c]++;
                    }
                }
            }
        }
    }


    // Shift tablepointers in patterns
    for (c = 0; c < MAX_PATT; c++) {
        for (d = 0; d <= MAX_PATTROWS; d++) {
            if (num < STBL) {
                if (pattern[c][d * 4 + 2] == CMD_SETWAVEPTR + num) {
                    if (!mode) {
                        if ((pattern[c][d * 4 + 3] - 1) >= pos) pattern[c][d * 4 + 3]++;
                    }
                    else {
                        if ((pattern[c][d * 4 + 3] - 1) > pos) pattern[c][d * 4 + 3]++;
                    }
                }
            }
            else {
                if ((pattern[c][d * 4 + 2] == CMD_FUNKTEMPO) ||
                    ((pattern[c][d * 4 + 2] >= CMD_PORTAUP) && (pattern[c][d * 4 + 2] <= CMD_VIBRATO))) {
                    if (!mode) {
                        if ((pattern[c][d * 4 + 3] - 1) >= pos) pattern[c][d * 4 + 3]++;
                    }
                    else {
                        if ((pattern[c][d * 4 + 3] - 1) > pos) pattern[c][d * 4 + 3]++;
                    }
                }
            }
        }
    }

    // Shift jumppointers in the table itself
    if (num != STBL) {
        for (c = 0; c < MAX_TABLELEN; c++) {
            if (ltable[num][c] == 0xff) {
                if (!mode) {
                    if ((rtable[num][c] - 1) >= pos) rtable[num][c]++;
                }
                else {
                    if ((rtable[num][c] - 1) > pos) rtable[num][c]++;
                }
            }
        }
    }

    for (c = MAX_TABLELEN - 1; c >= pos; c--) {
        if (c > pos) {
            ltable[num][c] = ltable[num][c - 1];
            rtable[num][c] = rtable[num][c - 1];
        }
        else {
            if ((num == WTBL) && (mode == 1)) {
                ltable[num][c] = 0xe9;
                rtable[num][c] = 0;
            }
            else {
                ltable[num][c] = 0;
                rtable[num][c] = 0;
            }
        }
    }
}

int gettablelen(int num) {
    int c;

    for (c = MAX_TABLELEN - 1; c >= 0; c--) {
        if (ltable[num][c] | rtable[num][c]) break;
    }
    return c + 1;
}

int gettablepartlen(int num, int pos) {
    int c;

    if (pos < 0) return 0;
    if (num == STBL) return 1;

    for (c = pos; c < MAX_TABLELEN; c++) {
        if (ltable[num][c] == 0xff) {
            c++;
            break;
        }
    }
    return c - pos;
}

void optimizetable(int num) {
    int c, d;

    memset(table_used, 0, sizeof table_used);

    for (c = 0; c < MAX_PATT; c++) {
        for (d = 0; d < pattlen[c]; d++) {
            if ((pattern[c][d * 4 + 2] >= CMD_SETWAVEPTR) && (pattern[c][d * 4 + 2] <= CMD_SETFILTERPTR))
                exectable(pattern[c][d * 4 + 2] - CMD_SETWAVEPTR, pattern[c][d * 4 + 3]);
            if ((pattern[c][d * 4 + 2] >= CMD_PORTAUP) && (pattern[c][d * 4 + 2] <= CMD_VIBRATO))
                exectable(STBL, pattern[c][d * 4 + 3]);
            if (pattern[c][d * 4 + 2] == CMD_FUNKTEMPO) exectable(STBL, pattern[c][d * 4 + 3]);
        }
    }

    for (c = 0; c < MAX_INSTR; c++) {
        for (d = 0; d < MAX_TABLES; d++) {
            exectable(d, instr[c].ptr[d]);
        }
    }

    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[WTBL][c + 1]) {
            if ((ltable[WTBL][c] >= WAVECMD) && (ltable[WTBL][c] <= WAVELASTCMD)) {
                d = -1;

                switch (ltable[WTBL][c] - WAVECMD) {
                case CMD_PORTAUP:
                case CMD_PORTADOWN:
                case CMD_TONEPORTA:
                case CMD_VIBRATO: d = STBL; break;

                case CMD_SETPULSEPTR: d = PTBL; break;

                case CMD_SETFILTERPTR: d = FTBL; break;
                }

                if (d != -1) exectable(d, rtable[WTBL][c]);
            }
        }
    }

    for (c = MAX_TABLELEN - 1; c >= 0; c--) {
        if ((ltable[num][c]) || (rtable[num][c])) break;
    }
    for (; c >= 0; c--) {
        if (!table_used[num][c + 1]) delete_table(num, c);
    }
}

int makespeedtable(unsigned data, int mode, int makenew) {
    int           c;
    unsigned char l = 0, r = 0;

    if (!data) return -1;

    switch (mode) {
    case MST_NOFINEVIB:
        l = (data & 0xf0) >> 4;
        r = (data & 0x0f) << 4;
        break;

    case MST_FINEVIB:
        l = (data & 0x70) >> 4;
        r = ((data & 0x0f) << 4) | ((data & 0x80) >> 4);
        break;

    case MST_FUNKTEMPO:
        l = (data & 0xf0) >> 4;
        r = data & 0x0f;
        break;

    case MST_PORTAMENTO:
        l = (data << 2) >> 8;
        r = (data << 2) & 0xff;
        break;

    case MST_RAW:
        r = data & 0xff;
        l = data >> 8;
        break;
    }

    if (makenew == 0) {
        for (c = 0; c < MAX_TABLELEN; c++) {
            if ((ltable[STBL][c] == l) && (rtable[STBL][c] == r)) return c;
        }
    }

    for (c = 0; c < MAX_TABLELEN; c++) {
        if ((!ltable[STBL][c]) && (!rtable[STBL][c])) {
            ltable[STBL][c] = l;
            rtable[STBL][c] = r;

            settableview(STBL, c);
            return c;
        }
    }
    return -1;
}

void deleteinstrtable(int i) {
    int c, d;
    int eraseok = 1;

    for (c = 0; c < MAX_TABLES; c++) {
        if (instr[i].ptr[c]) {
            int pos = instr[i].ptr[c] - 1;
            int len = gettablepartlen(c, pos);

            // Check that this table area isn't used by another instrument
            for (d = 1; d < MAX_INSTR; d++) {
                if ((d != i) && (instr[d].ptr[c])) {
                    int cmppos = instr[d].ptr[c] - 1;
                    if ((cmppos >= pos) && (cmppos < pos + len)) eraseok = 0;
                }
            }
            if (eraseok)
                while (len--) delete_table(c, pos);
        }
    }
}

void gototable(int num, int pos) {
    if (editorInfo.editmode == EditMode::Pattern) {
        if (num == STBL) {
            if (editorInfo.editTableMode != EditTableMode::None) editorInfo.editTableMode = EditTableMode::Wave;
        }
    }

    editorInfo.editmode = EditMode::Tables;
    settableview(num, pos);
}

void settableview(int num, int pos) {
    // If we're focusing on a table, continuefocus on the correct table
    if (editorInfo.editTableMode != EditTableMode::None) {
        if (num != STBL) editorInfo.editTableMode = static_cast<EditTableMode>(num + 1);
    }

    editorInfo.etnum    = num;
    editorInfo.etcolumn = 0;
    editorInfo.etpos    = pos;

    validatetableview();
}

void settableviewfirst(int num, int pos) {
    editorInfo.etview[num] = pos;
    settableview(num, pos);
}
void validatetableview() {
    if (editorInfo.etpos - editorInfo.etview[editorInfo.etnum] < 0)
        editorInfo.etview[editorInfo.etnum] = editorInfo.etpos;
    if (editorInfo.etpos - editorInfo.etview[editorInfo.etnum] >= VISIBLETABLEROWS)
        editorInfo.etview[editorInfo.etnum] = editorInfo.etpos - VISIBLETABLEROWS + 1;

    // Table view lock?
    if (editorInfo.etlock) {
        int c;

        for (c = 0; c < MAX_TABLES; c++) editorInfo.etview[c] = editorInfo.etview[editorInfo.etnum];
    }
}

void tableup() {
    if (shiftOrCtrlPressed) {
        if ((editorInfo.etmarknum != editorInfo.etnum) || (editorInfo.etpos != editorInfo.etmarkend)) {
            editorInfo.etmarknum   = editorInfo.etnum;
            editorInfo.etmarkstart = editorInfo.etmarkend = editorInfo.etpos;
        }
    }
    editorInfo.etpos--;
    if (editorInfo.etpos < 0) editorInfo.etpos = 0;
    if (shiftOrCtrlPressed) {
        editorInfo.etmarkend = editorInfo.etpos;
        if (editorInfo.etmarkend == editorInfo.etmarkstart) editorInfo.etmarknum = -1;
    }
}

void tabledown() {
    if (shiftOrCtrlPressed) {
        if ((editorInfo.etmarknum != editorInfo.etnum) || (editorInfo.etpos != editorInfo.etmarkend)) {
            editorInfo.etmarknum   = editorInfo.etnum;
            editorInfo.etmarkstart = editorInfo.etmarkend = editorInfo.etpos;
        }
    }
    editorInfo.etpos++;
    if (editorInfo.etpos >= MAX_TABLELEN) editorInfo.etpos = MAX_TABLELEN - 1;
    if (shiftOrCtrlPressed) {
        editorInfo.etmarkend = editorInfo.etpos;
        if (editorInfo.etmarkend == editorInfo.etmarkstart) editorInfo.etmarknum = -1;
    }
}

void exectable(int num, int ptr) {
    // Jump error check
    if ((num != STBL) && (ptr) && (ptr <= MAX_TABLELEN)) {
        if (ltable[num][ptr - 1] == 0xff) {
            table_error = TableError::Jump;
            return;
        }
    }

    for (;;) {
        // Exit when table stopped
        if (!ptr) break;
        // Overflow check
        if ((num != STBL) && (ptr > MAX_TABLELEN)) {
            table_error = TableError::Overflow;
            break;
        }
        // If were already here, exit
        if (table_used[num][ptr]) break;
        // Mark current position used
        table_used[num][ptr] = 1;
        // Go to next ptr.
        if (num != STBL) {
            if (ltable[num][ptr - 1] == 0xff) {
                ptr = rtable[num][ptr - 1];
            }
            else ptr++;
        }
        else break;
    }
}

int findfreespeedtable() {
    int c;
    for (c = 0; c < MAX_TABLELEN; c++) {
        if ((!ltable[STBL][c]) && (!rtable[STBL][c])) {
            return c;
        }
    }
    return -1;
}

/*
Handle user changing values in detailed wave table
*/
void modifyWaveTableDetailed(int hexnybble) {
    if (editorInfo.etcolumn >= 2) modify_wave_table_detailed_right(hexnybble);
    else modify_wave_table_detailed_left(hexnybble);
}

int jt = 0;

void allowEnterToReturnToPosition() {
    disableEnterToReturnToLastPos = 0;
    memcpy((char*)&editorInfoBackup, (char*)&editorInfo, sizeof(EDITOR_INFO));
}

static void table_mark_patterns_dirty_for_undo() {
    for (int i = 0; i < MAX_PATT; i++) undoAreaSetCheckForChange(UNDO_AREA_PATTERN, i, UNDO_AREA_DIRTY_CHECK);
}

void table_list_insert(GTOBJECT* gt) {
    table_mark_patterns_dirty_for_undo();
    insert_table(editorInfo.etnum, editorInfo.etpos, shiftOrCtrlPressed);
    (void)gt;
}

void table_list_delete(GTOBJECT* gt) {
    table_mark_patterns_dirty_for_undo();
    delete_table(editorInfo.etnum, editorInfo.etpos);
    (void)gt;
}

void table_copy_or_cut(int cut) {
    int c;

    if (editorInfo.etmarknum == -1) {
        editorInfo.etmarknum   = editorInfo.etnum;
        editorInfo.etmarkstart = editorInfo.etpos;
        editorInfo.etmarkend   = editorInfo.etmarkstart;
    }

    if (editorInfo.etmarknum != -1) {
        int d = 0;
        if (editorInfo.etmarkstart <= editorInfo.etmarkend) {
            for (c = editorInfo.etmarkstart; c <= editorInfo.etmarkend; c++) {
                ltablecopybuffer[d] = ltable[editorInfo.etmarknum][c];
                rtablecopybuffer[d] = rtable[editorInfo.etmarknum][c];
                if (cut) {
                    ltable[editorInfo.etmarknum][c] = 0;
                    rtable[editorInfo.etmarknum][c] = 0;
                }
                d++;
            }
        }
        else {
            for (c = editorInfo.etmarkend; c <= editorInfo.etmarkstart; c++) {
                ltablecopybuffer[d] = ltable[editorInfo.etmarknum][c];
                rtablecopybuffer[d] = rtable[editorInfo.etmarknum][c];
                if (cut) {
                    ltable[editorInfo.etmarknum][c] = 0;
                    rtable[editorInfo.etmarknum][c] = 0;
                }
                d++;
            }
        }
        tablecopyrows = d;
    }
    editorInfo.etmarknum = -1;
}

void table_paste() {
    int c;

    if (!tablecopyrows) return;

    for (c = 0; c < tablecopyrows; c++) {
        ltable[editorInfo.etnum][editorInfo.etpos] = ltablecopybuffer[c];
        rtable[editorInfo.etnum][editorInfo.etpos] = rtablecopybuffer[c];
        editorInfo.etpos++;
        if (editorInfo.etpos >= MAX_TABLELEN) editorInfo.etpos = MAX_TABLELEN - 1;
    }
}

void table_optimize() {
    if (shiftOrCtrlPressed) optimizetable(editorInfo.etnum);
}

void table_toggle_lock() {
    if (!shiftOrCtrlPressed) return;

    editorInfo.etlock = !editorInfo.etlock;
    validatetableview();
    gtui::set_status("Table Lock: %s", editorInfo.etlock ? "Enabled" : "Disabled");
}

void table_test_note(GTOBJECT* gt) {
    if (!shiftOrCtrlPressed)
        playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn, gt);
}

void table_release_note(GTOBJECT* gt) {
    if (shiftOrCtrlPressed) releasenote(editorInfo.epchn, gt);
}

void table_negate_value() {
    if (!shiftOrCtrlPressed) return;

    switch (editorInfo.etnum) {
    case FTBL:
        if (!ltable[editorInfo.etnum][editorInfo.etpos]) break;
    case PTBL:
        if (ltable[editorInfo.etnum][editorInfo.etpos] < 0x80)
            rtable[editorInfo.etnum][editorInfo.etpos] = (rtable[editorInfo.etnum][editorInfo.etpos] ^ 0xff) + 1;
        break;

    case WTBL:
        if ((ltable[editorInfo.etnum][editorInfo.etpos] != 0xff) &&
            (rtable[editorInfo.etnum][editorInfo.etpos] < 0x80))
            rtable[editorInfo.etnum][editorInfo.etpos] =
                (0x80 - rtable[editorInfo.etnum][editorInfo.etpos]) & 0x7f;
        break;
    }
}

void table_convert_note() {
    if (editorInfo.etnum != WTBL) return;

    if (ltable[editorInfo.etnum][editorInfo.etpos] == 0xff) return;

    int basenote = editorInfo.epoctave * 12;
    int note     = rtable[editorInfo.etnum][editorInfo.etpos];

    if (note >= 0x80) {
        note -= basenote;
        note &= 0x7f;
    }
    else {
        note += basenote;
        note |= 0x80;
    }

    rtable[editorInfo.etnum][editorInfo.etpos] = note;
}
