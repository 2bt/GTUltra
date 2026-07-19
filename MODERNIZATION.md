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

### Stage B — switch from C to C++

All of the project's own sources were renamed `.c` → `.cpp` (via `git mv`, so
history is preserved) and now compile as C++. Out of scope (will be replaced
wholesale later, so left as C, compiled as C):

- `src/asm/*` — the 6502 assembler (includes flex-generated `lexyy.c`)
- `src/bme/*` — the media engine

`resid` / `resid-fp` were already C++.

Because `asm` and `bme` stay C, the C-header includes are wrapped in
`extern "C"` at their include sites in the C++ code (`goattrk2.h` for `bme.h`,
`greloc.cpp` for the `asm` headers, and each standalone tool for `bme_end.h`)
so their symbols link correctly.

Migration flags applied to the C++ translation units (`-fpermissive`,
`-Wno-narrowing`) absorb the lax conversions and `char[]` byte-table
initializers the old C relied on. A handful of genuine C++ errors were fixed
by hand rather than flagged away:

- `gplay.cpp` / `greloc.cpp`: `goto` statements that jumped across local
  variable initializations (legal in C, ill-formed in C++) — variables were
  re-scoped / hoisted.
- `goattrk2.h`: empty-parameter-list prototypes (`f()`) for functions actually
  taking arguments — given real signatures, since in C++ `f()` means "no args".

Result: `cmake --build build` produces all five binaries as C++. Tightening
`-fpermissive`/`-Wno-narrowing` back down to clean, idiomatic C++ is left for
follow-up work.

### Stage C — remove obsolete build cruft

Now that CMake owns the build, the following were removed (all recoverable
from git history):

- `linux/`, `mac/`, `win32/` — these held build outputs / committed prebuilt
  binaries. Builds now go to `build/`. (Prebuilt Mac/Windows binaries and the
  bundled `win32/SDL2.dll` / `win32/gtultra.cfg` went with them; the editor
  regenerates its own config on first run.)
- the per-platform makefiles (`src/makefile*`, `src/bme/makefile*`) and the
  `.bat` helpers (`_make.bat`, `remakedata.bat`).
- committed Windows host-tool binaries (`src/**/datafile.exe`,
  `src/**/dat2inc.exe`) — these are built from source by CMake now.

## Planned milestones

- **New UI — Dear ImGui.** Replace the legacy text-mode UI with Dear ImGui,
  keeping the reusable model / editing logic. Full roadmap (M2–M6), informed by
  a study of the Furnace tracker, is in
  [docs/UI_MIGRATION_PLAN.md](docs/UI_MIGRATION_PLAN.md). The M6 chargen /
  legacy-renderer teardown is detailed in
  [docs/M6_LEGACY_RENDERER_REMOVAL.md](docs/M6_LEGACY_RENDERER_REMOVAL.md).
- **Configuration system (TOML).** User-configurable themes, fonts, and all
  keybindings in a real `config.toml`. Depends on the ImGui action/theme
  scaffolding; detailed as milestone **M7** in the UI migration plan.
- **SDL2 (not SDL3 yet).** The Linux build already links system **SDL2** (via
  `pkg-config sdl2`); the bundled `src/SDL` headers are legacy SDL1 used only
  by the old win32 build. Decision (2026-07): **stay on SDL2** — SDL3 has no
  Ubuntu 24.04 package yet, and almost all SDL usage lives inside `bme`, so the
  version choice is best made during the `bme` rewrite. A dedicated milestone
  will make SDL2 the sole explicit target (dropping the bundled SDL1 headers
  and the `<SDL/…>` include style).
- **Replace `bme`** (media engine) with a modern implementation.
- **Replace `asm`** (6502 assembler) — possibly with a 64-bit assembler.
