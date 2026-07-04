# GTUltra UI Modernization Plan — Dear ImGui + Config System

Status: **planning** (no code yet). This document is the agreed roadmap for
replacing GTUltra's legacy text-mode UI with Dear ImGui, and for adding a
proper configuration system (themes, fonts, keybindings).

It draws heavily on a study of the **Furnace** tracker
(`/home/dlangner/Programming/c++/furnace`), which is a mature ImGui-based
tracker and our primary reference. File paths below marked *(furnace)* point
into that tree; unmarked paths are GTUltra's.

---

## 1. Where we are (the seam we exploit)

GTUltra is better-layered than a typical tracker, which makes this tractable:

- **Model** — plain global C arrays in `gsong.h` / `gsong.cpp`
  (`pattern[]`, `songorder[]`, `ltable/rtable[]`, `instr[]`, `pattlen[]`, …)
  and the player/runtime state in `GTOBJECT` (`gplay.h`). Render-free.
- **Editor state** — one central struct `EDITOR_INFO editorInfo`
  (`gpattern.h:15`): cursor positions, scroll positions, marks/selection,
  current instrument/octave, mouse-drag tracking. *(Some editor state also
  lives inside `GTOBJECT.editorUndoInfo.editorInfo[]`, e.g. which pattern each
  channel edits — the new UI must drive both.)*
- **Editing logic** — the per-view `*commands()` functions
  (`patterncommands` `gpattern.cpp:35`, `orderlistcommands` `gorder.cpp:22`,
  `instrumentcommands` `ginstr.cpp:12`, `tablecommands` `gtable.cpp:14`) are
  **already free of drawing calls**. They mutate model + `editorInfo`.
  Dispatch is `docommand()` (`gt2stereo.cpp:1262`) switching on
  `editorInfo.editmode`, wrapped in undo dirty-checking.
- **Render** — the fake text-mode: `printtext`/`printbyte`/`printbg`/`fillArea`
  write cells into `scrbuffer`/`colorbuffer` (`gconsole.cpp`), which
  `fliptoscreen()` (`gconsole.cpp:468`) rasterizes via the chargen font into a
  paletted surface that `gfx_flip` (bme) presents through SDL2.
- **Input** — `getkey()` (`gconsole.cpp:787`) polls bme/SDL into globals
  (`key`, `rawkey`, `shiftpressed`, `mouseb`, `mousex/y` in **text-cell**
  coords). `waitkeymouse()` (`gt2stereo.cpp:1009`) is the busy loop that also
  folds in autosave, MIDI, and QWERTY jamming. Keybindings are **hardcoded
  `switch(key)`/`switch(rawkey)`** statements; the only data-driven binding
  today is the F-key macro system (`gfkeys.cpp`).

**Reuse verdict:**
- ✅ Reusable ~as-is: the model storage + operations, the `*commands()` editing
  *semantics*, the `editorInfo` state blob, undo (`gundo.cpp`), audio
  (`bme_snd`), data/VFS (`bme_io`).
- ⚠️ Needs rework: the `*commands()` functions read `key`/`rawkey` globals and
  many model functions implicitly read `editorInfo` instead of taking explicit
  indices — both must be adapted.
- ❌ Replaced entirely: `gdisplay.cpp` + `ginfo.cpp` (drawing), the
  `fliptoscreen`/chargen renderer, `waitkeymouse`/`mousecommands` input loop
  and all hardcoded key switches, and eventually bme's gfx/win/kbd/mou modules.
  The hardest single piece is **mouse hit-testing** (~112 comparisons against
  hardcoded text-cell coordinates in `mousecommands`).

---

## 2. Target architecture

```
            +-------------------- SDL2 (window + input + audio) --------------------+
            |                                                                        |
  ImGui platform (imgui_impl_sdl2)                      bme_snd (audio, kept for now)
            |                                                                        |
   +-------- Render interface (virtual) --------+                                    |
   |  SDL_Renderer backend  (add GL/etc later)  |                                    |
   +--------------------------------------------+                                    |
            |                                                                        |
   ImGui frame loop (drawHalt / WAKE_UP)                                             |
            |                                                                        |
   Panels (draw*)  ── read/write ──>  editorInfo + model (gsong) + GTOBJECT  <── audio/player
            |                                    ^
   Input/Action layer  ── edits via ──> reused *commands() semantics (undo-wrapped)
            |
   Theme (color roles) + Keymap (action->chord) + Fonts   <── Config (TOML), M7
```

Key decisions, all validated against Furnace:

1. **SDL2 stays the fixed platform layer** (windowing + input + audio); only the
   *renderer* is abstracted behind a thin virtual interface. Start with the
   `SDL_Renderer` backend only; the interface leaves room for OpenGL later
   without touching UI code. *(furnace: `src/gui/gui.h:1668` `FurnaceGUIRender`,
   `src/gui/render/renderSDL.cpp`.)* This also keeps us aligned with the earlier
   decision to stay on SDL2 (SDL3 has no Ubuntu 24.04 package yet).
