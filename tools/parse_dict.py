# -*- coding: utf-8 -*-
"""Parse AE's zh_CN localization dictionary, line by line (streaming, no big regex).

Finds the key families that hold effect / preset display names.
Read-only. Writes _dict-scan.txt
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

AE = Path(r"H:\adobe\Adobe After Effects 2026\Support Files")
DICT = AE / "Dictionaries" / "zh_CN" / "after_effects_zh_CN.dat"
OUT = Path(__file__).with_name("_dict-scan.txt")

PAIR = re.compile(r'^"(\$\$\$/[^=]+)=(.*)"\s*$')
CJK = re.compile(r"[\u4e00-\u9fff]")

lines_out: list[str] = []
def emit(s: str = "") -> None:
    lines_out.append(s)

pairs: list[tuple[str, str]] = []
bad = 0
with DICT.open("r", encoding="utf-8", errors="replace") as fh:
    for raw in fh:
        line = raw.rstrip("\r\n")
        if line.startswith("\ufeff"):
            line = line[1:]
        m = PAIR.match(line)
        if not m:
            if line.strip():
                bad += 1
            continue
        key, val = m.group(1), m.group(2)
        val = (val.replace("\\r\\r", " ")
                  .replace("\\r", " ")
                  .replace("\\n", " ")
                  .replace('\\"', '"')
                  .replace("\\\\", "\\"))
        pairs.append((key, val))

emit(f"dict: {DICT}")
emit(f"parsed entries: {len(pairs)}   unparsed non-empty lines: {bad}")

fam3: Counter[str] = Counter()
fam4: Counter[str] = Counter()
for key, _ in pairs:
    p = key.split("/")
    fam3["/".join(p[1:4])] += 1
    fam4["/".join(p[1:5])] += 1

emit()
emit("=== top 50 key families ($$$/AE/<3 segments>) ===")
for k, n in fam3.most_common(50):
    emit(f"{n:>6}  {k}")

emit()
emit("=== families with CJK values, top 40 ===")
cfam: Counter[str] = Counter()
for key, val in pairs:
    if CJK.search(val):
        cfam["/".join(key.split("/")[1:4])] += 1
for k, n in cfam.most_common(40):
    emit(f"{n:>6}  {k}")

def dump(title: str, pred, limit: int = 60) -> None:
    hits = [(k, v) for k, v in pairs if pred(k)]
    emit()
    emit(f"=== {title}: {len(hits)} ===")
    for k, v in hits[:limit]:
        emit(f"  {k}  =>  {v[:70]}")

dump("keys containing 'Effect'", lambda k: "Effect" in k)
dump("keys starting $$$/AE/Preset/", lambda k: k.startswith("$$$/AE/Preset/"))
dump("keys matching /EffectName or /Effects/", lambda k: re.search(r"/Effect(Name)?s?/", k) is not None)
dump("keys with 'Filter'", lambda k: "Filter" in k)

emit()
emit("=== 40 sample entries spread across the file ===")
step = max(1, len(pairs) // 40)
for i in range(0, len(pairs), step):
    k, v = pairs[i]
    emit(f"  {k}  =>  {v[:70]}")

OUT.write_text("\n".join(lines_out), encoding="utf-8")
print(f"wrote {OUT} (entries={len(pairs)}, unparsed={bad})")
