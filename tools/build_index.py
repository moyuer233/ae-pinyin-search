# -*- coding: utf-8 -*-
"""Build the pinyin search index for the AE floating search bar.

Sources (read-only):
  - AE's own zh_CN localization dictionary  -> effect display names
  - AE's Presets folder (.ffx file names)   -> preset display names
  - third-party .aex file names             -> third-party effect names
  - PresetEffects.xml matchnames            -> english names where available

Output: build/ae-index.json (gen_pinyin_data.py turns it into the table that is
compiled into the plug-in). The JSON carries no timestamp, so two runs over the
same inputs are byte-identical; the "when" is written to build/ae-index.stamp.

Every entry carries a precomputed match key, so the plug-in only does substring
scans.

Exit code = number of problems, including missing inputs: an index that quietly
shrinks (a new After Effects version moved the folders) has to fail the step
instead of producing a plug-in that cannot find anything.
"""
from __future__ import annotations

import json
import re
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

from pypinyin import Style, lazy_pinyin

import ae_paths
from build_aex_effects import MIN_EFFECTS, NON_EFFECT_FOLDERS

AE = ae_paths.ae_support()
DICT = AE / "Dictionaries" / "zh_CN" / "after_effects_zh_CN.dat"
PRESETS = AE / "Presets"
PRESET_XML = AE / "PresetEffects.xml"
AEX_EFFECTS = ae_paths.BUILD / "aex-effects.json"
BUILD = ae_paths.BUILD

# Floors below which the inputs clearly did not show up (this machine: 223
# dictionary effects, 679 presets, ~1270 third-party effects).
MIN_DICT_EFFECTS = 100
MIN_PRESETS = 100

PAIR = re.compile(r'^"(\$\$\$/[^=]+)=(.*)"\s*$')
EFFECT_PREFIX = "$$$/AE/Effect/Name/"

# Effects that exist in newer builds under different names, plus entries whose zh
# value is missing in this build's dictionary but whose display name is well known.
MANUAL_EFFECTS: list[tuple[str, str]] = [
    # (english name, chinese display name) - only for names the dictionary lacks
    ("Gaussian Blur", "高斯模糊"),
    ("Fast Box Blur", "快速方框模糊"),
    ("Camera Lens Blur", "摄像机镜头模糊"),
    ("Curves", "曲线"),
    ("Lumetri Color", "Lumetri 颜色"),
    ("Levels", "色阶"),
    ("Motion Tile", "动态拼贴"),
    ("Turbulent Displace", "湍流置换"),
    ("Fractal Noise", "分形杂色"),
    ("Ramp", "渐变"),
    ("Gradient Ramp", "渐变擦除"),
    ("Fill", "填充"),
    ("Tint", "着色"),
    ("Drop Shadow", "投影"),
    ("Motion Blur", "运动模糊"),
    ("Time Remap", "时间重映射"),
    ("Timewarp", "时间扭曲"),
    ("Posterize Time", "色调分离时间"),
    ("Echo", "残影"),
    ("CC Force Motion Blur", "CC 强制运动模糊"),
    ("Lens Flare", "镜头光晕"),
    ("S_Glow", "S_Glow"),
]


def clean(zh: str) -> str:
    return (zh.replace("\\r\\r", " ").replace("\\r", " ")
              .replace("\\n", " ").replace('\\"', '"').replace("\\\\", "\\")).strip()


def split_english(key: str) -> str:
    """BoxBlur -> Box Blur ; 3DGlasses -> 3D Glasses ; Tracker3D -> Tracker 3D"""
    s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", key)
    s = re.sub(r"(?<=[A-Z])(?=[A-Z][a-z])", " ", s)
    s = re.sub(r"(?<=[0-9])(?=[A-Z][a-z])", " ", s)
    s = re.sub(r"(?<=[a-z])(?=[0-9])", " ", s)
    s = re.sub(r"(?<=[0-9])(?=[a-z])", " ", s)
    # Keep "3 D" / "2 D" together as "3D" / "2D".
    s = re.sub(r"\b([0-9]) ([Dd])\b", lambda m: m.group(1) + "D", s)
    return re.sub(r"\s+", " ", s).strip()


