//
// GTUltra packer/relocator
//

#include "goattrk2.hpp"
#include "embed.hpp"
#include "gendian.hpp"
#include "gpattern.hpp"
#include "gsong.hpp"
#include "gtable.hpp"

#include <cstdlib>
#include <string_view>
#include <vector>

#ifndef GT2RELOC
#include "guialert.hpp"
#endif

extern "C" {

#include "parse.h"
}

#ifdef GT2RELOC
#ifdef __WIN32__
extern FILE *STDOUT, *STDERR;
#else
#define STDOUT stdout
#define STDERR stderr
#endif
extern char packedsongname[MAX_PATHNAME];
#endif

// Exported usage maps (see greloc.hpp).
uint8_t    patt_used[MAX_PATT];
uint8_t    instr_used[MAX_INSTR];
uint8_t    table_used[MAX_TABLES][MAX_TABLELEN + 1];
TableError table_error;

namespace {

constexpr int k_max_bytes_per_row = 16;

constexpr int CAUSE_NONE       = 0;
constexpr int CAUSE_PATTERN    = 1;
constexpr int CAUSE_INSTRUMENT = 2;
constexpr int CAUSE_WAVECMD    = 3;

const char* tableleftname[] = {
    "mt_wavetbl",
    "mt_pulsetimetbl",
    "mt_filttimetbl",
    "mt_speedlefttbl",
};

const char* tablerightname[] = {
    "mt_notetbl",
    "mt_pulsespdtbl",
    "mt_filtspdtbl",
    "mt_speedrighttbl",
};

uint8_t chnused[MAX_CHN];
uint8_t pattmap[MAX_PATT];
uint8_t instrmap[MAX_INSTR];
uint8_t tablemap[MAX_TABLES][MAX_TABLELEN + 1];
int     pattoffset[MAX_PATT];
int     pattsize[MAX_PATT];
int     songoffset[MAX_SONGS][MAX_CHN];
int     songsize[MAX_SONGS][MAX_CHN];
int     channels;
int     fixedparams;
int     simplepulse;
int     firstnote;
int     lastnote;
int     patternlastnote;
int     nofilter;
int     nofiltermod;
int     nopulse;
int     nopulsemod;
int     nowavedelay;
int     norepeat;
int     notrans;
int     noportamento;
int     notoneporta;
int     novib;
int     noinsvib;
int     nosetad;
int     nosetsr;
int     nosetwave;
int     nosetwaveptr;
int     nosetpulseptr;
int     nosetfiltptr;
int     nosetfiltcutoff;
int     nosetfiltctrl;
int     nosetmastervol;
int     nofunktempo;
int     noglobaltempo;
int     nochanneltempo;
int     nogate;
int     noeffects;
int     nowavecmd;
int     nofirstwavecmd;
int     nocalculatedspeed;
int     nonormalspeed;
int     nozerospeed;
int     sidPlayAddr;

membuf src  = STATIC_MEMBUF_INIT;
membuf dest = STATIC_MEMBUF_INIT;

void reloc_alert(const char* msg) {
#ifdef GT2RELOC
    fputs(msg, STDERR);
    fputc('\n', STDERR);
#else
    gt_ui_error(msg);
#endif
}

void insert_text(std::string_view text) { membuf_append(&src, text.data(), static_cast<int>(text.size())); }

void insert_define(const char* name, int value) {
    char insertbuffer[96];
    snprintf(insertbuffer, sizeof insertbuffer, "%-16s = %d\n", name, value);
    insert_text(insertbuffer);
}

void insert_label(const char* name) {
    char insertbuffer[MAX_PATHNAME + 8];
    snprintf(insertbuffer, sizeof insertbuffer, "%s:\n", name);
    insert_text(insertbuffer);
}

void insert_bytes(const uint8_t* bytes, int size) {
    char insertbuffer[80];
    int  row = 0;

    while (size--) {
        if (!row) {
            insert_text("                .BYTE (");
            snprintf(insertbuffer, sizeof insertbuffer, "$%02x", *bytes);
            insert_text(insertbuffer);
            bytes++;
            row++;
        }
        else {
            snprintf(insertbuffer, sizeof insertbuffer, ",$%02x", *bytes);
            insert_text(insertbuffer);
            bytes++;
            row++;
            if (row == k_max_bytes_per_row) {
                insert_text(")\n");
                row = 0;
            }
        }
    }
    if (row) insert_text(")\n");
}

void insert_byte(uint8_t byte) {
    char insertbuffer[80];
    snprintf(insertbuffer, sizeof insertbuffer, "                .BYTE ($%02x)\n", byte);
    insert_text(insertbuffer);
}

void insert_addr_lo(const char* name) {
    char insertbuffer[MAX_PATHNAME + 32];
    snprintf(insertbuffer, sizeof insertbuffer, "                .BYTE (%s %% 256)\n", name);
    insert_text(insertbuffer);
}

void insert_addr_hi(const char* name) {
    char insertbuffer[MAX_PATHNAME + 32];
    snprintf(insertbuffer, sizeof insertbuffer, "                .BYTE (%s / 256)\n", name);
    insert_text(insertbuffer);
}

constexpr uint8_t swap_nybbles(uint8_t n) { return static_cast<uint8_t>(((n & 0xf) << 4) | (n >> 4)); }

void calc_speed_test(uint8_t pos) {
    if (!pos) {
        nozerospeed = 0;
        return;
    }
    if (ltable[STBL][pos - 1] >= 0x80) nocalculatedspeed = 0;
    else nonormalspeed = 0;
}

bool is_used_and_selfcontained(int num, int start) {
    int len = gettablepartlen(num, start - 1);
    int end = start + len - 1;

    // Don't use jumps only
    if (len == 1) return false;

    // Check that whole table is used
    for (int c = start; c <= end; c++) {
        if (table_used[num][c] == 0) return false;
    }
    // Check for jump to outside
    if (rtable[num][end - 1] != 0) {
        if ((rtable[num][end - 1] < start) || (rtable[num][end - 1] > end)) return false;
    }
    // Check for jump from outside
    for (int c = 1; c < start; c++)
        if ((table_used[num][c]) && (ltable[num][c - 1] == 0xff) && (rtable[num][c - 1] >= start) &&
            (rtable[num][c - 1] <= end))
            return false;
    for (int c = end + 1; c <= MAX_TABLELEN; c++)
        if ((table_used[num][c]) && (ltable[num][c - 1] == 0xff) && (rtable[num][c - 1] >= start) &&
            (rtable[num][c - 1] <= end))
            return false;

    return true;
}

void find_table_duplicates(int num) {
    int c, d, e;

    if (num == STBL) {
        for (c = 1; c <= MAX_TABLELEN; c++) {
            if (table_used[num][c]) {
                for (d = c + 1; d <= MAX_TABLELEN; d++) {
                    if (table_used[num][d]) {
                        if ((ltable[num][d - 1] == ltable[num][c - 1]) &&
                            (rtable[num][d - 1] == rtable[num][c - 1])) {
                            // Duplicate found, remove and map to the original
                            table_used[num][d] = 0;
                            for (e = d; e <= MAX_TABLELEN; e++)
                                if (table_used[num][e]) tablemap[num][e]--;
                            tablemap[num][d] = tablemap[num][c];
                        }
                    }
                }
            }
        }
    }
    else {
        for (c = 1; c <= MAX_TABLELEN; c++) {
            if (is_used_and_selfcontained(num, c)) {
                for (d = c + gettablepartlen(num, c - 1); d <= MAX_TABLELEN;) {
                    int len = gettablepartlen(num, d - 1);

                    if (is_used_and_selfcontained(num, d)) {
                        for (e = 0; e < len; e++) {
                            if (e < len - 1) {
                                // Is table data the same?
                                if ((ltable[num][d + e - 1] != ltable[num][c + e - 1]) ||
                                    (rtable[num][d + e - 1] != rtable[num][c + e - 1]))
                                    break;
                            }
                            else {
                                // Do both parts have a jump in the end?
                                if (ltable[num][d + e - 1] != ltable[num][c + e - 1]) break;
                                // Do both parts end?
                                if (rtable[num][d + e - 1] == 0) {
                                    if (rtable[num][c + e - 1] != 0) break;
                                }
                                else {
                                    // Do both parts loop in the same way?
                                    if ((rtable[num][d + e - 1] - d) != (rtable[num][c + e - 1] - c)) break;
                                }
                            }
                        }
                        if (e == len) {
                            // Duplicate found, remove and map to the original
                            for (e = 0; e < len; e++) table_used[num][d + e] = 0;
                            for (e = d; e < MAX_TABLELEN; e++)
                                if (table_used[num][e]) tablemap[num][e] -= len;
                            for (e = 0; e < len; e++) tablemap[num][d + e] = tablemap[num][c + e];
                        }
                    }
                    d += len;
                }
            }
        }
    }
}

int pack_pattern(uint8_t* dest, uint8_t* src, int rows) {
    uint8_t temp1[MAX_PATTROWS * 4];
    uint8_t temp2[512];
    uint8_t instr      = 0;
    int     command    = -1;
    int     databyte   = -1;
    int     destsizeim = 0;
    int     destsize   = 0;
    int     c, d;

    // First optimize instrument changes
    for (c = 0; c < rows; c++) {
        if ((c) && (src[c * 4 + 1]) && (src[c * 4 + 1] == instr)) {
            temp1[c * 4]     = src[c * 4];
            temp1[c * 4 + 1] = 0;
            temp1[c * 4 + 2] = src[c * 4 + 2];
            temp1[c * 4 + 3] = src[c * 4 + 3];
        }
        else {
            temp1[c * 4]     = src[c * 4];
            temp1[c * 4 + 1] = src[c * 4 + 1];
            temp1[c * 4 + 2] = src[c * 4 + 2];
            temp1[c * 4 + 3] = src[c * 4 + 3];
            if (src[c * 4 + 1]) instr = src[c * 4 + 1];
        }

        switch (temp1[c * 4 + 2]) {
            // Remap speedtable commands
        case CMD_PORTAUP:
        case CMD_PORTADOWN:
            noportamento     = 0;
            temp1[c * 4 + 3] = tablemap[STBL][temp1[c * 4 + 3]];
            break;

        case CMD_TONEPORTA:
            notoneporta      = 0;
            temp1[c * 4 + 3] = tablemap[STBL][temp1[c * 4 + 3]];
            break;

        case CMD_VIBRATO:
            novib            = 0;
            temp1[c * 4 + 3] = tablemap[STBL][temp1[c * 4 + 3]];
            break;

        case CMD_SETAD: nosetad = 0; break;

        case CMD_SETSR: nosetsr = 0; break;

        case CMD_SETWAVE:
            nosetwave = 0;
            break;

            // Remap table commands
        case CMD_SETWAVEPTR:
            nosetwaveptr     = 0;
            temp1[c * 4 + 3] = tablemap[WTBL][temp1[c * 4 + 3]];
            break;

        case CMD_SETPULSEPTR:
            nosetpulseptr    = 0;
            nopulse          = 0;
            temp1[c * 4 + 3] = tablemap[PTBL][temp1[c * 4 + 3]];
            break;

        case CMD_SETFILTERPTR:
            nosetfiltptr     = 0;
            nofilter         = 0;
            temp1[c * 4 + 3] = tablemap[FTBL][temp1[c * 4 + 3]];
            break;

        case CMD_SETFILTERCTRL:
            nosetfiltctrl = 0;
            nofilter      = 0;
            break;

        case CMD_SETFILTERCUTOFF:
            nosetfiltcutoff = 0;
            nofilter        = 0;
            break;

        case CMD_SETMASTERVOL:
            nosetmastervol = 0;
            // If no authorinfo being saved, erase timingmarks (not supported)
            if (!(playerversion & player_feature::author_info)) {
                if (temp1[c * 4 + 3] > 0x0f) {
                    temp1[c * 4 + 2] = 0;
                    temp1[c * 4 + 3] = 0;
                }
            }
            break;

        case CMD_FUNKTEMPO:
            nofunktempo      = 0;
            temp1[c * 4 + 3] = tablemap[STBL][temp1[c * 4 + 3]];
            break;

        case CMD_SETTEMPO:
            if (temp1[c * 4 + 3] >= 0x80) nochanneltempo = 0;
            else noglobaltempo = 0;
            // Decrease databyte of all tempo commands for the playroutine
            // Do not touch funktempo
            if ((temp1[c * 4 + 3] & 0x7f) >= 3) temp1[c * 4 + 3]--;
            break;
        }
    }

    if (noeffects) {
        command  = 0;
        databyte = 0;
    }

    uint8_t lastNote = REST;
    // Write in playroutine format
    for (c = 0; c < rows; c++) {
        uint8_t newNote = temp1[c * 4];
        // Instrument change with mapping
        if (temp1[c * 4 + 1]) {
            temp2[destsizeim++] = instrmap[INSTRCHG + temp1[c * 4 + 1]];
        }
        int foundRest = 0;
        // Rest+FX
        if (temp1[c * 4] == REST) {
            if (SIDTracker64ForIPadIsAmazing != 0 &&
                (c == 0 || (lastNote != REST && lastNote != KEYOFF))) // JP - Force a key off
                temp1[c * 4] = KEYOFF;
            else {
                foundRest = 1;

                if ((temp1[c * 4 + 2] != command) || (temp1[c * 4 + 3] != databyte)) {
                    command             = temp1[c * 4 + 2];
                    databyte            = temp1[c * 4 + 3];
                    temp2[destsizeim++] = FXONLY + command;
                    if (command) temp2[destsizeim++] = databyte;
                }
                else temp2[destsizeim++] = REST;
            }
        }
        if (foundRest == 0) {
            // Normal note
            if ((temp1[c * 4 + 2] != command) || (temp1[c * 4 + 3] != databyte)) {
                command             = temp1[c * 4 + 2];
                databyte            = temp1[c * 4 + 3];
                temp2[destsizeim++] = FX + command;
                if (command) temp2[destsizeim++] = databyte;
            }
            temp2[destsizeim++] = temp1[c * 4];
        }
        lastNote = newNote;
    }

    if (SIDTracker64ForIPadIsAmazing != 0) {

        // SPECIAL VERSION THAT ALSO COMPRESSES KEY_ONS - ALLOWING FOR SIDTRACKER STYLE USE OF KEYON TO SET NOTE
        // LENGTHS Final step: optimize long singlebyte rests with "packed rest"
        for (c = 0; c < destsizeim;) {
            int packok = 1;

            // Never pack first row or sequencer goes crazy
            if (!c) packok = 0;

            // There must be no instrument or command changes on the row to be packed
            if (temp2[c] < FX) {
                dest[destsize++] = temp2[c++]; // instrument
                packok           = 0;
            }
            if ((temp2[c] >= FXONLY) && (temp2[c] < FIRSTNOTE)) {
                int fxnum        = temp2[c] - FXONLY; // REST, FX + Optional COMMAND
                dest[destsize++] = temp2[c++];
                if (fxnum) dest[destsize++] = temp2[c++];
                packok = 0;
                goto NEXTROW;
            }
            if (temp2[c] < FXONLY) {
                int fxnum        = temp2[c] - FX; // NOTE, FX + Optional COMMAND
                dest[destsize++] = temp2[c++];
                if (fxnum) dest[destsize++] = temp2[c++];
                packok = 0;
            }

            if (temp2[c] != REST && temp2[c] != KEYON) packok = 0;

            if (!packok) dest[destsize++] = temp2[c++];
            else {
                uint8_t toFind = REST;
                if (temp2[c] == KEYON) toFind = KEYON;

                for (d = c; d < destsizeim;) {
                    if (temp2[d] == toFind) {
                        d++;
                        if (d - c == 64) break;
                    }
                    else break;
                }
                d -= c;
                if (d > 1) {
                    dest[destsize++] = -d;
                    c += d;
                }
                else dest[destsize++] = temp2[c++];
            }
        NEXTROW: {}
        }
    }
    else {
        // Final step: optimize long singlebyte rests with "packed rest"
        for (c = 0; c < destsizeim;) {
            int packok = 1;

            // Never pack first row or sequencer goes crazy
            if (!c) packok = 0;

            // There must be no instrument or command changes on the row to be packed
            if (temp2[c] < FX) {
                dest[destsize++] = temp2[c++]; // instrument
                packok           = 0;
            }
            if ((temp2[c] >= FXONLY) && (temp2[c] < FIRSTNOTE)) {
                int fxnum        = temp2[c] - FXONLY; // REST, FX + Optional COMMAND
                dest[destsize++] = temp2[c++];
                if (fxnum) dest[destsize++] = temp2[c++];
                packok = 0;
                goto NEXTROW2;
            }
            if (temp2[c] < FXONLY) {
                int fxnum        = temp2[c] - FX; // NOTE, FX + Optional COMMAND
                dest[destsize++] = temp2[c++];
                if (fxnum) dest[destsize++] = temp2[c++];
                packok = 0;
            }

            if (temp2[c] != REST) packok = 0;

            if (!packok) dest[destsize++] = temp2[c++];
            else {
                for (d = c; d < destsizeim;) {
                    if (temp2[d] == REST) {
                        d++;
                        if (d - c == 64) break;
                    }
                    else break;
                }
                d -= c;
                if (d > 1) {
                    dest[destsize++] = -d;
                    c += d;
                }
                else dest[destsize++] = temp2[c++];
            }
        NEXTROW2: {}
        }
    }
    // See if pattern too big
    if (destsize > 256) return -1;

    // If less than 256 bytes, insert endmark
    if (destsize < 256) dest[destsize++] = 0x00;

    return destsize;
}

} // namespace

