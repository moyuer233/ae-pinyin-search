@echo off
rem Build and run the matching-layer self-test outside After Effects.
rem Exit code = number of failed checks. ASCII only (Windows PowerShell 5.1 /
rem cmd read a BOM-less non-ASCII script as ANSI and break on it).
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "PROJ=H:\ae-sdk\AfterEffectsSDK_26.5_win\Examples\AEGP\AEPinyinSearch"
set "OUT=H:\ae-pinyin-search\build\mathtest"
set "SRC=H:\ae-pinyin-search\tools\pinyin_match_test.cpp"

if not exist "%VCVARS%" (
  echo [FAIL] vcvars64.bat not found: "%VCVARS%"
  exit /b 1
)
if not exist "%OUT%" mkdir "%OUT%"

call "%VCVARS%" >nul
if errorlevel 1 (
  echo [FAIL] vcvars64.bat failed
  exit /b 1
)

cl /nologo /utf-8 /EHsc /W3 /I"%PROJ%" /Fo"%OUT%\\" /Fe"%OUT%\match_test.exe" "%SRC%" "%PROJ%\pinyin_data.cpp"
if errorlevel 1 (
  echo [FAIL] compile failed
  exit /b 1
)

"%OUT%\match_test.exe"
exit /b %errorlevel%
