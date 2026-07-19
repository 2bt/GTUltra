# M6 — Legacy Renderer Removal

Status: **Phase 0–6 done** (chargen teardown complete on `2bt`). Remaining work is
smoke/regression + post-M6 parity — not further chargen deletion.
Parent roadmap: [UI_MIGRATION_PLAN.md](UI_MIGRATION_PLAN.md) (M2–M5 largely done; M7 config deferred).

**Phase progress**

| Phase | Status |
|-------|--------|
| 0 ImGui-only / kill Legacy toggle | **done** |
| 1 Replace hard chargen modals (Help, reloc errors, follow extract) | **done** |
| 2 Delete dead Legacy subsystems | **done** |
| 3 ImGui-first present | **done** |
| 4 Delete chargen rasterize + assets | **done** |
| 5 Input dead-code sweep | **done** |
| 6 Optional bme surface shrink | **done** |

This document is the complete teardown plan for removing GTUltra's chargen /
text-mode renderer while keeping the ImGui editor as the sole UI. It is
intentionally more detailed than the short M6 bullet list in the parent plan.

**Product reference:** `GTUltra.pdf` (v1.5.3). Feature-parity gaps that are
*not* chargen-dependent (detailed tables, filter HUD, transport knobs, etc.)
are explicitly **out of scope** for M6 — they can land afterwards on the
single ImGui surface.

---

## 1. Goal

Make GTUltra an **ImGui-only** application:

- No dual UI / Legacy toggle.
- No chargen cell buffers, `fliptoscreen` rasterize, or text-mode panels.
- No dead modal UIs that only existed for the chargen skin (char editor,
  palette editor, legacy file selector screens, chargen MIDI select, F12
  chargen help).
- Presentation owned by a thin SDL present path that draws ImGui (still
  using bme `win`/`gfx` as the window/renderer owner in this milestone).

Editing semantics (`*commands()`, undo, player, song model) stay.

---

## 2. Non-goals (explicitly deferred)

| Item | Why deferred |
|------|----------------|
| Detailed wave/pulse/filter table UIs + waveform editor (PDF §33–36) | ImGui feature work; no chargen dependency |
| Filter HUD, song total time, multi-song slots, drag-drop load, etc. | Same — post-M6 parity |
| Full TOML config / themes / keybind UI (M7) | Separate milestone |
| Replacing bme entirely (snd/io/end rewrite, SDL3) | Separate modernization track |
| Moving window/renderer ownership out of bme `gfx`/`win` | Optional late M6 phase; not required to delete chargen |
| Rewriting `*commands()` into idiomatic C++ | Keep; only delete draw-only and Legacy-gated forks |
| Shipping feature-perfect transport / piano overlay | Piano can be disabled or stubbed until an ImGui port |

---

## 3. Current architecture (what we are removing)

```
editor_frame_update
  └─ displayupdate → doDisplay
        ├─ [ImGui on]  updateDisplayWhenFollowingAndPlaying  (STATE — keep)
        │              optional displayKeyboard path          (DRAW — remove/port)
        │              gfx_flip → bme_overlay_render_hook     (PRESENT — keep/reshape)
        └─ [Legacy]    printstatus → print*/fillArea → fliptoscreen → gfx_flip

initscreen (gconsole)
  └─ win_openwindow + gfx_init + load chargen.bin/cursor → gimgui_init(window, renderer)

gfx_flip (bme_gfx)
  └─ upload legacy texture (may be empty/dirty-none) → ImGui overlay → SDL_RenderPresent
```

Today ImGui is an **overlay** on the legacy present path, not a standalone
presenter. Deleting chargen without first making present ImGui-first will
black-screen the app.

Default UI is ImGui-only. Dual-UI toggle (`g_show_new_ui` / `guiflags.cpp` /
`gimgui_new_ui_active()`) and `editPaletteMode` / `transportShowKeyboard` have
been removed. Chargen code that remains is present-path / asset residue for
Phases 3–4.