void relocator(GTOBJECT* gt, bool gt2reloc_mode) {
    // Hoisted so the many `goto PRCLEANUP` statements do not jump across these
    // initializations (ill-formed in C++, harmless in C).
    int sds       = 0;
    int jpA000Fix = 0;
    int doAgain   = 0;

    //	char *tempFirstSIDBuffer;		// Used for 9 channel SID creation
    //	int tempSecondSIDOffset;

    uint8_t*  packeddata = nullptr;
    embed::Id player_id  = embed::Id::player;

    TableError tableerrortype    = TableError::None;
    int        tableerrorcause   = CAUSE_NONE;
    int        tableerrorsource1 = 0;
    int        tableerrorsource2 = 0;
    int        patterns          = 0;
    int        songs             = 0;
    int        instruments       = 0;
    int        numlegato         = 0;
    int        numnohr           = 0;
    int        numnormal         = 0;
    int        freenormal;
    int        freenohr;
    int        freelegato;
    int        transuprange   = 0;
    int        transdownrange = 0;
    int        pattdatasize   = 0;
    int        patttblsize    = 0;
    int        songdatasize   = 0;
    int        songtblsize    = 0;
    int        instrsize      = 0;
    int        wavetblsize    = 0;
    int        pulsetblsize   = 0;
    int        filttblsize    = 0;
    int        speedtblsize   = 0;
#ifdef GT2RELOC
    int playersize = 0;
#endif
    int     packedsize  = 0;
    FILE*   songhandle  = nullptr;
    uint8_t speedcode[] = { 0xa2, 0x00, 0x8e, 0x04, 0xdc, 0xa2, 0x00, 0x8e, 0x05, 0xdc };
    int     c, d, e;
    uint8_t patttemp[512];
    // Declared early so `goto PRCLEANUP` does not jump over non-trivial ctors.
    std::vector<uint8_t> songwork;
    std::vector<uint8_t> pattwork;
    std::vector<uint8_t> instrwork;

    channels          = editorInfo.maxSIDChannels;
    fixedparams       = 1;
    simplepulse       = 1;
    firstnote         = MAX_NOTES - 1;
    lastnote          = 0;
    patternlastnote   = 0;
    noeffects         = 1;
    nogate            = 1;
    nofilter          = 1;
    nofiltermod       = 1;
    nopulse           = 1;
    nopulsemod        = 1;
    nowavedelay       = 1;
    nowavecmd         = 1;
    norepeat          = 1;
    notrans           = 1;
    noportamento      = 1;
    notoneporta       = 1;
    novib             = 1;
    noinsvib          = 1;
    nosetad           = 1;
    nosetsr           = 1;
    nosetwave         = 1;
    nosetwaveptr      = 1;
    nosetpulseptr     = 1;
    nosetfiltptr      = 1;
    nosetfiltcutoff   = 1;
    nosetfiltctrl     = 1;
    nosetmastervol    = 1;
    nofunktempo       = 1;
    noglobaltempo     = 1;
    nochanneltempo    = 1;
    nofirstwavecmd    = 1;
    nocalculatedspeed = 1;
    nonormalspeed     = 1;
    nozerospeed       = 1;

    // Set default SID chip addresses. SidAddr2 is read from cfg file. By default, put SIDs 3+4 0x20 and 0x40 after
    // this.

    initRemapArrays();
    patternRemapOrderIndex = 0;
    for (int i = 0; i < MAX_SONGS; i++) {
        playUntilEnd2(i); // run through all songs to create pattern map in order of playback
    }

    if (!gt2reloc_mode) {
        if (gt->songinit != PlayMode::Stopped) {
            stopsong(gt);
        }
    }

    songhandle = nullptr;

    memset(patt_used, 0, sizeof patt_used);
    memset(instr_used, 0, sizeof instr_used);
    memset(chnused, 0, sizeof chnused);
    memset(table_used, 0, sizeof table_used);
    memset(tablemap, 0, sizeof tablemap);
    table_error = TableError::None;

    // Moved to handle data in 0xa000 region issue
    //	membuf_free(&src);
    //	membuf_free(&dest);

    fixedparams       = 1;
    simplepulse       = 1;
    firstnote         = MAX_NOTES - 1;
    lastnote          = 0;
    patternlastnote   = 0;
    noeffects         = 1;
    nogate            = 1;
    nofilter          = 1;
    nofiltermod       = 1;
    nopulse           = 1;
    nopulsemod        = 1;
    nowavedelay       = 1;
    nowavecmd         = 1;
    norepeat          = 1;
    notrans           = 1;
    noportamento      = 1;
    notoneporta       = 1;
    novib             = 1;
    noinsvib          = 1;
    nosetad           = 1;
    nosetsr           = 1;
    nosetwave         = 1;
    nosetwaveptr      = 1;
    nosetpulseptr     = 1;
    nosetfiltptr      = 1;
    nosetfiltcutoff   = 1;
    nosetfiltctrl     = 1;
    nosetmastervol    = 1;
    nofunktempo       = 1;
    noglobaltempo     = 1;
    nochanneltempo    = 1;
    nofirstwavecmd    = 1;
    nocalculatedspeed = 1;
    nonormalspeed     = 1;
    nozerospeed       = 1;

    // Process song-orderlists
    countpatternlengths();
    // Calculate amount of songs with nonzero length
    for (c = 0; c < MAX_SONGS; c++) {
        // JP - Fix 3SID Export V1.2.3
        int oddEvenSubSong = c & 1;
        //-----

        if ((songlen[c][0]) && (songlen[c][1]) && (songlen[c][2])) {
            // See which patterns are used in the whole .sng file
            for (d = 0; d < MAX_CHN; d++) {
                if (editorInfo.maxSIDChannels == 9 && oddEvenSubSong == 1 && d >= 3) break;
                if (editorInfo.maxSIDChannels == 3 && d >= 3) break;

                songdatasize += songlen[c][d] + 2;
                for (e = 0; e < songlen[c][d]; e++) {
                    if (songorder[c][d][e] < REPEAT) {
                        int f;
                        int num = songorder[c][d][e];

                        patt_used[num] = 1;
                        for (f = 0; f < pattlen[num]; f++) {
                            if ((pattern[num][f * 4] != REST) || (pattern[num][f * 4 + 1]) ||
                                (pattern[num][f * 4 + 2]))
                                chnused[d] = 1; // JP - Copied from 3 channel version
                        }
                    }
                    else {
                        if (songorder[c][d][e] >= TRANSDOWN) {
                            notrans = 0;
                            if (songorder[c][d][e] < TRANSUP) {
                                int newtransdownrange = -(songorder[c][d][e] - TRANSUP);
                                if (newtransdownrange > transdownrange) transdownrange = newtransdownrange;
                            }
                            else {
                                int newtransuprange = songorder[c][d][e] - TRANSUP;
                                if (newtransuprange > transuprange) transuprange = newtransuprange;
                            }
                        }
                        else norepeat = 0;
                    }
                }
                if (songorder[c][d][songlen[c][d] + 1] >= songlen[c][d]) {
                    sprintf(textbuffer,
                            "ILLEGAL SONG RESTART POSITION! (SUBTUNE %02X, CHANNEL %d ODDEVEN %d)",
                            c,
                            d + 1,
                            oddEvenSubSong);
                    reloc_alert(textbuffer);
                    goto PRCLEANUP;
                }
            }
            songs++;
        }
    }

    // Optimize amount of used channels
    if (editorInfo.maxSIDChannels == 3) // JP - Copied from 3 channel version
    {
        if (!chnused[2]) channels = 2;
        if ((!chnused[1]) && (!chnused[2])) channels = 1;
    }

    if (!songs) {
        reloc_alert("NO SONGS, NO DATA TO SAVE!");
        goto PRCLEANUP;
    }

    // Build the pattern-mapping
    // Instrument 1 is always used
    instr_used[1] = 1;
    for (c = 0; c < MAX_PATT; c++) {
        if (patt_used[c]) {
            pattmap[c] = patternOrderArray[c]; // patterns;
            // printf("pattern %x = new pattern %x\n", c, pattmap[c]);
            patterns++;

            // See which instruments/tablecommands are used
            for (d = 0; d < pattlen[c]; d++) {
                table_error = TableError::None;

                if ((pattern[c][d * 4] == KEYOFF) || (pattern[c][d * 4] == KEYON)) nogate = 0;
                if (pattern[c][d * 4 + 1]) instr_used[pattern[c][d * 4 + 1]] = 1;
                if (pattern[c][d * 4 + 2]) noeffects = 0;
                if ((pattern[c][d * 4 + 2] >= CMD_SETWAVEPTR) && (pattern[c][d * 4 + 2] <= CMD_SETFILTERPTR))
                    exectable(pattern[c][d * 4 + 2] - CMD_SETWAVEPTR, pattern[c][d * 4 + 3]);
                if ((pattern[c][d * 4 + 2] >= CMD_PORTAUP) && (pattern[c][d * 4 + 2] <= CMD_VIBRATO)) {
                    exectable(STBL, pattern[c][d * 4 + 3]);
                    calc_speed_test(pattern[c][d * 4 + 3]);
                }
                if (pattern[c][d * 4 + 2] == CMD_FUNKTEMPO) exectable(STBL, pattern[c][d * 4 + 3]);
                if (pattern[c][d * 4 + 2] == CMD_FUNKTEMPO) {
                    nofunktempo   = 0;
                    noglobaltempo = 0;
                }
                if ((pattern[c][d * 4 + 2] == CMD_SETTEMPO) && ((pattern[c][d * 4 + 3] & 0x7f) < 3))
                    nofunktempo = 0;

                // See, which are the highest/lowest notes used
                if ((pattern[c][d * 4] >= FIRSTNOTE) && (pattern[c][d * 4] <= LASTNOTE)) {
                    int newfirstnote = pattern[c][d * 4] - FIRSTNOTE - transdownrange;
                    int newlastnote  = pattern[c][d * 4] - FIRSTNOTE + transuprange;
                    if (newfirstnote < 0) newfirstnote = 0;
                    if (newlastnote > MAX_NOTES - 1) newlastnote = MAX_NOTES - 1;

                    if (newfirstnote < firstnote) firstnote = newfirstnote;
                    if (newlastnote > lastnote) {
                        patternlastnote = newlastnote;
                        lastnote        = newlastnote;
                    }
                    if (newfirstnote > lastnote) {
                        patternlastnote = newfirstnote;
                        lastnote        = newfirstnote;
                    }
                }
                if (table_error != TableError::None && tableerrortype == TableError::None) {
                    tableerrortype    = table_error;
                    tableerrorcause   = CAUSE_PATTERN;
                    tableerrorsource1 = c;
                    tableerrorsource2 = d;
                }
            }
        }
    }

    // Count amount of normal, nohr, and legato instruments
    // Also see if special first wave parameters are used
    for (c = 0; c < MAX_INSTR; c++) {
        if (instr_used[c]) {
            if (instr[c].gatetimer & 0x40) numlegato++;
            else {
                if (instr[c].gatetimer & 0x80) numnohr++;
                else numnormal++;
            }
            if ((!instr[c].firstwave) || (instr[c].firstwave >= 0xfe)) nofirstwavecmd = 0;
        }
    }

    freenormal = 1;
    freenohr   = freenormal + numnormal;
    freelegato = freenohr + numnohr;

    // Build the instrument-mapping
    for (c = 0; c < MAX_INSTR; c++) {
        if (instr_used[c]) {
            if (instr[c].gatetimer & 0x40) instrmap[c] = freelegato++;
            else {
                if (instr[c].gatetimer & 0x80) instrmap[c] = freenohr++;
                else instrmap[c] = freenormal++;
            }
            instruments++;
            for (d = 0; d < MAX_TABLES; d++) {
                table_error = TableError::None;
                exectable(d, instr[c].ptr[d]);
                if (d == STBL) calc_speed_test(instr[c].ptr[d]);
                if (table_error != TableError::None && tableerrortype == TableError::None) {
                    tableerrortype    = table_error;
                    tableerrorcause   = CAUSE_INSTRUMENT;
                    tableerrorsource1 = c;
                    tableerrorsource2 = d;
                }
            }
        }
    }

    // Execute tableprograms invoked from wavetable commands
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[WTBL][c + 1]) {
            if ((ltable[WTBL][c] >= WAVECMD) && (ltable[WTBL][c] <= WAVELASTCMD)) {
                d           = -1;
                table_error = TableError::None;

                switch (ltable[WTBL][c] - WAVECMD) {
                case CMD_PORTAUP:
                case CMD_PORTADOWN:
                case CMD_TONEPORTA:
                case CMD_VIBRATO:
                    d = STBL;
                    calc_speed_test(rtable[WTBL][c]);
                    break;

                case CMD_SETPULSEPTR:
                    d       = PTBL;
                    nopulse = 0;
                    break;

                case CMD_SETFILTERPTR:
                    d        = FTBL;
                    nofilter = 0;
                    break;

                case CMD_DONOTHING:
                case CMD_SETWAVEPTR:
                case CMD_FUNKTEMPO:
                    sprintf(textbuffer,
                            "ILLEGAL WAVETABLE COMMAND (ROW %02X, COMMAND %X)",
                            c + 1,
                            ltable[WTBL][c] - WAVECMD);
                    reloc_alert(textbuffer);
                    goto PRCLEANUP;
                }

                if (d != -1) exectable(d, rtable[WTBL][c]);

                if (table_error != TableError::None && tableerrortype == TableError::None) {
                    tableerrortype    = table_error;
                    tableerrorcause   = CAUSE_WAVECMD;
                    tableerrorsource1 = c + 1;
                    tableerrorsource2 = d;
                }
            }
        }
    }

    // Build the table-mapping
    for (c = 0; c < MAX_TABLES; c++) {
        int e = 1;
        for (d = 0; d < MAX_TABLELEN; d++) {
            if (table_used[c][d + 1]) {
                tablemap[c][d + 1] = e;
                e++;
            }
        }
    }

    // Check for table errors
    if (tableerrorcause) {
        switch (tableerrortype) {
        case TableError::Jump: sprintf(textbuffer, "TABLE POINTER POINTS TO A JUMP! "); break;

        case TableError::Overflow: sprintf(textbuffer, "TABLE EXECUTION OVERFLOWS! "); break;
        }
        switch (tableerrorcause) {
        case CAUSE_PATTERN:
            sprintf(textbuffer + strlen(textbuffer),
                    "(PATTERN %02X, ROW %02d)",
                    tableerrorsource1,
                    tableerrorsource2);
            break;

        case CAUSE_WAVECMD:
            sprintf(textbuffer + strlen(textbuffer), "WAVETABLE CMD (ROW %02X, ", tableerrorsource1);
            goto TABLETYPE;

        case CAUSE_INSTRUMENT:
            sprintf(textbuffer + strlen(textbuffer), "(INSTRUMENT %02X, ", tableerrorsource1);
        TABLETYPE: {
            const char* kind = "";
            switch (tableerrorsource2) {
            case WTBL: kind = "WAVE"; break;
            case PTBL: kind = "PULSE"; break;
            case FTBL: kind = "FILTER"; break;
            }
            const size_t len = strlen(textbuffer);
            snprintf(textbuffer + len, sizeof(textbuffer) - len, "%s)", kind);
            break;
        }
        }
        reloc_alert(textbuffer);
        goto PRCLEANUP;
    }

    // Find duplicate ranges in tables
    for (c = 0; c < MAX_TABLES; c++) find_table_duplicates(c);

    // Disable optimizations if necessary
    if (playerversion & player_feature::no_optimization) {
        fixedparams = 0;
        if (!numlegato) numlegato++;

        simplepulse       = 0;
        firstnote         = 0;
        lastnote          = MAX_NOTES - 1;
        nogate            = 0;
        noeffects         = 0;
        nofilter          = 0;
        nofiltermod       = 0;
        nopulse           = 0;
        nopulsemod        = 0;
        nowavedelay       = 0;
        nowavecmd         = 0;
        norepeat          = 0;
        notrans           = 0;
        noportamento      = 0;
        notoneporta       = 0;
        novib             = 0;
        noinsvib          = 0;
        nosetad           = 0;
        nosetsr           = 0;
        nosetwave         = 0;
        nosetwaveptr      = 0;
        nosetpulseptr     = 0;
        nosetfiltptr      = 0;
        nosetfiltcutoff   = 0;
        nosetfiltctrl     = 0;
        nosetmastervol    = 0;
        nofunktempo       = 0;
        noglobaltempo     = 0;
        nochanneltempo    = 0;
        nofirstwavecmd    = 0;
        nocalculatedspeed = 0;
        nonormalspeed     = 0;
        nozerospeed       = 0;
    }

    // Make sure buffering is used if it is needed
    if ((playerversion & player_feature::sound_effects) || (playerversion & player_feature::zp_ghost_regs))
        playerversion |= player_feature::buffered;

    if (editorInfo.maxSIDChannels == 3) {
        // Sound effect or ghostreg players always use full 3 channels
        if ((playerversion & player_feature::sound_effects) || (playerversion & player_feature::full_buffered) ||
            (playerversion & player_feature::zp_ghost_regs))
            channels = 3;
    }

    // Allocate memory for song-orderlists

    if (editorInfo.maxSIDChannels >= 9)
        songtblsize = ((songs + 1) / 2) * editorInfo.maxSIDChannels; // 9 or 12 channels. Half the number of songs
    else songtblsize = songs * editorInfo.maxSIDChannels;            // 3 or 6 channels

        //----------------
