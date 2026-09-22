# Mirror the built plug-in sources out of the AE SDK examples tree into this repo.
#
# The project has to live at <SDK>\Examples\AEGP\AEPinyinSearch\ because its
# vcxproj reaches the SDK headers through ..\..\..\Headers. This repo keeps a
# read-only copy so the sources are version controlled; build from the SDK tree.
#
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\sync_aex_sources.ps1
#
# AE_SDK_DIR overrides the SDK location. Every file in the list is attempted even
# when one is missing, and the summary says what happened: exiting on the first
# missing file left the mirror half old with no report. Exit code = number of
# files that could not be copied. ASCII only.

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$sdk = if ($env:AE_SDK_DIR) { $env:AE_SDK_DIR } else { 'H:\ae-sdk\AfterEffectsSDK_26.5_win' }
$srcRoot = Join-Path $sdk 'Examples\AEGP\AEPinyinSearch'
$dstRoot = Join-Path $repo 'aex'

$files = @(
    'AEPinyinSearch.cpp',
    'AEPinyinSearch.h',
    'AEPinyinSearch_Strings.cpp',
    'AEPinyinSearch_PiPL.r',
    'PT_Err.h',
    'pinyin_data.h',
    'pinyin_data.cpp',
    'pinyin_match.h',
    'Win\AEPinyinSearch.vcxproj',
    'Win\AEPinyinSearch_PiPL.rc',
    'Win\DiagLog.h',
    'Win\EffectNames.h',
    'Win\EffectNameMatch.h',
    'Win\PinyinPopup.cpp',
    'Win\PinyinPopup.h'
)

$copied = 0
$missing = @()
foreach ($rel in $files) {
    $from = Join-Path $srcRoot $rel
    $to = Join-Path $dstRoot $rel
    if (-not (Test-Path $from)) {
        Write-Host "MISSING in SDK tree: $rel"
        $missing += $rel
        continue
    }
    $toDir = Split-Path $to -Parent
    if (-not (Test-Path $toDir)) {
        New-Item -ItemType Directory -Force -Path $toDir | Out-Null
    }
    Copy-Item $from $to -Force
    $copied++
}

# Anything under aex\ that the list above does not cover is a leftover from an
# earlier design; report it instead of deleting it silently.
$known = @{}
foreach ($rel in $files) { $known[(Join-Path $dstRoot $rel).ToLowerInvariant()] = $true }
$stale = Get-ChildItem $dstRoot -Recurse -File | Where-Object { -not $known.ContainsKey($_.FullName.ToLowerInvariant()) }
foreach ($f in $stale) { Write-Host "STALE (not in the file list): $($f.FullName)" }

Write-Host "copied $copied of $($files.Count) files into $dstRoot"
if ($missing.Count -gt 0) {
    Write-Host "FAIL: $($missing.Count) source file(s) missing: $($missing -join ', ')"
    exit $missing.Count
}
exit 0
