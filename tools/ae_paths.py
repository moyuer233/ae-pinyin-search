# -*- coding: utf-8 -*-
"""Where the tools find After Effects, its SDK and this repository.

Nothing here is tied to one machine's layout: the defaults below are just this
machine's, and every one of them can be overridden with an environment variable.

  AE_INSTALL_DIR   the After Effects installation folder
  AE_SDK_DIR       the unzipped After Effects SDK
  AE_MEDIACORE     Adobe's shared plug-ins folder
  MSBUILD_EXE      MSBuild.exe (otherwise looked up with vswhere)
  DUMPBIN_EXE      dumpbin.exe (otherwise looked up under the newest MSVC toolset)
  VCVARS64         vcvars64.bat, used by build_match_test.cmd

Read-only helpers: importing this module never writes anything.
"""
from __future__ import annotations

import os
import re
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BUILD = REPO / "build"
TOOLS = REPO / "tools"

# This machine's layout; override with the environment variables above.
DEFAULT_AE = Path(r"H:\adobe\Adobe After Effects 2026")
DEFAULT_SDK = Path(r"H:\ae-sdk\AfterEffectsSDK_26.5_win")
DEFAULT_MEDIACORE = Path(r"C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore")

_PROGRAM_FILES_X86 = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
_VSWHERE = _PROGRAM_FILES_X86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"


def _vswhere(args: list[str]) -> str:
    """First line of vswhere's output, or "" when vswhere is not installed."""
    if not _VSWHERE.exists():
        return ""
    try:
        done = subprocess.run([str(_VSWHERE), *args], capture_output=True, text=True, timeout=120)
    except OSError:
        return ""
    return done.stdout.strip().splitlines()[0] if done.stdout.strip() else ""


def ae_install() -> Path:
    env = os.environ.get("AE_INSTALL_DIR")
    if env:
        return Path(env)
    if DEFAULT_AE.is_dir():
        return DEFAULT_AE
    # A newer After Effects installs under its own folder name: take the newest.
    for root in (Path(r"C:\Program Files\Adobe"), Path(r"H:\adobe")):
        if root.is_dir():
            found = sorted((p for p in root.glob("Adobe After Effects *") if p.is_dir()), reverse=True)
            if found:
                return found[0]
    return DEFAULT_AE


def ae_support() -> Path:
    return ae_install() / "Support Files"


def ae_plugins() -> Path:
    return ae_support() / "Plug-ins"


def mediacore() -> Path:
    return Path(os.environ.get("AE_MEDIACORE", str(DEFAULT_MEDIACORE)))


def sdk_dir() -> Path:
    return Path(os.environ.get("AE_SDK_DIR", str(DEFAULT_SDK)))


def sdk_example() -> Path:
    return sdk_dir() / "Examples" / "AEGP" / "AEPinyinSearch"


def msbuild() -> str:
    env = os.environ.get("MSBUILD_EXE")
    if env:
        return env
    return _vswhere(
        [
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.Component.MSBuild",
            "-find",
            r"MSBuild\**\Bin\MSBuild.exe",
        ]
    )


def _version_key(path: Path) -> tuple:
    numbers = re.findall(r"\d+", path.parent.parent.parent.name)
    return tuple(int(n) for n in numbers) if numbers else (0,)


def dumpbin() -> str:
    env = os.environ.get("DUMPBIN_EXE")
    if env:
        return env
    roots = []
    install = _vswhere(["-latest", "-products", "*", "-property", "installationPath"])
    if install:
        roots.append(Path(install))
    roots.append(_PROGRAM_FILES_X86 / "Microsoft Visual Studio" / "18" / "BuildTools")
    found: list[Path] = []
    for root in roots:
        found += [p for p in root.glob(r"VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe") if p.exists()]
    if not found:
        return ""
    return str(sorted(found, key=_version_key)[-1])


def vcvars64() -> str:
    env = os.environ.get("VCVARS64")
    if env:
        return env
    found = _vswhere(
        ["-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
         "-find", r"VC\Auxiliary\Build\vcvars64.bat"]
    )
    return found
