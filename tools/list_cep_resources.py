# -*- coding: utf-8 -*-
"""List interesting files in the Adobe-CEP/CEP-Resources repo (docs + samples)."""
import json
import urllib.request

URL = "https://api.github.com/repos/Adobe-CEP/CEP-Resources/git/trees/master?recursive=1"

req = urllib.request.Request(URL, headers={"User-Agent": "dsh-agent"})
with urllib.request.urlopen(req, timeout=60) as r:
    tree = json.load(r).get("tree", [])

paths = [t["path"] for t in tree if t.get("type") == "blob"]
print("total files:", len(paths))

KEYWORDS = ("cookbook", "window", "shortcut", "palette", "modeless", "position",
            "geometry", "sample", "example", "csinterface", "getting-started")

print("\n=== files whose NAME matches interesting keywords ===")
for p in paths:
    name = p.split("/")[-1].lower()
    if any(k in name for k in KEYWORDS):
        print("  ", p)

print("\n=== documentation files (CEP 12) ===")
for p in paths:
    if p.startswith("CEP_12.x/Documentation") or p.startswith("Documentation"):
        print("  ", p)
