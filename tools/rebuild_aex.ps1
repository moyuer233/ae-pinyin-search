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
#   6. probe the artifact (strings, encodings, PE exports/imports, freshness)
#   7. mirror the sources into the repo
#   8. optionally deploy (elevated; waits for the loaded .aex to be released)
#
# Exit code = number of failed steps. ASCII only (PS 5.1 reads a BOM-less
# non-ASCII script as ANSI and breaks on it).
#
# Paths come from the environment when set, otherwise from this machine's layout:
#   AE_SDK_DIR (default H:\ae-sdk\AfterEffectsSDK_26.5_win), MSBUILD_EXE,
#   AE_PLUGIN_BUILD_DIR, DUMPBIN_EXE, AE_INSTALL_DIR.

param(
    [switch]$Deploy,
    [switch]$SkipScan
)

$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
$sdk = if ($env:AE_SDK_DIR) { $env:AE_SDK_DIR } else { 'H:\ae-sdk\AfterEffectsSDK_26.5_win' }
$proj = Join-Path $sdk 'Examples\AEGP\AEPinyinSearch\Win\AEPinyinSearch.vcxproj'

if (-not $env:AE_PLUGIN_BUILD_DIR) { $env:AE_PLUGIN_BUILD_DIR = Join-Path $sdk 'build' }

# A command that cannot even be found leaves $LASTEXITCODE untouched (it keeps the
# previous step's 0), so every tool is resolved and checked up front instead of
# being trusted to report its own failure.
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
$msbuild = $env:MSBUILD_EXE
if (-not $msbuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
    }
}

$failed = 0
$missing = @()
if (-not $python) { $missing += 'python (put it on PATH)' }
if (-not $msbuild) { $missing += 'MSBuild.exe (set MSBUILD_EXE, or install VS Build Tools)' }
if (-not (Test-Path -LiteralPath $proj)) { $missing += "project: $proj (set AE_SDK_DIR)" }
if ($missing.Count -gt 0) {
    foreach ($m in $missing) { Write-Host "[FAIL] missing: $m" -ForegroundColor Red }
    Write-Host ''
    Write-Host "failed steps: $($missing.Count)"
    exit $missing.Count
}

function Step([string]$name, [scriptblock]$body) {
    Write-Host "=== $name" -ForegroundColor Cyan
    & $body
    if ($null -eq $LASTEXITCODE) {
        # No exit code at all: the command never ran. Counting that as success is
        # how a rebuild used to report "failed steps: 0" with nothing rebuilt.
        Write-Host "[FAIL] $name produced no exit code" -ForegroundColor Red
        $script:failed++
    } elseif ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] $name (exit $LASTEXITCODE)" -ForegroundColor Red
        $script:failed++
    } else {
        Write-Host "[OK] $name"
    }
}

if (-not $SkipScan) {
    Step 'rescan plug-in folders' { & $python (Join-Path $repo 'tools\build_aex_effects.py') }
}
Step 'build the index' { & $python (Join-Path $repo 'tools\build_index.py') }
Step 'generate pinyin_data + @ aliases' { & $python (Join-Path $repo 'tools\gen_pinyin_data.py') }
Step 'matcher self-test' { cmd /c (Join-Path $repo 'tools\build_match_test.cmd') }

Write-Host '=== build the .aex' -ForegroundColor Cyan
& $msbuild $proj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] build (exit $LASTEXITCODE)" -ForegroundColor Red
    $failed++
} else {
    Write-Host '[OK] build'
}

Step 'probe the artifact' { & $python (Join-Path $repo 'tools\verify_aex.py') }
Step 'mirror sources into the repo' {
    powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\sync_aex_sources.ps1')
}

if ($Deploy) {
    Write-Host '=== deploy (elevated)' -ForegroundColor Cyan
    $deployScript = Join-Path $repo 'tools\deploy_aex.ps1'
    $p = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $deployScript)
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
