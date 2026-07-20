#pragma once

#include <cstddef>
#include <cstdint>

// Build-time embedded resources (sources in assets/, HEX'd into the binary).

namespace embed {

enum class Id {
    player,
    altplayer,
    player3,
    altplayer3,
    player9,
    altplayer9,
    player12,
    altplayer12,
    window_icon,
    font,
    count,
};

struct Blob {
    const uint8_t* data;
    std::size_t    size;
};

Blob const& get(Id id);

} // namespace embed
