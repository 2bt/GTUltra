//
// GTULTRA V1.5.3
// Based on source code of GOATTRACKER v2.76 Stereo
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#define GOATTRK2_C

#ifdef __WIN32__
#include <windows.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <time.h>

#include "goattrk2.hpp"
#include "gactions.hpp"
#include "gplatform.hpp"

#include "gimgui.hpp"
#include "guialert.hpp"

bool songExportSuccessFlag = false;
int  sidAddr1              = 0xd400;
int  sidAddr2              = 0xd420;
int  sidAddr3              = 0xd440;
int  sidAddr4              = 0xd460;
bool songExported          = false;
bool doExportToWAV         = false;
int  menu                  = 0;
bool autoNextPattern       = false;
bool recordmode            = true;
bool followplay            = false;
int  hexnybble             = -1;
int  stepsize              = 4;
int  autoadvance           = 0;
int  defaultpatternlength  = 64;
int  cursorflash           = 0;
int  cursorcolortable[]    = { 1, 2, 7, 2 };
bool exitprogram           = false;
// int editorInfo.eacolumn = 0;
int  eamode            = 0;
int  backupTimeSeconds = 30;
int  debugTicks; // used to measure CPU use when looking to improve performance
bool midiEnabled               = false;
bool forceSave3ChannelSng      = false;
bool normalizeWAV              = false;
bool useRepeatsWhenCompressing = true;
bool debugEnabled              = false;

int selectingInOrderList           = 0;
int selectingInOrderListDeltaTime  = 0;
int selectingInOrderListDeltaTicks = 0;


int leftKeyTicksDelta = 0;
int leftKeyTicks      = 0;

char appFileName[MAX_PATHNAME];
char packedsongname[MAX_FILENAME];


int SID_StereoPanPositions[4][4] = {
    { 7, 0, 0, 0 },
    { 0, 14, 0, 0 },
    { 0, 14, 7, 0 },
    { 0, 14, 0, 14 },
};

int sidPanInts[4] = { 0x0007, 0x00e0, 0x07e0, 0xe0e0 };

// int SID2_StereoPanPositions[] = { 0,14 };
// int SID3_StereoPanPositions[] = { 0,14,7 };
// int SID4_StereoPanPositions[] = { 0,14,0,14 };
char editPan = 0;


KeyPreset  keypreset     = KeyPreset::Tracker;
unsigned   playerversion = 0;
PackFormat fileformat    = PackFormat::Prg;
int        zeropageadr   = 0xfc;
int        playeradr     = 0x1000;


// unsigned editorInfo.adparam = 0x0f00;
// unsigned editorInfo.ntsc = 0;
unsigned patterndispmode = 0;
unsigned sidaddress      = 0xd400d420;
// unsigned finevibrato = 1;
// unsigned editorInfo.optimizepulse = 1;
// unsigned editorInfo.optimizerealtime = 1;
unsigned customclockrate = 0;
// unsigned editorInfo.usefinevib = 0;
unsigned b                     = DEFAULTBUF;
unsigned mr                    = DEFAULTMIXRATE;
unsigned writer                = 0;
unsigned hardsid               = 0;
unsigned catweasel             = 0;
unsigned interpolate           = 3;
unsigned residdelay            = 0;
unsigned hardsidbufinteractive = 20;
unsigned hardsidbufplayback    = 400;
unsigned monomode              = 0;
unsigned stereoMode = 1; // 0=mono, 1 = SID Stereo (SID 0+2 = Left, SID 1+3 = Right), 2 = True Stereo (emulation
                         // only - uses pan value per voice)
float        basepitch               = 0.0f;
float        equaldivisionsperoctave = 12.0f;
int          tuningcount             = 0;
double       tuning[96];
unsigned     bigwindow                    = 1; // window size tier 1..3 (win_init_editor)
int          checkUndoFlag                = 0;
unsigned int lmanMode                     = 1;
unsigned int enablekeyrepeat              = 0;
unsigned int enableAntiAlias              = 1;
bool         useOriginalGTFunctionKeys    = false;
int          SIDTracker64ForIPadIsAmazing = 1;

float masterVolume    = 1.0f;
float detuneCent      = 0;
int   displayingPanel = 0;
int   displayStopped  = 0;

char configbuf[MAX_PATHNAME];
char loadedsongfilename[MAX_PATHNAME]; // JP was MAX_FILENAME
char wavfilename[MAX_PATHNAME];
char songfilename[MAX_PATHNAME]; // JP was MAX_FILENAME
char wavfilter[MAX_FILENAME];
char songfilter[MAX_FILENAME];
char songpath[MAX_PATHNAME];
char instrfilename[MAX_FILENAME];
char instrfilter[MAX_FILENAME];
char instrpath[MAX_PATHNAME];
char packedpath[MAX_PATHNAME];
char tempSngFilename[MAX_PATHNAME];
char backupSngFilename[MAX_PATHNAME];

extern const char* notename[];
const char*        programname = "$VER: GTUltra V1.5.4";
char               specialnotenames[186];
char               scalatuningfilepath[MAX_PATHNAME];
char               tuningname[64];

char startPaletteName[MAX_PATHNAME];


char debugTextbuffer[MAX_PATHNAME];
char textbuffer[MAX_PATHNAME];

char transportPolySIDEnabled[4]; // 0 = OFF 1 = ON (all OFF = mono)
char transportLoopPattern           = 0;
char transportLoopPatternSelectArea = 0;
char transportRecord                = 1;
char transportPlay                  = 1;
char jdebugPlaying                  = 0;

char jpdebug = 0;

int selectedMIDIPort = 0;


const char hexkeytbl[] = "0123456789abcdef";


// int editorInfo.maxSIDChannels = 3;	//12;
int gMIDINote = -1;

int loadedSongFlag = 0;

int jdebug[16];


