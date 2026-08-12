@echo off
rem ===========================================================================
rem Build SDL3 as a STATIC library with MSVC, for the standalone SMACK! build.
rem
rem The SDL3-devel-VC SDK ships only SDL3.dll plus an import library, so a
rem single-file smack.exe needs SDL3 compiled from source. This fetches the
rem source and builds it with the static CRT (/MT), matching what
rem `nmake /f Makefile.msvc STATIC=1` links against.
rem
rem Run from a VS2019 x64 developer prompt:
rem
rem     msvc\build-sdl3-static.bat [install-prefix]
rem
rem Default prefix is C:\Source\SDL3-static, which is where Makefile.msvc and
rem the ReleaseStatic project configuration look unless you override
rem SDL3_STATIC_DIR. Takes a few minutes; only needs doing once per SDL
rem version.
rem ===========================================================================
setlocal enabledelayedexpansion

set SDL_VER=3.4.12
set PREFIX=%~1
if "%PREFIX%"=="" set PREFIX=C:\Source\SDL3-static
set WORK=%TEMP%\smack-sdl3-static
set SRC=%WORK%\SDL3-%SDL_VER%
set BUILD=%WORK%\build

rem --- locate a native CMake (VS2019 bundles one; a Cygwin/MSYS cmake will not
rem     drive the MSVC generator correctly, so do not fall back to PATH blindly)
rem NB: %ProgramFiles(x86)% must be copied into a plain variable and expanded
rem with ! rather than % here -- expanded with %, its literal "(x86)" closes the
rem for-loop's parenthesised list at parse time.
set "PF86=%ProgramFiles(x86)%"
set CMAKE=
for %%E in (Community BuildTools Professional Enterprise) do (
  set "CANDIDATE=!PF86!\Microsoft Visual Studio\2019\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  if exist "!CANDIDATE!" if "!CMAKE!"=="" set "CMAKE=!CANDIDATE!"
)
if "%CMAKE%"=="" (
  echo ERROR: no native CMake found under the VS2019 install.
  echo Install the "C++ CMake tools for Windows" component, or set CMAKE by hand.
  exit /b 1
)
echo Using CMake: %CMAKE%

if not exist "%WORK%" mkdir "%WORK%"

rem --- fetch the source (skipped if already unpacked)
if not exist "%SRC%\CMakeLists.txt" (
  if not exist "%WORK%\SDL3-%SDL_VER%.zip" (
    echo Downloading SDL3 %SDL_VER% source...
    curl -L -o "%WORK%\SDL3-%SDL_VER%.zip" ^
      "https://github.com/libsdl-org/SDL/releases/download/release-%SDL_VER%/SDL3-%SDL_VER%.zip"
    if errorlevel 1 echo ERROR: download failed & exit /b 1
  )
  echo Extracting...
  rem Call Windows' own bsdtar by full path: a Cygwin/MSYS tar earlier on PATH
  rem reads "C:\..." as a remote host:path and fails with "Cannot connect to C".
  "%SystemRoot%\System32\tar.exe" -xf "%WORK%\SDL3-%SDL_VER%.zip" -C "%WORK%"
  if errorlevel 1 echo ERROR: extract failed & exit /b 1
)

rem --- configure
rem CMAKE_MSVC_RUNTIME_LIBRARY needs policy CMP0091 in NEW mode to take effect.
rem MultiThreaded (no DLL suffix) is /MT, i.e. the static CRT -- it must match
rem the /MT that Makefile.msvc STATIC=1 compiles the engine with, or the two
rem end up with separate CRT heaps.
echo Configuring...
"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G "Visual Studio 16 2019" -A x64 ^
  -DSDL_SHARED=OFF ^
  -DSDL_STATIC=ON ^
  -DSDL_TEST_LIBRARY=OFF ^
  -DSDL_TESTS=OFF ^
  -DSDL_EXAMPLES=OFF ^
  -DSDL_INSTALL_TESTS=OFF ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX%"
if errorlevel 1 echo ERROR: configure failed & exit /b 1

echo Building (this takes a few minutes)...
"%CMAKE%" --build "%BUILD%" --config Release --parallel
if errorlevel 1 echo ERROR: build failed & exit /b 1

echo Installing to %PREFIX% ...
"%CMAKE%" --install "%BUILD%" --config Release
if errorlevel 1 echo ERROR: install failed & exit /b 1

echo.
echo Done. Static SDL3 installed under %PREFIX%
dir /b "%PREFIX%\lib"
endlocal
