# GTUltra Modernization

Notes on the ongoing effort to modernize GTUltra (a fork of GoatTracker 2 Stereo).

## Milestone 1 — CMake + C++

### Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Binaries land in `build/`: `gtultra` (the editor), `gt2reloc`, `ins2snd2`,
`mod2sng2`, `ss2stereo`.

Requirements: CMake ≥ 3.16, a C++14 compiler, SDL2, ALSA, pthreads
(discovered via `pkg-config`).

### Stage A — switch to CMake

The old per-platform makefiles (`src/makefile*`) are replaced by a single
top-level `CMakeLists.txt`. It reproduces the original build exactly:

- **Host codegen tools** `datafile` and `dat2inc` are built, then used to pack
  the bundled player `.s` sources / palettes / fonts into `gt2stereo.dat` and
  emit it as the `goatdata` C array. Generation now happens entirely under
  `build/generated/` — the source tree is never written to (previously
  `src/goatdata.c` and `src/gt2stereo.dat` were regenerated in place).
- **Vendored libs** are split into static libraries: `resid`, `residfp`
  (already C++), `gtasm` (the 6502 assembler, stays C), and `bme` (the
  media engine, stays C).
- **`gtcore`** holds the shared editor sources used by both `gtultra` and
  `gt2reloc`. As in the original build, `greloc` and `gt2stereo` are not part
  of the shared set: `gt2reloc.c` `#include`s `greloc.c` itself (with
  `GT2RELOC` defined) while `gtultra` links `greloc` separately.

Notes:
- `src/` is intentionally kept off the angle-`<>` include path so the bundled
  (incomplete) `src/SDL` headers don't shadow the system SDL. Project headers
  are quote-includes resolved next to each source, matching the old build.
- Build artifacts (object files, binaries) are no longer committed; see
  `.gitignore`. The stale prebuilt binaries under `linux/` were untracked.

### Stage B — switch from C to C++ (in progress)

The project's own sources are being migrated from C to C++. Out of scope
(will be replaced wholesale later, so left as C):

- `src/asm/*` — the 6502 assembler
- `src/bme/*` — the media engine

`resid` / `resid-fp` were already C++.

## Planned milestones

- **SDL2/SDL3.** The Linux build already links system **SDL2** (via
  `pkg-config sdl2`); the bundled `src/SDL` headers are legacy SDL1 used only
  by the old win32 build. A dedicated milestone will make SDL2 the sole,
  explicit target (dropping the bundled SDL1 headers and the `<SDL/…>` include
  style), and evaluate moving to **SDL3**. This is coupled to the `bme`
  rewrite, since almost all SDL usage lives inside `bme`.
- **Replace `bme`** (media engine) with a modern implementation.
- **Replace `asm`** (6502 assembler) — possibly with a 64-bit assembler.