#ifdef DISPLAY_FREE_MEM
    reloc_alert(textbuffer);
#endif
    //-----------------

    try {
        songwork.resize(static_cast<size_t>(songdatasize));
    }
    catch (const std::bad_alloc&) {
        reloc_alert(textbuffer);
        goto PRCLEANUP;
    }

    // Generate songorderlists & songtable
    // songdatasize = 0;

    sds = 0;

    for (c = 0; c < songs; c++) {
        int oddEvenSubSong = c & 1;

        if ((songlen[c][0]) && (songlen[c][1]) && (songlen[c][2])) {
            for (d = 0; d < MAX_CHN; d++) {
                if (editorInfo.maxSIDChannels == 9 && oddEvenSubSong == 1 && d >= 3) {
                    songoffset[c][d] = sds;
                    songsize[c][d]   = 0;
                    continue;
                }

                if (editorInfo.maxSIDChannels == 3 && d >= 3) {
                    songoffset[c][d] = sds;
                    songsize[c][d]   = 0;
                    continue;
                }

                songoffset[c][d] = sds;
                songsize[c][d]   = songlen[c][d] + 2;

                for (e = 0; e < songlen[c][d]; e++) {
                    // Pattern
                    if (songorder[c][d][e] < REPEAT) songwork[sds++] = pattmap[songorder[c][d][e]];
                    else {
                        // Transpose
                        if (songorder[c][d][e] >= TRANSDOWN) {
                            songwork[sds++] = songorder[c][d][e];
                        }
                        // Repeat sequence: must be swapped
                        else {
                            // See that repeat amount is more than 1
                            if (songorder[c][d][e] > REPEAT) {
                                // Insanity check that a pattern indeed follows
                                if (songorder[c][d][e + 1] < REPEAT) {
                                    songwork[sds++] = pattmap[songorder[c][d][e + 1]];
                                    songwork[sds++] = songorder[c][d][e];
                                    e++;
                                }
                                else songwork[sds++] = songorder[c][d][e];
                            }
                        }
                    }
                }
                // Endmark & repeat position
                songwork[sds++] = songorder[c][d][e++];
                songwork[sds++] = songorder[c][d][e++];
            }
        }
        else {
            for (d = 0; d < MAX_CHN; d++) {
                songoffset[c][d] = sds;
                songsize[c][d]   = 0;
            }
        }
    }

    // Calculate total size of patterns
    for (c = 0; c < MAX_PATT; c++) {
        int d = patternOrderList[c];
        //		if (patt_used[c])
        if (d != -1) {
            // printf("Packing pattern %x\n", c);

            int result = pack_pattern(patttemp, pattern[d], pattlen[d]);

            if (result < 0) {
                reloc_alert(textbuffer);
                goto PRCLEANUP;
            }
            pattdatasize += result;
        }
    }

    //----------------
