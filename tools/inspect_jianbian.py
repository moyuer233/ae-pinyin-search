# -*- coding: utf-8 -*-
"""What does the index actually hold for the 渐变 family?"""
import json
from pathlib import Path

data = json.loads(Path(r"H:\ae-pinyin-search\build\ae-index.json").read_text(encoding="utf-8"))

print("=== effects whose name contains 渐变 ===")
for e in data["effects"]:
    if "渐变" in e.get("n", ""):
        print(f"  n={e['n']!r:<22} e={e.get('e','')!r:<26} f={e.get('f','')!r:<22} i={e.get('i','')!r:<10} src={e.get('src','')} vendor={e.get('vendor','')!r}")

print()
print("=== rows whose pinyin keys contain 'jianbian' ===")
for kind in ("effects", "presets"):
    for e in data[kind]:
        if "jianbian" in e.get("f", "") or "jianbian" in e.get("a", ""):
            print(f"  [{kind[:-1]}] n={e['n']!r:<24} e={e.get('e','')!r:<24} f={e.get('f','')!r:<24} i={e.get('i','')!r}")

print()
print("=== exactly '渐变' ===")
for kind in ("effects", "presets"):
    for e in data[kind]:
        if e.get("n") == "渐变":
            print(f"  [{kind}] {json.dumps(e, ensure_ascii=False)}")
