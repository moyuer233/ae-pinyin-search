# Retire the machinery the .aex plug-in replaced.
#
# The CEP panel, the ScriptUI palette and the C# side-button tool were all
# superseded by AEPinyinSearch.aex. They are moved into _retired_<stamp>\ rather
# than deleted, so nothing is lost and the working tree stops advertising them.
#
#   powershell -File tools\cleanup_retired.ps1            (project + user level)
#   powershell -File tools\cleanup_retired.ps1 -System    (also the system CEP copy; needs admin)
#
# ASCII only (PS 5.1 reads a BOM-less non-ASCII script as ANSI and breaks on it).

param(
    [switch]$System
)

$ErrorActionPreference = 'Continue'
$repo = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format 'yyyyMMdd_HHmm'
$retired = Join-Path $repo "_retired_$stamp"

function MoveInto([string]$path, [string]$sub) {
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Host "  (skip, missing) $path"
        return
    }
    $target = Join-Path $retired $sub
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Move-Item -LiteralPath $path -Destination $target -Force
    Write-Host "  retired: $path  ->  $target"
}

New-Item -ItemType Directory -Force -Path $retired | Out-Null
Write-Host "retired folder: $retired"

Write-Host '--- project level'
MoveInto (Join-Path $repo 'dist') 'dist'
MoveInto (Join-Path $repo 'sidekey') 'sidekey'
MoveInto (Join-Path $repo 'extension') 'extension'

Write-Host '--- retired tools'
$toolDir = Join-Path $retired 'tools'
New-Item -ItemType Directory -Force -Path $toolDir | Out-Null
$retiredTools = @(
    'ae-selftest.jsx', 'ae-sizetest.jsx', 'AEPinyinAutoRun.jsx',
    'build_palette.py', 'palette_scriptui.jsx', 'palette_v2.jsx',
    'export-ae-effects.jsx', 'extract_aex_names.py', 'rename_aex_project.py',
    'selftest.mjs', 'analyze_groups.py', 'inspect_index.py', 'inspect_vendors.py'
)
foreach ($t in $retiredTools) {
    $p = Join-Path $repo "tools\$t"
    if (Test-Path -LiteralPath $p) {
        Move-Item -LiteralPath $p -Destination $toolDir -Force
        Write-Host "  retired: tools\$t"
    }
}

Write-Host '--- the ScriptUI palette I had put in the user Scripts folder'
$userScript = Join-Path $env:APPDATA 'Adobe\After Effects\26.0\Scripts\AEPinyinSearch.jsx'
MoveInto $userScript 'user_scripts'

Write-Host '--- CEP cache left behind by the host'
$cache = Join-Path $env:LOCALAPPDATA 'Temp\cep_cache'
if (Test-Path -LiteralPath $cache) {
    $cacheTarget = Join-Path $retired 'cep_cache'
    New-Item -ItemType Directory -Force -Path $cacheTarget | Out-Null
    Get-ChildItem -LiteralPath $cache -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like '*pinyin*' } |
        ForEach-Object {
            Move-Item -LiteralPath $_.FullName -Destination $cacheTarget -Force
            Write-Host "  retired: $($_.FullName)"
        }
}

$cep = 'C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.pinyin.ae.search'
if ($System) {
    Write-Host '--- system CEP extension'
    MoveInto $cep 'cep_extension'
} elseif (Test-Path -LiteralPath $cep) {
    Write-Host "[note] system CEP extension still installed (needs admin): $cep"
    Write-Host '       re-run with -System to retire it.'
}

Write-Host ''
Write-Host "counts in $retired :"
$files = 0
cmd /c "dir /s /b /a-d `"$retired`" 2>nul" | ForEach-Object { $files++ }
Write-Host "  $files file(s)"
