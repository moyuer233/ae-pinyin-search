# -*- coding: utf-8 -*-
"""Mine FX Console's .aex for strings that reveal how it does hotkey/panel show-hide."""
import re
from pathlib import Path

AEX = Path(r"H:\fxc\pluginpath\VideoCopilot\FXConsole.aex")
blob = AEX.read_bytes()
print("aex size:", len(blob))

# ASCII strings
ascii_strings = re.findall(rb"[\x20-\x7e]{4,}", blob)
text = [s.decode("ascii", "ignore") for s in ascii_strings]

# UTF-16LE strings (Windows resources / APIs often appear this way)
utf16 = re.findall(rb"(?:[\x20-\x7e]\x00){4,}", blob)
text += [s.decode("utf-16-le", "ignore") for s in utf16]

print("total strings:", len(text))

KW = ("hotkey", "shortcut", "registerhotkey", "toggle", "hide", "show",
      "showwindow", "setwindowpos", "window", "panel", "search", "keyboard",
      "vk_space", "keydown", "keyup", "menucmd", "aegp", "aegp_register")

print("\n=== keyword hits ===")
seen = set()
for s in text:
    low = s.lower()
    if any(k in low for k in KW):
        if s in seen:
            continue
        seen.add(s)
        print("  ", s[:110])
