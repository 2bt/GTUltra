# Slim goattrk2.hpp Includes Implementation Plan

> **For agentic workers:** Implement task-by-task. Steps use checkbox syntax.

**Goal:** Slim `goattrk2.hpp` to declaration-needed includes; fix compile fallout; one opportunistic non-umbrella include.

**Architecture:** Header includes only `gfile.hpp` + `greloc.hpp` + std headers. Callers that broke get direct includes. `gfollow.hpp` drops the umbrella.

**Tech Stack:** CMake C++ project; targets `gtultra`, `gt2reloc`.

## Global Constraints

- Do not commit unless user asks.
- Prefer smallest fix that restores the build.

---

### Task 1: Slim header + opportunistic B

- [x] Replace includes in `src/goattrk2.hpp` with `<cstdint>`, `<cstdio>`, `gfile.hpp`, `greloc.hpp`
- [x] Change `src/gfollow.hpp` to `#include "gplay.hpp"`

### Task 2: Fix compile fallout

- [x] `cmake --build build --target gtultra gt2reloc` and add missing includes until clean

### Task 3: Verify

- [x] Confirm both targets link successfully
