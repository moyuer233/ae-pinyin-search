# -*- coding: utf-8 -*-
"""Pull key sections of Adobe's CEP 12 HTML Extension Cookbook."""
import re
import urllib.request

BASE = "https://raw.githubusercontent.com/Adobe-CEP/CEP-Resources/master/CEP_12.x/"
COOKBOOK = BASE + "Documentation/CEP%2012%20HTML%20Extension%20Cookbook.md"


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "dsh-agent"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read().decode("utf-8", errors="replace")


txt = fetch(COOKBOOK)
lines = txt.splitlines()
print("cookbook chars:", len(txt), " lines:", len(lines))

print("\n=== table of contents (headings) ===")
for i, ln in enumerate(lines):
    if re.match(r"^#{1,3} ", ln):
        print("  ", ln.strip()[:100])

print("\n=== sections mentioning window / position / shortcut / modeless ===")
want = re.compile(r"(?i)modeless|window position|move|shortcut|keyEventsInterest|panel.*(size|position)")
for i, ln in enumerate(lines):
    if want.search(ln):
        print(f"  L{i}: {ln.strip()[:120]}")
