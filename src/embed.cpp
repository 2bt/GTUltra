#include "embed.hpp"

#include "embed_data.inc"

#include <cctype>

namespace {

constexpr embed::File k_catalog[] = {
    { "player.s", k_bytes_player_s, k_size_player_s },
    { "altplayer.s", k_bytes_altplayer_s, k_size_altplayer_s },
    { "player3.s", k_bytes_player3_s, k_size_player3_s },
    { "altplayer3.s", k_bytes_altplayer3_s, k_size_altplayer3_s },
    { "player9.s", k_bytes_player9_s, k_size_player9_s },
    { "altplayer9.s", k_bytes_altplayer9_s, k_size_altplayer9_s },
    { "player12.s", k_bytes_player12_s, k_size_player12_s },
    { "altplayer12.s", k_bytes_altplayer12_s, k_size_altplayer12_s },
    { "goat32.png", k_bytes_goat32_png, k_size_goat32_png },
};

bool name_eq_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

} // namespace

namespace embed {

const File* find(std::string_view name) {
    // Allow "path/player.s" — match on basename only.
    if (const auto slash = name.find_last_of("/\\"); slash != std::string_view::npos)
        name = name.substr(slash + 1);

    for (const File& file : k_catalog) {
        if (name_eq_ci(name, file.name)) return &file;
    }
    return nullptr;
}

} // namespace embed