int main(int argc, char** argv) {


    //	char palettename[MAX_PATHNAME];

    FILE* configfile;
    int   c, d;

    // JP: SDL2 produces no audio for Windows32 without explicitly setting this (otherwise, it's set to "dummy
    // sound" as the output)
#ifdef __WIN32__
    SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
#endif


    editorInfo.multiplier  = 1;
    editorInfo.finevibrato = 1;
    editorInfo.adparam     = 0x0f00;

    programname += sizeof "$VER:";

    // Load configuration
#ifdef __WIN32__
    GetModuleFileName(NULL, appFileName, MAX_PATHNAME);
    appFileName[strlen(appFileName) - 3] = 'c';
    appFileName[strlen(appFileName) - 2] = 'f';
    appFileName[strlen(appFileName) - 1] = 'g';
#elif __amigaos__
    strcpy(appFileName, "PROGDIR:gtultra.cfg");
#else
    strcpy(appFileName, getenv("HOME"));
    strcat(appFileName, "/.goattrk/gtultra.cfg");
#endif

    createFilename(appFileName, backupSngFilename, "gtubackup.sng");

    // Skins are ImGui guicolors (legacy .gtp / charset.bin removed).

    configfile = fopen(appFileName, "rt");
    if (configfile) {
        getparam(configfile, &b);
        getparam(configfile, &mr);
        getparam(configfile, &hardsid);
        getparam(configfile, &editorInfo.sidmodel);
        getparam(configfile, &editorInfo.ntsc);
        unsigned fileformat_u = 0;
        getparam(configfile, &fileformat_u);
        fileformat = static_cast<PackFormat>(fileformat_u);
        getparam(configfile, (unsigned*)&playeradr);
        getparam(configfile, (unsigned*)&zeropageadr);
        getparam(configfile, &playerversion);
        unsigned keypreset_u = 0;
        getparam(configfile, &keypreset_u);
        keypreset = static_cast<KeyPreset>(keypreset_u);
        getparam(configfile, (unsigned*)&stepsize);

        getparam(configfile, &editorInfo.multiplier);
        getparam(configfile, &catweasel);
        getparam(configfile, &editorInfo.adparam);

        getparam(configfile, &interpolate);

        getparam(configfile, &patterndispmode);

        getparam(configfile, &sidaddress);

        getparam(configfile, (unsigned int*)&editorInfo.finevibrato);
        getparam(configfile, &editorInfo.optimizepulse);
        getparam(configfile, &editorInfo.optimizerealtime);
        getparam(configfile, &residdelay);
        getparam(configfile, &customclockrate);
        getparam(configfile, &hardsidbufinteractive);
        getparam(configfile, &hardsidbufplayback);

        getfloatparam(configfile, &filterparams.distortionrate);
        getfloatparam(configfile, &filterparams.distortionpoint);
        getfloatparam(configfile, &filterparams.distortioncfthreshold);
        getfloatparam(configfile, &filterparams.type3baseresistance);
        getfloatparam(configfile, &filterparams.type3offset);
        getfloatparam(configfile, &filterparams.type3steepness);
        getfloatparam(configfile, &filterparams.type3minimumfetresistance);
        getfloatparam(configfile, &filterparams.type4k);
        getfloatparam(configfile, &filterparams.type4b);
        getfloatparam(configfile, &filterparams.voicenonlinearity);
        getparam(configfile, (unsigned int*)&win_fullscreen);

        getparam(configfile, &bigwindow);
        getfloatparam(configfile, &basepitch);
        getfloatparam(configfile, &equaldivisionsperoctave);
        getstringparam(configfile, specialnotenames);
        getstringparam(configfile, scalatuningfilepath);
        getparam(configfile, (unsigned int*)&editorInfo.maxSIDChannels);
        getstringparam(configfile, startPaletteName);
        getfloatparam(configfile, &masterVolume);
        getfloatparam(configfile, &detuneCent);
        getparam(configfile, &enablekeyrepeat);
        getparam(configfile, (unsigned int*)&selectedMIDIPort);
        getparam(configfile, (unsigned int*)&enableAntiAlias);
        getparam(configfile, (unsigned int*)&sidPanInts[0]);
        getparam(configfile, (unsigned int*)&sidPanInts[1]);
        getparam(configfile, (unsigned int*)&sidPanInts[2]);
        getparam(configfile, (unsigned int*)&sidPanInts[3]);
        getparam(configfile, (unsigned int*)&backupTimeSeconds);
        getparam(configfile, (unsigned int*)&autoNextPattern);
        getparam(configfile, (unsigned int*)&useRepeatsWhenCompressing);
        getparam(configfile, (unsigned int*)&SIDTracker64ForIPadIsAmazing);
        getparam(configfile, (unsigned int*)&debugEnabled);

        fclose(configfile);
    }

    setSIDTracker64KeyOnStyle();

    // Init pathnames
    initpaths();

    // Scan command line
    for (c = 1; c < argc; c++) {
#ifdef __WIN32__
        if ((argv[c][0] == '-') || (argv[c][0] == '/'))
#else
        if (argv[c][0] == '-')
#endif
        {
            switch (argv[c][1]) // switch (toupper(argv[c][1]))
            {
            case '?':
                if (argv[c][2] == '?') {
                    gtaction::print_help_cli();
                    return 0;
                }

                std::puts("Usage: GTUltra [songname] [options]");
                std::puts("Options: (see documentation / gtultra -?? for full help)");
                std::puts("-?   Show this short usage");
                std::puts("-??  Print online help to stdout");
                return 0;

            case 'Z': sscanf(&argv[c][2], "%u", &residdelay); break;

            case 'A': sscanf(&argv[c][2], "%x", &editorInfo.adparam); break;

            case 'S': sscanf(&argv[c][2], "%u", &editorInfo.multiplier); break;

            case 'B': sscanf(&argv[c][2], "%u", &b); break;

            case 'D': sscanf(&argv[c][2], "%u", &patterndispmode); break;

            case 'E': sscanf(&argv[c][2], "%u", &editorInfo.sidmodel); break;

            case 'I': sscanf(&argv[c][2], "%u", &interpolate); break;

            case 'K': {
                unsigned keypreset_u = 0;
                sscanf(&argv[c][2], "%u", &keypreset_u);
                keypreset = static_cast<KeyPreset>(keypreset_u);
                break;
            }

            case 'L': sscanf(&argv[c][2], "%x", &sidaddress); break;

            case 'N':
                editorInfo.ntsc = 1;
                customclockrate = 0;
                break;

            case 'P':
                editorInfo.ntsc = 0;
                customclockrate = 0;
                break;

            case 'F': sscanf(&argv[c][2], "%u", &customclockrate); break;

            case 'M': sscanf(&argv[c][2], "%u", &mr); break;

            case 'O': sscanf(&argv[c][2], "%u", &editorInfo.optimizepulse); break;

            case 'R': sscanf(&argv[c][2], "%u", &editorInfo.optimizerealtime); break;

            case 'H': sscanf(&argv[c][2], "%x", &hardsid); break;

            case 'V': sscanf(&argv[c][2], "%u", &editorInfo.finevibrato); break;

            case 'T': sscanf(&argv[c][2], "%u", &hardsidbufinteractive); break;

            case 'U': sscanf(&argv[c][2], "%u", &hardsidbufplayback); break;

            case 'W': writer = 1; break;

            case 'X': sscanf(&argv[c][2], "%u", &win_fullscreen); break;

            case 'C': sscanf(&argv[c][2], "%u", &catweasel); break;

            case 'G': sscanf(&argv[c][2], "%f", &basepitch); break;

            case 'Q': sscanf(&argv[c][2], "%f", &equaldivisionsperoctave); break;

            case 'J': sscanf(&argv[c][2], "%s", specialnotenames); break;

            case 'Y': sscanf(&argv[c][2], "%s", scalatuningfilepath); break;

            case 'w': sscanf(&argv[c][2], "%u", &bigwindow); break;

            case 'c': sscanf(&argv[c][2], "%d", &editorInfo.maxSIDChannels);

            case 'p':
                // legacy palette preset CLI ignored (ImGui themes)

            case 'v': sscanf(&argv[c][2], "%f", &masterVolume);

            case 'd': sscanf(&argv[c][2], "%f", &detuneCent);

            case 'k': sscanf(&argv[c][2], "%d", &enablekeyrepeat);

            case 'm': sscanf(&argv[c][2], "%d", &selectedMIDIPort);

            case 'a': sscanf(&argv[c][2], "%d", &enableAntiAlias);

            case 'b': sscanf(&argv[c][2], "%d", &backupTimeSeconds);
            }
        }
        else {
            memset(specialnotenames, 0, 186);

            char startpath[MAX_PATHNAME];

            // JP - BUG in original GTStereo fix!
            // if argv[c] is > 60, only strcpy if the path contains no folders
            // strcpy(songfilename, argv[c]);

            int found_file_name = 0; // JP Fix
            for (d = strlen(argv[c]) - 1; d >= 0; d--) {
                if ((argv[c][d] == '/') || (argv[c][d] == '\\')) {
                    strcpy(startpath, argv[c]);
                    startpath[d + 1] = 0;
                    chdir(startpath);
                    initpaths();
                    strcpy(songfilename, &argv[c][d + 1]);
                    found_file_name++;
                    break;
                }
            }
            if (!found_file_name) {
                strcpy(songfilename, argv[c]); // JP - Fix bug
            }
        }
    }

    // Validate parameters

    if (selectedMIDIPort == 9999) // GAHHHH!!! 1.4.1 fix. (just had the single = )
        midiEnabled = false; // No MIDI processing will take place if MIDIPort is set to 9999 within .cfg file or
                             // via -m commandline option
    else midiEnabled = true;

    for (int i = 0; i < 4; i++) {
        convertInsToPans(i);
    }

    // startPaletteName is still read/written in cfg for field layout; skins are ImGui guicolors.

    if (editorInfo.maxSIDChannels != 3 && editorInfo.maxSIDChannels != 6 && editorInfo.maxSIDChannels != 9 &&
        editorInfo.maxSIDChannels != 12)
        editorInfo.maxSIDChannels = 6;

    editorInfo.sidmodel &= 1;
    editorInfo.adparam &= 0xffff;
    zeropageadr &= 0xff;
    playeradr &= 0xff00;
    if (!stepsize) stepsize = 4;
    if (editorInfo.multiplier > 16) editorInfo.multiplier = 16;
    if (static_cast<unsigned>(keypreset) > 2) keypreset = KeyPreset::Tracker;
    if ((editorInfo.finevibrato == 1) && (editorInfo.multiplier < 2)) editorInfo.usefinevib = 1;
    if (editorInfo.finevibrato > 1) editorInfo.usefinevib = 1;
    if (editorInfo.optimizepulse > 1) editorInfo.optimizepulse = 1;
    if (editorInfo.optimizerealtime > 1) editorInfo.optimizerealtime = 1;
    if (residdelay > 63) residdelay = 63;
    if (customclockrate < 100) customclockrate = 0;

    if ((detuneCent < -1) || (detuneCent > 1)) {
        detuneCent = 0;
    }

    if (enablekeyrepeat > 1) enablekeyrepeat = 0;

    // Read Scala tuning file
    if (scalatuningfilepath[0] != '0' && scalatuningfilepath[1] != '\0') {
        readscalatuningfile();
    }

    // Calculate frequencytable if necessary
    if (basepitch < 0.0f) basepitch = 0.0f;
    if (basepitch > 0.0f || detuneCent != 1) calculatefreqtable();

    // Set special note names
    if (specialnotenames[1] != '\0') {
        setspecialnotenames();
    }

    // JP - Init MIDI (yes. MIDI)

    if (midiEnabled) selectedMIDIPort = initMidi(selectedMIDIPort);

    if (!win_init_editor(bigwindow, (int)enableAntiAlias)) return 1;

    // Composite the experimental ImGui layer on top of the legacy editor.
    gimgui_init();

    initPolyKeyboard();
    // Reset channels/song
    initchannels(&gtObject);
    clearsong(true, true, true, true, true, &gtObject);

    copyExpandedSongValidFlag = 0;

    gtObject.masterLoopSubSong = 0;
    gtObject.masterLoopChannel = 0;
    initAreaListFlag           = 0;
    initUndoBufferFlag         = 0;
    undoInitAllAreas(
        &gtObject); // Must be called after clearSong. Creates undo buffers, containing duplicates of each GT area.


    // Init sound
    if (!sound_init(b,
                    mr,
                    writer,
                    hardsid,
                    editorInfo.sidmodel,
                    editorInfo.ntsc,
                    editorInfo.multiplier,
                    catweasel,
                    interpolate,
                    customclockrate)) {
        gt_ui_warn("Sound init failed. Continuing without sound "
                   "(song timer will not start).");
    }


    // JP - Init Editor info
    editorInfo.editmode     = EditMode::Pattern;
    editorInfo.epoctave     = 2;
    editorInfo.epmarkchn    = -1;
    editorInfo.esmarkchn    = -1;
    editorInfo.esmarkchnend = -1;
    editorInfo.etlock       = false; // was true; false for LMAN mode (tables unlocked)
    editorInfo.etmarknum    = -1;

    editorInfo.einum              = 1; // jp
    disableEnterToReturnToLastPos = 1;

    // JP - Init GTObject
    gtObject.masterfader           = 0xf;
    gtObject.controlEditor         = 1;
    gtObject.noSIDWrites           = 0;
    gtEditorObject.noSIDWrites     = 1;
    gtLoopObject.noSIDWrites       = 1;
    gtEditorLoopObject.noSIDWrites = 1;

    initSID(&gtObject);

    playUntilEnd(editorInfo.esnum); // Get length of time of loaded or empty song

    initSngMemory();
    lastValidSongFileIndex = 0;
    currentSongFile        = 0; // V1.4.0
    allocateSngMemory(0);       // V1.4.0
    copyCurrentToSngBuffer(&gtObject, 0);
    allocateSngMemory(1); // V1.4.0
    copyCurrentToSngBuffer(&gtObject, 1);

    allocateSngMemory(MAX_SONG_FILES); // this is never overwritten. Use to create an empty sng
    copyCurrentToSngBuffer(&gtObject, MAX_SONG_FILES);


    //-------------------------------------------------------------
#if 0
	strcpy(songfilename, "ultestura.sng");
	editorInfo.maxSIDChannels = 3;
#endif
    // Load song if applicable
    if (strlen(songfilename)) {
        int ok = loadsong(&gtObject, false);
        if (ok) {
            loadedSongFlag = 1;
            undoInitAllAreas(&gtObject); // recreate undo buffers, using the loaded song as the original info
            countInstruments();
        }

        playUntilEnd(editorInfo.esnum); // Get length of time of loaded or empty song
        copyCurrentToSngBuffer(&gtObject, editorInfo.currentSongFile);
    }


#if 0
	initsong(editorInfo.esnum, PlayMode::Beginning, &gtObject);
	followplay = shift_or_ctrl_pressed;
	while (!exitprogram)
	{
		//waitkeymouse(&gtObject);
		if (ascii_key)
		{
			// Shutdown sound output now
			sound_uninit();
			return 0;
		}

	}

#endif

    // Start editor mainloop
    gfx_present();

    //	SDL_Thread* threadID = SDL_CreateThread(doDisplay, "DisplayThread", (void*)&gtObject);


    while (!exitprogram) {

        if (doExportToWAV) {
            doExportToWAV = false;
            ExportAsPCM(editorInfo.esnum, normalizeWAV, &gtObject);
        }
        //	int ch = checkFor3ChannelSong();

        waitkeymouse(&gtObject);
        docommand();


        //	sprintf(textbuffer, "jpdebug %d", jdebug[0]);	//, specialnotenames[0], specialnotenames[1]);
    }

    // SDL_WaitThread(threadID, NULL);

    // Shutdown sound output now
    sound_uninit();

    // Save configuration
#ifndef __WIN32__
#ifdef __amigaos__
    strcpy(appFileName, "PROGDIR:goattrk2.cfg");
#else
    strcpy(appFileName, getenv("HOME"));
    strcat(appFileName, "/.goattrk");
    mkdir(appFileName, S_IRUSR | S_IWUSR | S_IXUSR);
    strcat(appFileName, "/gtultra.cfg");
#endif
#endif
    configfile = fopen(appFileName, "wt");
    if (configfile) {
        fprintf(configfile,
                ";------------------------------------------------------------------------------\n"
                ";GT2 config file. Rows starting with ; are comments. Hexadecimal parameters are\n"
                ";to be preceded with $ and decimal parameters with nothing.                    \n"
                ";------------------------------------------------------------------------------\n"
                "\n"
                ";reSID buffer length (in milliseconds)\n%d\n\n"
                ";reSID mixing rate (in Hz)\n%d\n\n"
                ";Hardsid device number (0 = off)\n%d\n\n"
                ";reSID model (0 = 6581, 1 = 8580)\n%d\n\n"
                ";Timing mode (0 = PAL, 1 = editorInfo.ntsc)\n%d\n\n"
                ";Packer/relocator fileformat (0 = SID, 1 = PRG, 2 = BIN)\n%d\n\n"
                ";Packer/relocator player address\n$%04x\n\n"
                ";Packer/relocator zeropage baseaddress\n$%02x\n\n"
                ";Packer/relocator player type (0 = standard ... 3 = minimal)\n%d\n\n"
                ";Key entry mode (0 = Protracker, 1 = DMC, 2 = Janko)\n%d\n\n"
                ";Pattern highlight step size\n%d\n\n"
                ";Speed editorInfo. (0 = 25Hz, 1 = 1X, 2 = 2X etc.)\n%d\n\n"
                ";Use CatWeasel SID (0 = off, 1 = on)\n%d\n\n"
                ";Hardrestart ADSR parameter\n$%04x\n\n"
                ";reSID interpolation (0 = off, 1 = on, 2 = distortion, 3 = distortion & on)\n%d\n\n"
                ";Pattern display mode (0 = decimal, 1 = hex, 2 = decimal w/dots, 3 = hex w/dots)\n%d\n\n"
                ";SID baseaddresses\n$%08x\n\n"
                ";Finevibrato mode (0 = off, 1 = on)\n%d\n\n"
                ";Pulseskipping (0 = off, 1 = on)\n%d\n\n"
                ";Realtime effect skipping (0 = off, 1 = on)\n%d\n\n"
                ";Random reSID write delay in cycles (0 = off)\n%d\n\n"
                ";Custom SID clock cycles per second (0 = use PAL/editorInfo.ntsc default)\n%d\n\n"
                ";HardSID interactive mode buffer size (in milliseconds, 0 = maximum/no flush)\n%d\n\n"
                ";HardSID playback mode buffer size (in milliseconds, 0 = maximum/no flush)\n%d\n\n"
                ";reSID-fp distortion rate\n%f\n\n"
                ";reSID-fp distortion point\n%f\n\n"
                ";reSID-fp distortion CF threshold\n%f\n\n"
                ";reSID-fp type 3 base resistance\n%f\n\n"
                ";reSID-fp type 3 base offset\n%f\n\n"
                ";reSID-fp type 3 base steepness\n%f\n\n"
                ";reSID-fp type 3 minimum FET resistance\n%f\n\n"
                ";reSID-fp type 4 k\n%f\n\n"
                ";reSID-fp type 4 b\n%f\n\n"
                ";reSID-fp voice nonlinearity\n%f\n\n"
                ";Window type (0 = window, 1 = fullscreen)\n%d\n\n"
                ";window scale factor (1 = no scaling, 2 to 4 = 2 to 4 times bigger window)\n%d\n\n"
                ";Base pitch of A-4 in Hz (0 = use default frequencytable)\n%f\n\n"
                ";Equal divisions per octave (12 = default, 8.2019143 = Bohlen-Pierce)\n%f\n\n"
                ";Special note names (2 chars for every note in an octave/cycle)\n%s\n\n"
                ";Path to a Scala tuning file .scl\n%s\n\n"
                ";Default SID channel playback\n%d\n\n"
                ";Default palette name\n%s\n\n"
                ";Master Volume scaler (1 = normal volume. 2 = twice as loud 0.5 = half volume..)\n%f\n\n"
                ";Detune Cent (0-2... 1 = no detune. 0 =-100 cents. 2=+100 cents)\n%f\n\n"
                ";Enable Key repeat (0-1... 0=only on specific keys. 1=on all keys)\n%d\n\n"
                ";MIDI Port\n%d\n\n"
                ";Enable Antialias\n%d\n\n"
                ";SID1 Pan\n$%04x\n\n"
                ";SID2 Pan\n$%04x\n\n"
                ";SID3 Pan\n$%04x\n\n"
                ";SID4 Pan\n$%04x\n\n"
                ";Backup sng every n seconds (0=OFF. Default = 30)\n%d\n\n"
                ";AutoNextPattern Automatically move to next or previous pattern in order list when moving cursor "
                "in pattern view (0=OFF. 1=ON)\n%d\n\n"
                ";Use repeats when compressing from expanded orderlist view (0=NO. 1=YES)\n%d\n\n"
                ";SIDTracker64 style pattern editing (SIDTracker64 IS Amazing) (0=NO. 1=YES. WARNING. NOT "
                "COMPATIBLE WITH STANDARD GOATTRACKER EDITING!!)\n%d\n\n"
                ";Perform MemoryChecks (Debug)\n%d\n\n",
                b,
                mr,
                hardsid,
                editorInfo.sidmodel,
                editorInfo.ntsc,
                static_cast<int>(fileformat),
                playeradr,
                zeropageadr,
                playerversion,
                static_cast<unsigned>(keypreset),
                stepsize,
                editorInfo.multiplier,
                catweasel,
                editorInfo.adparam,
                interpolate,
                patterndispmode,
                sidaddress,
                editorInfo.finevibrato,
                editorInfo.optimizepulse,
                editorInfo.optimizerealtime,
                residdelay,
                customclockrate,
                hardsidbufinteractive,
                hardsidbufplayback,
                filterparams.distortionrate,
                filterparams.distortionpoint,
                filterparams.distortioncfthreshold,
                filterparams.type3baseresistance,
                filterparams.type3offset,
                filterparams.type3steepness,
                filterparams.type3minimumfetresistance,
                filterparams.type4k,
                filterparams.type4b,
                filterparams.voicenonlinearity,
                win_fullscreen,
                bigwindow,
                basepitch,
                equaldivisionsperoctave,
                specialnotenames,
                scalatuningfilepath,
                editorInfo.maxSIDChannels,
                startPaletteName, // cfg field kept for layout; skins are ImGui guicolors
                masterVolume,
                detuneCent,
                enablekeyrepeat,
                selectedMIDIPort,
                enableAntiAlias,
                sidPanInts[0],
                sidPanInts[1],
                sidPanInts[2],
                sidPanInts[3],
                backupTimeSeconds,
                autoNextPattern,
                useRepeatsWhenCompressing,
                SIDTracker64ForIPadIsAmazing,
                debugEnabled);

        fclose(configfile);
    }

    // JP - ONLY THIS MODE IS CURRENTLY SUPPORTED FOR STEREO SID PANNING
    // 0 crashes things..
    //	if (interpolate != 3)
    //		interpolate = 3;

    // Exit
    return 0;
}

