//
// GTUltra v2 instrument editor
//

#include "goattrk2.hpp"
#include "gimgui.hpp"
#include "ginstr.hpp"
#include "guimodel.hpp"

namespace {

void show_instr_table() {
    if (!editorInfo.etlock) {
        int c;

        for (c = MAX_TABLES - 1; c >= 0; c--) {
            if (instr[editorInfo.einum].ptr[c]) settableviewfirst(c, instr[editorInfo.einum].ptr[c] - 1);
        }
    }
}

} // namespace

INSTR instrcopybuffer;
int   cutinstr = -1;

bool instrument_cell_input(GTOBJECT* gt, const EditorInput* input) {
    const EditorInput in      = input ? *input : editor_input_snapshot();
    const int         jrawkey = in.rawkey;

    switch (jrawkey) {
    case 0x8:
    case SDL_SCANCODE_DELETE:
        if ((editorInfo.einum) && (in.shift_or_ctrl) && (editorInfo.eipos < LAST_INST)) {
            deleteinstrtable(editorInfo.einum);
            clearinstr(editorInfo.einum);
            return true;
        }
        break;

    case SDL_SCANCODE_X:
        if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST)) {
            cutinstr = editorInfo.einum;
            memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
            clearinstr(editorInfo.einum);
            return true;
        }
        break;

    case SDL_SCANCODE_C:
        if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST)) {
            cutinstr = -1;
            memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
            return true;
        }
        break;

    case SDL_SCANCODE_S:
        if ((editorInfo.einum) && (in.shift) && (editorInfo.eipos < LAST_INST)) {
            memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
            if (cutinstr != -1) {
                for (int c = 0; c < MAX_PATT; c++) {
                    for (int d = 0; d < pattlen[c]; d++)
                        if (pattern[c][d * 4 + 1] == cutinstr) pattern[c][d * 4 + 1] = editorInfo.einum;
                }
            }
            return true;
        }
        break;

    case SDL_SCANCODE_V:
        if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST)) {
            memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
            return true;
        }
        break;

    case SDL_SCANCODE_N:
        if ((editorInfo.eipos != LAST_INST) && (in.shift_or_ctrl)) {
            editorInfo.eipos = LAST_INST;
            return true;
        }
        break;

    case SDL_SCANCODE_U:
        if (in.shift_or_ctrl) {
            editorInfo.etlock = !editorInfo.etlock;
            validatetableview();

            gtui::set_status("Table Lock: %s", editorInfo.etlock ? "Enabled" : "Disabled");
            return true;
        }
        break;

    case SDL_SCANCODE_SPACE:
        if (editorInfo.eipos != LAST_INST) {
            if (!in.shift_or_ctrl)
                playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn, gt);
            else releasenote(editorInfo.epchn, gt);
            return true;
        }
        break;

    case SDL_SCANCODE_RETURN:
        if (!editorInfo.einum) break;
        switch (editorInfo.eipos) {
        case 2:
        case 3:
        case 4:
        case 5: {
            int pos;

            if (instr[editorInfo.einum].ptr[editorInfo.eipos - 2]) {
                if ((editorInfo.eipos == 5) && (in.shift_or_ctrl)) {
                    instr[editorInfo.einum].ptr[STBL] =
                        makespeedtable(instr[editorInfo.einum].ptr[STBL], editorInfo.finevibrato, 1) + 1;
                    return true;
                }
                pos = instr[editorInfo.einum].ptr[editorInfo.eipos - 2] - 1;
            }
            else {
                pos = gettablelen(editorInfo.eipos - 2);

                if (pos >= MAX_TABLELEN - 1) pos = MAX_TABLELEN - 1;
                if (in.shift_or_ctrl) instr[editorInfo.einum].ptr[editorInfo.eipos - 2] = pos + 1;
            }
            allowEnterToReturnToPosition();
            gototable(editorInfo.eipos - 2, pos);
            {
                int e = editorInfo.etpos;
                for (int i = 0; i < (VISIBLETABLEROWS - (VISIBLETABLEROWS / 4)); i++) {
                    tabledown();
                    validatetableview();
                }
                editorInfo.etpos = e;
            }
            return true;
        }

        case LAST_INST: editorInfo.eipos = 0; return true;
        }
        break;
    }

    return false;
}

void instrumentcommands(GTOBJECT* gt, const EditorInput* input) {
    (void)gt;
    (void)input;

    if ((hexnybble >= 0) && (editorInfo.eipos < LAST_INST) && (editorInfo.einum)) {
        unsigned char* ptr = &instr[editorInfo.einum].ad;
        ptr += editorInfo.eipos;

        switch (editorInfo.eicolumn) {
        case 0:
            *ptr &= 0x0f;
            *ptr |= hexnybble << 4;
            editorInfo.eicolumn++;
            break;

        case 1:
            *ptr &= 0xf0;
            *ptr |= hexnybble;
            editorInfo.eicolumn++;
            if (editorInfo.eicolumn > 1) {
                editorInfo.eicolumn = 0;
                editorInfo.eipos++;
                if (editorInfo.eipos >= LAST_INST) editorInfo.eipos = 0;
            }
            break;
        }
    }
    // Validate instrument parameters
    if (editorInfo.einum) {
        if (!(instr[editorInfo.einum].gatetimer & 0x3f)) instr[editorInfo.einum].gatetimer |= 1;
    }
}


void clearinstr(int num) {
    memset(&instr[num], 0, sizeof(INSTR));
    if (num) {
        if (editorInfo.multiplier) instr[num].gatetimer = 2 * editorInfo.multiplier;
        else instr[num].gatetimer = 1;

        instr[num].firstwave = 0x9;
        instr[num].pan       = 0x77;
    }
}

void gotoinstr(int i) {
    if (i < 0) return;
    if (i >= MAX_INSTR) return;

    editorInfo.einum = i;
    show_instr_table();

    editorInfo.editmode = EditMode::Instrument;
}

void nextinstr() {
    editorInfo.einum++;

    gtui::set_status("instr:%d", editorInfo.einum);

    if (editorInfo.einum >= MAX_INSTR) editorInfo.einum = MAX_INSTR - 1;
    show_instr_table();
}

void previnstr() {
    editorInfo.einum--;
    if (editorInfo.einum < 0) editorInfo.einum = 0;
    show_instr_table();
}
