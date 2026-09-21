# Rebuild the plug-in end to end after installing / removing effects or presets.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\rebuild_aex.ps1
#   powershell ... -File tools\rebuild_aex.ps1 -Deploy      (deploy too; needs AE closed)
#
# Steps:
#   1. rescan the plug-in folders for .aex names + vendor folders
#   2. merge dictionary + third-party + presets into build\ae-index.json
#   3. regenerate pinyin_data.h / pinyin_data.cpp (rows, pinyin keys, @ aliases)
#   4. run the matcher self-test (outside After Effects)
#   5. build AEPinyinSearch.aex
#   6. probe the artifact (strings, encodings, PE exports/imports)
#   7. mirror the sources into the repo
#   8. optionally deploy (elevated; waits for the loaded .aex to be released)
#
# Exit code = number of failed steps. ASCII only (PS 5.1 reads a BOM-less
# non-ASCII script as ANSI and breaks on it).

param(
    [switch]$Deploy,
    [switch]$SkipScan
)

$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
$proj = 'H:\ae-sdk\AfterEffectsSDK_26.5_win\Examples\AEGP\AEPinyinSearch'
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'

if (-not $env:AE_PLUGIN_BUILD_DIR) { $env:AE_PLUGIN_BUILD_DIR = 'H:\ae-sdk\build' }

$failed = 0
function Step([string]$name, [scriptblock]$body) {
    Write-Host "=== $name" -ForegroundColor Cyan
    & $body
    if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) {
        Write-Host "[FAIL] $name (exit $LASTEXITCODE)" -ForegroundColor Red
        $script:failed++
    } elseif ($LASTEXITCODE -eq $null) {
        Write-Host "[skip] $name produced no exit code"
    } else {
        Write-Host "[OK] $name"
    }
}

if (-not $SkipScan) {
    Step 'rescan plug-in folders' { python "$repo\tools\build_aex_effects.py" }
}
Step 'build the index' { python "$repo\tools\build_index.py" }
Step 'generate pinyin_data + @ aliases' { python "$repo\tools\gen_pinyin_data.py" }
Step 'matcher self-test' { cmd /c "$repo\tools\build_match_test.cmd" }

Write-Host '=== build the .aex' -ForegroundColor Cyan
& $msbuild "$proj\Win\AEPinyinSearch.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host '[FAIL] build' -ForegroundColor Red
    $failed++
} else {
    Write-Host '[OK] build'
}

Step 'probe the artifact' { python "$repo\tools\verify_aex.py" }
Step 'mirror sources into the repo' { powershell -NoProfile -ExecutionPolicy Bypass -File "$repo\tools\sync_aex_sources.ps1" }

if ($Deploy) {
    Write-Host '=== deploy (elevated)' -ForegroundColor Cyan
    $p = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "$repo\tools\deploy_aex.ps1")
    if ($p.ExitCode -ne 0) {
        Write-Host "[FAIL] deploy (exit $($p.ExitCode))" -ForegroundColor Red
        $failed++
    } else {
        Write-Host '[OK] deploy'
    }
} else {
    Write-Host '[note] not deployed; re-run with -Deploy (and close After Effects first)'
}

Write-Host ''
Write-Host "failed steps: $failed"
exit $failed
