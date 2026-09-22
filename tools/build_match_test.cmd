@echo off
rem Build and run the matching-layer self-test outside After Effects.
rem Exit code = number of failed checks. ASCII only (Windows PowerShell 5.1 /
rem cmd read a BOM-less non-ASCII script as ANSI and break on it).
rem
rem AE_SDK_DIR / VCVARS64 override the defaults below.
setlocal enabledelayedexpansion

if not defined AE_SDK_DIR set "AE_SDK_DIR=H:\ae-sdk\AfterEffectsSDK_26.5_win"
set "PROJ=%AE_SDK_DIR%\Examples\AEGP\AEPinyinSearch"
set "REPO=%~dp0.."
set "OUT=%REPO%\build\mathtest"
set "SRC=%~dp0pinyin_match_test.cpp"

if not exist "%PROJ%\pinyin_match.h" (
  echo [FAIL] SDK project not found: "%PROJ%"  ^(set AE_SDK_DIR^)
  exit /b 1
)

if not defined VCVARS64 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do set "VCVARS64=%%i"
  )
)
if not defined VCVARS64 set "VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS64%" (
  echo [FAIL] vcvars64.bat not found: "%VCVARS64%"  ^(set VCVARS64^)
  exit /b 1
)
if not exist "%OUT%" mkdir "%OUT%"

call "%VCVARS64%" >nul
if errorlevel 1 (
  echo [FAIL] vcvars64.bat failed
  exit /b 1
)

cl /nologo /utf-8 /EHsc /W3 /I"%PROJ%" /I"%PROJ%\Win" /Fo"%OUT%\\" /Fe"%OUT%\match_test.exe" "%SRC%" "%PROJ%\pinyin_data.cpp"
if errorlevel 1 (
  echo [FAIL] compile failed
  exit /b 1
)

"%OUT%\match_test.exe"
exit /b %errorlevel%