void waitkey(GTOBJECT* gt) {
    for (;;) {
        if (!jdebugPlaying) {
            displayupdate(gt);
        }
        getkey();
        if ((scancode) || (ascii_key)) break;
        if (win_quitted) break;
    }

    converthex();
}

MIDI_MESSAGE midiMessage;

int refreshSongTime          = 0;
int refreshSongInfoDeltaTime = 0;
int refreshCount             = 0;

int jcnt2     = 0;
int forceKeys = 1;

int backupSongTimer = 0;

// Frame timing for the auto-backup interval (was shared via ginfo).
namespace {
int      msDelta = 0;
uint32_t lastMS  = 0;
} // namespace

void handleLoad(GTOBJECT* gt, char* dragdropfile);

// Per-frame editor upkeep decoupled from the input-wait loop (M3). Polls SDL
// input, runs autosave/MIDI/jamming side effects, and refreshes the display.
void editor_frame_update(GTOBJECT* gt) {
    if (dropFileDir != nullptr) {
        handleLoad(gt, dropFileDir);
        dropFileDir = nullptr;
    }

    if (backupTimeSeconds > 0) {
        msDelta = SDL_GetTicks() - lastMS;

        backupSongTimer += msDelta;
        if (backupSongTimer > backupTimeSeconds * 1000) {
            if (gt->songinit == PlayMode::Stopped) {
                if (currentUndoPosition != lastUndoPosition) // Only auto-save if something has changed..
                {
                    lastUndoPosition = currentUndoPosition;
                    int allowBackup  = 1;
                    if (editorInfo.expandOrderListView) {
                        int maxSize = validateAllSongs();
                        if (maxSize < 0xff) compressAllSongs();
                        else allowBackup = 0;
                    }
                    if (allowBackup) saveBackupSong();
                    backupSongTimer = 0;
                }
            }
        }
        lastMS = SDL_GetTicks();
    }

    if (!jdebugPlaying) displayupdate(gt);

    getkey();

    editorInfo.mouseTrack = 0;

    if (win_mousewheel) {
        int keyUp   = SDL_SCANCODE_UP;
        int keyDown = SDL_SCANCODE_DOWN;

        // Legacy horizontal order list used left/right for position; the ImGui
        // vertical layout uses up/down for rows.
        if (editorInfo.editmode == EditMode::OrderList && editorInfo.expandOrderListView == 0) {
            keyUp   = SDL_SCANCODE_UP;
            keyDown = SDL_SCANCODE_DOWN;
        }
        else if (editorInfo.editmode == EditMode::OrderList && editorInfo.expandOrderListView == 0) {
            keyUp   = SDL_SCANCODE_LEFT;
            keyDown = SDL_SCANCODE_RIGHT;
        }

        if (win_mousewheel < 0) scancode = keyDown;
        else scancode = keyUp;

        win_mousewheel = 0;
    }

    handlePolyphonicKeyboard(&gtObject);

    if (!jdebugPlaying) {
        if (recordmode && editorInfo.editmode == EditMode::Pattern && midiEnabled) {
            if (midiEnabled) {
                checkForMidiInput(&midiMessage, selectedMIDIPort);
                int i = 0;
                for (int c = 0; c < midiMessage.size / 3; c++) {
                    unsigned char midiInstruction = midiMessage.message[i];
                    unsigned char midiNote        = midiMessage.message[i + 1];
                    unsigned char midiVel         = midiMessage.message[i + 2];
                    i += 3;

                    if (midiInstruction == 0x90 && midiVel > 0) // key on
                    {
                        gMIDINote =
                            midiNote + FIRSTNOTE; // editing pattern data and have received keyon from MIDI device
                        ascii_key    = 0;
                        scancode = 0;
                        handleMIDIPolykeyboard(&gtObject, midiMessage);
                        return;
                    }
                    else handleMIDIPolykeyboard(&gtObject, midiMessage);
                }
            }
        }
        else if ((!recordmode) ||
                 (editorInfo.epcolumn == 0 &&
                  editorInfo.editmode == EditMode::Pattern)) // else if ((!recordmode) || (recordmode &&
                                                             // editorInfo.editmode != EditMode::Pattern))
        {
            if (midiEnabled) {
                do {

                    checkForMidiInput(&midiMessage, selectedMIDIPort);
                    handleMIDIPolykeyboard(&gtObject, midiMessage);

                } while (midiMessage.size);
            }

            handlePolyphonicKeyboard(&gtObject); // update for QWERTY too

            if (!checkAnyPolyPlaying()) {
                for (int i = 0; i < KEYBOARD_POLYPHONY; i++) {
                    clearPolyChannel(i, gt);
                }
            }
            // The info line (cursor help / status / live note offsets) is
            // refreshed by gtui::context_help_refresh() during the ImGui draw.
        }
    }
}

