# -*- coding: utf-8 -*-
"""Probe GitHub for After Effects SDK / plugin sources, print candidates."""
import json
import os
import urllib.parse
import urllib.request

QUERIES = [
    "after effects sdk plugin",
    "aftereffects aex plugin",
    "adobe after effects sdk",
]


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "dsh-agent"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


for q in QUERIES:
    print("=== query:", q)
    url = ("https://api.github.com/search/repositories?q="
           + urllib.parse.quote(q) + "&sort=stars&per_page=8")
    try:
        d = get(url)
    except Exception as e:  # noqa: BLE001
        print("   request failed:", e)
        continue
    for r in d.get("items", [])[:8]:
        desc = (r.get("description") or "")[:70]
        print("  {:>6}  {}  -  {}".format(r.get("stargazers_count"), r.get("full_name"), desc))
    print()
