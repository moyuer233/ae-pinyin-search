# -*- coding: utf-8 -*-
"""Build pinyin entries for AE effect names from the local zh_CN dictionary.

Outputs:
  effects.json        - structured index: english key, zh name, full pinyin, initials
  ae-effects-phrases.txt - "pinyin<TAB>1<TAB>phrase" lines (same shape as the MC phrase lists)
  ms-userdict.dat     - Microsoft Pinyin custom dictionary (mschxudp), only entries
                        that satisfy the format's hard limits

All data comes from AE's own localization dictionary, read-only.
"""
from __future__ import annotations

import json
import re
import struct
import sys
import time
from pathlib import Path

from pypinyin import Style, lazy_pinyin

AE = Path(r"H:\adobe\Adobe After Effects 2026\Support Files")
DICT = AE / "Dictionaries" / "zh_CN" / "after_effects_zh_CN.dat"
ROOT = Path(r"H:\ae-pinyin-search")
OUT_DIR = ROOT / "build"
OUT_DIR.mkdir(parents=True, exist_ok=True)

PAIR = re.compile(r'^"(\$\$\$/[^=]+)=(.*)"\s*$')
PREFIX = "$$$/AE/Effect/Name/"

# Keys that look like a name but are not an effect.
NOT_AN_EFFECT = {
    "BodyTracker", "BeatBinder",  # keep: they are real effect-ish entries? verified below
}