void waitkeymouse(GTOBJECT* gt) {
    for (;;) {
        SDL_Delay(10); // add this

        gMIDINote        = -1;
        midiMessage.size = 0;

        editor_frame_update(gt);

        if (mouse_buttons) break;
        if (prev_mouse_buttons) break; // Handle modifying values when hold / dragging. We've released the mouse

        if ((scancode) || (ascii_key)) break;
        if (win_quitted) break;

        win_enable_key_repeat();
    }
    converthex();
}

void converthex() {
    int c;

    hexnybble = -1;
    for (c = 0; c < 16; c++) {
        if (tolower(ascii_key) == hexkeytbl[c]) {
            if (c >= 10) {
                if (!shift_or_ctrl_pressed) hexnybble = c;
            }
            else {
                hexnybble = c;
            }
        }
    }
}


void docommand(void) {

    // int i = 0;
    //	for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
    //		sprintf(textbuffer, "Audio driver %d: %s\n", i,  SDL_GetAudioDriver(0));
    //	}

    int       c2;
    GTOBJECT* gt;

    gt = &gtObject;

    const int legacy_hex = hexnybble;

    // "GUI" operation :)
    // M6: chargen mouse hit-testing removed; ImGui owns pointer input.

    GTUNDO_OBJECT* ed = undoCreateEditorInfo();

    // Mode-specific commands
    switch (editorInfo.editmode) {

    case EditMode::OrderList:

        // We need to check all channels in order list incase user presses shift1-6 to swap them around
        // (we could just set this for the other channe in orderlistcommands - but this is just safer overall..)

        if (editorInfo.expandOrderListView == 0) {
            for (int i = 0; i < MAX_CHN; i++) {
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
            }

            undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LEN, 0, UNDO_AREA_DIRTY_CHECK);
        }
        else {
            for (int i = 0; i < MAX_CHN; i++) {
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_PATTERN_EXPANDED,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_TRANSPOSE_EXPANDED,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
            }
            undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LENGTH_EXPANDED, 0, UNDO_AREA_DIRTY_CHECK);
        }

        c2 = getActualChannel(editorInfo.esnum, editorInfo.eschn);

        //	undoAreaSetCheckForChange(UNDO_AREA_CHANNEL_EDITOR_INFO, c2, UNDO_AREA_DIRTY_CHECK);

        if (!gtaction::dispatch_mode_navigation()) {
            const EditorInput in = editor_input_snapshot();
            if (!gtaction::dispatch_global(gtaction::Ctx::Order)) orderlistcommands(gt, &in);
        }
        break;

    case EditMode::Instrument:


        if (!gtaction::dispatch_mode_navigation()) {
            const EditorInput in = editor_input_snapshot();
            if (!gtaction::dispatch_instrument_cell_input(&in)) instrumentcommands(gt, &in);
        }
        break;

    case EditMode::Tables:

        if (!gtaction::dispatch_mode_navigation()) {
            const EditorInput in = editor_input_snapshot();
            if (!gtaction::dispatch_table_cell_input(&in)) tablecommands(gt, &in);
        }
        break;

    case EditMode::Pattern:

        c2 = getActualChannel(editorInfo.esnum, editorInfo.epchn);
        undoAreaSetCheckForChange(UNDO_AREA_PATTERN,
                                  gt->editorUndoInfo.editorInfo[c2].epnum,
                                  UNDO_AREA_DIRTY_CHECK);
        undoAreaSetCheckForChange(UNDO_AREA_PATTERN_LEN, 0, UNDO_AREA_DIRTY_CHECK);

        if (editorInfo.expandOrderListView == 0) {
            for (int i = 0; i < MAX_CHN; i++) {
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
            }
            undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LEN, 0, UNDO_AREA_DIRTY_CHECK);
        }
        else {
            for (int i = 0; i < MAX_CHN; i++) {
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_PATTERN_EXPANDED,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
                undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_TRANSPOSE_EXPANDED,
                                          i + (editorInfo.esnum * MAX_CHN),
                                          UNDO_AREA_DIRTY_CHECK);
            }
            undoAreaSetCheckForChange(UNDO_AREA_ORDERLIST_LENGTH_EXPANDED, 0, UNDO_AREA_DIRTY_CHECK);
        }

        // JP REMOVED THIS. SEEMS TO CAUSE PROBLEMS..
        //	undoAreaSetCheckForChange(UNDO_AREA_CHANNEL_EDITOR_INFO, c2, UNDO_AREA_DIRTY_CHECK);

        // if gMIDINote!=-1, then use this as input instead of QWERTY note input
        // Also, if this is the case, set ascii_key and scancode=0 so that only note input is recognised - just in case..
        if (!gtaction::dispatch_mode_navigation()) {
            const EditorInput in = editor_input_snapshot();
            gtaction::dispatch_pattern_cell_input(gMIDINote, &in);
        }

        countInstrumentsInPattern(gt->editorUndoInfo.editorInfo[c2].epnum);
        calculateTotalInstrumentsFromAllPatterns();
        break;

    case EditMode::Names: gtaction::dispatch_mode_navigation(); break;
    }


    if (undoValidateUndoAreas(ed) == 0) {
        undoFreeUndoObject((GTUNDO_OBJECT*)ed);
    }

    // Global commands — action layer handles migrated bindings first.
    if (!gtaction::consume_legacy_hex_input(legacy_hex)) {
        const gtaction::Ctx actx = gtaction::context_from_editmode(editorInfo.editmode);
        if (!gtaction::dispatch_global(actx)) generalcommands(gt);
    }
}

