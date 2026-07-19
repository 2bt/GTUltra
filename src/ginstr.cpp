//
// GTUltra v2 instrument editor
//

#define GINSTR_C

#include "goattrk2.hpp"
#include "gimgui.hpp"

INSTR instrcopybuffer;
int cutinstr = -1;

bool instrument_cell_input(GTOBJECT *gt, const EditorInput *input)
{
	const EditorInput in = input ? *input : editor_input_snapshot();
	const int jrawkey = in.rawkey;

	switch (jrawkey)
	{
	case 0x8:
	case KEY_DEL:
		if ((editorInfo.einum) && (in.shift_or_ctrl) && (editorInfo.eipos < LAST_INST))
		{
			deleteinstrtable(editorInfo.einum);
			clearinstr(editorInfo.einum);
			return true;
		}
		break;

	case KEY_X:
		if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST))
		{
			cutinstr = editorInfo.einum;
			memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
			clearinstr(editorInfo.einum);
			return true;
		}
		break;

	case KEY_C:
		if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST))
		{
			cutinstr = -1;
			memcpy(&instrcopybuffer, &instr[editorInfo.einum], sizeof(INSTR));
			return true;
		}
		break;

	case KEY_S:
		if ((editorInfo.einum) && (in.shift) && (editorInfo.eipos < LAST_INST))
		{
			memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
			if (cutinstr != -1)
			{
				for (int c = 0; c < MAX_PATT; c++)
				{
					for (int d = 0; d < pattlen[c]; d++)
						if (pattern[c][d * 4 + 1] == cutinstr) pattern[c][d * 4 + 1] = editorInfo.einum;
				}
			}
			return true;
		}
		break;

	case KEY_V:
		if ((editorInfo.einum) && (in.ctrl) && (editorInfo.eipos <= LAST_INST))
		{
			memcpy(&instr[editorInfo.einum], &instrcopybuffer, sizeof(INSTR));
			return true;
		}
		break;

	case KEY_N:
		if ((editorInfo.eipos != LAST_INST) && (in.shift_or_ctrl))
		{
			editorInfo.eipos = LAST_INST;
			return true;
		}
		break;

	case KEY_U:
		if (in.shift_or_ctrl)
		{
			editorInfo.etlock ^= 1;
			validatetableview();

			if (editorInfo.etlock)
				sprintf(infoTextBuffer, "Table Lock: Enabled");
			else
				sprintf(infoTextBuffer, "Table Lock: Disabled");
			forceInfoLine = 1;
			return true;
		}
		break;

	case KEY_SPACE:
		if (editorInfo.eipos != LAST_INST)
		{
			if (!in.shift_or_ctrl)
				playtestnote(FIRSTNOTE + editorInfo.epoctave * 12, editorInfo.einum, editorInfo.epchn, gt);
			else
				releasenote(editorInfo.epchn, gt);
			return true;
		}
		break;

	case KEY_ENTER:
		if (!editorInfo.einum) break;
		switch (editorInfo.eipos)
		{
		case 2:
		case 3:
		case 4:
		case 5:
		{
			int pos;

			if (instr[editorInfo.einum].ptr[editorInfo.eipos - 2])
			{
				if ((editorInfo.eipos == 5) && (in.shift_or_ctrl))
				{
					instr[editorInfo.einum].ptr[STBL] = makespeedtable(instr[editorInfo.einum].ptr[STBL], editorInfo.finevibrato, 1) + 1;
					return true;
				}
				pos = instr[editorInfo.einum].ptr[editorInfo.eipos - 2] - 1;
			}
			else
			{
				pos = gettablelen(editorInfo.eipos - 2);

				if (pos >= MAX_TABLELEN - 1) pos = MAX_TABLELEN - 1;
				if (in.shift_or_ctrl) instr[editorInfo.einum].ptr[editorInfo.eipos - 2] = pos + 1;
			}
			allowEnterToReturnToPosition();
			gototable(editorInfo.eipos - 2, pos);
			{
				int e = editorInfo.etpos;
				for (int i = 0; i < (VISIBLETABLEROWS - (VISIBLETABLEROWS / 4)); i++)
				{
					tabledown();
					validatetableview();
				}
				editorInfo.etpos = e;
			}
			return true;
		}

		case LAST_INST:
			editorInfo.eipos = 0;
			return true;
		}
		break;
	}

	return false;
}

void instrumentcommands(GTOBJECT *gt, const EditorInput *input)
{
	const EditorInput in = input ? *input : editor_input_snapshot();
	const int jrawkey = in.rawkey;

	goto instr_hex_input;


instr_hex_input:
	if ((hexnybble >= 0) && (editorInfo.eipos < LAST_INST) && (editorInfo.einum))
	{
		unsigned char *ptr = &instr[editorInfo.einum].ad;
		ptr += editorInfo.eipos;

		switch (editorInfo.eicolumn)
		{
		case 0:
			*ptr &= 0x0f;
			*ptr |= hexnybble << 4;
			editorInfo.eicolumn++;
			break;

		case 1:
			*ptr &= 0xf0;
			*ptr |= hexnybble;
			editorInfo.eicolumn++;
			if (editorInfo.eicolumn > 1)
			{
				editorInfo.eicolumn = 0;
				editorInfo.eipos++;
				if (editorInfo.eipos >= LAST_INST) editorInfo.eipos = 0;
			}
			break;
		}
		lastEditWindow = -1;	// force redraw of Info bar with updated info
		setTableBackgroundColours(editorInfo.einum);
	}
instr_validate:
	// Validate instrument parameters
	if (editorInfo.einum)
	{
		if (!(instr[editorInfo.einum].gatetimer & 0x3f)) instr[editorInfo.einum].gatetimer |= 1;
	}
}


void clearinstr(int num)
{
	memset(&instr[num], 0, sizeof(INSTR));
	if (num)
	{
		if (editorInfo.multiplier)
			instr[num].gatetimer = 2 * editorInfo.multiplier;
		else
			instr[num].gatetimer = 1;

		instr[num].firstwave = 0x9;
		instr[num].pan = 0x77;
	}
}

void gotoinstr(int i)
{
	if (i < 0) return;
	if (i >= MAX_INSTR) return;

	editorInfo.einum = i;
	showinstrtable();

	editorInfo.editmode = EDIT_INSTRUMENT;
}

void nextinstr(void)
{
	editorInfo.einum++;

	sprintf(infoTextBuffer, "instr:%d", editorInfo.einum);

	if (editorInfo.einum >= MAX_INSTR)
		editorInfo.einum = MAX_INSTR - 1;
	showinstrtable();
}

void previnstr(void)
{
	editorInfo.einum--;
	if (editorInfo.einum < 0)
		editorInfo.einum = 0;
	showinstrtable();
	setTableBackgroundColours(editorInfo.einum);
}

void showinstrtable(void)
{
	setTableBackgroundColours(editorInfo.einum);

	if (!editorInfo.etlock)
	{
		int c;

		for (c = MAX_TABLES - 1; c >= 0; c--)
		{
			if (instr[editorInfo.einum].ptr[c])
				settableviewfirst(c, instr[editorInfo.einum].ptr[c] - 1);
		}
	}
}

