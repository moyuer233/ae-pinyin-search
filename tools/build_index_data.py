# -*- coding: utf-8 -*-
"""Write the palette's index as a newline-delimited data file.

Format: one record per line, fields separated by '|', no quoting needed because
the fields are pre-sanitised (see build_palette.py). This keeps the ScriptUI
script small and makes the data trivial to parse inside ExtendScript (no JSON
parser, no giant string literal in the script itself).

Columns: name | english | full pinyin | initials | isEffect(1/0)
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(r"H:\ae-pinyin-search")
INDEX = ROOT / "build" / "ae-index.json"
OUT = ROOT / "extension" / "AEIndex.txt"


def field(s: str) -> str:
    return (s or "").replace("|", "/").replace("\n", " ").replace("\r", " ").strip()


def main() -> int:
    data = json.loads(INDEX.read_text(encoding="utf-8"))
    lines: list[str] = []

    for e in data["effects"]:
        lines.append("|".join([
            field(e["n"]), field(e.get("e", "")),
            field(e.get("f", "")), field(e.get("i", "")), "1",
        ]))
    for e in data["presets"]:
        lines.append("|".join([
            field(e["n"]), field(e.get("e", "")),
            field(e.get("f", "")), field(e.get("i", "")), "0",
        ]))

    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"records: {len(lines)}")
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
