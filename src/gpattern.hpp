#pragma once

#include "gcommon.hpp"
#include "ginput.hpp"
#include "gmidi.hpp"
#include "gplay.hpp"

constexpr int KEYBOARD_POLYPHONY = 12;

enum class EditMode : int {
    Pattern    = 0,
    OrderList  = 1,
    Instrument = 2,
    Tables     = 3,
    Names      = 4,
};

enum class EditTableMode : int {
    None   = 0,
    Wave   = 1,
    Pulse  = 2,
    Filter = 3,
    Speed  = 4,
};

extern int  playingChannelOnKey[KEYBOARD_POLYPHONY];
extern int  MIDINotesHeld;
extern char keyOffsetText[100];

struct EDITOR_INFO {
    int          currentSongFile;
    unsigned int eacolumn;
    unsigned int multiplier;
    unsigned int sidmodel;
    unsigned int adparam;
    int          maxSIDChannels;

    unsigned int finevibrato;
    unsigned int optimizepulse;
    unsigned int optimizerealtime;
    unsigned int ntsc;
    unsigned int usefinevib;

    EditMode      editmode;
    EditTableMode editTableMode;
    int           nameIndex;
    int           mouseTrack;
    int           mouseTrackX;
    int           mouseTrackY;
    int           mouseTrackOriginalValue;
    int           mouseTrackDoUndo;

    int cursorX;
    int cursorY;

    int eseditpos;
    int esview;
    int escolumn;
    int eschn;
    int esnum;
    int esmarkchn;
    int esmarkstart;
    int esmarkend;
    int esmarkchnend;
    int enpos;

    int eppos;
    int epview;
    int epcolumn;
    int epchn;
    int epoctave;
    int epmarkchn;
    int epmarkstart;
    int epmarkend;

    int highlightLoopStart;
    int highlightLoopEnd;
    int highlightLoopPatternNumber;
    int highlightLoopChannel;

    int einum;
    int eipos;
    int eicolumn;

    int  etview[MAX_TABLES];
    int  etnum;
    int  etpos;
    int  etcolumn;
    bool etlock;
    int  etmarknum;
    int  etmarkstart;
    int  etmarkend;

    int  etDetailedWaveTableColumn;
    bool expandOrderListView;
};

extern EDITOR_INFO editorInfo;
extern EDITOR_INFO editorInfoBackup;
extern int         disableEnterToReturnToLastPos;

void patterncommands(GTOBJECT* gt, int midiNote, const EditorInput* input = nullptr);
bool pattern_cell_input(GTOBJECT* gt, int midiNote, const EditorInput* input = nullptr);
void nextpattern(GTOBJECT* gt);
void prevpattern(GTOBJECT* gt);
int  patternup(GTOBJECT* gt);
int  patterndown(GTOBJECT* gt);
void pattern_col_left(GTOBJECT* gt);
void pattern_col_right(GTOBJECT* gt);
void pattern_chn_next(GTOBJECT* gt);
void pattern_chn_prev(GTOBJECT* gt);
void pattern_nav_up(GTOBJECT* gt);
void pattern_nav_down(GTOBJECT* gt);
void pattern_nav_home(GTOBJECT* gt);
void pattern_nav_end(GTOBJECT* gt);
void pattern_nav_page_up(GTOBJECT* gt);
void pattern_nav_page_down(GTOBJECT* gt);
void shrinkpattern(GTOBJECT* gt);
void expandpattern(GTOBJECT* gt);
void splitpattern(GTOBJECT* gt);
void joinpattern(GTOBJECT* gt);

void pattern_list_insert(GTOBJECT* gt);
void pattern_list_delete(GTOBJECT* gt);
void pattern_copy_or_cut(GTOBJECT* gt, int cut);
void pattern_paste(GTOBJECT* gt);
void pattern_mark_toggle();
void pattern_toggle_jam();
void pattern_play_from_cursor(GTOBJECT* gt);
void pattern_mute_channel(GTOBJECT* gt, int ch);
void pattern_toggle_autoadvance();
void pattern_cmd_copy(GTOBJECT* gt);
void pattern_cmd_paste(GTOBJECT* gt);
void pattern_invert(GTOBJECT* gt);
void pattern_transpose_up(GTOBJECT* gt);
void pattern_transpose_down(GTOBJECT* gt);
void pattern_octave_up(GTOBJECT* gt);
void pattern_octave_down(GTOBJECT* gt);
void pattern_step_size_up();
void pattern_step_size_down();
void pattern_mark_all(GTOBJECT* gt);
void pattern_auto_pitchbend(GTOBJECT* gt);
void pattern_portamento_helper(GTOBJECT* gt);

void  handleShiftSpace(GTOBJECT* gt, int playChannel, int startPatternPos, bool follow, bool enable_loop);
int   handlePolyphonicKeyboard(GTOBJECT* gt);
int   handleMIDIPolykeyboard(GTOBJECT* gt, MIDI_MESSAGE midiData);
int   getNote(int rawkey);
int   findFreePolyChannel(int note);
bool  clearPolyChannel(int c, GTOBJECT* gt);
void  calculateNoteOffsets();
int   findNote(int lowestNote);
void  autoPitchbendToNextNote(GTOBJECT* gt);
short getNoteFreq(int noteIndex);
void  getPlayStartPosition(GTOBJECT* gte, int songNum, int c2, int songPos, int patternPos);
void  keyOn(int qwertyKey, int note, GTOBJECT* gt);
void  keyOff(int note, GTOBJECT* gt);
int   getNoteFromChannel(int c);
int   checkAnyPolyPlaying();
void  initPolyKeyboard();
void  clearKeyOns(GTOBJECT* gt, int c2, int i2);
