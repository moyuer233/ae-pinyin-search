# -*- coding: utf-8 -*-
"""List files of the official CEP invisible-extension sample."""
import json
import urllib.parse
import urllib.request

API = "https://api.github.com/repos/Adobe-CEP/CEP-Resources/git/trees/master?recursive=1"
req = urllib.request.Request(API, headers={"User-Agent": "dsh-agent"})
with urllib.request.urlopen(req, timeout=60) as r:
    tree = json.load(r).get("tree", [])

prefix = "CEP_12.x/Samples/CEP_HTML_Invisible_Extension-12.0/"
for t in tree:
    if t.get("type") == "blob" and t["path"].startswith(prefix):
        print("  ", t["path"][len(prefix):], t.get("size", ""))
