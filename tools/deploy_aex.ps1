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

param(
    [int]$WaitSeconds = 900
)

$ErrorActionPreference = 'Stop'

$src = 'H:\ae-sdk\build\AEGP\AEPinyinSearch.aex'
$dstDir = 'H:\adobe\Adobe After Effects 2026\Support Files\Plug-ins\Extensions'
$dst = Join-Path $dstDir 'AEPinyinSearch.aex'
$old = 'C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\AEPinyinSearch.aex'
$log = 'H:\ae-pinyin-search\build\deploy_aex.log'

function Say([string]$message) {
    $line = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')  $message"
    Write-Host $line
    Add-Content -Path $log -Value $line -Encoding UTF8
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

if (Test-Path $old) {
    Remove-Item $old -Force
    Say "removed old copy: $old"
} else {
    Say "no MediaCore copy to remove"
}

if (-not (Test-Path $dstDir)) {
    Say "FAIL: target folder missing: $dstDir"
    exit 4
}

Copy-Item $src $dst -Force
$srcHash = (Get-FileHash $src -Algorithm SHA256).Hash
$dstHash = (Get-FileHash $dst -Algorithm SHA256).Hash
Say "source  $srcHash  $src"
Say "target  $dstHash  $dst"
if ($srcHash -ne $dstHash) {
    Say "FAIL: hash mismatch"
    exit 5
}

Say "deploy OK; restart After Effects to load it"
exit 0
