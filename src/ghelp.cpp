//
// GTUltra online help — reference prose + topic index.
// Keybind tables are generated from gtaction bindings (see gimgui / print_help_cli).
//

#include "ghelp.hpp"

namespace gthelp {
namespace {

constexpr std::string_view kPatternNotes[] = {
    "Type notes on the piano keyboard (ProTracker or DMC layout).",
    "Enter hex digits 0-9 and A-F into command/data columns.",
};

constexpr std::string_view kOrderNotes[] = {
    "Enter hex digits 0-9 and A-F for pattern numbers and transpose values.",
};

constexpr std::string_view kInstrNotes[] = {
    "Enter hex digits 0-9 and A-F into instrument parameter fields.",
    "Click or press Enter on a name cell to edit the instrument name.",
};

constexpr std::string_view kTableNotes[] = {
    "Enter hex digits 0-9 and A-F into table cells.",
};

constexpr std::string_view kNamesNotes[] = {
    "Type to edit song title, author, and copyright.",
};

constexpr std::string_view kPattCmds[] = {
    "Command 0XY: Do nothing. Databyte will always be 00.",
    "Command 1XY: Portamento up. XY is index to a 16-bit speed in speedtable.",
    "Command 2XY: Portamento down. XY is index to a 16-bit speed in speedtable.",
    "Command 3XY: Toneportamento. Raise or lower pitch until target note has been reached. XY is index to a "
    "16-bit speed or 00 for \"tie note\".",
    "Command 4XY: Vibrato. XY is index to speedtable. Left side value determines how long until the direction "
    "changes (speed) and right side value is the amount of pitch change each tick (depth).",
    "Command 5XY: Set attack/decay register to value XY.",
    "Command 6XY: Set sustain/release register to value XY.",
    "Command 7XY: Set waveform register to value XY. If a wavetable is actively changing the channel's waveform "
    "at the same time, will be ineffective.",
    "Command 8XY: Set wavetable pointer. 00 stops wavetable execution.",
    "Command 9XY: Set pulsetable pointer. 00 stops pulsetable execution.",
    "Command AXY: Set filtertable pointer. 00 stops filtertable execution.",
    "Command BXY: Set filter control. X is resonance and Y is channel bitmask. 00 turns filter off and also stops "
    "filtertable execution.",
    "Command CXY: Set filter cutoff to XY. Can be ineffective if the filtertable is active and also changing the "
    "cutoff.",
    "Command DXY: Set mastervolume to Y, if X is 0. If X is not 0, value XY is copied to the timing mark "
    "location, which is playeraddress+$3F.",
    "Command EXY: Funktempo. XY is an index to speedtable. Will alternate left side and right side tempo values "
    "on each pattern step.",
    "Command FXY: Set tempo. Values 03-7F set tempo on all channels, values 83-FF only on current channel "
    "(subtract 80 to get actual tempo). Tempos 00 and 01 recall the funktempos set by EXY command.",
};

constexpr std::string_view kInstParm[] = {
    "Attack/Decay — 0 is fastest attack or decay, F is slowest.",
    "Sustain/Release — Sustain level 0 is silent and F is the loudest. Release behaves like Attack & Decay (F "
    "slowest).",
    "Wavetable Pos — Wavetable startposition. Value 00 stops the wavetable execution and is not very useful.",
    "Pulsetable Pos — Pulsetable startposition. Value 00 will leave pulse execution untouched.",
    "Filtertable Pos — Filtertable startposition. Value 00 will leave filter execution untouched. In most cases "
    "it makes sense to have a filter-controlling instrument only on one channel at a time.",
    "Vibrato Param — Instrument vibrato parameters. An index to the speedtable, see command 4XY.",
    "Vibrato Delay — How many ticks until instrument vibrato starts. Value 00 turns instrument vibrato off.",
    "HR/Gate Timer — How many ticks before note start note fetch, gateoff and hard restart happen. Can be at most "
    "tempo-1. So on tempo 4 highest acceptable value is 3. Bitvalue 80 disables hard restart and bitvalue 40 "
    "disables gateoff.",
    "1stFrame Wave — Waveform used on init frame of the note, usually 09 (gate + testbit). Values 00, FE and FF "
    "have special meaning: leave waveform unchanged and additionally set gate off (FE), gate on (FF), or gate "
    "unchanged (00).",
};

constexpr std::string_view kTableEncoding[] = {
    "Wavetable left side: 00 leave waveform unchanged; 01-0F delay 1-15 frames; 10-DF waveform values; E0-EF "
    "inaudible waveform 00-0F; F0-FE execute command 0XY-EXY (right side = parameter); FF jump (right side = "
    "position, 00 = stop).",
    "Wavetable right side: 00-5F relative notes; 60-7F negative relative notes; 80 keep frequency unchanged; "
    "81-DF absolute notes C#0 - B-7.",
    "Pulsetable left side: 01-7F pulse modulation step (left = time, right = signed 8-bit speed); 8X-FX set pulse "
    "width (X = high 4 bits, right = low 8 bits); FF jump (00 = stop).",
    "Filtertable left side: 00 set cutoff (right side); 01-7F filter modulation (left = time, right = signed "
    "speed); 80-F0 set filter parameters (high nybble = passband, right = resonance/channel bitmask as in BXY); "
    "FF jump (00 = stop).",
    "Speedtable vibrato: XX YY — left = ticks until direction change (speed), right = pitch delta each tick "
    "(depth).",
    "Speedtable portamento: XX YY — 16-bit value added to pitch each tick (MSB/LSB).",
    "Speedtable funktempo: XX YY — two 8-bit tempo values alternated each pattern row.",
    "For vibrato and portamento, if XX has the high bit ($80) set, note-independent depth/speed calculation is "
    "enabled and YY is the divisor (higher → quieter effect, more raster time).",
};

constexpr Topic kTopics[] = {
    { "General", "General keys", Kind::Keybinds, gtaction::Ctx::Global, {}, {} },
    { "Pattern", "Pattern editor", Kind::Keybinds, gtaction::Ctx::Pattern, kPatternNotes, {} },
    { "Order", "Order list", Kind::Keybinds, gtaction::Ctx::Order, kOrderNotes, {} },
    { "Instrument", "Instrument editor", Kind::Keybinds, gtaction::Ctx::Instrument, kInstrNotes, {} },
    { "Tables", "Table editor", Kind::Keybinds, gtaction::Ctx::Tables, kTableNotes, {} },
    { "Song", "Song metadata", Kind::Keybinds, gtaction::Ctx::Names, kNamesNotes, {} },
    { "FX commands", "Pattern effect commands", Kind::Reference, std::nullopt, {}, kPattCmds },
    { "Instr fields", "Instrument fields", Kind::Reference, std::nullopt, {}, kInstParm },
    { "Table encoding", "Table row encoding", Kind::Reference, std::nullopt, {}, kTableEncoding },
};

} // namespace

std::span<const Topic> topics() { return kTopics; }

std::size_t topic_index_for_edit_panel(int edit_panel) {
    const gtaction::Ctx want = gtaction::context_from_editmode(edit_panel);
    const auto          all  = topics();
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].kind == Kind::Keybinds && all[i].binds && *all[i].binds == want) return i;
    }
    return 0;
}

void print_reference(std::ostream& out) {
    for (const Topic& t : topics()) {
        if (t.kind != Kind::Reference || t.body.empty()) continue;
        out << t.title << '\n';
        for (std::string_view line : t.body) out << "  " << line << '\n';
        out << '\n';
    }
}

} // namespace gthelp
