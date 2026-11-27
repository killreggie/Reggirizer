@echo off
rem Builds the Reggirizer VST3 for Windows (64-bit) and installs it for FL Studio.
rem Needs: Visual Studio 2022 (free Community edition) with "Desktop development with C++".
rem Double-click this file, or run it from a Developer Command Prompt.

setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    rem Visual Studio ships its own CMake; find it.
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VSDIR=%%i"
    if defined VSDIR set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
)
where cmake >nul 2>nul
if errorlevel 1 (
    echo Could not find CMake. Install Visual Studio 2022 Community with "Desktop development with C++".
    pause
    exit /b 1
)

if not exist JUCE\CMakeLists.txt (
    echo The JUCE folder is missing next to this script.
    pause
    exit /b 1
)

echo === Configuring ===
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 || goto :fail

echo === Building (this takes a few minutes the first time) ===
cmake --build build-win --config Release --target Reggirizer_VST3 || goto :fail

set "OUT=build-win\Reggirizer_artefacts\Release\VST3\Reggirizer.vst3"
echo.
echo Built: %CD%\%OUT%
echo.
echo === Installing to C:\Program Files\Common Files\VST3 (needs admin) ===
powershell -NoProfile -Command "Start-Process cmd -Verb RunAs -Wait -ArgumentList '/c xcopy /E /I /Y \"%CD%\%OUT%\" \"C:\Program Files\Common Files\VST3\Reggirizer.vst3\"'"
if exist "C:\Program Files\Common Files\VST3\Reggirizer.vst3" (
    echo Installed. In FL Studio: Options - Manage plugins - Find plugins.
) else (
    echo Could not install automatically. Copy the Reggirizer.vst3 folder above into
    echo C:\Program Files\Common Files\VST3 yourself.
)
pause
exit /b 0

:fail
echo.
echo Build failed - see the messages above.
pause
exit /b 1