---

## 4. Inventory

### 4.1 Legacy display / modal files

| Path | ~Lines | Fate |
|------|--------|------|
| `src/gdisplay.cpp` / `.hpp` | ~3344 | **Split:** extract follow-play state helpers; delete all drawing |
| `src/gconsole.cpp` / `.hpp` | ~965 | **Shrink:** keep `getkey` / init / present glue; delete chargen buffers + `fliptoscreen` rasterize |
| `src/ginfo.cpp` / `.hpp` | ~481 | **Keep string builders** (feeds `gtui::context_help`); delete leftover `printtext` |
| `src/ghelp.cpp` | ~408 | **Replace then delete** (F12 still calls `onlinehelp` under ImGui) |
| `src/gchareditor.cpp` | ~320 | **Delete** |
| `src/gpaletteeditor.cpp` | ~806 | **Delete** (themes → M7) |
| `src/gmidiselect.cpp` | ~129 | **Delete** after confirming ImGui MIDI combo parity |
| `src/gfile.cpp` `fileselector()` | part of ~658 | **Delete UI**; keep non-UI file helpers if any remain |
| `src/greloc.cpp` interactive screens | part of ~2875 | **Keep packing engine**; replace error/progress UI; drop chargen option screens for editor path |
| `src/gt2stereo.cpp` `mousecommands`, skin clicks, bootstrap splash | large | **Delete mousecommands + Legacy skin UI**; keep loop / jam / MIDI |

### 4.2 Assets (embedded via `goatdata`)

| Asset | Fate |
|-------|------|
| `chargen.bin` | Drop from datafile once rasterize is gone |
| `cursor.bin` / `bcursor.bin` | Drop (SDL/ImGui cursor) |
| `palette.bin` + `src/gtpalettes/*.gtp` | Drop with palette/skin UI (ImGui uses `guicolors`) |
| Player `.s` / other embedded blobs | **Keep** |

### 4.3 Dual-UI / behavior gates (must collapse)

| Symbol | Location | Action |
|--------|----------|--------|
| `g_show_new_ui` | `guiflags.cpp` | Remove; ImGui is always on |
| `gimgui_new_ui_active()` | same / `gimgui.hpp` | Delete or make `constexpr true` then remove call sites |
| Legacy / New UI transport buttons | `gimgui.cpp` | Remove |
| `if (gimgui_new_ui_active())` forks | `gdisplay`, `gt2stereo`, `gpattern`, `gorder`, `gtable`, `ginstr`, `gactions` | Keep the ImGui branch; delete Legacy branch |

### 4.4 Still-live chargen entry points under ImGui today

These **block** deleting chargen until replaced or removed:

1. **F12 Help** — `Action::Help` → `stopScreenDisplay` / `onlinehelp` / `restartScreenDisplay` (`gactions.cpp`, `ghelp.cpp`).
2. **SID export errors / progress** — even `relocator(gt, 0, 1)` (autoSave) can `printtext` / `waitkeynoupdate` on failure (`greloc.cpp`).
3. **`transportShowKeyboard`** — if set, `doDisplay` still draws piano via chargen (usually unreachable from ImGui mouse path, but still compiled in).
4. **Bootstrap / sound-init failure splash** — `printtext` paths in `gt2stereo.cpp`.
5. **Present path** — `gfx_flip` + overlay hook; must become ImGui-first present before deleting the legacy texture upload.

### 4.5 Must keep (not render)

- Model / edit: `gsong`, `gpattern`, `gorder`, `ginstr`, `gtable`, `gplay`, `gundo`, `gfkeys`, …
- ImGui stack: `gimgui`, `guimodel`, `gactions`, `gfiledialog`, `guicolors`, `ginput`, `guiflags` (until toggle dies)
- `updateDisplayWhenFollowingAndPlaying*` **state updates** (cursor/follow sync while playing)
- `ginfo` / `guimodel` context-help **string** path (ImGui chrome bar already consumes it)
- Main loop: `waitkeymouse` / `editor_frame_update` / `getkey` / jam / MIDI (reshape later; do not delete)
- `relocator` packing + native save dialog wiring
- bme **`snd`**, **`io`**, **`end`**; **`win`/`gfx`** as window/renderer owner for this milestone

