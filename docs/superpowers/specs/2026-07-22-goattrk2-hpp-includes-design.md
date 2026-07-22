# Slim `goattrk2.hpp` includes

**Status:** Approved (chat, 2026-07-22)

## Goal

Reduce coupling from `goattrk2.hpp` by keeping only includes required for its own declarations. Opportunistically stop using it as an umbrella where the change is tiny.

## Design

`goattrk2.hpp` retains:

- `<cstdint>`, `<cstdio>`
- `gfile.hpp` (`MAX_PATHNAME`, `MAX_FILENAME`)
- `greloc.hpp` (`PackFormat`; also provides `GTOBJECT` via `gplay.hpp`)

Remove: POSIX headers and all other project module headers not needed for this header’s API.

Fallout: add direct includes (and POSIX headers) in `.cpp` files that relied on transitive includes. Build `gtultra` and `gt2reloc`.

Opportunistic B: `gfollow.hpp` includes `gplay.hpp` instead of `goattrk2.hpp`.

## Out of scope

Full per-TU include rewrite; splitting globals out of `goattrk2.hpp`.
