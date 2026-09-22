# -*- coding: utf-8 -*-
"""Build the effect list from .aex file names.

Two kinds of .aex live under the plug-in folders:

  * effects - AE's own (Support Files\\Plug-ins\\Effects\\*.aex) and third-party
    ones (BorisFX, Sapphire, ... anywhere), whose file name is the name the
    plug-in registers (S_ChannelSwitcher.aex -> "S_ChannelSwitcher",
    Ambient Light.aex -> "Ambient Light");
  * plug-ins that are NOT effects - importers/exporters, keyframe assistants and
    the extension managers. Those sit in folders AE owns: Format, Keyframe,
    Extensions. They never appear in the effect menu, so an index row for one of
    them can only end up as "AE 里找不到这个效果" when it is applied.

Read-only. Writes build/aex-effects.json.
Exit code = 0, or 1 when a scan root is missing or the result is implausibly
small - a silently shrinking index is worse than a failed step.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

import ae_paths

OUT = ae_paths.BUILD / "aex-effects.json"

# Folders AE owns that hold no effects at all (importers, keyframe assistants,
# extension managers, our own plug-in). Compared against the TOP-level folder
# name only, so a vendor folder nested anywhere else is never skipped.
NON_EFFECT_FOLDERS = {"format", "keyframe", "extensions"}

# This machine scans ~1270 effect files; anything far below that means a root
# moved (a new AE version) rather than "the user uninstalled everything".
MIN_EFFECTS = 200

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
    skipped: dict[str, int] = {}
    problems: list[str] = []

    roots = (ae_paths.mediacore(), ae_paths.ae_plugins())
    for root in roots:
        if not root.exists():
            problems.append(f"scan root missing: {root}")
            continue
        for p in sorted(root.rglob("*.aex")):
            try:
                rel = str(p.relative_to(root))
            except ValueError:
                rel = str(p)
            top = top_of(rel)
            if top.lower() in NON_EFFECT_FOLDERS:
                skipped[top] = skipped.get(top, 0) + 1
                continue

            vendor = vendor_of(rel)
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
        "source": {
            "mediacore": str(roots[0]),
            "aePlugins": str(roots[1]),
            "skippedFolders": skipped,
        },
        "count": len(rows),
        "byGroup": {},
        "effects": rows,
    }
    for r in rows:
        g = r["vendor"] or r["top"]
        payload["byGroup"][g] = payload["byGroup"].get(g, 0) + 1

    if len(rows) < MIN_EFFECTS:
        problems.append(f"only {len(rows)} effect file(s) found (expected >= {MIN_EFFECTS})")

    if problems:
        for line in problems:
            print(f"  !! {line}")
        print("FAIL: refusing to overwrite the effect list with this scan")
        return 1

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(payload, ensure_ascii=False, indent=1), encoding="utf-8")

    print(f"effect files: {len(rows)}")
    for v, c in sorted(payload["byGroup"].items(), key=lambda x: -x[1]):
        print(f"  {c:>5}  {v}")
    if skipped:
        print("skipped (no effects live there):")
        for folder, c in sorted(skipped.items()):
            print(f"  {c:>5}  {folder}\\")
    print(f"wrote {OUT}")
    print("\nsamples:")
    for r in rows[:12]:
        print(f"  {r['n']:<28} vendor={r['vendor'] or '-':<10} top={r['top']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
