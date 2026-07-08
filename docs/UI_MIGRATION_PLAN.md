# GTUltra UI Modernization Plan — Dear ImGui + Config System

Status: **in progress** (M2–M5 largely landed on branch `2bt`; M6/M7 remain).
This document is the agreed roadmap for replacing GTUltra's legacy text-mode UI
with Dear ImGui, and for adding a proper configuration system (themes, fonts,
keybindings).

**Product reference:** `GTUltra.pdf` (v1.5.3) — the numbered feature list in
§6 below tracks parity against that manual.

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
   ImGui frame loop (draw every frame + vsync)                                       |
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
2. **Just draw every frame with vsync** (corner cut, agreed). No
   redraw-on-demand for now — the loop renders unconditionally and
   `SDL_RenderPresent` (renderer created with `PRESENTVSYNC`) paces it. Furnace's
   `drawHalt`/`WAKE_UP` power-saving idiom *(furnace: `src/gui/gui.cpp:4342`,
   macro `gui.h:54`)* is a pure add-on we can bolt on later if idle CPU ever
   matters.
3. **Pattern grid = custom `ImDrawList`, not ImGui widgets.** This is the crux.
   *(furnace: `src/gui/pattern.cpp:83` — the single most important file to
   study.)* Order list can use ImGui tables *(furnace: `src/gui/orders.cpp`)*.
4. **Theme + actions are first-class from day one** — even before the config
   *file* exists (M7). Build the UI against a color-role enum and an
   action-enum keymap with hardcoded defaults, so wiring TOML on top later is
   purely additive. *(furnace: color roles `gui.h:162`, action enum `gui.h:767`,
   def tables `guiConst.cpp`.)*