#ifdef DISPLAY_FREE_MEM
    reloc_alert(textbuffer);
#endif
    //-----------------

    patttblsize = patterns * 2;
    try {
        pattwork.resize(static_cast<size_t>(pattdatasize));
    }
    catch (const std::bad_alloc&) {
        reloc_alert(textbuffer);
        goto PRCLEANUP;
    }

    // This time pack the patterns for real
    pattdatasize = 0;
    d            = 0;
    for (c = 0; c < MAX_PATT; c++) {
        int e = patternOrderList[c];
        //		if (patt_used[c])
        if (e != -1) {
            pattoffset[d] = pattdatasize;
            pattsize[d]   = pack_pattern(pattwork.data() + pattdatasize, pattern[e], pattlen[e]);
            pattdatasize += pattsize[d];
            d++;
        }
    }

    // Then process instruments
    instrsize = instruments * 9;

    //----------------
#ifdef DISPLAY_FREE_MEM
    reloc_alert(textbuffer);
#endif
    //-----------------

    try {
        instrwork.resize(static_cast<size_t>(instrsize));
    }
    catch (const std::bad_alloc&) {
        reloc_alert(textbuffer);
        goto PRCLEANUP;
    }

    for (c = 1; c < MAX_INSTR; c++) {
        if (instr_used[c]) {
            d                              = instrmap[c] - 1;
            instrwork[d]                   = instr[c].ad;
            instrwork[d + instruments]     = instr[c].sr;
            instrwork[d + instruments * 2] = tablemap[WTBL][instr[c].ptr[WTBL]];
            instrwork[d + instruments * 3] = tablemap[PTBL][instr[c].ptr[PTBL]];
            instrwork[d + instruments * 4] = tablemap[FTBL][instr[c].ptr[FTBL]];
            if (instr[c].vibdelay) {
                instrwork[d + instruments * 5] = tablemap[STBL][instr[c].ptr[STBL]];
                instrwork[d + instruments * 6] = instr[c].vibdelay - 1;
            }
            else {
                instrwork[d + instruments * 5] = 0;
                instrwork[d + instruments * 6] = 0;
            }
            instrwork[d + instruments * 7] = instr[c].gatetimer & 0x3f;
            instrwork[d + instruments * 8] = instr[c].firstwave;

            if (instr[c].ptr[STBL]) {
                novib    = 0;
                noinsvib = 0;
            }
            if (instr[c].ptr[PTBL]) nopulse = 0;
            if (instr[c].ptr[FTBL]) nofilter = 0;

            // See if all instruments use same gatetimer & firstwave parameters
            if ((instr[c].gatetimer != instr[1].gatetimer) || (instr[c].firstwave != instr[1].firstwave))
                fixedparams = 0;
            // or if special firstwave commands are in use
            if ((!instr[c].firstwave) || (instr[c].firstwave >= 0xfe)) fixedparams = 0;
        }
    }

    // Disable sameparam optimization for multispeed stability
    if (editorInfo.multiplier > 1) {
        fixedparams = 0;
        numlegato++;
        numnohr++;
    }

    if (fixedparams) instrsize -= instruments * 2;
    if (noinsvib) instrsize -= instruments * 2;
    if (nopulse) instrsize -= instruments;
    if (nofilter) instrsize -= instruments;

    // Process tables
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[WTBL][c + 1]) {
            wavetblsize += 2;
            if ((ltable[WTBL][c] >= WAVEDELAY) && (ltable[WTBL][c] <= WAVELASTDELAY)) nowavedelay = 0;
            if ((ltable[WTBL][c] >= WAVECMD) && (ltable[WTBL][c] <= WAVELASTCMD)) {
                nowavecmd = 0;
                noeffects = 0;
                switch (ltable[WTBL][c] - WAVECMD) {
                case CMD_PORTAUP:
                case CMD_PORTADOWN: noportamento = 0; break;

                case CMD_TONEPORTA: notoneporta = 0; break;

                case CMD_VIBRATO: novib = 0; break;

                case CMD_SETAD: nosetad = 0; break;

                case CMD_SETSR: nosetsr = 0; break;

                case CMD_SETWAVE: nosetwave = 0; break;

                case CMD_SETPULSEPTR: nosetpulseptr = 0; break;

                case CMD_SETFILTERPTR: nosetfiltptr = 0; break;

                case CMD_SETFILTERCUTOFF: nosetfiltcutoff = 0; break;

                case CMD_SETFILTERCTRL: nosetfiltctrl = 0; break;

                case CMD_SETMASTERVOL: nosetmastervol = 0; break;
                }
            }
            if (ltable[WTBL][c] < WAVECMD) {
                if (rtable[WTBL][c] <= 0x80) {
                    int newlastnote = rtable[WTBL][c] + patternlastnote;
                    if (newlastnote > MAX_NOTES - 1) newlastnote = MAX_NOTES - 1;
                    if (rtable[WTBL][c] >= 0x20) firstnote = 0;
                    if (newlastnote > lastnote) lastnote = newlastnote;
                }
                else {
                    int newfirstnote = rtable[WTBL][c] & 0x7f;
                    int newlastnote  = rtable[WTBL][c] & 0x7f;
                    if (newlastnote > MAX_NOTES - 1) newlastnote = MAX_NOTES - 1;
                    if (newfirstnote < firstnote) firstnote = newfirstnote;
                    if (newlastnote > lastnote) lastnote = newlastnote;
                }
            }
        }
    }
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[PTBL][c + 1]) {
            pulsetblsize += 2;
            if ((ltable[PTBL][c] >= 0x80) && (ltable[PTBL][c] != 0xff)) {
                if (rtable[PTBL][c] & 0xf) simplepulse = 0;
            }
            if (ltable[PTBL][c] < 0x80) {
                nopulsemod = 0;
                if (rtable[PTBL][c] & 0xf) simplepulse = 0;
            }
        }
    }
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[FTBL][c + 1]) {
            filttblsize += 2;
            if (ltable[FTBL][c] < 0x80) nofiltermod = 0;
        }
    }
    for (c = 0; c < MAX_TABLELEN; c++) {
        if (table_used[STBL][c + 1]) speedtblsize += 2;
    }
    // Zero entry of speedtable
    if ((!novib) || (!nofunktempo) || (!noportamento) || (!notoneporta)) speedtblsize += 2;

    if (nopulse) pulsetblsize = 0;
    if (nofilter) filttblsize = 0;

    // Validate frequencytable parameters
    if (lastnote < firstnote) lastnote = firstnote;
    if (firstnote < 0) firstnote = 0;
    if (!nocalculatedspeed) lastnote++; // Calculated speeds need the next frequency value
    if (lastnote > MAX_NOTES - 1) lastnote = MAX_NOTES - 1;
    // For sound effect support, always use the full table
    if (playerversion & player_feature::sound_effects) {
        firstnote = 0;
        lastnote  = MAX_NOTES - 1;
    }

    jpA000Fix = 0;
    doAgain   = 0;
    do {
        membuf_free(&src);
        membuf_free(&dest);

        // Insert baseaddresses
        insert_define("base", playeradr);
        insert_define("zpbase", zeropageadr);

        insert_define("SIDBASE", sidAddr1);
        if (editorInfo.maxSIDChannels > 3) insert_define("SID2BASE", sidAddr2);
        if (editorInfo.maxSIDChannels > 6) insert_define("SID3BASE", sidAddr3);
        if (editorInfo.maxSIDChannels > 9) insert_define("SID4BASE", sidAddr4);

        // Insert conditionals

        //		insert_define("JPA000FIXDEF", jpA000Fix);
        insert_define("SOUNDSUPPORT", (playerversion & player_feature::sound_effects) ? 1 : 0);
        insert_define("VOLSUPPORT", (playerversion & player_feature::volume) ? 1 : 0);
        insert_define("BUFFEREDWRITES", (playerversion & player_feature::buffered) ? 1 : 0);
        insert_define("ZPGHOSTREGS", (playerversion & player_feature::zp_ghost_regs) ? 1 : 0);
        if (editorInfo.maxSIDChannels == 3)
            insert_define("GHOSTREGS",
                          (playerversion & (player_feature::zp_ghost_regs | player_feature::full_buffered)) ? 1
                                                                                                            : 0);
        insert_define("FIXEDPARAMS", fixedparams);
        insert_define("SIMPLEPULSE", simplepulse);
        insert_define("PULSEOPTIMIZATION", editorInfo.optimizepulse);
        insert_define("REALTIMEOPTIMIZATION", editorInfo.optimizerealtime);
        insert_define("NOAUTHORINFO", (playerversion & player_feature::author_info) ? 0 : 1);
        insert_define("ZPPLAYSID", (playerversion & player_feature::zp_play_sid) ? 1 : 0);
        insert_define("NOEFFECTS", noeffects);
        insert_define("NOGATE", nogate);
        insert_define("NOFILTER", nofilter);
        insert_define("NOFILTERMOD", nofiltermod);
        insert_define("NOPULSE", nopulse);
        insert_define("NOPULSEMOD", nopulsemod);
        insert_define("NOWAVEDELAY", nowavedelay);
        insert_define("NOWAVECMD", nowavecmd);
        insert_define("NOREPEAT", norepeat);
        insert_define("NOTRANS", notrans);
        insert_define("NOPORTAMENTO", noportamento);
        insert_define("NOTONEPORTA", notoneporta);
        insert_define("NOVIB", novib);
        insert_define("NOINSTRVIB", noinsvib);
        insert_define("NOSETAD", nosetad);
        insert_define("NOSETSR", nosetsr);
        insert_define("NOSETWAVE", nosetwave);
        insert_define("NOSETWAVEPTR", nosetwaveptr);
        insert_define("NOSETPULSEPTR", nosetpulseptr);
        insert_define("NOSETFILTPTR", nosetfiltptr);
        insert_define("NOSETFILTCTRL", nosetfiltctrl);
        insert_define("NOSETFILTCUTOFF", nosetfiltcutoff);
        insert_define("NOSETMASTERVOL", nosetmastervol);
        insert_define("NOFUNKTEMPO", nofunktempo);
        insert_define("NOGLOBALTEMPO", noglobaltempo);
        insert_define("NOCHANNELTEMPO", nochanneltempo);
        insert_define("NOFIRSTWAVECMD", nofirstwavecmd);
        insert_define("NOCALCULATEDSPEED", nocalculatedspeed);
        insert_define("NONORMALSPEED", nonormalspeed);
        insert_define("NOZEROSPEED", nozerospeed);

        // Insert parameters
        insert_define("NUMCHANNELS", channels);
        insert_define("NUMSONGS", songs);
        insert_define("FIRSTNOTE", firstnote);
        insert_define("FIRSTNOHRINSTR", numnormal + 1);
        insert_define("FIRSTLEGATOINSTR", numnormal + numnohr + 1);
        insert_define("NUMHRINSTR", numnormal);
        insert_define("NUMNOHRINSTR", numnohr);
        insert_define("NUMLEGATOINSTR", numlegato);
        insert_define("ADPARAM", editorInfo.adparam >> 8);
        insert_define("SRPARAM", editorInfo.adparam & 0xff);
        if ((instr[MAX_INSTR - 1].ad >= 2) && (!(instr[MAX_INSTR - 1].ptr[WTBL])))
            insert_define("DEFAULTTEMPO", instr[MAX_INSTR - 1].ad - 1);
        else insert_define("DEFAULTTEMPO", editorInfo.multiplier ? (editorInfo.multiplier * 6 - 1) : 5);

        // Fixed firstwave & gatetimer
        if (fixedparams) {
            insert_define("FIRSTWAVEPARAM", instr[1].firstwave);
            insert_define("GATETIMERPARAM", instr[1].gatetimer & 0x3f);
        }

        // Insert source code of player
        if (editorInfo.adparam >= 0xf000) {
            if (editorInfo.maxSIDChannels == 3) player_id = embed::Id::altplayer3;
            else if (editorInfo.maxSIDChannels == 9) player_id = embed::Id::altplayer9;
            else if (editorInfo.maxSIDChannels == 12) player_id = embed::Id::altplayer12;
            else player_id = embed::Id::altplayer; // 6-channel GT 6502
        }
        else {
            if (editorInfo.maxSIDChannels == 3) player_id = embed::Id::player3;
            else if (editorInfo.maxSIDChannels == 9) player_id = embed::Id::player9;
            else if (editorInfo.maxSIDChannels == 12) player_id = embed::Id::player12;
            else player_id = embed::Id::player; // 6-channel GT 6502
        }

        {
            const embed::Blob& player = embed::get(player_id);
            membuf_append(&src, player.data, static_cast<int>(player.size));
        }

        // JP Added this (copied from 3channel GoatTracker) 31st March 2022
        // Modify ghostregs to not be zeropage if needed
        //----
        if ((playerversion & player_feature::full_buffered) &&
            (playerversion & player_feature::zp_ghost_regs) == 0) {
            int   bufsize = membuf_get_size(&src);
            char* bufdata = (char*)membuf_get(&src);
            int   c;
            for (c = 0; c < bufsize; c++) {
                if (bufdata[c] == '<') {
                    if (memcmp(bufdata + c + 1, "ghost", 5) == 0) bufdata[c] = ' ';
                }
            }
        }
        //------

        // Insert frequencytable
        insert_label("mt_freqtbllo");
        insert_bytes(&freqtbllo[firstnote], lastnote - firstnote + 1);
        insert_label("mt_freqtblhi");
        insert_bytes(&freqtblhi[firstnote], lastnote - firstnote + 1);

        // Insert songtable
        insert_label("mt_songtbllo");

        int songSize = songs * editorInfo.maxSIDChannels;
        if (editorInfo.maxSIDChannels >= 9) songSize = ((songs + 1) / 2) * editorInfo.maxSIDChannels;

        //	sprintf(textbuffer,";JP: songs %d songsize %d\n", songs, songSize);
        //	insert_label(textbuffer);

        for (c = 0; c < songSize; c++) // * 6 JP
        {
            sprintf(textbuffer, "mt_song%d", c);
            insert_addr_lo(textbuffer);
        }
        insert_label("mt_songtblhi");
        for (c = 0; c < songSize; c++) {
            sprintf(textbuffer, "mt_song%d", c);
            insert_addr_hi(textbuffer);
        }

        // Insert patterntable
        insert_label("mt_patttbllo");
        for (c = 0; c < patterns; c++) {
            sprintf(textbuffer, "mt_patt%d", c);
            insert_addr_lo(textbuffer);
        }
        insert_label("mt_patttblhi");
        for (c = 0; c < patterns; c++) {
            sprintf(textbuffer, "mt_patt%d", c);
            insert_addr_hi(textbuffer);
        }

        // Insert instruments
        insert_label("mt_insad");
        insert_bytes(instrwork.data(), instruments);
        insert_label("mt_inssr");
        insert_bytes(&instrwork[instruments], instruments);
        insert_label("mt_inswaveptr");
        insert_bytes(&instrwork[instruments * 2], instruments);
        if (!nopulse) {
            insert_label("mt_inspulseptr");
            insert_bytes(&instrwork[instruments * 3], instruments);
        }
        if (!nofilter) {
            insert_label("mt_insfiltptr");
            insert_bytes(&instrwork[instruments * 4], instruments);
        }
        if (!noinsvib) {
            insert_label("mt_insvibparam");
            insert_bytes(&instrwork[instruments * 5], instruments);
            insert_label("mt_insvibdelay");
            insert_bytes(&instrwork[instruments * 6], instruments);
        }
        if (!fixedparams) {
            insert_label("mt_insgatetimer");
            insert_bytes(&instrwork[instruments * 7], instruments);
            insert_label("mt_insfirstwave");
            insert_bytes(&instrwork[instruments * 8], instruments);
        }

        // Insert tables
        for (c = 0; c < MAX_TABLES; c++) {
            if ((c == PTBL) && (nopulse)) goto SKIPTABLE;
            if ((c == FTBL) && (nofilter)) goto SKIPTABLE;

            // Write table left side
            // Extra zero for speedtable
            if ((c == STBL) && ((!novib) || (!nofunktempo) || (!noportamento) || (!notoneporta))) insert_byte(0);
            // Table label
            insert_label(tableleftname[c]);

            // Table data
            for (d = 0; d < MAX_TABLELEN; d++) {
                if (table_used[c][d + 1]) {
                    switch (c) {
                        // In wavetable, convert waveform values for the playroutine
                    case WTBL: {
                        uint8_t wave = ltable[c][d];
                        if ((ltable[c][d] >= WAVESILENT) && (ltable[c][d] <= WAVELASTSILENT)) wave &= 0xf;
                        if ((ltable[c][d] > WAVELASTDELAY) && (ltable[c][d] <= WAVELASTSILENT) && (!nowavedelay))
                            wave += 0x10;
                        insert_byte(wave);
                    } break;

                    case PTBL:
                        if ((simplepulse) && (ltable[c][d] != 0xff) && (ltable[c][d] > 0x80)) insert_byte(0x80);
                        else insert_byte(ltable[c][d]);
                        break;

                        // In filtertable, modify passband bits
                    case FTBL:
                        if ((ltable[c][d] != 0xff) && (ltable[c][d] > 0x80))
                            insert_byte(((ltable[c][d] & 0x70) >> 1) | 0x80);
                        else insert_byte(ltable[c][d]);
                        break;

                    default: insert_byte(ltable[c][d]); break;
                    }
                }
            }

            // Write table right side, remapping jumps as necessary
            // Extra zero for speedtable
            if ((c == STBL) && ((!novib) || (!nofunktempo) || (!noportamento) || (!notoneporta))) insert_byte(0);
            // Table label
            insert_label(tablerightname[c]);

            for (d = 0; d < MAX_TABLELEN; d++) {
                if (table_used[c][d + 1]) {
                    if ((ltable[c][d] != 0xff) || (c == STBL)) {
                        switch (c) {
                        case WTBL:
                            if ((ltable[c][d] >= WAVECMD) && (ltable[c][d] <= WAVELASTCMD)) {
                                // Remap table-referencing commands
                                switch (ltable[c][d] - WAVECMD) {
                                case CMD_PORTAUP:
                                case CMD_PORTADOWN:
                                case CMD_TONEPORTA:
                                case CMD_VIBRATO: insert_byte(tablemap[STBL][rtable[c][d]]); break;

                                case CMD_SETPULSEPTR: insert_byte(tablemap[PTBL][rtable[c][d]]); break;

                                case CMD_SETFILTERPTR: insert_byte(tablemap[FTBL][rtable[c][d]]); break;

                                default: insert_byte(rtable[c][d]); break;
                                }
                            }
                            else {
                                // For normal notes, reverse all right side high bits
                                insert_byte(rtable[c][d] ^ 0x80);
                            }
                            break;

                        case PTBL:
                            if (simplepulse) {
                                if (ltable[c][d] >= 0x80)
                                    insert_byte((ltable[c][d] & 0x0f) | (rtable[c][d] & 0xf0));
                                else {
                                    int pulsespeed = rtable[c][d] >> 4;
                                    if (rtable[c][d] & 0x80) {
                                        pulsespeed |= 0xf0;
                                        pulsespeed--;
                                    }
                                    pulsespeed = swap_nybbles(pulsespeed);
                                    insert_byte(pulsespeed);
                                }
                            }
                            else insert_byte(rtable[c][d]);
                            break;

                        default: insert_byte(rtable[c][d]); break;
                        }
                    }
                    else insert_byte(tablemap[c][rtable[c][d]]);
                }
            }

        SKIPTABLE:;
        }

        int songIndex = 0;
        // Insert orderlists
        for (c = 0; c < songs; c++) {
            int oddEvenSubSong = c & 1;

            for (d = 0; d < MAX_CHN; d++) {
                if (editorInfo.maxSIDChannels == 9 && oddEvenSubSong == 1 && d >= 3) break;
                if (editorInfo.maxSIDChannels == 3 && d >= 3) break;

                sprintf(textbuffer, "mt_song%d", songIndex++);
                insert_label(textbuffer);
                insert_bytes(&songwork[songoffset[c][d]], songsize[c][d]);
            }
        }

        // Insert patterns
        for (c = 0; c < patterns; c++) {
            sprintf(textbuffer, "mt_patt%d", c);
            insert_label(textbuffer);
            insert_bytes(&pattwork[pattoffset[c]], pattsize[c]);
        }

        if (jpA000Fix == 1) {
            sprintf(textbuffer, "jpa000fix");
            insert_label(textbuffer);
            sprintf(textbuffer, "jmp $%04X", playeradr + 3); // play addr
            insert_text(textbuffer);
        }

        // sprintf(textbuffer, "debug_0.s");
        // FILE* handle = fopen(textbuffer, "wt");
        // fwrite(membuf_get(&src), membuf_memlen(&src), 1, handle);
        // fclose(handle);

        // Assemble; on error fail in a rude way (the parser does so too)

        if (assemble(&src, &dest)) {
            exit(1);
        }

        packedsize = membuf_memlen(&dest);

        int endaddr = playeradr + packedsize;
        sidPlayAddr = playeradr + 3;

        if (playeradr < 0xa000 && endaddr > 0xa000) {
            if (jpA000Fix == 0) {
                doAgain   = 1;
                jpA000Fix = 1;
                /*
                sprintf(textbuffer, "debug_0.s");

                FILE *handle = fopen(textbuffer, "wt");
                fwrite(membuf_get(&src), membuf_memlen(&src), 1, handle);
                fclose(handle);

                if (assemble(&src, &dest))
                {
                    exit(1);
                }
                */
            }
            else {
                doAgain     = 0;
                sidPlayAddr = endaddr - 3;
            }
        }
    } while (doAgain == 1);

    packeddata = static_cast<uint8_t*>(membuf_get(&dest));

