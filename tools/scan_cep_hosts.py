# -*- coding: utf-8 -*-
"""List every installed CEP extension with its host support and version.

Read-only. Writes _cep-hosts.txt
"""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

DIRS = [
    Path(r"C:\Program Files (x86)\Common Files\Adobe\CEP\extensions"),
    Path(r"C:\Program Files\Common Files\Adobe\CEP\extensions"),
    Path.home() / "AppData/Roaming/Adobe/CEP/extensions",
]

OUT = Path(__file__).with_name("_cep-hosts.txt")
lines: list[str] = []
def emit(s: str = "") -> None:
    lines.append(s)

for base in DIRS:
    emit(f"### {base}  exists={base.exists()}")
    if not base.exists():
        emit()
        continue
    for ext in sorted(base.iterdir()):
        if not ext.is_dir():
            continue
        mf = ext / "CSXS" / "manifest.xml"
        if not mf.exists():
            emit(f"  {ext.name}: NO MANIFEST")
            continue
        raw = mf.read_text(encoding="utf-8", errors="replace")
        # strip namespaces for simpler parsing
        clean = re.sub(r'\sxmlns(:\w+)?="[^"]*"', "", raw)
        try:
            root = ET.fromstring(clean)
        except ET.ParseError as exc:
            emit(f"  {ext.name}: manifest parse error {exc}")
            continue
        bundle_ver = root.get("ExtensionBundleVersion", "?")
        hosts: list[str] = []
        for h in root.iter("Host"):
            hosts.append(f"{h.get('Name')}>={h.get('Version')}")
        panel_names = []
        for d in root.iter("DispatchInfo"):
            panel_names.append(d.get("Id", "?"))
        emit(f"  {ext.name}  v{bundle_ver}")
        emit(f"      hosts: {', '.join(hosts) if hosts else '(none)'}")
        emit(f"      dispatch: {len(panel_names)}")
    emit()

OUT.write_text("\n".join(lines), encoding="utf-8")
print(f"wrote {OUT} ({len(lines)} lines)")
