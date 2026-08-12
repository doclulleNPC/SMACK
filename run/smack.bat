@echo off
rem ---------------------------------------------------------------------------
rem SMACK! launcher.
rem
rem Runs the smack.exe sitting next to this file, with this folder as the
rem working directory. Both matter: the engine finds smack.wad and writes
rem smack.cfg relative to its own path, looks for an IWAD in the current
rem directory first, and puts savegames there too.
rem
rem   smack.bat                      use whichever IWAD is in this folder
rem   smack.bat -iwad DOOM2.WAD      pick one explicitly
rem   smack.bat -warp 1 -skill 4     any smack.exe option is forwarded
rem
rem Replaces the original smmu.bat, which launched the 1999 DOS build from a
rem hard-coded C:\doom2 layout.
rem ---------------------------------------------------------------------------
setlocal
cd /d "%~dp0"

if not exist "smack.exe" (
  echo smack.exe not found in "%~dp0".
  echo.
  echo Build it first, from the repository root:
  echo     nmake /f Makefile.msvc          ^(Visual Studio^)
  echo     make -f Makefile.mingw          ^(mingw-w64^)
  echo.
  pause
  exit /b 1
)

rem Launch by full path rather than as a bare name: with
rem NoDefaultCurrentDirectoryInExePath set (MSYS/Git-Bash shells export it),
rem cmd does not search the current directory and a bare "smack.exe" fails
rem even though it is sitting right here.
"%~dp0smack.exe" %*

rem Keep the window up if it failed, so a double-click shows the reason.
if errorlevel 1 pause
endlocal