#ifdef GT2RELOC
    playersize = packedsize - songtblsize - songdatasize - patttblsize - pattdatasize - instrsize - wavetblsize -
                 pulsetblsize - filttblsize - speedtblsize;
#else
    (void)patttblsize;
    (void)songtblsize;
#endif

    // Copy author info
    if (playerversion & player_feature::author_info) {
        for (c = 0; c < 32; c++) {
            packeddata[32 + c] = authorname[c];
            // Convert 0 to space
            if (packeddata[32 + c] == 0) packeddata[32 + c] = 0x20;
        }
    }

#ifdef GT2RELOC
    printf("packing results:\n");
    printf("Playroutine:     %d bytes\n", playersize);
    printf("Songtable:       %d bytes\n", songtblsize);
    printf("Song-orderlists: %d bytes\n", songdatasize);
    printf("Patterntable:    %d bytes\n", patttblsize);
    printf("Patterns:        %d bytes\n", pattdatasize);
    printf("Instruments:     %d bytes\n", instrsize);
    printf("Tables:          %d bytes\n", wavetblsize + pulsetblsize + filttblsize + speedtblsize);
    printf("Total size:      %d bytes\n", packedsize);

    songhandle = fopen(packedsongname, "wb");
    if (!songhandle) {
        fprintf(STDERR, "error: could not open output file '%s'.\n", packedsongname);
        goto PRCLEANUP;
    }
