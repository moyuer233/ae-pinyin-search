# Deploy the built .aex into AE's own plug-ins folder.
#
# Why not MediaCore: that folder is shared by every Adobe video app and is not
# writable for this user, so every deploy there needs an elevated copy. AE's
# Support Files\Plug-ins\Extensions is where AE's own AEGP plugins live
# (CEPManager.aex and friends) and it is writable, so this only needs to run
# elevated once - to remove the copy left behind in MediaCore.
#
# The old .aex stays locked while AfterFX runs, so this waits for it to exit.
# Run elevated:
#   Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','<this file>'
#
# Order matters: everything is validated and copied first, and the copy left in
# MediaCore is only removed once the new one is in place - otherwise a failed
# copy leaves no plug-in installed at all.
#
# Paths come from the environment when set (AE_INSTALL_DIR, AE_MEDIACORE,
# AE_PLUGIN_BUILD_DIR), otherwise from this machine's layout. ASCII only.

param(
    [int]$WaitSeconds = 900
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$ae = if ($env:AE_INSTALL_DIR) { $env:AE_INSTALL_DIR } else { 'H:\adobe\Adobe After Effects 2026' }
$buildDir = if ($env:AE_PLUGIN_BUILD_DIR) { $env:AE_PLUGIN_BUILD_DIR } else { 'H:\ae-sdk\build' }
$mediaCore = if ($env:AE_MEDIACORE) { $env:AE_MEDIACORE } else { 'C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore' }

$src = Join-Path $buildDir 'AEGP\AEPinyinSearch.aex'
$dstDir = Join-Path $ae 'Support Files\Plug-ins\Extensions'
$dst = Join-Path $dstDir 'AEPinyinSearch.aex'
$old = Join-Path $mediaCore 'AEPinyinSearch.aex'
$log = Join-Path $repo 'build\deploy_aex.log'

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

function Say([string]$message) {
    $line = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  $message"
    Write-Host $line
    # BOM-less UTF-8: this is appended to, and PowerShell 5.1's -Encoding UTF8
    # writes a byte order mark on every new file.
    [System.IO.File]::AppendAllText($log, "$line`r`n", (New-Object System.Text.UTF8Encoding($false)))
}

# A loaded .aex is locked, so the real prerequisite is "the file can be opened
# for writing", not "no AfterFX process is running" - a plug-in-free AE (for
# example a leftover headless one) holds no lock and must not block a deploy.
function Test-Unlocked([string]$path) {
    if (-not (Test-Path $path)) { return $true }
    try {
        $fs = [System.IO.File]::Open($path, 'Open', 'ReadWrite', 'None')
        $fs.Close()
        return $true
    } catch {
        return $false
    }
}

Say "deploy start; waiting up to $WaitSeconds s for the loaded .aex to be released"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $WaitSeconds) {
    if ((Test-Unlocked $old) -and (Test-Unlocked $dst)) { break }
    Start-Sleep -Seconds 3
}
if (-not ((Test-Unlocked $old) -and (Test-Unlocked $dst))) {
    Say "TIMEOUT: a loaded copy is still locked (close After Effects); nothing was changed"
    exit 2
}
Say "both copies are unlocked"

if (-not (Test-Path $src)) {
    Say "FAIL: build output missing: $src"
    exit 3
}
if (-not (Test-Path $dstDir)) {
    Say "FAIL: target folder missing: $dstDir (is AE_INSTALL_DIR right?)"
    exit 4
}

Copy-Item $src $dst -Force
$srcHash = (Get-FileHash $src -Algorithm SHA256).Hash
$dstHash = (Get-FileHash $dst -Algorithm SHA256).Hash
Say "source  $srcHash  $src"
Say "target  $dstHash  $dst"
if ($srcHash -ne $dstHash) {
    Say "FAIL: hash mismatch after copying; the copy in MediaCore was left alone"
    exit 5
}

if (Test-Path $old) {
    Remove-Item $old -Force
    Say "removed the old copy: $old"
} else {
    Say "no MediaCore copy to remove"
}

Say "deploy OK; restart After Effects to load it"
exit 0