void generalcommands(GTOBJECT* gt) {
    if (win_quitted) exitprogram = true;
    (void)gt;
}

int load(GTOBJECT* gt, char* dragDropFileName) {
    // ImGui file I/O uses gtfile::; this path is drag-and-drop only.
    if (!dragDropFileName) return 0;

    win_enable_key_repeat();
    for (int i = 0; i < 256; i++) {
        songfilename[i] = dragDropFileName[i];
        if (dragDropFileName[i] == 0) break;
    }
    free(dragDropFileName);

    const int ok = loadsong(gt, false);
    if (ok) {
        loadedSongFlag = 1;
        undoInitAllAreas(&gtObject);
        countInstruments();
        expandAllSongs();
    }
    ascii_key    = 0;
    scancode = 0;
    return ok;
}

int quickSave() {
    if (loadedSongFlag) // set to 1 when song is loaded.set to 0 if song is cleared.
    {
        if (strlen(loadedsongfilename)) strcpy(songfilename, loadedsongfilename);
        savesong();
    }
    return loadedSongFlag;
}

void clear(GTOBJECT* gt) {
    if (gt_ui_confirm("Optimize everything?")) {
        optimizeeverything(true, true, &gtObject);
        countpatternlengths();
        ascii_key    = 0;
        scancode = 0;
        return;
    }

    const int cs = gt_ui_confirm("Clear orderlists?") ? 1 : 0;
    const int cp = gt_ui_confirm("Clear patterns?") ? 1 : 0;
    const int ci = gt_ui_confirm("Clear instruments?") ? 1 : 0;
    const int ct = gt_ui_confirm("Clear tables?") ? 1 : 0;
    const int cn = gt_ui_confirm("Clear song name?") ? 1 : 0;

    if (cs | cp | ci | ct | cn) {
        loadedSongFlag = 0;
        memset(songfilename, 0, sizeof songfilename);
    }
    clearsong(cs != 0, cp != 0, ci != 0, ct != 0, cn != 0, &gtObject);

    ascii_key    = 0;
    scancode = 0;
    (void)gt;
}


void convertPansToInts(int sidChips) {
    int intVal = 0;
    for (int i = 0; i < sidChips; i++) {
        intVal <<= 4;
        intVal += SID_StereoPanPositions[sidChips - 1][i];
    }
    sidPanInts[sidChips - 1] = intVal; // stored in .cfg file
}

void convertInsToPans(int sidChips) {
    for (int i = 0; i < 4; i++) {
        SID_StereoPanPositions[sidChips][i] = (sidPanInts[sidChips] >> (4 * i)) & 0xf;
    }
}


void editSIDPan(GTOBJECT* gt) {
    int sidChips = editorInfo.maxSIDChannels / 3;

    //	int	v = SID_StereoPanPositions[sidChips - 1][i];


    eamode              = 1;
    editorInfo.eacolumn = 0;

    for (;;) {
        waitkeymouse(gt);

        if (win_quitted) {
            exitprogram = true;
            ascii_key         = 0;
            scancode      = 0;
            return;
        }

        if (hexnybble >= 0 && hexnybble < 0xf) {
            SID_StereoPanPositions[sidChips - 1][editorInfo.eacolumn] = hexnybble;
            convertPansToInts(sidChips);

            editorInfo.eacolumn++;
        }

        switch (scancode) {

        case SDL_SCANCODE_F7:
            if (!shift_or_ctrl_pressed) break;

        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_TAB:
            eamode = 0;
            ascii_key    = 0;
            scancode = 0;
            return;

        case SDL_SCANCODE_BACKSPACE:
            if (!editorInfo.eacolumn) break;
        case SDL_SCANCODE_LEFT: editorInfo.eacolumn--; break;

        case SDL_SCANCODE_RIGHT: editorInfo.eacolumn++;
        }
        if (editorInfo.eacolumn < 0) editorInfo.eacolumn = sidChips - 1;
        editorInfo.eacolumn %= sidChips;

        if ((mouse_buttons) && (!prev_mouse_buttons)) {
            eamode = 0;
            return;
        }
    }
}


void editadsr(GTOBJECT* gt) {
    eamode              = 1;
    editorInfo.eacolumn = 0;

    for (;;) {
        waitkeymouse(gt);

        if (win_quitted) {
            exitprogram = true;
            ascii_key         = 0;
            scancode      = 0;
            return;
        }

        if (hexnybble >= 0) {
            undoCreateEditorInfoBackup();

            switch (editorInfo.eacolumn) {

            case 0:
                editorInfo.adparam &= 0x0fff;
                editorInfo.adparam |= hexnybble << 12;
                break;

            case 1:
                editorInfo.adparam &= 0xf0ff;
                editorInfo.adparam |= hexnybble << 8;
                break;

            case 2:
                editorInfo.adparam &= 0xff0f;
                editorInfo.adparam |= hexnybble << 4;
                break;

            case 3:
                editorInfo.adparam &= 0xfff0;
                editorInfo.adparam |= hexnybble;
                break;
            }
            editorInfo.eacolumn++;

            undoAddEditorSettingsToList();
        }

        switch (scancode) {

        case SDL_SCANCODE_Z:
            if (!ctrl_pressed) break;
            undoPerform(gt);
            break;

        case SDL_SCANCODE_F7:
            if (!shift_or_ctrl_pressed) break;

        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_TAB:
            eamode = 0;
            ascii_key    = 0;
            scancode = 0;
            return;

        case SDL_SCANCODE_BACKSPACE:
            if (!editorInfo.eacolumn) break;
        case SDL_SCANCODE_LEFT: editorInfo.eacolumn--; break;

        case SDL_SCANCODE_RIGHT: editorInfo.eacolumn++;
        }

        editorInfo.eacolumn &= 3;

        if ((mouse_buttons) && (!prev_mouse_buttons)) {
            eamode = 0;
            return;
        }
    }
}

void getparam(FILE* handle, unsigned int* value) {
    char* configptr;

    for (;;) {
        if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
        if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) &&
            (configbuf[0] != 10))
            break;
    }

    configptr = configbuf;
    if (*configptr == '$') {
        *value = 0;
        configptr++;
        for (;;) {
            char c = tolower(*configptr++);
            int  h = -1;

            if ((c >= 'a') && (c <= 'f')) h = c - 'a' + 10;
            if ((c >= '0') && (c <= '9')) h = c - '0';

            if (h >= 0) {
                *value *= 16;
                *value += h;
            }
            else break;
        }
    }
    else {
        *value = 0;
        for (;;) {
            char c = tolower(*configptr++);
            int  d = -1;

            if ((c >= '0') && (c <= '9')) d = c - '0';

            if (d >= 0) {
                *value *= 10;
                *value += d;
            }
            else break;
        }
    }
}

void getfloatparam(FILE* handle, float* value) {
    char* configptr;

    for (;;) {
        if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
        if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) &&
            (configbuf[0] != 10))
            break;
    }

    configptr = configbuf;
    *value    = 0.0f;
    sscanf(configptr, "%f", value);
}

