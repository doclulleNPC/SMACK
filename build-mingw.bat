@echo off
rem ===========================================================================
rem SMACK! - build the Windows binary with mingw-w64.
rem
rem A wrapper around Makefile.mingw: it locates the toolchain, then hands the
rem actual work to make, so this file cannot drift away from the real build.
rem
rem   build-mingw.bat            release build  -> obj-win\smack.exe, run\
rem   build-mingw.bat debug      debug build    -> objdebug-win\smack.exe, run\
rem   build-mingw.bat clean      remove obj-win\ and objdebug-win\
rem   build-mingw.bat rebuild    clean, then release build
rem
rem Set SDL3_DIR beforehand to use a different SDL3 SDK; the default is
rem C:\Source\SDL3.
rem
rem See build-vs2019.bat for the Visual Studio build. Both deploy into run\.
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem --- what to build -------------------------------------------------------
set "TARGET="
set "DOCLEAN="
if /i "%~1"=="debug"   set "TARGET=debug"
if /i "%~1"=="clean"   set "TARGET=clean"
if /i "%~1"=="rebuild" set "DOCLEAN=1"
if not "%~1"=="" if "%TARGET%%DOCLEAN%"=="" (
  echo Unknown option "%~1".  Use: debug ^| clean ^| rebuild ^(or no argument^).
  exit /b 2
)

rem --- GNU make ------------------------------------------------------------
rem Makefile.mingw uses POSIX recipes (mkdir -p, cp, ln -sf), so it needs a
rem make that runs them through a Unix shell -- Cygwin's or MSYS2's. A bare
rem mingw32-make driving cmd.exe cannot execute those.
set "MAKE="
for %%M in (make.exe) do if not "%%~$PATH:M"=="" set "MAKE=%%~$PATH:M"
if "%MAKE%"=="" if exist "C:\cygwin64\bin\make.exe" set "MAKE=C:\cygwin64\bin\make.exe"
if "%MAKE%"=="" if exist "C:\cygwin\bin\make.exe"   set "MAKE=C:\cygwin\bin\make.exe"
if "%MAKE%"=="" if exist "C:\msys64\usr\bin\make.exe" set "MAKE=C:\msys64\usr\bin\make.exe"
if "%MAKE%"=="" (
  echo ERROR: no GNU make found.
  echo Install make via Cygwin or MSYS2, or put it on PATH.
  exit /b 1
)

rem --- mingw-w64 gcc -------------------------------------------------------
set "MINGWCC="
for %%C in (x86_64-w64-mingw32-gcc.exe) do if not "%%~$PATH:C"=="" set "MINGWCC=x86_64-w64-mingw32-gcc"
if "%MINGWCC%"=="" if exist "C:\cygwin64\bin\x86_64-w64-mingw32-gcc.exe" set "MINGWCC=C:/cygwin64/bin/x86_64-w64-mingw32-gcc"
if "%MINGWCC%"=="" if exist "C:\cygwin\bin\x86_64-w64-mingw32-gcc.exe"   set "MINGWCC=C:/cygwin/bin/x86_64-w64-mingw32-gcc"
if "%MINGWCC%"=="" if exist "C:\msys64\mingw64\bin\gcc.exe"              set "MINGWCC=C:/msys64/mingw64/bin/gcc"
if "%MINGWCC%"=="" (
  echo ERROR: no mingw-w64 gcc found.
  echo Install the Cygwin package "mingw64-x86_64-gcc-core", or MSYS2's
  echo mingw-w64-x86_64-gcc, or put one on PATH.
  exit /b 1
)

rem --- SDL3 ----------------------------------------------------------------
if "%SDL3_DIR%"=="" set "SDL3_DIR=C:\Source\SDL3"
rem Makefile.mingw wants forward slashes: it is driven by a Unix shell here.
set "SDL3_FWD=%SDL3_DIR:\=/%"
if not exist "%SDL3_FWD%/include/SDL3/SDL.h" (
  echo ERROR: no SDL3 headers under "%SDL3_DIR%".
  echo Expected "%SDL3_DIR%\include\SDL3\SDL.h".
  echo Set SDL3_DIR to your SDL3 SDK.
  exit /b 1
)

echo make : %MAKE%
echo CC   : %MINGWCC%
echo SDL3 : %SDL3_DIR%
echo.

rem --- keep the four build files in step -----------------------------------
where powershell >nul 2>&1
if not errorlevel 1 (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\check-sources.ps1"
  if errorlevel 1 echo WARNING: build files disagree about the source list ^(continuing^).
  echo.
)

rem --- build ---------------------------------------------------------------
if defined DOCLEAN (
  "%MAKE%" -f Makefile.mingw clean
)
"%MAKE%" -f Makefile.mingw CC=%MINGWCC% SDL3_DIR=%SDL3_FWD% %TARGET%
if errorlevel 1 (
  echo.
  echo BUILD FAILED
  exit /b 1
)

if /i "%~1"=="clean" (
  echo.
  echo Cleaned.
  exit /b 0
)

echo.
echo Built run\smack.exe  ^(mingw-w64; needs SDL3.dll beside it^)
endlocal