5. **Never-broken app by layering the existing legacy frame inside ImGui.**
   bme *already* renders the whole editor into an `SDL_Texture` every frame:
   `gfx_flip` does `SDL_ConvertSurfaceFormat` → `SDL_UpdateTexture(sdlTexture)`
   → `SDL_RenderCopy` → `SDL_RenderPresent` ([bme_gfx.c:72-77](../src/bme/bme_gfx.c#L72)).
   That `sdlTexture` (currently a `static` in bme_gfx.c) is the complete old UI
   as a GPU texture — i.e. an `ImTextureID`, essentially free to reuse. So:
   **ImGui owns the window; the legacy frame is drawn as a base layer inside it,
   and real ImGui panels composite on top.** Panels are carved out one at a
   time while the rest of the editor keeps working — testable at every step,
   intermediate builds shippable.

   Progressive handoff has two levers: (a) cover a ported region with an opaque
   ImGui window — optionally blanking that region in the old text buffer so
   nothing shows through — and (b) gate input with
   `io.WantCaptureMouse/WantCaptureKeyboard` so clicks in a finished panel don't
   also reach the legacy `mousecommands`/`getkey` path.

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

### M2 — ImGui foundation (layer ImGui over the running app)
Goal: the full legacy editor keeps working, now composited inside an
ImGui-owned window, with ImGui able to draw panels on top. No functional ImGui
editor yet — this is the platform to iterate from.

> **Status: M2 complete + first native panel landed.**
> - Step 1: Dear ImGui (docking 1.92.9) vendored under `extern/imgui`, wired
>   into CMake behind `-DGTULTRA_IMGUI=ON` (C++17), composited over the running
>   editor via bme's renderer + overlay/event hooks.
> - Legacy base-layer: `gfx_flip` letterboxes the frame to an explicit dest rect
>   (fell out of the mouse/HiDPI fix); ImGui draws over it in full window pixels.
> - Step 2: input handoff — `getkey` consults `bme_input_capture_hook`; when
>   ImGui reports `WantCaptureMouse/Keyboard`, legacy input is swallowed. Default
>   build leaves the hook NULL (no-op).
> - **First native panel:** a read-only "SID Tables" window
>   (wave/pulse/filter/speed) reading live model state, with cursor highlight and
>   an `ImGuiListClipper`. Introduces `guimodel.{h,cpp}` — a **SDL-free
>   read/query bridge** between the legacy model and the ImGui layer (so ImGui
>   never pulls bme's bundled SDL). Verified against the legacy tables via
>   screenshot.
>
> Model/view seam: ImGui panels read the legacy globals only through
> `guimodel` — extend it per panel.
>
> - **Editable panel done:** the SID Tables cells are editable hex fields that
>   write via `gtui::table_set`, which brackets the write in the legacy undo
>   system (create editor info → mark `UNDO_AREA_TABLES+t` L/R → mutate →
>   validate). Edits show live in both UIs and are Ctrl-Z-undoable. Verified
>   in-process (51→62→undo→51).

### Decisions & notes
- **Instrument view = editable table (deviates from legacy).** One instrument
  per row (columns: name, AD, SR, wave/pulse/filter/vibrato pointers, vib delay,
  gate, 1st-frame wave, pan). Built as a native `ImGui::BeginTable` with a
  **text input** for the name and **hex inputs** for the fields (not the custom
  tracker grid) — appropriate for a data table and it sidesteps the keyboard-axis
  problem (ImGui owns focus/editing, no legacy keyboard nav). Edits commit on
  defocus/Enter through `gtui::instr_set_field`/`instr_set_name`, which bracket
  the write in the legacy undo system (`UNDO_AREA_INSTRUMENTS`). Verified
  write+undo round-trip.
- **Known limitation: order-list cursor keys.** With the order list now vertical
  but keyboard editing still going through the legacy (horizontal-layout) engine,
  the arrow keys move along the wrong axis. Accepted for the transition; it
  resolves when the ImGui order panel owns its own keyboard nav (part of legacy
  removal, M6).
- **Deferred: expanded order list.** A GTUltra-specific feature (not in original
  GoatTracker). Support it in the ImGui order panel later (relates to the
  `expandOrderListView` / `songOrderPatterns[]` caveat below). Full checklist:
  **§6 Order list** (PDF §42–47).
- **Order list ported (vertical, modern).** Chosen over the legacy horizontal
  layout: positions are rows, channels are columns (Furnace-style), reusing
  `gimgui_grid_body`. Cells decode to pattern numbers or commands (`+X`/`-X`
  transpose, `RX` repeat, `RST` loop) from `songorder[]`; click places the cursor
  (`gtui::order_set_cursor` → `EDIT_ORDERLIST`), keyboard via the legacy editor.
  Caveat: reads the *normal* `songorder[]`; when the legacy "expanded order list"
  mode (`expandOrderListView`) is active it edits `songOrderPatterns[]`, so the
  vertical view can lag until that data is synced — revisit if it matters.
- **Shared grid scaffold extracted.** `gimgui_grid_body` (a template in
  `gimgui.cpp`) owns the reusable parts of a scrolling monospace grid: child
  window, content reservation, row virtualization, no-lag cursor-follow (per-grid
  state via ImGui child storage), and click hit-testing. Each grid passes a
  `drawRow` and `onClick` lambda for its own cells/colours. Both the pattern
  editor and the SID tables use it; the order list (next grid panel) should too.
- **Deferred: table "detailed" view.** A GTUltra addition (not in original
  GoatTracker): clicking a table header (all except the speed table) toggles an
  alternative view that interprets the raw bytes in a more user-friendly way.
  Legacy code: `modifyWaveTableDetailed*` / `detailedTable*` /
  `checkForMouseInDetailed*Table` in `gtable.cpp`/`gdisplay.cpp`. Re-add an
  equivalent in the ImGui tables eventually (there's a `TODO` on the header in
  `gimgui_draw_one_table`). Full checklist: **§6 Instruments & tables** (PDF
  §33–36).
- **Tables panel re-skinned (done).** The stock `InputScalar` stopgap was
  replaced with a custom `ImDrawList` grid matching the pattern editor:
  monospace `II:LL RR` cells, cursor cell box (per `etcolumn`), Shift-select
  blue background, and click-to-place-cursor (`gtui::table_set_cursor` →
  `EDIT_TABLES`), with keyboard editing flowing through the legacy `tablecommands`
  (same reuse-the-legacy-engine approach as the pattern grid). Both grids now
  share one interaction model.
- **Transition model = overlay-and-cover, not a window split.** The legacy UI
  renders the whole editor into one framebuffer (monolithic), so a hardcoded
  split would just shrink+duplicate it. Instead, ImGui panels sit over the
  legacy frame and each ported panel eventually blanks its legacy region, until
  nothing legacy remains (then delete the legacy renderer, M6). A
  "legacy-as-a-dockable-window" variant was considered and declined — the plain
  overlay is fine.
- **Look must be "native tracker", not "ImGui app"** (user directive). The UI
  now uses a **fixed tiled layout** (no floating windows, no title bars, no
  user-dockable panels): every frame `gimgui_overlay_render` computes panel rects
  from the viewport and each panel is a `NoTitleBar|NoMove|NoResize` window with a
  custom edge-to-edge section header (`gimgui_begin_panel`). Opaque panels + a
  full-window background fill cover the legacy screen entirely. Layout: left col
  = Pattern (top) + SID Tables (bottom); right col = Order / Instruments / Song.
- **Font**: bundled **IBM Plex Mono** (`assets/fonts/`, OFL) loaded at 18px in
  `gimgui_init` (searched next-to-binary → `GTULTRA_ASSETS_DIR` → cwd). Replaces
  the tiny default ImGui bitmap font. Per-role font config is deferred to M7.
- **Style**: flat dark theme with a blue accent, square windows, minimal borders
  (`gimgui_apply_style`) — deliberately unlike `StyleColorsDark`. Full theme
  config is M7.

Next: the pattern grid (M4), starting read-only.

Reuse bme's **existing** window/renderer/frame rather than standing up a second
one. bme already exposes `SDL_Window *win_window` (bme_win.h:34) and
`SDL_Renderer *gfx_renderer` (bme_gfx.h:46), and already builds the whole frame
into `sdlTexture` inside `gfx_flip`.

- Add a CMake option `GTULTRA_IMGUI` (default OFF initially). Bump
  `CMAKE_CXX_STANDARD` 14 → 17.
- Vendor ImGui (docking branch) + `imgui_impl_sdl2` + `imgui_impl_sdlrenderer2`;
  build as a static lib.
- Init ImGui on bme's existing objects:
  `ImGui_ImplSDL2_InitForSDLRenderer(win_window, gfx_renderer)` +
  `ImGui_ImplSDLRenderer2_Init(gfx_renderer)`.
- **Three tiny bme hooks** (bme is C, ImGui is C++ → route through a small
  C-callable shim):
  1. expose `sdlTexture` (a getter, or lift ownership out of bme_gfx.c);
  2. add a "texture-only" mode to `gfx_flip` — update `sdlTexture` but skip its
     `SDL_RenderClear`/`RenderCopy`/`RenderPresent` ([bme_gfx.c:75-77](../src/bme/bme_gfx.c#L75));
  3. forward each SDL event to `ImGui_ImplSDL2_ProcessEvent` from inside
     `win_checkmessages` ([bme_win.c:193](../src/bme/bme_win.c#L193)).
- Main loop (per frame): let bme render the legacy frame into `sdlTexture`
  (texture-only), then `NewFrame` → draw the legacy texture as a full-window
  base-layer `ImGui::Image` → draw ImGui panels on top (start with just a menu
  bar + the demo window as a smoke test) → `ImGui::Render` →
  `SDL_RenderPresent` once. **Draw every frame; vsync paces it** (no
  drawHalt).
- Input handoff: gate the legacy `getkey`/`mousecommands` path on
  `!io.WantCaptureKeyboard` / `!io.WantCaptureMouse` so ImGui panels and the old
  UI don't fight over events.
- Font + theme scaffolding: load a mono + a UI font (merge an icon font),
  define the color-role enum with a default (dark) theme, single `dpiScale`.

### M3 — Input / action layer

> **Status: largely complete for the ImGui new UI path.**
> - `gactions.h/cpp`: `Action` enum by context, chord keymap (`kBindings[]`),
>   `resolve` / `dispatch_mode_navigation` / `dispatch_global` / `perform()`,
>   runtime `set_binding()` overrides.
> - `editor_frame_update()` split out of `waitkeymouse` (autosave, MIDI, jam).
> - ImGui panels: navigation + structural edits (insert/delete/copy/cut/paste,
>   page/home/end, jam toggle, …) routed through actions; hex/note/cell entry
>   still delegated to legacy `*commands()` after dispatch misses.
> - Order/table column semantics fixed for vertical ImGui layout (nibble cursor,
>   `EDIT_TABLE_NONE` raw hex, per-table `etview` scroll sync).
> - Hex input: `consume_legacy_hex_input()` prevents trailing global dispatch
>   from stealing digit keys after legacy editors consume a nybble.
> - Legacy `*commands()` key switches gated when ImGui is active (order/table/
>   instrument/pattern panels delegate navigation to actions; note/hex entry only
>   in pattern).
> - Names panel: Up/Down/Enter field navigation via `dispatch_names_navigation`,
>   ImGui `InputText` focus on the Song form.
> - Pattern power shortcuts (transpose, autoadvance, cmd copy/paste, invert,
>   step size, mark-all, pitchbend, portamento helper) in the action layer.
> - Legacy `mousecommands` skipped when ImGui is active (panel grids own clicks).
> - Expanded order-list column navigation via `order_col_*_expanded`.
> - `EditorInput` snapshot (`ginput.h/cpp`): `docommand()` captures keyboard
>   state once per frame and passes it into `*commands()` so handlers read
>   explicit input instead of `key`/`rawkey` globals (hex nybble still uses the
>   legacy global during multi-step entry; `editor_input_clear()` centralizes
>   consume-after-dispatch).
>
> **Deferred to M6:** full removal of input globals inside `*commands()` bodies
> (modifier flags, hex mutation paths).

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

> **Status: interactive grid complete for the new UI path.** Read-only draw +
> mouse hit-testing landed earlier; navigation and structural edits route through
> the M3 action layer; note/hex entry and Enter-driven jumps (instrument/table)
> route through `dispatch_pattern_cell_input()` → `pattern_cell_input()` /
> `pattern_note_input()` / `pattern_hex_input()` (extracted from legacy
> `patterncommands`, still calling `gotoinstr` / `gototable` for cross-panel
> navigation). Legacy `patterncommands()` is no longer called when the new UI is
> active.

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

> **Remaining work vs `GTUltra.pdf`:** see **§6** for the full parity checklist.
> Highest-impact gaps: expanded order list, detailed tables, info line, filter
> HUD, transport bar controls, multi-song slots, drag-drop load.

Port the other views to ImGui, retiring their legacy `display*` counterparts:
- Order list → ImGui tables *(furnace `orders.cpp`)*. **Done.**
- Instrument editor, the 4 tables (wave/pulse/filter/speed), song info,
  transport bar, top bar. **Done** (fixed tiled layout in `gimgui.cpp`).
- Modal dialogs → ImGui popups/windows:
  - **File load/save** — **done (new UI):** [portable-file-dialogs](https://github.com/samhocevar/portable-file-dialogs) via `gfiledialog.{h,cpp}` — songs (F10/F11/Ctrl+S/WAV) and instruments/tables (F10/F11 when that panel has focus). `win_native_modal_*` dims the window and discards queued input while zenity/kdialog is open. Legacy `fileselector()` unchanged until M6.
  - ~~**MIDI device select**~~ — **done:** transport-bar combo (`gimgui_draw_transport`),
    live `setMidiPort()` (no restart). Legacy modal kept until M6.
  - ~~Char editor~~ — **dropped** (chargen/font editing not needed in the new UI).
  - ~~Palette editor~~ — **dropped** (replaced by M7 theme/color roles in
    `config.toml`, not a port of `gpaletteeditor.cpp`).
- Remove each panel from the legacy bridge as it is ported.

### M6 — Remove the legacy renderer + bme gfx/win
Goal: delete dead code once every panel is ImGui.
- Remove the legacy bridge, `gdisplay.cpp`/`ginfo.cpp` drawing, the
  `fliptoscreen`/chargen renderer in `gconsole.cpp`, `mousecommands` and the
  old input loop.
- Delete dropped legacy-only subsystems: `gchareditor.*`, `gpaletteeditor.*`,
  `editPaletteMode` and related palette-preset editing UI (keep only what M7
  needs for runtime theme application).
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
- **Legacy layering cost is low** — we reuse bme's existing window/renderer and
  the `sdlTexture` it already builds, so the bridge is ~3 tiny hooks, not a
  second window + framebuffer copy. Preferred over a big-bang view rewrite.
- **QWERTY "jamming" / MIDI note entry** — currently entangled in
  `waitkeymouse`; must be re-expressed as ImGui-driven input in M3.
- **C++17 bump** for toml++ (and generally desirable); happens in M2.

## 4a. Refactoring strategy (do NOT big-bang the globals)

Tempting question: rewrite the global-state, C-style code into idiomatic C++
(`std::string`/`std::vector`, encapsulation) *before* the UI port. Decision:
**no up-front global refactor.** It has no functional payoff, high regression
risk (globals are load-bearing across ~20 files, the audio thread reads them,
and undo snapshots raw memory regions in `gundo.cpp`), is hard to regression-test
on a GUI tracker, and tends to produce the wrong abstractions when done ahead of
a consumer. Instead:

- **Refactor opportunistically, in the direction of the port** — clean each
  subsystem's interface as its panel is ported (M4/M5), letting the ImGui work
  pull the boundaries out.
- **Two targeted early exceptions that pay off:**
  1. **Consolidate `editorInfo` + the split `GTOBJECT.editorUndoInfo` state**
     (already M3) — both the UI and config bind to it.
  2. ~~**Rework the palette/preset subsystem**~~ — **superseded.** The legacy
     palette editor is dropped; M7 themes are a fresh color-role system in
     `config.toml` (`gimgui_apply_style` + M3 action scaffolding), not a port
     of `gpaletteeditor.cpp`.

## 5. Suggested next steps

Priority order (highest-impact gaps vs `GTUltra.pdf`):

1. **Expanded order list** (PDF §42–47) — ImGui order panel reads classic
   `songorder[]` only; expanded mode still edits `songOrderPatterns[]` in legacy.
2. **Detailed table views + waveform editor** (PDF §33–36) — `TODO` on table
   header in `gimgui_draw_one_table`.
3. **Info line** (PDF §21) — context help was `ginfo.cpp`; no ImGui equivalent.
4. **Filter info display** (PDF §24) — per-channel cutoff/resonance/type HUD.
5. **Transport bar completeness** (PDF §5) — player strip exists; many controls
   still missing (see §6).
6. **M6** — remove legacy renderer bridge and dropped subsystems (`gchareditor`,
   `gpaletteeditor`, legacy `fileselector` path). Verify every 🔧/❓ row in §6
   before deleting legacy code.
7. **M7** — TOML config (themes, keybinds, `gtultra.cfg` migration).

---

## 6. GTUltra feature parity (`GTUltra.pdf` v1.5.3)

The PDF lists **57 numbered features** plus v1.5 changelog items. This section
maps each to ImGui migration status. Numbers match the PDF table of contents.

### Status legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Done in the new ImGui UI path |
| 🔶 | Partial — some behaviour ported or action-layer only |
| ⬜ | Not started in ImGui |
| 🚫 | Dropped — replaced by M7 themes or not porting |
| 🔧 | Legacy-only today — works via `*commands()` / action layer; verify before M6 |
| ❓ | Needs explicit M6 regression check |

### Display & skinning

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 1 | Updated display / skinning (16 palette presets) | 🚫 | M7 color-role themes replace `gtpalette/` presets |
| 25 | Palette editor | 🚫 | M7 `[colors]` + theme presets, not `gpaletteeditor.cpp` |
| 26 | Char editor | 🚫 | Chargen not used in new UI; delete in M6 |

### Core editing (GoatTracker baseline)

Most baseline editing semantics live in `*commands()` and are reached through
the M3 action layer. Rows marked 🔧 need an explicit pass before M6 deletes
legacy input/render paths.

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 2 | Undo (Ctrl-Z) | ✅ | Legacy undo system; all ImGui writes bracketed |
| 8 | 3/6/9/12 channel playback (1–4 SID) | 🔧 | Player model; SID count UI partial in transport |
| 9 | Song pattern selection (shift-click order) | 🔶 | `OrderSelectPatterns` in action layer; ImGui click TBD |
| 10 | Song playback from anywhere | 🔧 | Double-click order; verify ImGui order panel |
| 11 | F3 = Shift+Space (play from cursor) | 🔧 | `PatternPlayFromCursor`; verify all edit modes |
| 12 | Jam mode polyphony (up to 12 ch) | 🔧 | QWERTY jam in `editor_frame_update` |
| 13 | Note / arp chord offsets in jam mode | ⬜ | Transport HUD not ported |
| 14 | MIDI note input | 🔶 | MIDI poll works; piano keyboard overlay missing |
| 16 | Auto prev/next pattern on scroll | 🔶 | CFG option; Shift+click Follow toggle not in ImGui |
| 18 | Auto-portamento (Shift-Y) | 🔧 | `PatternPortamentoHelper` in action layer |
| 20 | Quick save (Ctrl-S) | ✅ | Native file dialog (M5) |
| 23 | F8 → tables | 🔧 | `EditModeTables` action |
| 27 | F2 remapped (non-classic F-keys) | 🔧 | Classic F1–F3 toggle not in ImGui transport |
| 29 | Pattern looping (master-channel sync) | 🔧 | Loop toggle in transport; inter-pattern needs verify |
| 30 | Copy changes (Ctrl-C semantics) | 🔧 | Pattern/order/table/instr copy in action layer |
| 31 | Inter-pattern (marked-area) looping | 🔶 | Requires loop + area-loop both on; transport TBD |
| 32 | ENTER → jump to table / return | 🔶 | Pattern/instr/table cell input wired; verify all paths |
| 37 | MIDI port select | ✅ | Transport combo, live `setMidiPort()` (M5) |
| 38 | Ctrl+Left/Right song position | 🔧 | `SongPosPrev`/`SongPosNext` global actions |
| 53 | Auto-advance modes (Shift-Z) | 🔶 | `PatternToggleAutoAdvance`; no ImGui mode indicator |
| 54 | Mouse wheel scrolls active panel | ⬜ | Legacy `mousecommands`; not in ImGui panels |
| 55 | SIDTracker64 mode (Shift/Ctrl+F12) | 🔶 | Toggle in transport; Enter fill-keyons etc. verify |

### Transport bar (PDF §5)

Partial player strip in `gimgui_draw_transport` / `gimgui_draw_player_status`.
Sub-controls from the PDF:

| PDF §5 | Control | Status | Notes |
|--------|---------|--------|-------|
| a | Change skin (16 presets) | 🚫 | → M7 theme selector |
| a.i | Ctrl+click → palette editor | 🚫 | |
| a.ii | Ctrl+Shift+click → char editor | 🚫 | |
| b | SID count 1–4 | 🔶 | Partial in player status |
| c | Output volume | ⬜ | |
| d | Octave 1–6 | 🔶 | Model state; UI TBD |
| e | Follow on/off | 🔶 | Action exists; verify transport button |
| f | Loop pattern on/off | 🔶 | Action exists |
| g | Selected-area looping | ⬜ | Shift/Ctrl+click loop button; "P" indicator |
| h | Rewind (click / hold / double) | 🔶 | `SongRewind`; hold-to-start-of-song TBD |
| i | Record on/off | 🔶 | Jam/record toggle |
| j | Classic F1–F3 keys | ⬜ | Shift/Ctrl+click record; "F" indicator |
| k | Play / pause | 🔶 | Global play actions |
| l | Fast forward | 🔶 | `SongPosNext` |
| m | Jam-mode SID chip enable (1–4) | ⬜ | Per-chip mute for jam overlay |
| n | Piano keyboard on/off | ⬜ | Note display overlay |
| o | MIDI port (Shift/Ctrl+keyboard icon) | ✅ | Replaced by combo (M5) |
| p | Detune (−100…+100 cents) | ⬜ | |
| q | Mono / stereo / true stereo toggle | 🔶 | Stereo mode cycle action; full UI TBD |

### Stereo & panning

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 4 | Instrument true stereo panning | 🔶 | Pan column in instr table; dual-range random pan UI TBD |
| 6 | True stereo emulation | 🔶 | Player/model; `CycleStereoMode` action |
| 7 | SID chip pan positions (P3/P4 layout) | 🔶 | Per-SID pan sliders in player status (partial) |

### Instruments & tables

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 3 | Instrument use count (IC) | ⬜ | Not shown in ImGui instrument table |
| 17 | Tables separated by colour | 🔶 | Section breaks / muted unused in grid colours |
| 28 | Mouse drag to modify values | ⬜ | Legacy instrument/table panels only |
| 33 | Detailed wave table editing | ⬜ | Deferred — `gimgui_draw_one_table` TODO |
| 34 | Detailed pulse table editing | ⬜ | Deferred |
| 35 | Detailed filter table editing | ⬜ | Deferred |
| 36 | Waveform editor (TEST/RING/SYNC/GATE) | ⬜ | Tied to detailed table / instr editing |

### Order list

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| — | Vertical order list (ImGui) | ✅ | Classic `songorder[]` view |
| 22 | Master channel (yellow arrow) | 🔶 | `>` prefix on master channel header |
| 42 | Expanded order list toggle | ✅ | Classic/Expanded button; `expandAllSongs` / `compressAllSongs` |
| 43 | Expanded — copy/cut/paste/insert | 🔶 | Copy/paste/cut/ins/del via action layer; Ctrl+I insert still legacy path |
| 44 | Expanded — paste transpose only | 🔶 | `escolumn > 2` paste semantics in action layer |
| 45 | Expanded — set transpose values | 🔶 | Hex/+/- via `orderlistcommands`; live audition in player |
| 46 | Expanded — compressed size indicator | ✅ | Per-channel `XX` / `**` in order header |
| 47 | Expanded — repeat / end markers (FF) | 🔶 | FF rows render; loop position as 3-digit value |

CFG options for expanded order list (from PDF v1.5):

- **Use repeats when compressing** — optional; disable for easier editing
- **Auto prev/next pattern on cursor** — optional; Shift+click Follow

### Info, filters & song metadata

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 19 | Song total time display | ⬜ | Auto-calculated; not in ImGui song panel |
| 21 | Info line (cursor context help) | ⬜ | Was `ginfo.cpp` |
| 24 | Filter information (per-channel) | ⬜ | Cutoff/resonance/type HUD above channels |

### File I/O, export & multi-song

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 15 | Load / save screen (F10/F11) | ✅ | Native dialogs; green/red legacy screens dropped |
| 39 | SID export | 🔶 | Native export path; zeropage option below |
| 40 | Automatic `.sng` backup | 🔧 | `gtbackup/` timer in main loop; no ImGui config |
| 41 | Editor settings saved in `.sng` | 🔧 | FV/PO/RO/NTSC/SID model/HR/speed/SID count/stereo |
| 49 | Multiple `.sng` slots | ⬜ | FILE 1/2 toggle; copy between songs |
| 50 | Export to WAV (Shift-F11) | 🔶 | Dialog wired; normalization panel TBD |
| 51 | GT2Reloc standalone | ✅ | Separate tool; not a UI panel |
| 52 | Pattern order on SID export | 🔧 | Export logic (playback order, not UI) |
| 56 | Drag-and-drop `.sng` load | ⬜ | SDL drop events → load |
| 57 | SID export zeropage playback option | ⬜ | 3-channel export dialog option |

### Configuration & MIDI

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| 48 | Disable all MIDI (port 9999) | 🔧 | `gtultra.cfg` / `-m`; migrate to M7 `[audio]` |
| — | Debug memory-check mode | 🔧 | CFG flag; migrate to M7 |
| — | Backup interval (`-b`) | 🔧 | CFG / CLI; migrate to M7 |

### Help

| PDF | Feature | Status | Milestone / notes |
|-----|---------|--------|-------------------|
| — | F12 help screen | ⬜ | Not in PDF TOC; expected from legacy `ghelp.cpp` |

### Cross-reference to milestones

| Milestone | PDF features primarily addressed |
|-----------|----------------------------------|
| M4 | 30–32, 53, 55 (pattern editing) |
| M5 | 15, 37, 42 (classic order), instruments, tables, transport shell |
| M6 | All 🔧/❓ rows — regression pass before legacy deletion |
| M7 | 1, 25, 40, 41, 48, 54 (config), theme replaces skin cycling |
| Post-M5 | 21, 24, 28, 33–36, 42–47, 49, 50, 56, 57, help |

Update the status column in this section when a feature lands; it is the
single parity checklist for the ImGui migration.