#else
    songhandle = fopen(packedsongname, "wb");
    if (!songhandle) {
        reloc_alert("Could not open output file for relocator export.");
        goto PRCLEANUP;
    }
#endif

    if (fileformat == PackFormat::Prg) {
        fwritele16(songhandle, playeradr);
    }
    if (fileformat == PackFormat::Sid) {
        // See: https://www.hvsc.de/download/C64Music/DOCUMENTS/SID_file_format.txt

        // Identification
        uint8_t ident[] = { 'P', 'S', 'I', 'D', 0x00, 0x04, 0x00, 0x7c };
        if (editorInfo.maxSIDChannels == 3)
            ident[5] = 2; // JP - 3 channel, so we want to use ID 2 (otherwise it plays in stereo..urgh..)
        else if (editorInfo.maxSIDChannels == 9)
            ident[5] = 4; // JP - 9 channel, so we want to use ID 4 (Only format that handles 3 SIDs)
        else
            ident[5] = 3; // JP - original 6 channel ID (we don't care about 12 channel .SID file format, as the
                          // format only handles 3 SID max)

        uint8_t byte;
        fwrite(ident, sizeof ident, 1, songhandle);

        // Load address
        byte = 0x00;
        fwrite8(songhandle, byte);
        fwrite8(songhandle, byte);

        // Init address
        if ((editorInfo.multiplier > 1) || (!editorInfo.multiplier)) {
            uint32_t speedvalue;
            byte = (playeradr - 10) >> 8;
            fwrite8(songhandle, byte);
            byte = (playeradr - 10) & 0xff;
            fwrite8(songhandle, byte);

            if (editorInfo.multiplier) {
                if (editorInfo.ntsc) speedvalue = 0x42c6 / editorInfo.multiplier;
                else speedvalue = 0x4cc7 / editorInfo.multiplier;
            }
            else {
                if (editorInfo.ntsc) speedvalue = 0x42c6 * 2;
                else speedvalue = 0x4cc7 * 2;
            }
            speedcode[1] = speedvalue & 0xff;
            speedcode[6] = speedvalue >> 8;
        }
        else {
            byte = (playeradr) >> 8;
            fwrite8(songhandle, byte);
            byte = (playeradr) & 0xff;
            fwrite8(songhandle, byte);
        }

        // Play address
        byte = (sidPlayAddr) >> 8; // playeradr+3
        fwrite8(songhandle, byte);
        byte = (sidPlayAddr) & 0xff; // playeradr+3
        fwrite8(songhandle, byte);

        // Number of subtunes
        byte = 0x00;
        fwrite8(songhandle, byte);

        int songCount = songs;
        if (editorInfo.maxSIDChannels >= 9) songCount /= 2;
        byte = songCount;
        fwrite8(songhandle, byte);

        // Default subtune
        byte = 0x00;
        fwrite8(songhandle, byte);
        byte = 0x01;
        fwrite8(songhandle, byte);

        // Song speed bits
        byte = 0x00;
        if ((editorInfo.ntsc) || (editorInfo.multiplier > 1) || (!editorInfo.multiplier)) byte = 0xff;
        fwrite8(songhandle, byte);
        fwrite8(songhandle, byte);
        fwrite8(songhandle, byte);
        fwrite8(songhandle, byte);

        // Songname etc.
        fwrite(songname, sizeof songname, 1, songhandle);
        fwrite(authorname, sizeof authorname, 1, songhandle);
        fwrite(copyrightname, sizeof copyrightname, 1, songhandle);

        // Flags
        byte = 0x00;
        fwrite8(songhandle, byte);
        if (editorInfo.ntsc) byte = 8;
        else byte = 4;
        // Set model for both SIDs
        if (editorInfo.maxSIDChannels == 3) {
            if (editorInfo.sidmodel) byte |= 32;
            else byte |= 16;
        }
        else {
            if (editorInfo.sidmodel) byte |= 32 + 128; // If bits for SID2+3 are 0, then use the bits for SID1
            else byte |= 16 + 64;
        }

        fwrite8(songhandle, byte); // 0x75

        if (editorInfo.maxSIDChannels == 9) {
            if (editorInfo.sidmodel) byte = 1; // If bits for SID2+3 are 0, then use the bits for SID1
            else byte = 2;
            fwrite8(songhandle, byte);
        }
        else fwrite8(songhandle, 0);

        if (editorInfo.maxSIDChannels == 3) {
            // Reserved longword
            byte = 0x00;
            //	fwrite8(songhandle, byte);
            fwrite8(songhandle, byte);
            fwrite8(songhandle, byte);
            fwrite8(songhandle, byte);
        }
        else {
            // JP - .SID file can only handle 3 SID chips. AND it doesn't allow you to specific SID chip 1 address
            // So we CAN NOT create a .sid file that can play 4 channel SIDs
            // Relocation and second SID address
            byte = 0x00;               // SID Chip 3 = use same info as SID Chip 1 (eg. same sid model)
            fwrite8(songhandle, byte); // 79 Page length

            fwrite8(songhandle, ((sidAddr2 >> 0) & 0x0ff0) >> 4);                                    // SID 2
            if (editorInfo.maxSIDChannels > 6) fwrite8(songhandle, ((sidAddr3 >> 0) & 0x0ff0) >> 4); // SID 3
            else fwrite8(songhandle, 0);
        }
        // Load address
        if ((editorInfo.multiplier > 1) || (!editorInfo.multiplier)) {
            byte = (playeradr - 10) & 0xff;
            fwrite8(songhandle, byte);
            byte = (playeradr - 10) >> 8;
            fwrite8(songhandle, byte);
        }
        else {
            byte = (playeradr) & 0xff;
            fwrite8(songhandle, byte);
            byte = (playeradr) >> 8;
            fwrite8(songhandle, byte);
        }
        if ((editorInfo.multiplier > 1) || (!editorInfo.multiplier)) fwrite(speedcode, 10, 1, songhandle);
    }

    fwrite(packeddata, packedsize, 1, songhandle);
    fclose(songhandle);

    songExported = true;
    goto PREXPORTCOMPLETE;

PRCLEANUP:

PREXPORTCOMPLETE:

    membuf_free(&src);
    membuf_free(&dest);

    ascii_key    = 0;
    scancode = 0;
}
