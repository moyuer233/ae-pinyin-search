# -*- coding: utf-8 -*-
"""Print specific line ranges of the CEP 12 cookbook (1-based, inclusive)."""
import sys
import urllib.request

URL = ("https://raw.githubusercontent.com/Adobe-CEP/CEP-Resources/master/"
       "CEP_12.x/Documentation/CEP%2012%20HTML%20Extension%20Cookbook.md")


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "dsh-agent"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read().decode("utf-8", errors="replace")


lines = fetch(URL).splitlines()
start = int(sys.argv[1])
end = int(sys.argv[2])
for i in range(start - 1, min(end, len(lines))):
    print(f"{i + 1:5}| {lines[i]}")
