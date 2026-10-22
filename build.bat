@echo off
rem Build the winmm.dll proxy and, optionally, copy it into a game folder.
rem Usage: build.bat                build only (build\winmm.dll, package\winmm.dll)
rem        build.bat "<game dir>"   build and install there
rem Needs Visual Studio 2022+ or Build Tools with the C++ workload (x86 libraries).
setlocal
set ROOT=%~dp0
set GAME=%~1
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set VS=%%i
if not defined VS (echo Visual Studio not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist "%ROOT%build" mkdir "%ROOT%build"
if not exist "%ROOT%package" mkdir "%ROOT%package"
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /LD /I"%ROOT%src" "%ROOT%src\proxy.cpp" "%ROOT%src\coop.cpp" "%ROOT%src\overlay.cpp" /Fo"%ROOT%build\\" /Fe"%ROOT%build\winmm.dll" /link user32.lib gdi32.lib shell32.lib hid.lib setupapi.lib d3d11.lib dxgi.lib || exit /b 1
copy /y "%ROOT%build\winmm.dll" "%ROOT%package\winmm.dll" >nul || exit /b 1
if "%GAME%"=="" (echo Built build\winmm.dll & exit /b 0)
copy /y "%ROOT%build\winmm.dll" "%GAME%\winmm.dll" >nul || exit /b 1
echo Installed into %GAME%
