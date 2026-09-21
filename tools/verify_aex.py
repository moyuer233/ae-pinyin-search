# -*- coding: utf-8 -*-
"""Verify the built .aex carries the popup UI, the search index and no panel leftovers.

Two encodings are in play: char* literals land in the binary as UTF-8, while
L"..." wide literals land as UTF-16LE. Probing the wrong one reports a false
MISS on a perfectly good build, so every probe states its encoding.
"""
import subprocess
import sys
from pathlib import Path

AEX = Path(r"H:\ae-sdk\build\AEGP\AEPinyinSearch.aex")
DUMPBIN = (
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
    r"\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe"
)
data = AEX.read_bytes()
print("aex:", AEX)
print("size:", len(data), "bytes")


def u16(text):
    return text.encode("utf-16-le")


def u8(text):
    return text.encode("utf-8")


must_utf8 = [
    ("menu name (char*)", u8("拼音搜索")),
    ("status code", b"NOEFFECT"),
    ("status code", b"NOPRESET"),
    ("jsx bridge marker", b"ADBE Effect Parade"),
    ("jsx preset call", b"applyPreset"),
    ("hotkey window class", b"AEPinyinSearchHotkeyWnd"),
    ("vendor in table", b"Boris FX"),
    ("vendor in table", b"Red Giant Universe"),
    ("vendor in table", b"Sapphire"),
]

must_utf16 = [
    ("popup window class", u16("AEPinyinSearchPopup")),
    ("window title", u16("拼音搜索")),
    ("search hint", u16("@ 只看某一类")),
    ("no-match line", u16("没有匹配项")),
    ("no-class line", u16("没有这一类")),
    ("class badge", u16("分类")),
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

must_not = [
    ("panel UI class", b"AEPinyinSearchUI_Plat"),
    ("panel flyout menu", b"Normal Fill Color"),
    ("old ansi dialog title", b"AE Pinyin Search"),
    ("hard-coded presets path (must be derived now)", b"H:/adobe/Adobe After Effects 2026/Support Files/Presets"),
    ("old count wording", u16(" 个效果")),
]

bad = 0
for group, probes in (
    ("char* / UTF-8", must_utf8),
    ("wchar_t / UTF-16LE", must_utf16),
    ("index", must_index),
):
    print(f"[{group}]")
    for label, needle in probes:
        hit = needle in data
        bad += 0 if hit else 1
        print(f"  {'OK  ' if hit else 'MISS'}  {label:28} {needle[:40]!r}")

print("[must be gone]")
for label, needle in must_not:
    hit = needle in data
    bad += 1 if hit else 0
    print(f"  {'STILL THERE' if hit else 'gone'}  {label:28} {needle[:40]!r}")

if Path(DUMPBIN).exists():
    print("[pe]")
    out = subprocess.run([DUMPBIN, "/exports", str(AEX)], capture_output=True, text=True).stdout
    ok = "EntryPointFunc" in out
    bad += 0 if ok else 1
    print(f"  {'OK  ' if ok else 'MISS'}  exports EntryPointFunc")
    out = subprocess.run([DUMPBIN, "/imports", str(AEX)], capture_output=True, text=True).stdout
    for label, symbol, want in (
        ("no ANSI message box", "MessageBoxA", False),
        ("unicode message box", "MessageBoxW", True),
        ("death-hook cleanup", "UnhookWindowsHookEx", True),
    ):
        hit = symbol in out
        bad += 0 if (hit == want) else 1
        print(f"  {'OK  ' if hit == want else 'MISS'}  {label} ({symbol})")

print()
print(f"problems: {bad}")
sys.exit(bad)