---

## 5. Phased plan

Each phase should leave the tree **buildable and runnable**. Prefer one
focused PR/commit series per phase. Do not skip acceptance checks.

### Phase 0 — Preconditions / freeze Legacy — **DONE**

**Intent:** Stop investing in the dual path; make ImGui the only supported mode
in daily use before deleting code.

Tasks:

1. ~~Remove the **Legacy** transport button and the **New UI** fallback bar.~~
2. ~~Force ImGui-only (`gimgui_new_ui_active()` always true; remove `g_show_new_ui`).~~
3. ~~Force `transportShowKeyboard = 0` (piano overlay disabled until ImGui port).~~
4. ~~Update plan docs to “M6 in progress”.~~
5. ~~`doDisplay` no longer calls `printstatus` / chargen piano.~~

**Acceptance:**

- App boots straight into ImGui; no way to re-enter chargen panels from UI.
- Pattern / order / instr / table / names editing still works.
- No regressions in file load/save, play, undo.

**Exit:** Dual-UI is dead in product terms; chargen code is now unreferenced
from normal UX (except Help / reloc errors / present path).

---

### Phase 1 — Replace hard chargen modals — **DONE**

**Intent:** Eliminate the remaining *runtime* callers of chargen UI under the
ImGui path.

#### 1a. Help (F12) — done

- ImGui Help **modal** (`gimgui_open_help` / F12 / View→Help; Esc = Cancel).
- Keybind tabs are generated from `gtaction` (`binding_rows_for`); reference
  tabs stay authored in `ghelp.cpp`. Intra-context chord collisions surface via
  `conflicts_for()` in Help UI and CLI `-??`.
- CLI `-??` walks `gthelp::topics()` then `gthelp::print_reference()`.

#### 1b. Relocator errors / progress — done

- `gt_ui_error` / `reloc_alert` replace chargen wait loops on the autoSave
  export path. Interactive `!autoSave` option screens remain until Phase 2
  (unreachable from ImGui Relocate).

#### 1c. Bootstrap / fatal errors — done

- `-?` / `-??` are stdout CLI; sound-init failure uses `gt_ui_warn`.

#### 1d. Follow-play extraction — done

- `gfollow.cpp` / `gfollow.hpp` own follow-play state updates; `doDisplay`
  calls them without `printstatus`.

**Acceptance:** met for ImGui path (Phase 2 deletes remaining dead callers).

---

### Phase 2 — Delete dead Legacy subsystems — **DONE**

**Intent:** Remove unreachable chargen-only code and CMake/data references.

Completed:

1. Deleted `gchareditor.*`, `gmidiselect.*`, `gpaletteeditor.*` (boot loaders
   extracted to `gpalette.cpp` / `gpalette.hpp`).
2. Deleted `mousecommands`, `mouseTransportBar`, and related
   `checkForMouse*` / `mouseTrack*` helpers from `gt2stereo.cpp`.
3. Replaced chargen `fileselector()` with a stub; ImGui uses `gfiledialog`.
4. Skin-cycle / char / palette / MIDI modal click paths removed with transport
   mouse bar; `setSkin` / `initPaletteDisplay` kept for chargen palette until
   Phase 4.
5. `gdisplay.cpp` reduced to ~108 lines (present + follow-play + timers +
   note names). Chargen draw tree gone.
6. CMake / includes updated; dual-UI forks collapsed on hot edit-mode paths.

**Acceptance:** met (`gtultra` / `gt2reloc` build).

---

### Phase 3 — ImGui-first present path — **DONE**

