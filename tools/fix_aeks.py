# -*- coding: utf-8 -*-
"""Set one key's binding inside AE's keyboard-shortcut file.

The aeks file is a UTF-8 text INI. Editing it by hand is error-prone (the file
contains Chinese comments and long backslash-continued lines), so this tool
changes exactly the target line and leaves every other byte alone.

Usage:
  python fix_aeks.py --file <path> --set ExecuteScriptMenuItem02= --set ExecuteScriptMenuItem01=(Ctrl+Alt+U)

A value of '' means an empty binding: "()".
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def to_binding(value: str) -> str:
    """Turn 'Ctrl+Alt+U' into '(Ctrl+Alt+U)'; empty string becomes '()'."""
    v = value.strip()
    if not v or v == "()":
        return "()"
    if not (v.startswith("(") and v.endswith(")")):
        v = "(" + v + ")"
    return v


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", required=True)
    ap.add_argument("--set", action="append", default=[],
                    help="KEY=VALUE ; VALUE may be empty to clear the binding")
    ap.add_argument("--no-backup", action="store_true")
    args = ap.parse_args()

    path = Path(args.file)
    if not path.exists():
        print(f"FAIL: not found: {path}")
        return 1

    wanted: dict[str, str] = {}
    for item in args.set:
        if "=" not in item:
            print(f"FAIL: bad --set '{item}', expected KEY=VALUE")
            return 1
        key, value = item.split("=", 1)
        wanted[key.strip()] = to_binding(value)

    if not wanted:
        print("FAIL: nothing to set")
        return 1

    original = path.read_text(encoding="utf-8")
    lines = original.splitlines(keepends=True)

    changes: list[tuple[int, str, str]] = []
    out: list[str] = []
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        replaced = False
        for key, binding in wanted.items():
            prefix = '"' + key + '"'
            if stripped.startswith(prefix) and "=" in stripped:
                old_value = stripped.split("=", 1)[1].strip()
                if old_value == binding:
                    break
                indent = line[: len(line) - len(line.lstrip())]
                ending = "\r\n" if line.endswith("\r\n") else ("\n" if line.endswith("\n") else "")
                out.append(f'{indent}"{key}" = "{binding}"{ending}')
                changes.append((i, old_value, binding))
                replaced = True
                break
        if not replaced:
            out.append(line)

    if not changes:
        print("no change needed (already in the requested state)")
        return 0

    if not args.no_backup:
        bak = path.with_suffix(path.suffix + ".bak")
        shutil.copy2(path, bak)
        print(f"backup: {bak}")

    new_text = "".join(out)
    path.write_text(new_text, encoding="utf-8")

    # Verify: same line count, and only the intended lines differ.
    after = path.read_text(encoding="utf-8").splitlines()
    before = original.splitlines()
    print(f"lines before={len(before)} after={len(after)}")
    diffs = [i for i, (a, b) in enumerate(zip(before, after), 1) if a != b]
    print(f"changed lines: {diffs}")
    for i, old, new in changes:
        print(f"  L{i}: {old}  ->  {new}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
