# -*- coding: utf-8 -*-
"""Extract string literals + identifier names from a minified JS bundle, then
report which ones look like search / input / localization logic.

Usage: python probe_bundle.py <file.js> [out.txt]
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

src_path = Path(sys.argv[1])
out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else src_path.with_suffix(".probe.txt")
src = src_path.read_text(encoding="utf-8", errors="replace")

lines: list[str] = []
def emit(s: str = "") -> None:
    lines.append(s)

emit(f"file: {src_path}")
emit(f"size: {len(src)} chars, {src.count(chr(10)) + 1} lines")
emit()

# --- string literals ---
strs = re.findall(r'"((?:[^"\\\n]|\\.){1,200})"', src)
strs += re.findall(r"'((?:[^'\\\n]|\\.){1,200})'", src)
uniq = Counter(strs)
emit(f"=== {len(strs)} string literals, {len(uniq)} unique ===")

KW = re.compile(
    r"(?i)search|suggest|filter|match|input|keyup|keydown|keypress|menu|dict|locale|lang|"
    r"trie|index|query|result|focus|blur|fuzzy|pinyin|py|char|code|scan"
)
hits = [(s, n) for s, n in uniq.items() if KW.search(s)]
hits.sort(key=lambda x: (-x[1], x[0]))
emit(f"--- {len(hits)} literals matching search/input/localization keywords ---")
for s, n in hits[:200]:
    emit(f"{n:>4}x  {s[:120]}")

emit()
emit("--- all literals that contain CJK ---")
cjk = [(s, n) for s, n in uniq.items() if re.search(r"[\u4e00-\u9fff]", s)]
emit(f"count: {len(cjk)}")
for s, n in cjk[:60]:
    emit(f"{n:>4}x  {s[:120]}")

# --- identifiers that look relevant ---
idents = set(re.findall(r"[A-Za-z_$][A-Za-z0-9_$]{2,40}", src))
id_hits = sorted(i for i in idents if KW.search(i))
emit()
emit(f"--- {len(id_hits)} identifiers matching keywords (first 150) ---")
for i in id_hits[:150]:
    emit(f"  {i}")

# --- function definitions ---
fn = re.findall(r"function\s+([A-Za-z_$][A-Za-z0-9_$]{0,40})\s*\(", src)
emit()
emit(f"=== named functions ({len(fn)}) ===")
emit("  " + ", ".join(sorted(set(fn))))

# --- require()d modules ---
req = re.findall(r'require\("([^"]+)"\)', src)
emit()
emit(f"=== requires: {sorted(set(req))} ===")

# --- context around search-ish tokens ---
emit()
emit("=== context windows around 'search'/'suggest' (first 12) ===")
shown = 0
for m in re.finditer(r"(?i)search|suggest", src):
    a = max(0, m.start() - 120)
    b = min(len(src), m.end() + 120)
    emit(f"... {src[a:b]} ...")
    shown += 1
    if shown >= 12:
        break

out_path.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {out_path} ({len(lines)} lines)")
