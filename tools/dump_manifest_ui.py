# -*- coding: utf-8 -*-
"""Dump every UI/Lifecycle block from a CEP manifest, compactly."""
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
txt = path.read_text(encoding="utf-8", errors="replace")

for m in re.finditer(r"<Extension\s+Id=\"([^\"]+)\">(.*?)</Extension>", txt, re.S):
    ext_id, body = m.group(1), m.group(2)
    ui = re.search(r"<UI>(.*?)</UI>", body, re.S)
    life = re.search(r"<Lifecycle>(.*?)</Lifecycle>", body, re.S)
    print("===", ext_id)
    if life:
        print("  lifecycle:", " ".join(life.group(1).split()))
    if ui:
        print("  ui       :", " ".join(ui.group(1).split()))
    print()

print("=== does the manifest mention any position/x/y attribute at all? ===")
for kw in ("x=", "y=", "X=", "Y=", "Position", "position", "Left", "Top"):
    hits = [ln.strip() for ln in txt.splitlines() if kw in ln]
    print(f"  {kw!r}: {len(hits)} hit(s)" + (("  e.g. " + hits[0][:90]) if hits else ""))