void getstringparam(FILE* handle, char* value) {
    char* configptr;

    int found_semi = 0;

    for (;;) {
        int currentoffset = ftell(handle);

        if (!fgets(configbuf, MAX_PATHNAME, handle)) return;
        if (configbuf[0] == ';') // Found comment (should be the comment for this string param)
        {
            if (found_semi) // Already found a comment? Means that there was no string inbetween...
            {
                fseek(handle, currentoffset, SEEK_SET); // seek back to this comment.
                return;
            }
            found_semi++;
        }

        if ((configbuf[0]) && (configbuf[0] != ';') && (configbuf[0] != ' ') && (configbuf[0] != 13) &&
            (configbuf[0] != 10))
            break;
    }

    configptr = configbuf;

    sscanf(configptr, "%s", value);
}

int prevmultiplier() {
    if (editorInfo.multiplier > 0) {
        editorInfo.multiplier--;
        reInitSID();
        playUntilEnd(editorInfo.esnum);
        return 1;
    }
    return 0;
}

int nextmultiplier() {
    if (editorInfo.multiplier < 16) {
        editorInfo.multiplier++;
        reInitSID();
        playUntilEnd(editorInfo.esnum);
        return 1;
    }
    return 0;
}

void reInitSID() {
    sound_init(b,
               mr,
               writer,
               hardsid,
               editorInfo.sidmodel,
               editorInfo.ntsc,
               editorInfo.multiplier,
               catweasel,
               interpolate,
               customclockrate);
}

void calculatefreqtable() {
    float bp = basepitch;
    if (!bp) bp = 440.0f;

    double basefreq      = (double)bp * (16777216.0 / 985248.0) * pow(2.0, 0.25) / 32.0;
    double cyclebasefreq = basefreq;
    double freq          = basefreq;
    int    c;
    int    i;


    if (tuningcount) {
        c = 0;
        while (c < 96) {
            for (i = 0; i < tuningcount; i++) {
                if (c < 96) {
                    int intfreq = freq + 0.5;
                    if (intfreq > 0xffff) intfreq = 0xffff;
                    freqtbllo[c] = intfreq & 0xff;
                    freqtblhi[c] = intfreq >> 8;
                    freq         = cyclebasefreq * tuning[i];
                    c++;
                }
            }
            cyclebasefreq = freq;
        }
    }
    else {
        for (c = 0; c < 8 * 12; c++) {
            double note = c * 100; // * 100 so we can handle detune by +/- 100 cents
            note += (double)detuneCent * 100;
            double freq    = basefreq * pow(2.0, note / (double)(equaldivisionsperoctave * 100));
            int    intfreq = freq + 0.5;
            if (intfreq > 0xffff) intfreq = 0xffff;
            freqtbllo[c] = intfreq & 0xff;
            freqtblhi[c] = intfreq >> 8;
        }
    }
}

void setspecialnotenames() {
    int   i;
    int   j;
    int   oct;
    char* name;
    char  octave[11];

    i   = 0;
    oct = 0;
    while (i < 93) {
        for (j = 0; j < 186; j += 2) {
            if (specialnotenames[j] == '\0') break;
            if (i < 93) {
                name = malloc(4);
                strncpy(name, specialnotenames + j, 2);
                sprintf(octave, "%d", oct);
                strcpy(name + 2, octave);
                notename[i] = name;
                i++;
            }
        }
        oct++;
    }
}

void readscalatuningfile() {
    FILE*  scalatuningfile;
    char*  configptr;
    char   strbuf[64];
    char   name[3];
    int    i;
    double numerator;
    double denominator;
    double centvalue;

    scalatuningfile = fopen(scalatuningfilepath, "rt");
    if (scalatuningfile) {
        // Tuning name
        for (;;) {
            if (feof(scalatuningfile)) return;
            fgets(configbuf, MAX_PATHNAME, scalatuningfile);
            if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
        }
        configptr = configbuf;
        sscanf(configptr, "%63[^\t\n]", tuningname);

        // Tuning count
        for (;;) {
            if (feof(scalatuningfile)) return;
            fgets(configbuf, MAX_PATHNAME, scalatuningfile);
            if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
        }
        configptr = configbuf;
        sscanf(configptr, "%d", &tuningcount);

        // Tunings
        for (i = 0; i < tuningcount; i++) {
            for (;;) {
                if (feof(scalatuningfile)) return;
                fgets(configbuf, MAX_PATHNAME, scalatuningfile);
                if ((configbuf[0]) && (configbuf[0] != '!') && (configbuf[0] != 13) && (configbuf[0] != 10)) break;
            }
            configptr = configbuf;
            name[0]   = '\0';
            sscanf(configptr, "%63s %2s", strbuf, name);
            if (!i) {
                strcpy(specialnotenames, name);
            }
            else {
                if (i == tuningcount - 1) {
                    char* tmp = strdup(specialnotenames);
                    strcpy(specialnotenames, name);
                    strcat(specialnotenames, tmp);
                    free(tmp);
                }
                else {
                    strcat(specialnotenames, name);
                }
            }
            if (!strchr(strbuf, '.')) {
                sscanf(strbuf, "%lf", &numerator);
                if (strchr(strbuf, '/')) {
                    sscanf(strchr(strbuf, '/') + 1, "%lf", &denominator);
                    tuning[i] = numerator / denominator;
                }
            }
            else {
                sscanf(configptr, "%lf", &centvalue);
                tuning[i] = pow(2.0, centvalue / 1200.0);
            }
        }
        fclose(scalatuningfile);
    }
}

// Used to get time of overall length of song
// Either when first channel hits an END SONG or when last channel has looped

int patternOrderArray[256];
int patternOrderList[256];
int patternRemapOrderIndex;

void initRemapArrays() {
    for (int i = 0; i < 256; i++) {
        patternOrderArray[i] = -1;
        patternOrderList[i]  = -1;
    }
}


// JUST NEED TO TEST THIS NOW..

void ExportAsPCM(int songNumber, int doNormalize, GTOBJECT* gt) {


    // Stop playback & then stop SID processing
    if (gt->songinit != PlayMode::Stopped) {
        stopsong(gt);
        setMasterLoopChannel(gt, "debug_9");
    }

    playUntilEnd(songNumber);

    bypassPlayRoutine = 1; // Stop interrupt from updating play routine. We're going to do it manually
    SDL_Delay(50);

    GenerateExportFileName();
    OpenExportFileNameForWriting();


    int sng                    = getActualSongNumber(songNumber, 0); // editorInfo.esnum
    int currentLoopEnabledFlag = gt->loopEnabledFlag;
    int currentFollowFlag      = followplay;


    initsong(sng, PlayMode::Beginning, gt);
    gt->loopEnabledFlag = 0;
    followplay          = true;

    int samplesToExport =
        (mr * 2) / 100; // For 44100, IT APPEARS TO GENERATE 882 SAMPLES FOR 1x speed. 441 for 2x.. 220 for 4x...
    if (editorInfo.multiplier == 0) samplesToExport *= 2; // Handle 1/2 speed
    else samplesToExport /= editorInfo.multiplier;
    largestExportValue = 0; // Used for normalizing PCM
    int writeCounter   = 0;

    int allDone;
    do {

        if (writeCounter == 0) {
            SDL_Delay(10);
            getkey();
            displayupdate(gt);
        }
        writeCounter++;
        writeCounter %= 100;


        playroutine(gt);
        ExportSIDToPCMFile(samplesToExport, doNormalize);

        if (gt->songinit == PlayMode::Stopped) // Error in song data
        {
            break;
        }

        allDone = 1;
        for (int i = 0; i < editorInfo.maxSIDChannels; i++) {
            if (gt->chn[i].loopCount == 0) // wait until all channels have looped (or song ends)
            {
                allDone = 0; // hasn't looped
                break;
            }
        }
    } while (allDone == 0);

    ExportCloseFileHandle();

    convertRAWToWAV(doNormalize);


    gt->loopEnabledFlag = currentLoopEnabledFlag;
    followplay          = currentFollowFlag;

    bypassPlayRoutine = 0;

    if (gt->songinit != PlayMode::Stopped) {
        stopsong(gt);
        setMasterLoopChannel(gt, "debug_9");
    }
}


void playUntilEnd(int songNumber) {
    patternRemapOrderIndex = 0;
    initRemapArrays();
    playUntilEnd2(songNumber);
    setSongLengthTime(&gtEditorObject);
}

void playUntilEnd2(int songNumber) {
    int       sng = getActualSongNumber(songNumber, 0); // editorInfo.esnum
    GTOBJECT* gte = &gtEditorObject;

    initsong(sng, PlayMode::Beginning, gte); // JP FEB
    gte->loopEnabledFlag = 0;

    // printf("---- SubSong %x ----\n", songNumber);

    int allDone;
    do {
        playroutine(gte);

        // Create arrays that are used to remap exported SID patterns in playing order
        for (int i = 0; i < editorInfo.maxSIDChannels; i++) {
            int pat = gte->chn[i].pattnum;
            if (patternOrderArray[pat] == -1) {
                //		printf("Pattern %x\n", pat);
                patternOrderList[patternRemapOrderIndex] = pat; // contains list of patterns in order of playing
                patternOrderArray[pat]                   = patternRemapOrderIndex;
                patternRemapOrderIndex++;
            }
        }
        if (gte->songinit == PlayMode::Stopped) // Error in song data
        {
            break;
        }

        allDone = 1;
        for (int i = 0; i < editorInfo.maxSIDChannels; i++) {
            if (gte->chn[i].loopCount == 0) // wait until all channels have looped (or song ends)
            {
                allDone = 0; // hasn't looped
                break;
            }
        }
    } while (allDone == 0);
}