2. **Redraw-on-demand loop.** Adopt Furnace's `drawHalt`/`WAKE_UP` idiom
   *(furnace: `src/gui/gui.cpp:4342`, macro `gui.h:54`)*: idle → block on
   `SDL_WaitEventTimeout`; any input or active playback forces ~5 full-speed
   frames. Battery-friendly but instantly responsive.
3. **Pattern grid = custom `ImDrawList`, not ImGui widgets.** This is the crux.
   *(furnace: `src/gui/pattern.cpp:83` — the single most important file to
   study.)* Order list can use ImGui tables *(furnace: `src/gui/orders.cpp`)*.
4. **Theme + actions are first-class from day one** — even before the config
   *file* exists (M7). Build the UI against a color-role enum and an
   action-enum keymap with hardcoded defaults, so wiring TOML on top later is
   purely additive. *(furnace: color roles `gui.h:162`, action enum `gui.h:767`,
   def tables `guiConst.cpp`.)*
5. **Never-broken app via a legacy bridge (recommended).** During the port,
   render the existing `scrbuffer`/`colorbuffer` to an SDL texture and show it
   inside an ImGui window. Panels are then carved out to real ImGui one at a
   time while the rest of the editor keeps working. Optional but strongly
   de-risks the migration and keeps intermediate builds shippable.

### Library / build choices
- **Dear ImGui**, *docking* branch (dockable, movable panels), vendored under
  `extern/imgui/`, built as a static lib by CMake. Backends:
  `imgui_impl_sdl2` + `imgui_impl_sdlrenderer2`.
- **toml++** (header-only, `extern/`) for the config file. Requires **C++17** —
  bump `CMAKE_CXX_STANDARD` from 14 → 17 (ImGui and the current code are fine
  with 17).
- Embed one **monospace** font (pattern grid) + one **UI** font, plus a merged
  icon font; scale everything by a single `dpiScale`.

---

## 3. Milestones

> Numbering continues from the completed work: M1 = CMake + C++ (done),
> plus the cleanup. These are UI milestones M2–M7. Each should build and run.

### M2 — ImGui foundation (shell alongside the legacy path)
Goal: an ImGui window renders, audio + data loading still work, legacy editor
visible via the bridge. No functional ImGui editor yet.
- Add a CMake option `GTULTRA_IMGUI` (default OFF initially). When ON, GTUltra
  creates its **own** SDL2 window + ImGui context and does **not** init bme
  gfx/win/kbd/mou; it *keeps* `bme_snd` (audio) and `bme_io` (data).
- Vendor ImGui (docking) + the two backends; build as a static lib.
- Implement the thin `Render` interface + `SDL_Renderer` backend.
- Implement the `drawHalt`/`WAKE_UP` main loop, a menu bar, and the ImGui demo
  window as a smoke test.
- **Legacy bridge:** blit `gfx_screen` (or `scrbuffer`) to an `SDL_Texture`,
  display as an `ImGui::Image` in a "Legacy" window. Feed ImGui mouse/keyboard
  back into the old `key`/`rawkey`/`mousex`/`mousey` globals so the legacy UI
  is *interactive* inside ImGui. → app fully usable, now hosted by ImGui.
- Font + theme scaffolding: load mono + UI fonts, define the color-role enum
  with a default (dark) theme, `dpiScale` handling.

### M3 — Input / action layer
Goal: replace hardcoded key switches with a data-driven action system (also the
foundation the config keymap needs later).
- Define `enum Action` partitioned by context (global / pattern / order /
  instrument / tables), each with a def table `{name, friendlyName,
  defaultChord[]}`. Chord = single int (SDL keycode | modifier bits).
  *(furnace: `guiConst.cpp` `guiActions[]`, encoding `gui.h:1095`.)*
- Build reverse maps (chord → action) per context; on keypress compose the
  chord and dispatch through one `doAction(Action, ctx)`.
- Route `doAction` into the existing `*commands()` editing semantics, keeping
  the undo dirty-check wrapper from `docommand()`. Where `*commands()` read
  `key`/`rawkey` globals, refactor them to take the resolved action/char.
- Pull the non-input concerns out of `waitkeymouse` (autosave timer, MIDI poll,
  QWERTY jamming) into their own per-frame update calls.

### M4 — Pattern grid (the crux)
Goal: a real ImGui pattern editor replacing the legacy pattern panel.
Follow Furnace's `drawPattern()` recipe (`src/gui/pattern.cpp`):
- One window with `ImGuiWindowFlags_HorizontalScrollbar`; draw everything on
  `GetWindowDrawList()`.
- Precompute per-frame metrics from the **mono** font (`CalcTextSize("A").x`
  as the cell unit) and column X offsets into arrays (`chanX[]`, `fineOffsets[]`).
- **Virtual scrolling:** size the window to the full pattern height so the
  scrollbar is natural, but only emit the visible row slice; pad with dummy
  rows so the cursor can center.
- **Layered draw per visible row:** row backgrounds (edit row / playhead /
  beat-highlights) → selection rect → cursor + blinking caret → cell text
  (note/instr/vol/fx), all colored from the color-role palette.