def parse_effects() -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    with DICT.open("r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            if line.startswith("\ufeff"):
                line = line[1:]
            m = PAIR.match(line)
            if not m:
                continue
            key, val = m.group(1), m.group(2)
            if not key.startswith(PREFIX):
                continue
            eng = key[len(PREFIX):]
            if "/" in eng:
                continue
            val = (val.replace("\\r\\r", " ").replace("\\r", " ")
                      .replace("\\n", " ").replace('\\"', '"'))
            val = val.strip()
            if not val:
                continue
            rows.append((eng, val))
    return sorted(set(rows))


def split_english(key: str) -> str:
    """BoxBlur -> Box Blur ; 3DGlasses -> 3D Glasses ; ApplyColorLUT -> Apply Color LUT"""
    s = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", key)
    s = re.sub(r"(?<=[A-Z])(?=[A-Z][a-z])", " ", s)
    s = re.sub(r"(?<=[0-9])(?=[A-Za-z])", " ", s)
    s = re.sub(r"(?<=[A-Za-z])(?=[0-9])", " ", s)
    return re.sub(r"\s+", " ", s).strip()


def full_pinyin(zh: str) -> str:
    parts = lazy_pinyin(zh, style=Style.NORMAL, errors=lambda x: list(x))
    s = "".join(parts).lower()
    return re.sub(r"[^a-z]", "", s)


def initials(zh: str) -> str:
    """First letter of each hanzi; ASCII words kept whole and lowercased."""
    out: list[str] = []
    for ch in zh:
        if "\u4e00" <= ch <= "\u9fff":
            py = lazy_pinyin(ch, style=Style.NORMAL)
            if py and py[0]:
                out.append(py[0][0])
        elif ch.isascii() and ch.isalpha():
            out.append(ch.lower())
    return re.sub(r"[^a-z0-9]", "", "".join(out))


# ---------------- Microsoft Pinyin dictionary (mschxudp) ----------------
def build_dat(entries: list[tuple[str, int, str]], timestamp: int) -> bytes:
    """Same binary layout as H:\\1\\generate_mspy_dict.py (validated there)."""
    n = len(entries)
    if n == 0:
        raise ValueError("no entries")
    blobs: list[bytes] = []
    for pinyin, position, phrase in entries:
        if not (1 <= position <= 9):
            raise ValueError(f"bad position {position!r}")
        if not pinyin.isascii() or not pinyin.islower() or not pinyin.isalpha():
            raise ValueError(f"bad pinyin {pinyin!r}")
        if len(pinyin) > 32:
            raise ValueError(f"pinyin too long: {pinyin!r}")
        p_utf16 = pinyin.encode("utf-16-le")
        ph_utf16 = phrase.encode("utf-16-le")
        p4 = 18 + 2 * len(pinyin)
        blobs.append(
            b"\x10\x00\x10\x00"
            + struct.pack("<H", p4)
            + bytes([position, 0x06])
            + b"\x00\x00\x00\x00"
            + b"\xfe\xca\xef\xbe"
            + p_utf16 + b"\x00\x00"
            + ph_utf16 + b"\x00\x00"
        )
    prefix_size = 0x40 + 4 * n
    offsets: list[int] = []
    off = 0
    for b in blobs:
        offsets.append(off)
        off += len(b)
    total = prefix_size + off
    prefix = bytearray()
    prefix += b"mschxudp"
    prefix += bytes.fromhex("0200600001000000")
    prefix += struct.pack("<I", 0x40)
    prefix += struct.pack("<I", prefix_size)
    prefix += struct.pack("<I", total)
    prefix += struct.pack("<I", n)
    prefix += struct.pack("<I", timestamp)
    prefix += b"\x00" * 28
    for o in offsets:
        prefix += struct.pack("<I", o)
    assert len(prefix) == prefix_size
    return bytes(prefix) + b"".join(blobs)


def main() -> int:
    rows = parse_effects()
    print(f"effects parsed: {len(rows)}")

    records = []
    problems = 0
    for eng, zh in rows:
        fp = full_pinyin(zh)
        ini = initials(zh)
        if not fp:
            print(f"  !! no pinyin for {eng} => {zh!r}")
            problems += 1
            continue
        records.append({
            "key": eng,
            "english": split_english(eng),
            "zh": zh,
            "full": fp,
            "initials": ini,
            "full_len": len(fp),
        })

    (OUT_DIR / "effects.json").write_text(
        json.dumps(records, ensure_ascii=False, indent=1), encoding="utf-8")

    # phrase list: full pinyin + initials, both mapped to the Chinese display name
    phrase_lines: list[str] = []
    seen: set[tuple[str, str]] = set()
    for r in records:
        for py in (r["full"], r["initials"]):
            if len(py) < 2:
                continue
            if (py, r["zh"]) in seen:
                continue
            seen.add((py, r["zh"]))
            phrase_lines.append(f"{py}\t1\t{r['zh']}")
    (OUT_DIR / "ae-effects-phrases.txt").write_text(
        "\n".join(phrase_lines) + "\n", encoding="utf-8")

    # MS IME dictionary: only what the format can carry (pinyin <= 32 ascii lower)
    ent: list[tuple[str, int, str]] = []
    skipped_long = 0
    used_py: set[str] = set()
    for r in records:
        for py in (r["full"], r["initials"]):
            if len(py) < 2 or len(py) > 32 or py in used_py:
                if len(py) > 32:
                    skipped_long += 1
                continue
            used_py.add(py)
            ent.append((py, 1, r["zh"]))
    dat = build_dat(ent, int(time.time()))
    (OUT_DIR / "ae-ms-userdict.dat").write_bytes(dat)

    longest = max((r["full_len"] for r in records), default=0)
    print(f"records: {len(records)}  problems: {problems}")
    print(f"phrase lines: {len(phrase_lines)}")
    print(f"ime entries: {len(ent)}  skipped(too long): {skipped_long}")
    print(f"longest full pinyin: {longest}")
    print(f"wrote: {OUT_DIR / 'effects.json'}")
    print(f"wrote: {OUT_DIR / 'ae-effects-phrases.txt'}")
    print(f"wrote: {OUT_DIR / 'ae-ms-userdict.dat'} ({len(dat)} bytes)")

    # samples for eyeballing
    print("\nsample (first 15):")
    for r in records[:15]:
        print(f"  {r['key']:<28} {r['english']:<28} {r['zh']:<14} full={r['full']:<24} ini={r['initials']}")
    return problems


if __name__ == "__main__":
    sys.exit(main())