void handlePressRewind(int doubleClick, GTOBJECT* gt) {
    if (doubleClick) previousSongPos(&gtObject, 1);
    else {
        if (gt->songinit == PlayMode::Stopped) previousSongPos(&gtObject, 1); // move to start of previous pattern
        else if (editorInfo.eppos)
            previousSongPos(&gtObject, 0);  // playing. Not at start of pattern. move to start of current pattern
        else previousSongPos(&gtObject, 1); // playing. At start of pattern. move to start of previous pattern
    }
}


void handleSIDChannelCountChange(GTOBJECT* gt) {
    if (gt->songinit != PlayMode::Stopped) {
        stopsong(gt);
    }
    SDL_Delay(100); // ensure that GT player has done an update, so that playing channels are now silent prior to
                    // setting new channel count

    //	if (gt->masterLoopChannel >= editorInfo.maxSIDChannels)
    //		gt->masterLoopChannel = 0;


    if (editorInfo.eschn >= editorInfo.maxSIDChannels) editorInfo.eschn = 0;


    if ((editorInfo.eseditpos == songlen[editorInfo.esnum][editorInfo.eschn]) ||
        (editorInfo.eseditpos > songlen[editorInfo.esnum][editorInfo.eschn] + 1)) {
        editorInfo.eseditpos = songlen[editorInfo.esnum][editorInfo.eschn] + 1;
        editorInfo.escolumn  = 0;
    }
    setMasterLoopChannel(gt, "debug_a");

    orderSelectPatternsFromSelected(gt);
    return;

    int resetSong = 0;

    for (int i = 0; i < MAX_PLAY_CH; i++) {
        int c2  = getActualChannel(editorInfo.esnum, i);
        int sng = getActualSongNumber(editorInfo.esnum, i);

        int ep  = gt->editorUndoInfo.editorInfo[c2].espos;
        int ep2 = ep;

        if (editorInfo.expandOrderListView == 0) {
            if (songlen[sng][c2 % 6] > 0) {
                do {
                    ep2 = ep;
                    if ((songorder[sng][c2 % 6][ep] >= REPEAT) && (songorder[sng][c2 % 6][ep] < TRANSDOWN)) ep++;
                    if ((songorder[sng][c2 % 6][ep] >= TRANSDOWN) && (songorder[sng][c2 % 6][ep] < LOOPSONG)) ep++;
                } while (ep != ep2);
                gt->editorUndoInfo.editorInfo[c2].epnum = songorder[sng][c2 % 6][ep];
                gt->editorUndoInfo.editorInfo[c2].espos = ep; // set current channel pos
            }
            else {
                resetSong                               = 1;
                gt->editorUndoInfo.editorInfo[c2].epnum = 0;
                gt->editorUndoInfo.editorInfo[c2].espos = 0; // reset current channel pos
            }
        }
        else {
            if (songOrderLength[sng][c2 % 6] > 0) {
                gt->editorUndoInfo.editorInfo[c2].epnum = songOrderPatterns[sng][c2 % 6][ep];
                gt->editorUndoInfo.editorInfo[c2].espos = ep; // set current channel pos
            }
            else {
                resetSong                               = 1;
                gt->editorUndoInfo.editorInfo[c2].epnum = 0;
                gt->editorUndoInfo.editorInfo[c2].espos = 0; // reset current channel pos
            }
        }
    }

    // overkill??
    if (resetSong) {
        editorInfo.esnum = 1;
        songchange(gt, true);
        editorInfo.esnum = 0;
        songchange(gt, true);
    }
}


int backupPatternPos[MAX_PLAY_CH];
int oldepViewValue;
int oldepPosValue;


void backupPatternDisplayInfo(GTOBJECT* gt) {
    // JP - orderSelectPatternsFromSelected resets the pattern step position. We need to preserve this when
    // changing subsong
    oldepViewValue = editorInfo.epview;
    oldepPosValue  = editorInfo.eppos;

    for (int c = 0; c < editorInfo.maxSIDChannels; c++) // V1.2.2
    {
        int c2              = getActualChannel(editorInfo.esnum, c); // 0-12
        backupPatternPos[c] = gt->chn[c2].pattptr;
    }
}

void restorePatternDisplayInfo(GTOBJECT* gt) {
    editorInfo.epview = oldepViewValue;
    editorInfo.eppos  = oldepPosValue;

    for (int c = 0; c < editorInfo.maxSIDChannels;
         c++) // V1.2.2 restore pattern play position when selecting another pattern in orderlist
    {
        int c2              = getActualChannel(editorInfo.esnum, c); // 0-12
        gt->chn[c2].pattptr = backupPatternPos[c];
        // check if cursor > patlen. And reset to 0 if it is
        if ((c2 % 6) == editorInfo.epchn) {
            if (editorInfo.eppos > pattlen[gt->editorUndoInfo.editorInfo[c2].epnum]) {
                editorInfo.eppos  = 0;
                editorInfo.epview = -VISIBLEPATTROWS / 2;
                //				jdebug[10]++;
                //				sprintf(textbuffer, "out of range: %d", jdebug[10]);
            }
        }
    }
}

int jcc = 0;

void nextSongPos(GTOBJECT* gt) {
    int songNum = getActualSongNumber(editorInfo.esnum, gt->masterLoopChannel); // editorInfo.epchn);
    int ac      = getActualChannel(editorInfo.esnum, gt->masterLoopChannel);    // editorInfo.epchn);	// 0-12
    int c3      = ac % 6;

    int len = songlen[songNum][c3];
    if (editorInfo.expandOrderListView) len = songOrderLength[songNum][c3];

    if (gt->songinit == PlayMode::Stopped) {


        if (gt->editorUndoInfo.editorInfo[ac].espos < len - 1) {
            //			sprintf(textbuffer, "%d ac %d c3 %d esp %d sn %d sl %d", jcc++, ac, c3,
            // gt->editorUndoInfo.editorInfo[ac].espos, songNum, songlen[songNum][c3]);

            backupPatternDisplayInfo(gt); // V1.2.2 - keep pattern editing position when selecting a new song pos

            editorInfo.eseditpos = gt->editorUndoInfo.editorInfo[ac].espos + 1;
            orderSelectPatternsFromSelected(gt);
            if (gt->editorUndoInfo.editorInfo[ac].espos - editorInfo.esview >= VISIBLEORDERLIST) {
                editorInfo.esview    = gt->editorUndoInfo.editorInfo[ac].espos - VISIBLEORDERLIST + 1;
                editorInfo.eseditpos = gt->editorUndoInfo.editorInfo[ac].espos;
            }

            restorePatternDisplayInfo(gt); // V1.2.2 - keep pattern editing position when selecting a new song pos

            updateviewtopos(gt);
        }
    }
    else {
        if (gt->chn[gt->masterLoopChannel].songptr < len) {
            orderPlayFromPosition(gt, 0, gt->chn[gt->masterLoopChannel].songptr, gt->masterLoopChannel, false);
        }
    }
}


void previousSongPos(GTOBJECT* gt, int songDffset) {
    int songNum = getActualSongNumber(editorInfo.esnum, gt->masterLoopChannel); // editorInfo.epchn);
    int ac      = getActualChannel(editorInfo.esnum, gt->masterLoopChannel);    // editorInfo.epchn);	// 0-12
    int c3      = ac % 6;

    if (gt->songinit == PlayMode::Stopped) {

        editorInfo.eseditpos =
            gt->editorUndoInfo.editorInfo[ac].espos -
            songDffset; // move back n positions (0 if just moving to top of pattern. 1 otherwise)
        if (editorInfo.eseditpos < 0) editorInfo.eseditpos = 0;
        else if (songDffset) {
            /*
            Check if we're on a transpose or repeat. If so, keep moving backwards to a valid pattern
            */
            if (editorInfo.expandOrderListView == 0) {
                while ((songorder[songNum][c3][editorInfo.eseditpos] >= REPEAT) &&
                       (songorder[songNum][c3][editorInfo.eseditpos] < LOOPSONG)) {
                    editorInfo.eseditpos--;
                    if (editorInfo.eseditpos < 0) {
                        editorInfo.eseditpos = 0;
                        break;
                    }
                }
            }
        }

        backupPatternDisplayInfo(gt); // V1.2.2 - keep pattern editing position when selecting a new song pos
        orderSelectPatternsFromSelected(gt);
        restorePatternDisplayInfo(gt); // V1.2.2 - keep pattern editing position when selecting a new song pos

        if (gt->editorUndoInfo.editorInfo[ac].espos < editorInfo.esview) {
            editorInfo.esview    = gt->editorUndoInfo.editorInfo[ac].espos;
            editorInfo.eseditpos = gt->editorUndoInfo.editorInfo[ac].espos;
        }
        updateviewtopos(gt);
    }
    else {
        if (gt->chn[gt->masterLoopChannel].songptr) {
            int so = gt->chn[gt->masterLoopChannel].songptr - 1 - songDffset;
            if (so < 0) so = 0;

            if (songDffset) {
                /*
                Check if we're on a transpose or repeat. If so, keep moving backwards to a valid pattern
                */

                if (editorInfo.expandOrderListView == 0) {
                    while ((songorder[songNum][c3][so] >= REPEAT) && (songorder[songNum][c3][so] < LOOPSONG)) {
                        so--;
                        if (so < 0) {
                            so = 0;
                            break;
                        }
                    }
                }
            }

            orderPlayFromPosition(gt, 0, so, gt->masterLoopChannel, false);
        }
    }
}