- **Interaction decoupled from drawing:** one `ItemAdd` reserves the grid for
  input; map mouse pos → (channel, column, row) by scanning the precomputed
  offset arrays; editing flows through the M3 action layer.
- Wire selection/marks and cursor to `editorInfo` (`eppos`, `epcolumn`,
  `epchn`, `epview`, `epmark*`) and `GTOBJECT.editorUndoInfo.editorInfo[].epnum`.

### M5 — Remaining panels
Port the other views to ImGui, retiring their legacy `display*` counterparts:
- Order list → ImGui tables *(furnace `orders.cpp`)*.
- Instrument editor, the 4 tables (wave/pulse/filter/speed), song info,
  transport bar, top bar. (`displayTable` is only ~40 lines — a good *first*
  panel to prototype before M4 if we want an easy win.)
- Modal dialogs (file load/save, char editor, palette editor, MIDI select)
  → ImGui popups/windows.
- Remove each panel from the legacy bridge as it is ported.

### M6 — Remove the legacy renderer + bme gfx/win
Goal: delete dead code once every panel is ImGui.
- Remove the legacy bridge, `gdisplay.cpp`/`ginfo.cpp` drawing, the
  `fliptoscreen`/chargen renderer in `gconsole.cpp`, `mousecommands` and the
  old input loop.
- Drop bme `gfx`/`win`/`kbd`/`mou` from the build (keep `snd`/`io`/`end` until
  their own replacement milestone). Flip `GTULTRA_IMGUI` to the default/only
  path.

### M7 — Configuration system (TOML) — *scheduled later*
Goal: user-configurable everything (themes, fonts, all keybindings) in a real
TOML file. Builds on the M3 action layer and M2 color-role/font scaffolding.
Design refined from Furnace's system (Furnace uses a flat home-grown
`key=value` format — we improve on it):
- **Format:** `config.toml` with real sections and native types
  (`[general]`, `[audio]`, `[fonts]`, `[colors]`, `[keybinds]`), plus
  `active_theme`/`active_keymap` selectors and named `[theme.<name>]` presets
  — something Furnace lacks. Use quoted `"#RRGGBBAA"` colors and human-readable
  chord strings (`["Ctrl+S", "Ctrl+Shift+S"]`); no base64/int-packing hacks.
- **Serialization seam:** one small config class wrapping toml++ with typed
  `get(key, default)` accessors, so the file format is swappable in isolation.
  *(furnace: `src/engine/config.{h,cpp}` is the analog.)*
- **Settings model:** a typed settings struct with mirror `read()`/`write()`
  functions (default at the call site, missing key ⇒ default = zero-migration
  for new options). *(furnace: `src/gui/settings/loadSettings.cpp`.)*
  Prefer a single defaults table over Furnace's duplicated defaults.
- **Colors:** the M4 color-role enum + def table (name, label, default),
  serialized under `[colors]`; derive ImGui style + hover/active shades in code
  from a few base roles. Import/export + named presets.
- **Keybinds:** the M3 action enum serialized under `[keybinds]` as chord
  strings; a rebinding UI (capture next keypress); import/export keymaps.
- **Fonts:** family/path/size per role + `ui_scale` under `[fonts]`; changing
  them triggers an ImGui atlas rebuild.
- **On disk:** `$XDG_CONFIG_HOME/gtultra/config.toml` (Linux),
  platform-appropriate elsewhere; atomic write via temp-file + `rename`, keep
  one `.bak`. Keep ImGui window layout in a *separate* `imgui.ini`.
- Migrate the existing ad-hoc `gtultra.cfg` settings into this system.

---

## 4. Risks & open questions
- **Mouse hit-testing rewrite** is the largest chunk — it's fully baked into the
  text-grid geometry and has no clean seam; it gets rewritten per panel in
  M4/M5, not ported.
- **`editorInfo` vs `GTOBJECT` split state** — both hold editor cursor/edit
  state; the UI must keep them consistent. Consider consolidating during M3.
- **Model functions reading `editorInfo`** — many `gsong.cpp` operations assume
  "current instrument/pattern" from globals. Decide per-function whether to add
  explicit-index overloads (cleaner) or keep populating `editorInfo`.
- **Legacy bridge cost** — worth it for a never-broken app, but it's real
  plumbing (texture upload + input remap). Alternative is a faster but riskier
  big-bang view rewrite. Recommendation: build the bridge.
- **QWERTY "jamming" / MIDI note entry** — currently entangled in
  `waitkeymouse`; must be re-expressed as ImGui-driven input in M3.
- **C++17 bump** for toml++ (and generally desirable).

## 5. Suggested first step
Prototype **M2** (ImGui shell + SDL_Renderer + the legacy-framebuffer bridge)
behind `-DGTULTRA_IMGUI=ON`. That yields a running, ImGui-hosted GTUltra with
the old editor fully usable inside it — the safe platform to iterate from — and
a natural place to prototype `displayTable` as the first native panel.
