# -*- coding: utf-8 -*-
"""Scan AE preset files and report their real names (UTF-8 safe).

Also looks for candidate localization data files that may hold effect names.
Read-only.
"""
from __future__ import annotations

import sys
from pathlib import Path

AE = Path(r"H:\adobe\Adobe After Effects 2026\Support Files")
PRESETS = AE / "Presets"

out = Path(__file__).with_name("_preset-scan.txt")
lines: list[str] = []


def emit(s: str) -> None:
    lines.append(s)


emit(f"# presets root: {PRESETS}")
ffx = sorted(PRESETS.rglob("*.ffx"))
emit(f"# ffx count: {len(ffx)}")

by_dir: dict[str, list[str]] = {}
for p in ffx:
    rel = p.relative_to(PRESETS)
    by_dir.setdefault(str(rel.parent), []).append(p.stem)

for d in sorted(by_dir):
    emit("")
    emit(f"[{d}]  ({len(by_dir[d])})")
    for name in sorted(by_dir[d]):
        emit(f"  {name}")

# ffx files are XML; pull the effect display name / matchName where present.
emit("")
emit("=== ffx internals (first 5) ===")
import re

for p in ffx[:5]:
    try:
        raw = p.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:  # noqa: BLE001
        emit(f"{p.stem}: unreadable ({exc})")
        continue
    mats = re.findall(r'matchName="([^"]+)"', raw)[:4]
    names = re.findall(r'name="([^"]+)"', raw)[:4]
    emit(f"{p.stem}: matchNames={mats} names={names}")

# Look for other name-bearing data files under AE.
emit("")
emit("=== candidate name data files under Support Files ===")
cands = []
for pat in ("*.xml", "*.json", "*.dat", "*.txt"):
    for p in AE.rglob(pat):
        if "Presets" in p.parts:
            continue
        if p.stat().st_size > 4_000_000:
            continue
        cands.append((p.stat().st_size, p))
cands.sort(reverse=True)
for size, p in cands[:40]:
    emit(f"{size:>10}  {p.relative_to(AE)}")

out.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {out} ({len(lines)} lines, {len(ffx)} presets)")