void setSongToBeginning(GTOBJECT* gt) {

    editorInfo.eseditpos = 0;
    editorInfo.eschn     = editorInfo.epchn;
    if (gt->songinit == PlayMode::Stopped) {
        backupPatternDisplayInfo(gt); // V1.2.2 Preserve pattern editing position if possible
        orderSelectPatternsFromSelected(gt);
        restorePatternDisplayInfo(gt); // V1.2.2 Preserve pattern editing position if possible
    }
    else {
        orderPlayFromPosition(gt, 0, 0, 0, false);
        editorInfo.esview    = 0;
        editorInfo.eseditpos = 0;
    }

    updateviewtopos(gt);
}

void playFromCurrentPosition(GTOBJECT* gt, int currentPos) {


    int t1                          = followplay;
    int t2                          = gt->interPatternLoopEnabledFlag;
    int t3                          = transportLoopPattern;
    gt->loopEnabledFlag             = 0;
    gt->interPatternLoopEnabledFlag = 0;
    int c2                          = getActualChannel(editorInfo.esnum, editorInfo.epchn);
    handleShiftSpace(gt, c2, currentPos * 4, false, true);

    gt->loopEnabledFlag             = t3; // transportLoopPattern;
    gt->interPatternLoopEnabledFlag = t2;
    followplay                      = t1;
}

void ModifyTrackGetOriginalValue() {
    if (editorInfo.editmode == EditMode::Tables) {
        if (editorInfo.etcolumn < 2) // columns 0+1 = lefttable value
            editorInfo.mouseTrackOriginalValue = ltable[editorInfo.etnum][editorInfo.etpos];
        else editorInfo.mouseTrackOriginalValue = rtable[editorInfo.etnum][editorInfo.etpos];
    }
    if (editorInfo.editmode == EditMode::Instrument) {
        unsigned char* ptr = &instr[editorInfo.einum].ad;
        ptr += editorInfo.eipos;
        editorInfo.mouseTrackOriginalValue = *ptr;
    }
}

// M6 Phase 5: detailed table / waveform mouse helpers (chargen hit-testing) removed.

// Wrote all this, then realised I could just easily modify the existing calculatefreqtable
// Will leave it here anyway. May use it again one day...

// 1200 per octave (100 per semitone)
float centToHz(int cent) {
    float a = 440.0f; // frequency of A (coomon value is 440Hz)
    return (a / 32) * pow(2, ((cent - 900) / 1200.0));
}

int HzToSIDFreq(float hz) {
    float phi = 985248; // PAL

    //	phi = 1022727;	//editorInfo.ntsc

    float freqCons = (256 * 256 * 256) / phi;
    float sidFreq  = freqCons * hz;
    return sidFreq;
}

float noteToHz(int note) {
    note *= 100;
    return centToHz(note);
}

void detunePitchTable() {
    for (int i = 0; i < 0x60; i++) {
        int cent = (12 + i) * 100;
        cent += detuneCent - 100; // -99 > + 99

        float hz      = centToHz(cent);
        int   SIDFreq = HzToSIDFreq(hz);
        freqtbllo[i]  = SIDFreq & 0xff;
        freqtblhi[i]  = (SIDFreq >> 8) & 0xff;
    }
}


void createFilename(char* filePath, char* newfileName, const char* filename) {
    int d = 0;
    for (d = strlen(filePath) - 1; d >= 0; d--) {
        if ((filePath[d] == '/') || (filePath[d] == '\\')) {
            strcpy(newfileName, filePath);
            break;
        }
    }
    strcpy(&newfileName[d + 1], filename);
}


void validateStereoMode() {
    if (stereoMode == 1 && editorInfo.maxSIDChannels == 3) stereoMode++;
    if (stereoMode == 0) monomode = 1;
    else monomode = 0;
}


char backupFolderName[MAX_FILENAME];

void saveBackupSong() {
    createBackupFolder();

    time_t mytime                  = time(nullptr);
    char*  time_str                = ctime(&mytime);
    time_str[strlen(time_str) - 1] = '\0';
    replacechar(time_str, ':', '_');

    strcpy(backupSngFilename, backupFolderName);
    strcat(backupSngFilename, "/GTUltra_");
    strcat(backupSngFilename, time_str);
    strcat(backupSngFilename, ".sng");

    strcpy(tempSngFilename, songfilename);
    strcpy(songfilename, backupSngFilename);
    int tempforce3chan   = forceSave3ChannelSng;
    forceSave3ChannelSng = false;
    savesong();
    forceSave3ChannelSng = tempforce3chan;
    strcpy(songfilename, tempSngFilename);
    strcpy(loadedsongfilename, tempSngFilename);
}

int replacechar(char* str, char orig, char rep) {
    char* ix = str;
    int   n  = 0;
    while ((ix = strchr(ix, orig)) != nullptr) {
        *ix++ = rep;
        n++;
    }
    return n;
}


int createBackupFolder() {

    DIR* folder;

    memset(backupFolderName, '\0', sizeof(backupFolderName));

    // JP - NOT TESTED ! SETTING DIFFERENT PATH FOR LINUX FOR READING PALETTES FROM .EXE LOCATION INSTEAD OF .CFG
#ifdef __WIN32__
    createFilename(appFileName, backupFolderName, "gtbackup");
#else
    strcpy(backupFolderName, getenv("HOME"));
    strcat(backupFolderName, "/.goattrk/gtbackup");
#endif

    folder = opendir(backupFolderName);
    if (folder == nullptr) {
#ifdef __WIN32__
        mkdir(backupFolderName); // default backup folder didn't exist in config file location. It does now..
#else
        mkdir(backupFolderName, 0777);
#endif
        return 0;
    }
    return 1;
}


void stopScreenDisplay() {
    displayingPanel = 1;
    return;

    while (displayStopped == 0) {
        SDL_Delay(1000 / 60);
    }
}

void restartScreenDisplay() { displayingPanel = 0; }


void handleLoad(GTOBJECT* gt, char* dragdropfile) {
    stopScreenDisplay();

    int ok = load(gt, dragdropfile);
    if (ok) {
        songExported         = 0;
        forceSave3ChannelSng = false;

        // Set up song 1 and then 0... This allows editor pattern numbers to be complete, so that F3 works from the
        // very start. (Bit of a nasty hack..Meh. Never mind)
        editorInfo.esnum = 1;
        songchange(gt, true);
        editorInfo.esnum = 0;
        songchange(gt, true);

        playUntilEnd(editorInfo.esnum);

        copyCurrentToSngBuffer(gt, currentSongFile); // V1.4.0
    }

    restartScreenDisplay();
}

void handleLoadPath(GTOBJECT* gt, const char* path, int merge) {
    if (!path || !path[0]) return;

    stopScreenDisplay();
    win_enable_key_repeat();

    snprintf(songfilename, MAX_PATHNAME, "%s", path);
    {
        char dirbuf[MAX_PATHNAME];
        snprintf(dirbuf, sizeof dirbuf, "%s", path);
        char* slash = strrchr(dirbuf, '/');
#ifdef _WIN32
        if (!slash) slash = strrchr(dirbuf, '\\');
#endif
        if (slash) {
            *slash = '\0';
            snprintf(songpath, MAX_PATHNAME, "%s", dirbuf);
            chdir(songpath);
        }
    }

    int ok = 0;
    if ((editorInfo.editmode != EditMode::Instrument) && (editorInfo.editmode != EditMode::Tables))
        ok = merge ? mergesong(gt) : loadsong(gt, false);

    if (ok) {
        loadedSongFlag = 1;
        undoInitAllAreas(&gtObject);
        countInstruments();
        expandAllSongs();

        songExported         = 0;
        forceSave3ChannelSng = false;

        editorInfo.esnum = 1;
        songchange(gt, true);
        editorInfo.esnum = 0;
        songchange(gt, true);

        playUntilEnd(editorInfo.esnum);
        copyCurrentToSngBuffer(gt, currentSongFile);
    }

    ascii_key    = 0;
    scancode = 0;
    restartScreenDisplay();
}

int saveSongAtPath(GTOBJECT* gt, const char* path) {
    (void)gt;
    if (!path || !path[0]) return 0;

    snprintf(songfilename, MAX_PATHNAME, "%s", path);
    {
        char dirbuf[MAX_PATHNAME];
        snprintf(dirbuf, sizeof dirbuf, "%s", path);
        char* slash = strrchr(dirbuf, '/');
#ifdef _WIN32
        if (!slash) slash = strrchr(dirbuf, '\\');
#endif
        if (slash) {
            *slash = '\0';
            snprintf(songpath, MAX_PATHNAME, "%s", dirbuf);
            chdir(songpath);
        }
    }

    return savesong();
}
