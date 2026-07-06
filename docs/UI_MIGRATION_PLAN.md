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
   ImGui frame loop (draw every frame + vsync)                                      |
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
  `expandOrderListView` / `songOrderPatterns[]` caveat below).
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
  `gimgui_draw_one_table`). Ignore for now.
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
>
> **Still open:** mouse → actions, refactor `*commands()` off `key`/`rawkey`
>   globals, expanded order list, pattern power shortcuts (transpose, …).

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

> **Status: read-only grid landed.** Custom `ImDrawList` "Pattern" window:
> monospace metrics (`CalcTextSize`), row virtualization, per-field coloring
> (note/instr/cmd, dimmed REST + dots for empty fields), beat-row + cursor-row +
> cursor-channel highlighting, `===` for ENDPATT, auto-follows the edit cursor.
> Reads live model via new `guimodel` pattern accessors (`pattern_cell`,
> channel/cursor/step queries; note-name decode via the legacy `notename`
> table). Verified against the legacy view (headers `CH0 0A/CH1 0C/CH2 0B`,
> row 0 `C-2 01 F07` / `C-3 0F 105`).
>
> **Interactive (transition approach): reuse the legacy edit engine.** Keyboard
> editing already works — the legacy `patterncommands` handles note/hex entry,
> cursor moves, and undo, and the grid mirrors it live (verified: Shift+Down
> drove the selection). So the ImGui grid adds only **mouse**: clicking a cell
> calls `gtui::pattern_set_cursor` (sets `EDIT_PATTERN` + `epchn/eppos/epcolumn`
> + `masterLoopChannel`), using the legacy click→column mapping; keyboard then
> flows to the legacy editor untouched. Cursor is a per-sub-field cell box;
> Shift+Up/Down selection drawn as a blue background. (Model-write verified in
> process; the click itself needs a real display — ImGui gets no mouse focus
> under headless Xvfb.)
>
> Still to do toward full M4: keyboard-driven editing owned by the ImGui layer
> (for the eventual legacy removal, M6), then re-skin the SID Tables with this
> same grid widget.

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
  2. **Rework the palette/preset subsystem into clean C++** — small, isolated,
     currently C-with-globals (`char *paletteNames[16]`, `char* paletteText[]`,
     raw `malloc`/`sprintf` in [gpaletteeditor.cpp](../src/gpaletteeditor.cpp)),
     and it becomes the **color-theme system in M7**. A `std::vector<Palette>`
     with `std::string` names + load/save methods is a low-risk warm-up that
     produces reusable design. Good candidate for the *first* concrete task.

## 5. Suggested first step
Either:
- **(a) M2 scaffold** — layer ImGui over the running app (`-DGTULTRA_IMGUI=ON`):
  reuse bme's window/renderer + `sdlTexture`, draw it as the base layer, draw a
  menu bar + demo window on top. Yields a running, ImGui-hosted GTUltra with the
  old editor fully usable inside it — the safe platform to iterate from — and a
  natural place to prototype `displayTable` (~40 lines) as the first native
  panel. Best for momentum.
- **(b) Palette/preset C++ rework** — small, self-contained, de-risks M7. Best
  as a low-risk warm-up that yields reusable design.