**Intent:** Stop depending on chargen cell buffers for presentation.

Completed:

1. Added `gfx_present()` — clear → overlay hook → `SDL_RenderPresent` (no
   chargen texture upload / `RenderCopy`).
2. Editor frame path (`doDisplay` / `printmainscreen`) calls `gfx_present()`.
3. Still uses bme `win_window` + `gfx_renderer`; overlay hook remains the
   ImGui draw entry.
4. `gfx_flip()` kept for any leftover `fliptoscreen` callers (reloc interactive
   / waitkey stubs) until Phase 4/5.

**Acceptance:** met.

---

### Phase 4 — Delete chargen rasterize + assets — **DONE**

**Intent:** Remove the text-mode renderer itself.

Done:

1. `gconsole.cpp` slimmed to window init + `getkey` + no-op `print*` /
   `fillArea` / `clearscreen`. `fliptoscreen` → `gfx_present()`.
2. Chargen / cursor / `palette.bin` / `goattrk2.bmp` dropped from
   `CMakeLists.txt` `DATAFILE_INPUTS` and `gt2stereo.seq`.
3. OS cursor shown (`MOUSE_ALWAYS_VISIBLE`); software cursor sprite gone.
4. `*.gtp` presets **kept** for `loadPalettes` / `setSkin` boot colours
   (ImGui still uses `guicolors`; dropping `.gtp` can wait until palette
   boot is cleaned).

**Acceptance:** met (no `chargen.bin` open; datafile pack excludes chargen
assets; `gtultra` / `gt2reloc` build).

---

### Phase 5 — Input / loop cleanup (render-adjacent only) — **DONE**

**Intent:** Remove input code that existed only to drive the text grid, without
yet redesigning the whole editor loop.

Done:

1. Removed unused chargen mouse helpers (`checkMouseRange`,
   `checkMouseInWaveformInfo`, detailed table click mutators).
2. Removed unused `waitkeymousenoupdate`. Kept `waitkeynoupdate` for
   `greloc` `!autoSave` sources (unreachable from ImGui Relocate).
3. Left `waitkeymouse` / `editor_frame_update` / jam / MIDI intact.
4. Remaining Legacy naming (`getkey`, `rawkey`, …) is tech debt — not
   blocking M6 done. Quit/clear prompts still use `printtext` (no-op) +
   `waitkey` — post-M6 ImGui confirms.

**Acceptance:** met.

---

### Phase 6 — Optional: shrink bme gfx surface path — **DONE**

**Intent:** Align with parent plan’s “drop bme gfx/win” *direction* without
requiring a full bme rewrite.

Done:

1. `gfx_init` no longer allocates INDEX8 `gfx_screen` or streaming texture.
2. `gfx_flip` → `gfx_present`; `mou_getpos` / `gfx_get_view` use virtual size.
3. Kept `win_openwindow`, `win_checkmessages`, `gfx_renderer`, modal dimming.
4. Full SDL ownership extraction out of bme remains post-M6
   (`MODERNIZATION.md`).

**Acceptance:** met (no unused fullscreen software surface for text cells).

---

## 6. Suggested commit / PR slicing

Prefer small merges that bisect cleanly:

| Slice | Contents |
|-------|----------|
| M6.0 | Force ImGui-only; remove Legacy toggle UI |
| M6.1 | ImGui help; delete `ghelp` |
| M6.2 | Reloc/bootstrap error reporting without chargen waits |
| M6.3 | Extract follow-play; stop calling `printstatus` |
| M6.4 | Delete char/palette/MIDI-select/fileselector/mousecommands |
| M6.5 | ImGui-first present |
| M6.6 | Delete chargen rasterize + assets |
| M6.7 | Input dead-code sweep + docs/parity table update |

Avoid a single megacommit that mixes Help UI with asset deletion.

---

