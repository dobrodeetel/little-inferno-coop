@echo off
setlocal
rem Little Inferno co-op uninstaller: removes winmm.dll (the game never shipped one).
rem Usage: uninstall.bat ["<Little Inferno folder>"]

set "GAME=%~1"
if "%GAME%"=="" set "GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno Beta"
if not exist "%GAME%\winmm.dll" if exist "%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno\winmm.dll" set "GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno"

if not exist "%GAME%\winmm.dll" (
  echo The mod is not installed in "%GAME%".
  pause
  exit /b 1
)
del "%GAME%\winmm.dll"
if exist "%GAME%\winmm.dll" goto fail
echo Removed. licoop.ini and licoop.log were left in place.
pause
exit /b 0

:fail
echo Uninstall failed. Is the game running?
pause
exit /b 1
