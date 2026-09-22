# -*- coding: utf-8 -*-
"""Verify the built .aex carries the popup UI, the search index and no panel leftovers.

Two encodings are in play: char* literals land in the binary as UTF-8, while
L"..." wide literals land as UTF-16LE. Probing the wrong one reports a false
MISS on a perfectly good build, so every probe states its encoding.

The expectations are read from the sources rather than repeated here:
  * the artifact path comes from AE_PLUGIN_BUILD_DIR (the same variable the build
    uses), so this can never inspect a different .aex than the one just built;
  * the version comes from AEPinyinSearch.cpp;
  * the vendor names come from the index that was just generated;
  * the "must be gone" hard-coded path is probed in both encodings.
Exit code = number of problems.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import ae_paths

AEX = Path(os.environ.get("AE_PLUGIN_BUILD_DIR", str(ae_paths.sdk_dir() / "build"))) / "AEGP" / "AEPinyinSearch.aex"
EXAMPLE = ae_paths.sdk_example()
INDEX = ae_paths.BUILD / "ae-index.json"

problems = []


def fail(message: str) -> None:
    problems.append(message)


if not AEX.exists():
    print(f"!! artifact missing: {AEX}")
    print("problems: 1")
    sys.exit(1)

data = AEX.read_bytes()
print("aex:", AEX)
print("size:", len(data), "bytes")

# The probe is worthless if it inspects an artifact the build never touched: the
# MSBuild step reports "(nothing to do)" with exit code 0 when the project path is
# wrong, and then every check below would pass on last week's binary.
newest_source = 0.0
newest_name = ""
for src in list(EXAMPLE.rglob("*.cpp")) + list(EXAMPLE.rglob("*.h")):
    if src.stat().st_mtime > newest_source:
        newest_source = src.stat().st_mtime
        newest_name = src.name
for extra in (INDEX,):
    if extra.exists() and extra.stat().st_mtime > newest_source:
        newest_source = extra.stat().st_mtime
        newest_name = extra.name
if newest_source and AEX.stat().st_mtime < newest_source:
    fail(f"artifact is older than {newest_name}: it was not rebuilt")
if not newest_source:
    fail(f"no sources found under {EXAMPLE} (AE_SDK_DIR wrong?)")


def u16(text):
    return text.encode("utf-16-le")


def u8(text):
    return text.encode("utf-8")


# --- version, straight out of the source -------------------------------------
source = (EXAMPLE / "AEPinyinSearch.cpp").read_text(encoding="utf-8", errors="replace")
m = re.search(r'kVersion\s*=\s*"([^"]+)"', source)
version = m.group(1) if m else ""
if not version:
    fail("kVersion not found in AEPinyinSearch.cpp")
print("version:", version or "(unknown)")

must_utf8 = [
    ("menu name (char*)", u8("拼音搜索")),
    ("status code", b"NOEFFECT"),
    ("status code", b"NOPRESET"),
    ("jsx bridge marker", b"ADBE Effect Parade"),
    ("jsx preset call", b"applyPreset"),
    ("hotkey window class", b"AEPinyinSearchHotkeyWnd"),
    ("host effect names log", b"effect names: %d installed effect(s) from the host"),
    ("host suite fallback log", b"effect names: AEGP Effect Suite 5 unavailable"),
    ("input thread start log", b"mouse hook: %s (this thread only)"),
    ("toggle source log", b"mouse x-button"),
]
if version:
    must_utf8.append(("version string", u8(version)))

must_utf16 = [
    ("popup window class", u16("AEPinyinSearchPopup")),
    ("window title", u16("拼音搜索")),
    ("search hint", u16("@ 分类 · # 类型")),
    ("no-match line", u16("没有匹配项")),
    ("no-class line", u16("没有这一类")),
    ("no-kind line", u16("没有这一类型")),
    ("class badge", u16("分类")),
    ("kind badge", u16("类型")),
    ("recent row wording", u16("用过 ")),
    ("recent row unit", u16(" 次")),
    ("count unit", u16(" 项")),
    ("badge preset", u16("预设")),
    ("badge effect", u16("效果")),
    ("fail: no comp", u16("当前没有打开的合成")),
    ("fail: no layer", u16("先在时间线里选中至少一个图层")),
    ("fail: no effect", u16("AE 里找不到这个效果")),
    ("fail: no preset", u16("预设文件缺失或应用失败")),
    ("presets folder is found at runtime", u16("\\Support Files\\Presets")),
]

must_index = [
    ("full pinyin key", b"gaosimohu"),
    ("initials key", b"gsmh"),
    ("chinese name", u8("高斯模糊")),
    ("english name", b"Gaussian Blur"),
    ("bcc row", b"BCCAlphaProcess"),
]

# Which names have to be gone depends on what is installed (see the index), so
# they are read from the index instead of being hoped for.
must_not = [
    ("panel UI class", b"AEPinyinSearchUI_Plat"),
    ("panel flyout menu", b"Normal Fill Color"),
    ("old ansi dialog title", b"AE Pinyin Search"),
    ("old count wording", u16(" 个效果")),
]
for banned in (
    r"H:/adobe/Adobe After Effects 2026/Support Files/Presets",
    str(ae_paths.ae_support() / "Presets").replace("\\", "/"),
):
    must_not.append((f"hard-coded presets path {banned[:28]}", u8(banned)))
    must_not.append((f"hard-coded presets path (wide) {banned[:28]}", u16(banned)))

def probe_group(title: str, probes, expect_present: bool = True) -> None:
    print(f"[{title}]")
    for label, needle in probes:
        hit = needle in data
        if hit != expect_present:
            fail(label)
        state = ("OK  " if hit else "MISS") if expect_present else ("STILL THERE" if hit else "gone")
        print(f"  {state}  {label:28} {needle[:40]!r}")


probe_group("char* / UTF-8", must_utf8)
probe_group("wchar_t / UTF-16LE", must_utf16)
probe_group("index", must_index)
probe_group("must be gone", must_not, expect_present=False)

# --- index-derived expectations ---------------------------------------------
if INDEX.exists():
    index = json.loads(INDEX.read_text(encoding="utf-8"))
    vendors = sorted({e.get("vendor") for e in index["effects"] if e.get("vendor")})
    print("[installed vendors from the index]")
    for vendor in vendors:
        hit = u8(vendor) in data
        if not hit:
            fail(f"vendor not compiled in: {vendor}")
        print(f"  {'OK  ' if hit else 'MISS'}  {vendor}")
else:
    fail(f"index missing: {INDEX}")

# --- PE ---------------------------------------------------------------------
DUMPBIN = ae_paths.dumpbin()
if not DUMPBIN or not Path(DUMPBIN).exists():
    # Skipping the only structural check silently is how a green light gets
    # weaker than it looks; say it is a problem instead.
    fail(f"dumpbin not found (set DUMPBIN_EXE): {DUMPBIN or '(none)'}")
else:
    print("[pe]", DUMPBIN)
    out = subprocess.run([DUMPBIN, "/exports", str(AEX)], capture_output=True, text=True).stdout
    ok = "EntryPointFunc" in out
    if not ok:
        fail("exports EntryPointFunc")
    print(f"  {'OK  ' if ok else 'MISS'}  exports EntryPointFunc")
    out = subprocess.run([DUMPBIN, "/imports", str(AEX)], capture_output=True, text=True).stdout
    for label, symbol, want in (
        ("no ANSI message box", "MessageBoxA", False),
        ("unicode message box", "MessageBoxW", True),
        ("death-hook cleanup", "UnhookWindowsHookEx", True),
    ):
        hit = symbol in out
        if hit != want:
            fail(label)
        print(f"  {'OK  ' if hit == want else 'MISS'}  {label} ({symbol})")

print()
print(f"problems: {len(problems)}")
for line in problems:
    print(f"  !! {line}")
sys.exit(len(problems))
