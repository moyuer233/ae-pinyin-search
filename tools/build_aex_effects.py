# -*- coding: utf-8 -*-
"""Build the third-party effect list from .aex file names.

This is the reliable source: every installed effect ships exactly one .aex, and
its file name is the effect name the plug-in registers
(S_ChannelSwitcher.aex -> "S_ChannelSwitcher", Ambient Light.aex -> "Ambient Light").

Read-only. Writes build/aex-effects.json
"""
from __future__ import annotations

import json
import re
from pathlib import Path

MEDIACORE = Path(r"C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore")
AE_PLUGINS = Path(r"H:\adobe\Adobe After Effects 2026\Support Files\Plug-ins")
OUT = Path(r"H:\ae-pinyin-search\build\aex-effects.json")

# Vendor detection from the path, so the panel can group / label results.
# These are real vendors only; anything unmatched keeps its top-level folder name.
VENDORS = [
    ("BorisFX", "Boris FX"),
    ("Sapphire Plug-ins", "Sapphire"),
    ("Red Giant Universe", "Red Giant Universe"),
    ("Red Giant VFX", "Red Giant VFX"),
    ("Trapcode", "Trapcode"),
    ("Magic Bullet", "Magic Bullet"),
    ("Maxon Studio", "Maxon"),
    ("VideoCopilot", "Video Copilot"),
]

# Suffixes that identify the binary, not the effect.
VARIANTS = re.compile(r"(_64|_32|_x64)$")


def vendor_of(rel: str) -> str:
    for needle, label in VENDORS:
        if needle.lower() in rel.lower():
            return label
    return ""  # let the panel show the top-level folder instead


def top_of(rel: str) -> str:
    parts = rel.split("\\")
    if not parts:
        return ""
    # A bare file at the root: use its own name region.
    return parts[0] if len(parts) > 1 else ""


def clean_name(stem: str) -> str:
    s = stem.strip()
    s = re.sub(r"\s+", " ", s)
    return s


def main() -> int:
    rows: list[dict] = []
    seen: set[str] = set()

    for root in (MEDIACORE, AE_PLUGINS):
        if not root.exists():
            continue
        for p in sorted(root.rglob("*.aex")):
            try:
                rel = str(p.relative_to(root))
            except ValueError:
                rel = str(p)
            vendor = vendor_of(rel)
            top = top_of(rel)
            stem = p.stem
            base = VARIANTS.sub("", stem)

            name = clean_name(base)
            if not name or len(name) < 2:
                continue

            key = name.lower()
            if key in seen:
                continue
            seen.add(key)

            rows.append({
                "n": name,              # display name (english)
                "e": name,
                "k": "effect",
                "vendor": vendor,       # "" when unknown
                "top": top,
                "src": "aex",
                "file": rel,
            })

    rows.sort(key=lambda r: (r["vendor"] or r["top"], r["n"]))
    payload = {
        "source": {"mediacore": str(MEDIACORE), "aePlugins": str(AE_PLUGINS)},
        "count": len(rows),
        "byGroup": {},
        "effects": rows,
    }
    for r in rows:
        g = r["vendor"] or r["top"]
        payload["byGroup"][g] = payload["byGroup"].get(g, 0) + 1

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(payload, ensure_ascii=False, indent=1), encoding="utf-8")

    print(f"third-party effects: {len(rows)}")
    for v, c in sorted(payload["byGroup"].items(), key=lambda x: -x[1]):
        print(f"  {c:>5}  {v}")
    print(f"wrote {OUT}")
    print("\nsamples:")
    for r in rows[:12]:
        print(f"  {r['n']:<28} vendor={r['vendor'] or '-':<10} top={r['top']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