def full_pinyin(zh: str) -> str:
    parts = lazy_pinyin(zh, style=Style.NORMAL, errors=lambda x: list(x))
    return re.sub(r"[^a-z0-9]", "", "".join(parts).lower())


def initials(zh: str) -> str:
    out: list[str] = []
    for ch in zh:
        if "\u4e00" <= ch <= "\u9fff":
            py = lazy_pinyin(ch, style=Style.NORMAL)
            if py and py[0]:
                out.append(py[0][0])
        elif ch.isascii() and ch.isalpha():
            out.append(ch.lower())
    return re.sub(r"[^a-z]", "", "".join(out))


def english_tokens(s: str) -> str:
    """Letters+digits of an english name, lowercased, for ascii matching."""
    return re.sub(r"[^a-z0-9]", "", s.lower())


# ---------------- effects from the dictionary ----------------
def parse_effects_from_dict() -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    if not DICT.exists():
        return rows
    with DICT.open("r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            if line.startswith("\ufeff"):
                line = line[1:]
            m = PAIR.match(line)
            if not m:
                continue
            key, val = m.group(1), m.group(2)
            if not key.startswith(EFFECT_PREFIX):
                continue
            eng_key = key[len(EFFECT_PREFIX):]
            if "/" in eng_key:
                continue
            zh = clean(val)
            if zh:
                rows.append((split_english(eng_key), zh))
    return rows


# ---------------- presets from .ffx file names ----------------
def parse_presets() -> list[tuple[str, str, str]]:
    """-> (category, name, source_path_relative)"""
    rows: list[tuple[str, str, str]] = []
    if not PRESETS.exists():
        return rows
    for p in sorted(PRESETS.rglob("*.ffx")):
        rel = p.relative_to(PRESETS)
        rows.append((str(rel.parent), p.stem, str(rel)))
    return rows


def preset_effects_from_xml() -> list[tuple[str, str]]:
    """PresetEffects.xml: <Effect matchname="ADBE X" name="$$$/AE/Preset/K=English">"""
    rows: list[tuple[str, str]] = []
    if not PRESET_XML.exists():
        return rows
    try:
        tree = ET.parse(PRESET_XML)
    except ET.ParseError:
        return rows
    for el in tree.getroot().iter("Effect"):
        mn = el.get("matchname") or ""
        nm = el.get("name") or ""
        eng = nm
        if nm.startswith("$$$/"):
            eng = nm.split("=", 1)[1] if "=" in nm else mn
        eng = eng.replace("ADBE ", "").strip()
        if eng:
            rows.append((eng, clean(mn)))
    return rows


def make_entry(name: str, english: str, kind: str, extra: dict | None = None) -> dict | None:
    if name.isascii():
        # Third-party effects are registered under english names: index the latin
        # name itself, so typing "s_chrom" or "ambient" hits directly.
        fp = english_tokens(name)
        ini = ""
    else:
        fp = full_pinyin(name)
        ini = initials(name)
    if not fp and not english:
        return None
    ent = {
        "n": name,                 # display name (what the panel shows)
        "e": english,              # english name (ascii matching / tooltip)
        "k": kind,                 # effect | preset
        "f": fp,                   # full pinyin
        "i": ini,                  # initials
        "a": english_tokens(english),
    }
    if extra:
        ent.update(extra)
    return ent


def main() -> int:
    problems = 0
    missing: list[str] = []
    for path, what in (
        (DICT, "the zh_CN dictionary"),
        (PRESETS, "the Presets folder"),
        (PRESET_XML, "PresetEffects.xml"),
        (AEX_EFFECTS, "build/aex-effects.json (run build_aex_effects.py first)"),
    ):
        if not path.exists():
            missing.append(f"{what} not found: {path}")

    effects: dict[str, dict] = {}

    dict_effects = parse_effects_from_dict()
    for english, zh in dict_effects:
        e = make_entry(zh, english, "effect")
        if not e:
            problems += 1
            continue
        effects.setdefault(zh, e)

    for english, zh in MANUAL_EFFECTS:
        e = make_entry(zh, english, "effect", {"manual": 1})
        if not e:
            problems += 1
            continue
        effects.setdefault(zh, e)

    # --- third-party effects, discovered from .aex file names ---
    third = 0
    if AEX_EFFECTS.exists():
        try:
            aex = json.loads(AEX_EFFECTS.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            print(f"  !! cannot read {AEX_EFFECTS}: {exc}")
            aex = {"effects": []}
        for row in aex.get("effects", []):
            name = row.get("n") or ""
            if not name:
                continue
            key = name.lower()
            if key in effects:
                continue
            e = make_entry(name, row.get("e") or name, "effect", {
                "vendor": row.get("vendor") or "",
                "top": row.get("top") or "",
                "src": "aex",
            })
            if not e:
                problems += 1
                continue
            effects[key] = e
            third += 1
        print(f"third-party effects merged: {third}")

    presets: dict[str, dict] = {}
    for cat, name, rel in parse_presets():
        e = make_entry(name, name, "preset", {"cat": cat, "path": rel})
        if not e:
            problems += 1
            continue
        presets.setdefault(f"{cat}/{name}", e)

    eff_list = sorted(effects.values(), key=lambda x: x["n"])
    pre_list = sorted(presets.values(), key=lambda x: (x.get("cat", ""), x["n"]))

    # Floors and one invariant, checked before anything is written: these are the
    # failures that used to come out as "effects: 22  presets: 0  problems: 0".
    if len(dict_effects) < MIN_DICT_EFFECTS:
        missing.append(f"only {len(dict_effects)} effect(s) from the dictionary (expected >= {MIN_DICT_EFFECTS})")
    if len(pre_list) < MIN_PRESETS:
        missing.append(f"only {len(pre_list)} preset(s) (expected >= {MIN_PRESETS})")
    if third < MIN_EFFECTS:
        missing.append(f"only {third} third-party effect(s) (expected >= {MIN_EFFECTS})")
    leaked = sorted({e.get("top", "") for e in eff_list if e.get("top", "").lower() in NON_EFFECT_FOLDERS})
    if leaked:
        missing.append("no-effect folders leaked into the index: " + ", ".join(leaked))

    data = {
        "source": {
            "dict": str(DICT),
            "presets": str(PRESETS),
            "presetXml": str(PRESET_XML),
            "aex": str(AEX_EFFECTS),
        },
        "counts": {
            "effects": len(eff_list),
            "thirdParty": third,
            "presets": len(pre_list),
        },
        "effects": eff_list,
        "presets": pre_list,
    }

    print(f"effects: {len(eff_list)}  presets: {len(pre_list)}  third-party: {third}")
    print("\nsample effects:")
    for e in eff_list[:8]:
        print(f"  {e['n']:<18} en={e['e']:<22} full={e['f']:<20} ini={e['i']}")

    print("\nsample presets:")
    for e in pre_list[:5]:
        print(f"  {e['n']:<20} cat={e.get('cat','')[:28]:<30} full={e['f']:<18} ini={e['i']}")

    if missing:
        print()
        for line in missing:
            print(f"  !! {line}")
        print("FAIL: not writing an index that is missing its inputs")
        return problems + len(missing)

    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / "ae-index.json").write_text(
        json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    # Provenance lives in its own file, so the JSON itself stays byte-comparable
    # between two runs over the same inputs.
    (BUILD / "ae-index.stamp").write_text(
        time.strftime("%Y-%m-%d %H:%M:%S") + "\n", encoding="utf-8")
    print(f"wrote {BUILD / 'ae-index.json'}")
    return problems


if __name__ == "__main__":
    sys.exit(main())
