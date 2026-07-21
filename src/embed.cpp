#include "embed.hpp"

#include "embed_data.inc"

#include <array>
#include <cassert>

namespace {

constexpr std::array<embed::Blob, static_cast<size_t>(embed::Id::count)> k_blobs = { {
    { k_bytes_player_s, k_size_player_s },
    { k_bytes_altplayer_s, k_size_altplayer_s },
    { k_bytes_player3_s, k_size_player3_s },
    { k_bytes_altplayer3_s, k_size_altplayer3_s },
    { k_bytes_player9_s, k_size_player9_s },
    { k_bytes_altplayer9_s, k_size_altplayer9_s },
    { k_bytes_player12_s, k_size_player12_s },
    { k_bytes_altplayer12_s, k_size_altplayer12_s },
    { k_bytes_icon128_rgba, k_size_icon128_rgba },
    { k_bytes_font_otf, k_size_font_otf },
} };

static_assert(k_blobs.size() == static_cast<size_t>(embed::Id::count));

} // namespace

namespace embed {

Blob const& get(Id id) {
    auto i = static_cast<size_t>(id);
    assert(i < k_blobs.size());
    return k_blobs[i];
}

} // namespace embed
