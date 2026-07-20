#pragma once

#include <cstddef>
#include <string_view>

// Build-time embedded resources (player .s sources, window icon).
// Data lives in the binary; looked up by filename via a thin catalog.

namespace embed {

struct File {
    const char*          name;
    const unsigned char* data;
    std::size_t          size;
};

// Case-insensitive basename match (e.g. "player.s" / "PLAYER.S").
const File* find(std::string_view name);

} // namespace embed