## 7. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Black screen after deleting `fliptoscreen` | Phase 3 before Phase 4; feature-flag skip rasterize while present still works |
| Follow-play cursor desync | Extract and test Phase 1d before deleting `gdisplay` |
| SID export failure hangs or silent-fails | Explicit ImGui/log error path; manual fail test |
| F12 crashes | Phase 1a first; don’t delete `ghelp` until action retargeted |
| `gt2reloc` / shared `greloc.cpp` break | Keep `GT2RELOC` stubs; test both `gtultra` and `gt2reloc` targets |
| Window size still tied to chargen metrics | Accept in M6; free sizing can wait |
| Accidental use of Legacy toggle during teardown | Phase 0 removes it immediately |
| Over-deletion of `ginfo` string logic | Only delete drawing; keep `infoTextBuffer` producers until ImGui context bar is self-contained |

---

## 8. Regression checklist (run before declaring M6 done)

Manual smoke (ImGui path only):

- [ ] Boot, resize window, quit cleanly
- [ ] Load / save `.sng` (F10 / F11 / Ctrl+S)
- [ ] Export SID (success + deliberate failure)
- [ ] Export WAV
- [ ] Play / pause / stop / rewind / ff / follow / loop
- [ ] Pattern edit: notes, hex columns, mark, copy/paste, undo
- [ ] Order list: classic + expanded, insert/delete, play from row
- [ ] Instruments + all four tables: navigate, edit, Enter jumps
- [ ] Song name / author / copyright fields
- [ ] MIDI port combo + note input (if device available)
- [ ] QWERTY jam / record
- [ ] F12 help opens and closes without freezing input
- [ ] Mouse wheel scrolls the hovered panel
- [ ] Context help chrome bar updates with cursor
- [ ] `gt2reloc` still builds and runs on a sample song

Parent plan §6 rows marked 🔧 should be spot-checked; failures are bugs to
fix, not a reason to keep chargen.

---

## 9. Definition of done

M6 is complete when all of the following are true:

1. No Legacy / dual-UI toggle exists; ImGui is the only editor UI.
2. `gdisplay` drawing, `gchareditor`, `gpaletteeditor`, `ghelp` chargen help,
   `gmidiselect`, and `fileselector` UI are gone from the build.
3. No chargen assets are required at runtime or in `goatdata`.
4. Frame present is ImGui-first (clear → ImGui → present); no cell-buffer
   rasterize.
5. Follow-play, file I/O, export, and core editing work on the smoke list
   above.
6. bme `snd`/`io` remain; `win`/`gfx` may remain as SDL owners.
7. This document and [UI_MIGRATION_PLAN.md](UI_MIGRATION_PLAN.md) §M6 are
   marked **done**, with §6 parity symbols updated where status changed.
8. Remaining parity (detailed tables, filter HUD, …) is tracked as post-M6
   work — not as M6 blockers.

---

## 10. Relationship to later work

```
M6 (this doc)          → ImGui-only app, chargen dead
post-M6 parity         → detailed tables, filter HUD, transport completeness, …
M7                     → config.toml themes / keybinds / fonts
Replace bme            → own SDL window/audio without bme_gfx/win
```

Doing M6 now is correct: leftover visual features do not need the chargen
renderer, and keeping it only preserves a second UI surface and thousands of
lines of dead draw code.

---

## 11. Quick reference — delete vs keep

**Delete (by end of M6):**
`gchareditor.*`, `gpaletteeditor.*`, `ghelp` (after ImGui help),
`gmidiselect.*`, `mousecommands`, Legacy toggle, `printstatus` / `display*`
draw tree, chargen buffers + `fliptoscreen` rasterize, chargen/cursor/palette
assets, skin/palette edit modes.

**Keep:**
`*commands` semantics, undo, player, `gactions`/`guimodel`/`gimgui`/
`gfiledialog`/`guicolors`, follow-play **state** helpers, context-help
**strings**, main loop + `getkey` (for now), `relocator` engine, bme snd/io,
SDL window/renderer ownership (bme win/gfx OK for now).
