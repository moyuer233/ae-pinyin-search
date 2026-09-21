# -*- coding: utf-8 -*-
"""Inspect PresetEffects.xml and the zh_CN localization dictionary (read-only)."""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

AE = Path(r"H:\adobe\Adobe After Effects 2026\Support Files")
OUT = Path(__file__).with_name("_sources-scan.txt")
lines: list[str] = []


def emit(s: str = "") -> None:
    lines.append(s)


# ---------- 1. PresetEffects.xml ----------
xml_path = AE / "PresetEffects.xml"
emit(f"=== PresetEffects.xml ({xml_path.stat().st_size} bytes) ===")
try:
    tree = ET.parse(xml_path)
    root = tree.getroot()
    emit(f"root tag={root.tag} attrib={root.attrib}")
    tags: dict[str, int] = {}
    for el in root.iter():
        tags[el.tag] = tags.get(el.tag, 0) + 1
    emit(f"tag histogram: {tags}")
    emit("")
    emit("--- first 12 elements with any of name/matchName ---")
    shown = 0
    for el in root.iter():
        keys = {k.lower(): v for k, v in el.attrib.items()}
        if "name" in keys or "matchname" in keys:
            emit(f"<{el.tag}> {el.attrib}")
            shown += 1
            if shown >= 12:
                break
    emit("")
    emit("--- first 5 elements raw (first 400 chars each) ---")
    raw = xml_path.read_text(encoding="utf-8", errors="replace")
    emit(raw[:1200])
except ET.ParseError as exc:
    emit(f"XML parse error: {exc}")
    raw = xml_path.read_text(encoding="utf-8", errors="replace")
    emit(raw[:1500])

# ---------- 2. zh_CN dictionary ----------
emit("")
for name in ("after_effects_zh_CN.dat", "after_effects_en_US.dat"):
    p = AE / "Dictionaries" / ("zh_CN" if "zh" in name else "en_US") / name
    if not p.exists():
        # en_US may live elsewhere; report search result later
        emit(f"=== {name}: NOT FOUND at {p} ===")
        continue
    blob = p.read_bytes()
    emit(f"=== Dictionaries\\{p.parent.name}\\{name} ({len(blob)} bytes) ===")
    emit(f"head hex: {blob[:64].hex(' ')}")
    for enc in ("utf-16-le", "utf-8", "gbk"):
        try:
            text = blob.decode(enc, errors="strict")
        except UnicodeDecodeError:
            emit(f"  {enc}: decode failed (strict)")
            continue
        emit(f"  {enc}: decoded OK, {len(text)} chars")
        sample = text[:600].replace("\x00", "")
        emit(f"  sample: {sample!r}")
        # look for CJK
        cjk = re.findall(r"[\u4e00-\u9fff]{2,}", text)
        emit(f"  CJK runs: {len(cjk)}; first 15: {cjk[:15]}")
        break
    else:
        emit("  no encoding decoded cleanly")

# ---------- 3. locate en_US dictionary ----------
emit("")
emit("=== all Dictionaries subdirs ===")
ddir = AE / "Dictionaries"
for d in sorted(ddir.iterdir()):
    if d.is_dir():
        files = [f.name for f in d.glob("*.dat")]
        emit(f"  {d.name}: {files}")

OUT.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {OUT}")
