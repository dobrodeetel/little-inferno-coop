@echo off
setlocal
rem Little Inferno co-op installer.
rem Usage: install.bat ["<Little Inferno folder>"]   (default: Steam's standard folder)

set "SRC=%~dp0"
set "GAME=%~1"
if "%GAME%"=="" set "GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno Beta"
if not exist "%GAME%\Little Inferno.exe" if exist "%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno\Little Inferno.exe" set "GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Little Inferno"

if not exist "%GAME%\Little Inferno.exe" (
  echo Little Inferno not found in "%GAME%".
  echo Give the game folder: install.bat "D:\Games\Little Inferno"
  pause
  exit /b 1
)

copy /y "%SRC%winmm.dll" "%GAME%\winmm.dll" >nul
if errorlevel 1 goto fail
if not exist "%GAME%\licoop.ini" copy "%SRC%licoop.ini" "%GAME%\licoop.ini" >nul

echo Installed into "%GAME%".
echo Settings: licoop.ini next to the game.
pause
exit /b 0

:fail
echo Install failed. Is the game running?
pause
exit /b 1
