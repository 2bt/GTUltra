//
// GOATTRACKER ULTRA Info display
//
// Pure decoder: turns the editor cursor position into the one-line context
// help shown in the info bar. No global state, no output buffer — every entry
// point returns the text as a std::string. See ginfo::describe().
//

#include "goattrk2.hpp"
#include "ginfo.hpp"
#include "gpattern.hpp"
#include "gsong.hpp"
#include "gtable.hpp"

#include <format>
#include <string>
#include <string_view>

namespace ginfo {

namespace {

// Command/instruction help. Indexed at runtime, so these are runtime format
// strings fed to std::vformat; each caller supplies the argument count the
// chosen entry expects.
constexpr std::string_view pattern_instruction_info_string[] = {
    "(1) Portamento up. Value: ${:04X}",
    "(2) Portamento down. Value: ${:04X}",
    "(3) Tone portamento. Value: ${:04X}",
    "(4) Vibrato Speedtable pointer. Speed: ${:02X} Depth: ${:02X}",
    "(5) Attack: ${:02X} Decay: ${:02X}",
    "(6) Sustain: ${:02X} Release: ${:02X}",
    "(7) Waveform: ${:02X}",
    "(8) Wavetable pointer (0 to stop) Value: ${:02X}",
    "(9) Pulsetable pointer (0 to stop) Value: ${:02X}",
    "(A) Filtertable pointer (0 to stop) Value: ${:02X}",
    "(B) Filter XY. Resonance: ${:X}. Channel Enabled Bitmask: ${:X}",
    "(C) Filter cutoff: ${:02X}",
    "(D) Volume/Marker:  XY ${:02X}",
    "(E) Funk Tempo. Tempo1: ${:02X} Tempo2: ${:02X}",
    "(F) Global Tempo: ${:02X}",
};

constexpr std::string_view instrument_info_string[] = {
    "Attack: ${:02X} Decay: ${:02X}",
    "Sustain: ${:02X} Release: ${:02X}",
    "Wavetable pointer: ${:02X}",
    "Pulsetable pointer: ${:02X} (00 = leave untouched)",
    "Filtertable pointer: ${:02X} (00 = leave untouched)",
    "Vibrato speedtable pointer. Speed: ${:02X} Depth: ${:02X}",
    "Vibrato Delay: ${:02X} ticks until vibrato starts",
    "HR/Gate Delay: ${:02X} ticks until note start",
    "1st Frame Wave: ${:02X} .Usually $09 (gate + testbit)",
};

constexpr std::string_view filter_type_string[] = {
    "No Filter Type",
    "Low Pass",
    "Band Pass",
    "High Pass",
};

constexpr std::string_view filter_channels_enabled_string[] = {
    "Active chn: None (0)", "Active chn: 1 (1)",   "Active chn: 2 (2)",   "Active chn: 1+2 (3)",
    "Active chn: 3 (4)",    "Active chn: 1+3 (5)", "Active chn: 2+3 (6)", "Active chn: All (7)",
};

// Format one of the runtime-indexed command strings above.
template <class... Args>
std::string cmd(std::string_view f, Args... args) {
    return std::vformat(f, std::make_format_args(args...));
}

std::string wave_table_left(std::string_view lr) {
    int ldata = ltable[WTBL][editorInfo.etpos];
    int rdata = rtable[WTBL][editorInfo.etpos];

    if (ldata == 0) return std::format("({}): 00 = leave waveform unchanged", lr);
    if (ldata < 0x10) return std::format("({}):Delay ${:02X} ({}) (delay this step for n frames)", lr, ldata, ldata);
    if (ldata < 0xe0) return std::format("({}):Waveform ${:02X}", lr, ldata);
    if (ldata < 0xf0) return std::format("({}):Inaudible waveform ${:02X} (converts to {:02X})", lr, ldata, ldata - 0xe0);
    if (ldata < 0xff) {
        int instr      = ldata - 0xf0;
        int instrIndex = instr - 1;

        if (instr == 1 || instr == 2 || instr == 3) {
            int speed = (ltable[STBL][rdata - 1] << 8) | rtable[STBL][rdata - 1];
            return cmd(pattern_instruction_info_string[instrIndex], speed);
        }
        if (instr == 5 || instr == 6 || instr == 0xb) {
            int nybHi = (rdata & 0xf0) >> 4;
            int nybLo = rdata & 0xf;
            return cmd(pattern_instruction_info_string[instrIndex], nybHi, nybLo);
        }
        if (instr == 7 || instr == 8 || instr == 9 || instr == 0xa || instr == 0xc || instr == 0xd) {
            return cmd(pattern_instruction_info_string[instrIndex], ldata);
        }
        if (instr == 4 || instr == 0xe) {
            int tempo1 = ltable[STBL][rdata - 1];
            int tempo2 = rtable[STBL][rdata - 1];
            return cmd(pattern_instruction_info_string[instrIndex], tempo1, tempo2);
        }
        return "";
    }
    // ldata == 0xff
    if (rdata) return std::format("({}): Jump to ${:02X} ($00 = stop)", lr, rdata);
    return std::format("({}): Stop. ($01-$FF = Jump to position)", lr);
}

std::string wave_table_right() {
    int ldata = ltable[WTBL][editorInfo.etpos];
    int rdata = rtable[WTBL][editorInfo.etpos];

    // Right data is part of the left data $Fx instruction.
    if (ldata >= 0xf0) return wave_table_left("right");

    if (rdata < 0x60) return std::format("(right): Note offset +${:02X} ({})", rdata, rdata);
    if (rdata < 0x80) {
        int v = 0x80 - rdata; // 0-0x1f
        return std::format("(right): Note offset -${:02X} (-{:02d})", v, v);
    }
    if (rdata == 0x80) return "(right): Keep note unchanged";
    if (rdata < 0xe0) return std::format("(right): Absolute note (${:02X} = {})", rdata, notenameTableView[rdata - 0x80]);
    return std::format("(right): Invalid value (${:02X}. Max value:$DF)", rdata);
}

std::string wave_table() {
    return (editorInfo.etcolumn / 2) == 0 ? wave_table_left("left") : wave_table_right();
}

std::string pulse_table() {
    int ldata = ltable[PTBL][editorInfo.etpos];
    int rdata = rtable[PTBL][editorInfo.etpos];

    if (ldata == 0) return "0 (undefined)";
    if (ldata < 0x80) {
        int s = rdata;
        if (s >= 0x80) {
            s = 256 - s;
            return std::format("For ${:02X} ({}) ticks, pulse - ${:02X} ({})", ldata, ldata, rdata, s);
        }
        return std::format("For ${:02X} ({}) ticks, pulse + ${:02X} ({})", ldata, ldata, rdata, rdata);
    }
    if (ldata < 0xff) {
        int s = rdata + ((ldata & 0xf) << 8);
        return std::format("PulseTable: Set pulse value ${:03X} ({})", s, s);
    }
    // ldata == 0xff
    if (rdata) return std::format("Jump to ${:02X} ($00 = stop)", rdata);
    return "Stop. ($01-$FF = Jump to position)";
}

std::string filter_table() {
    int ldata = ltable[FTBL][editorInfo.etpos];
    int rdata = rtable[FTBL][editorInfo.etpos];

    if (ldata == 0) return std::format("Set cutoff ${:02X} ({})", rdata, rdata);
    if (ldata < 0x80) {
        int s = rdata;
        if (s >= 0x80) {
            s = 256 - s;
            return std::format("For ${:02X} ({}) ticks, cutoff - ${:02X} ({})", ldata, ldata, s, s);
        }
        return std::format("For ${:02X} ({}) ticks, cutoff + ${:02X} ({})", ldata, ldata, rdata, rdata);
    }
    if (ldata <= 0xf0) {
        int filterType = (ldata & 0x70) >> 4; // remove top bit
        int ft         = 0;
        while (filterType != 0) {
            ft++;
            filterType >>= 1;
        }

        int resonance       = (rdata & 0xf0) >> 4;
        int channelsEnabled = rdata & 0xf;

        return std::format("{} (${:02X}). Resonance: ${:02X}. {}",
                           filter_type_string[ft],
                           ldata,
                           resonance,
                           filter_channels_enabled_string[channelsEnabled]);
    }
    // ldata == 0xff
    if (rdata) return std::format("Jump to ${:02X} ($00 = stop)", rdata);
    return "Stop. ($01-$FF = Jump to position)";
}

std::string table_info() {
    if (editorInfo.etnum == WTBL) return wave_table();
    if (editorInfo.etnum == PTBL) return pulse_table();
    if (editorInfo.etnum == FTBL) return filter_table();
    if (editorInfo.etnum == STBL) return "SpeedTable";
    return "";
}

std::string instrument_info() {
    int param = editorInfo.eipos;
    INSTR& in = instr[editorInfo.einum];

    switch (param) {
    case 0: return cmd(instrument_info_string[param], in.ad >> 4, in.ad & 0xf);
    case 1: return cmd(instrument_info_string[param], in.sr >> 4, in.sr & 0xf);
    case 2: return cmd(instrument_info_string[param], in.ptr[WTBL]);
    case 3: return cmd(instrument_info_string[param], in.ptr[PTBL]);
    case 4: return cmd(instrument_info_string[param], in.ptr[FTBL]);
    case 5: {
        int tempo1 = ltable[STBL][in.ptr[STBL] - 1];
        int tempo2 = rtable[STBL][in.ptr[STBL] - 1];
        return cmd(instrument_info_string[param], tempo1, tempo2);
    }
    case 6: return cmd(instrument_info_string[param], in.vibdelay);
    case 7: return cmd(instrument_info_string[param], in.gatetimer);
    case 8: return cmd(instrument_info_string[param], in.firstwave);
    }
    return "";
}

std::string pattern_info(const GTOBJECT& gt) {
    int c2      = getActualChannel(editorInfo.esnum, editorInfo.epchn);
    int epnum   = gt.editorUndoInfo.editorInfo[c2].epnum;
    int cmdByte = pattern[epnum][editorInfo.eppos * 4 + 2];

    if (!cmdByte) return "                                ";

    int instr      = cmdByte;
    int instrIndex = instr - 1;
    int data       = pattern[epnum][editorInfo.eppos * 4 + 3];

    if (instr == 1 || instr == 2 || instr == 3) {
        int speed = (ltable[STBL][data - 1] << 8) | rtable[STBL][data - 1];
        return cmd(pattern_instruction_info_string[instrIndex], speed);
    }
    if (instr == 5 || instr == 6 || instr == 0xb) {
        int nybHi = (data & 0xf0) >> 4;
        int nybLo = data & 0xf;
        return cmd(pattern_instruction_info_string[instrIndex], nybHi, nybLo);
    }
    if (instr == 7 || instr == 8 || instr == 9 || instr == 0xa || instr == 0xc || instr == 0xd) {
        return cmd(pattern_instruction_info_string[instrIndex], data);
    }
    if (instr == 4 || instr == 0xe) {
        int tempo1 = ltable[STBL][data - 1];
        int tempo2 = rtable[STBL][data - 1];
        return cmd(pattern_instruction_info_string[instrIndex], tempo1, tempo2);
    }
    if (instr == 0xf) {
        if (data < 0x80) return cmd(pattern_instruction_info_string[instrIndex], data);
        return std::format("Channel Tempo: {:02X}", data - 0x80);
    }
    return ""; // out-of-range command byte: no description
}

} // namespace

std::string describe(const GTOBJECT& gt) {
    switch (editorInfo.editmode) {
    case EditMode::Pattern: return pattern_info(gt);
    case EditMode::Instrument: return instrument_info();
    case EditMode::Tables: return table_info();
    case EditMode::OrderList: return "OrderTable";
    default: return "";
    }
}

} // namespace ginfo
