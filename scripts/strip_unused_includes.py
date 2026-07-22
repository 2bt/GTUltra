#!/usr/bin/env python3
"""Try removing each top-level #include; keep removals that still compile.

Uses CMake per-object build.make rules (plain `make path/to.o` is a false
positive when the .o already exists). Skips includes inside #if/#ifdef so
platform-guarded headers are left alone.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"

INCLUDE_RE = re.compile(r'^(\s*#\s*include\s+[<"].+[>"])\s*(?://.*)?$')
IF_RE = re.compile(r'^\s*#\s*if(n?def)?\b')
ELSE_RE = re.compile(r'^\s*#\s*el(se|if)\b')
ENDIF_RE = re.compile(r'^\s*#\s*endif\b')

# source -> (build.make relative to build/, object target) list; empty => full rebuild
OBJECTS: dict[str, list[tuple[str, str]]] = {
    "src/goattrk2.hpp": [],
    "src/gfollow.hpp": [],
    "src/gdisplay.hpp": [],
    "src/ginput.hpp": [],
    "src/gactions.cpp": [("CMakeFiles/gtultra.dir/build.make", "CMakeFiles/gtultra.dir/src/gactions.cpp.o")],
    "src/gdisplay.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gdisplay.cpp.o")],
    "src/gfiledialog.cpp": [("CMakeFiles/gtultra.dir/build.make", "CMakeFiles/gtultra.dir/src/gfiledialog.cpp.o")],
    "src/gfollow.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gfollow.cpp.o")],
    "src/ginfo.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/ginfo.cpp.o")],
    "src/ginstr.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/ginstr.cpp.o")],
    "src/gorder.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gorder.cpp.o")],
    "src/gpattern.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gpattern.cpp.o")],
    "src/gplay.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gplay.cpp.o")],
    "src/greloc.cpp": [
        ("CMakeFiles/gtultra.dir/build.make", "CMakeFiles/gtultra.dir/src/greloc.cpp.o"),
        ("CMakeFiles/gt2reloc.dir/build.make", "CMakeFiles/gt2reloc.dir/src/greloc.cpp.o"),
    ],
    "src/gsong.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gsong.cpp.o")],
    "src/gsound.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gsound.cpp.o")],
    "src/gt2reloc.cpp": [("CMakeFiles/gt2reloc.dir/build.make", "CMakeFiles/gt2reloc.dir/src/gt2reloc.cpp.o")],
    "src/gtable.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gtable.cpp.o")],
    "src/gtultra.cpp": [("CMakeFiles/gtultra.dir/build.make", "CMakeFiles/gtultra.dir/src/gtultra.cpp.o")],
    "src/guimodel.cpp": [("CMakeFiles/gtultra.dir/build.make", "CMakeFiles/gtultra.dir/src/guimodel.cpp.o")],
    "src/gundo.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gundo.cpp.o")],
    "src/ginput.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/ginput.cpp.o")],
    "src/gfile.cpp": [("CMakeFiles/gtcore.dir/build.make", "CMakeFiles/gtcore.dir/src/gfile.cpp.o")],
}


def run(cmd: list[str], cwd: Path) -> tuple[bool, str]:
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return r.returncode == 0, r.stdout


def compile_ok(objs: list[tuple[str, str]]) -> bool:
    if not objs:
        ok, out = run(["cmake", "--build", str(BUILD), "--target", "gtultra", "gt2reloc", "-j"], ROOT)
        if not ok:
            # show last error lines for headers
            for line in out.splitlines()[-8:]:
                if "error:" in line:
                    print(f"    {line}", flush=True)
        return ok
    for makefile, obj in objs:
        # Force rebuild: delete object so a missing rule can't false-succeed.
        (BUILD / obj).unlink(missing_ok=True)
        ok, out = run(["make", "-f", makefile, obj], BUILD)
        if not ok:
            for line in out.splitlines():
                if "error:" in line:
                    print(f"    {line}", flush=True)
                    break
            return False
    return True


def final_link_ok() -> bool:
    ok, out = run(["cmake", "--build", str(BUILD), "--target", "gtultra", "gt2reloc", "-j"], ROOT)
    if not ok:
        print(out[-2000:], file=sys.stderr)
    return ok


def top_level_include_indices(lines: list[str]) -> list[int]:
    depth = 0
    idxs: list[int] = []
    for i, ln in enumerate(lines):
        s = ln.rstrip("\n")
        if IF_RE.match(s):
            depth += 1
            continue
        if ENDIF_RE.match(s):
            depth = max(0, depth - 1)
            continue
        if ELSE_RE.match(s):
            continue
        if depth == 0 and INCLUDE_RE.match(s):
            idxs.append(i)
    return idxs


def collapse_blank_lines(text: str) -> str:
    out: list[str] = []
    blanks = 0
    for ln in text.splitlines(keepends=True):
        if ln.strip() == "":
            blanks += 1
            if blanks <= 1:
                out.append("\n" if not ln.endswith("\n") else ln)
        else:
            blanks = 0
            out.append(ln)
    return "".join(out)


def try_strip_file(rel: str, objs: list[tuple[str, str]]) -> list[str]:
    path = ROOT / rel
    lines = path.read_text().splitlines(keepends=True)
    removed: list[str] = []
    idxs = top_level_include_indices(lines)

    for i in idxs:
        # Recompute? indices stay valid if we blank lines in place.
        orig = lines[i]
        if not INCLUDE_RE.match(orig.rstrip("\n")):
            continue  # already blanked earlier somehow
        m = INCLUDE_RE.match(orig.rstrip("\n"))
        assert m
        include = m.group(1).strip()
        lines[i] = "\n"
        path.write_text("".join(lines))
        if compile_ok(objs):
            removed.append(include)
            print(f"  REMOVED {include}", flush=True)
        else:
            lines[i] = orig
            path.write_text("".join(lines))
            print(f"  keep    {include}", flush=True)

    path.write_text(collapse_blank_lines(path.read_text()))
    return removed


def main() -> int:
    print("Baseline rebuild...", flush=True)
    if not final_link_ok():
        print("Baseline build failed; abort.", file=sys.stderr)
        return 1

    report: dict[str, list[str]] = {}
    for rel, objs in OBJECTS.items():
        print(f"\n=== {rel} ===", flush=True)
        report[rel] = try_strip_file(rel, objs)

    print("\nFinal link check...", flush=True)
    if not final_link_ok():
        print("Final link failed after strips!", file=sys.stderr)
        return 1

    print("\n=== SUMMARY ===")
    for rel, removed in report.items():
        print(f"{rel}:")
        if removed:
            for inc in removed:
                print(f"  - {inc}")
        else:
            print("  (none)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
