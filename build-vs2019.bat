@echo off
rem ===========================================================================
rem SMACK! - build the Windows binary with Visual Studio 2019 (MSVC).
rem
rem A wrapper around Makefile.msvc: it enters the VS2019 x64 environment, then
rem hands the actual work to nmake, so this file cannot drift away from the
rem real build.
rem
rem   build-vs2019.bat            release   -> obj-msvc\smack.exe, run\
rem   build-vs2019.bat debug      debug     -> obj-msvc-debug\
rem   build-vs2019.bat static     standalone single-file exe (no SDL3.dll, no
rem                               VC++ redistributable). Run
rem                               msvc\build-sdl3-static.bat once first.
rem   build-vs2019.bat ide        build msvc\SMACK.sln with MSBuild instead
rem   build-vs2019.bat clean      remove the obj-msvc* directories
rem   build-vs2019.bat rebuild    clean, then release build
rem
rem Set SDL3_DIR / SDL3_STATIC_DIR beforehand to use different SDL3 SDKs.
rem
rem See build-mingw.bat for the mingw-w64 build. Both deploy into run\.
rem ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem --- what to build -------------------------------------------------------
set "NMAKEARGS="
set "DOCLEAN="
set "MODE=release"
if /i "%~1"=="debug"   set "NMAKEARGS=CFG=Debug"      & set "MODE=debug"
if /i "%~1"=="static"  set "NMAKEARGS=STATIC=1"       & set "MODE=static"
if /i "%~1"=="ide"     set "MODE=ide"
if /i "%~1"=="clean"   set "MODE=clean"
if /i "%~1"=="rebuild" set "DOCLEAN=1"
if not "%~1"=="" if "%MODE%"=="release" if not defined DOCLEAN (
  echo Unknown option "%~1".
  echo Use: debug ^| static ^| ide ^| clean ^| rebuild ^(or no argument^).
  exit /b 2
)

rem --- VS2019 environment --------------------------------------------------
rem NB: %ProgramFiles(x86)% has to go through a plain variable and be expanded
rem with ! here -- expanded with %, its literal "(x86)" would close the
rem for-loop's parenthesised list at parse time.
set "PF86=%ProgramFiles(x86)%"
set "VCVARS="
for %%E in (Community BuildTools Professional Enterprise) do (
  set "CAND=!PF86!\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat"
  if exist "!CAND!" if "!VCVARS!"=="" set "VCVARS=!CAND!"
)
if "%VCVARS%"=="" (
  echo ERROR: Visual Studio 2019 not found.
  rem !PF86! not %PF86%: inside a parenthesised block, % expansion happens at
  rem parse time and the literal "(x86)" would close the block early.
  echo Looked for VC\Auxiliary\Build\vcvars64.bat under
  echo   !PF86!\Microsoft Visual Studio\2019\{Community,BuildTools,Professional,Enterprise}
  exit /b 1
)
echo VS2019 : %VCVARS%

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: vcvars64.bat failed.
  exit /b 1
)

rem --- SDL3 ----------------------------------------------------------------
if "%SDL3_DIR%"=="" set "SDL3_DIR=C:\Source\SDL3"
if "%SDL3_STATIC_DIR%"=="" set "SDL3_STATIC_DIR=C:\Source\SDL3-static"

if /i "%MODE%"=="static" (
  echo SDL3   : %SDL3_STATIC_DIR%  ^(static^)
  if not exist "%SDL3_STATIC_DIR%\lib\SDL3-static.lib" (
    echo.
    echo ERROR: no static SDL3 at "%SDL3_STATIC_DIR%\lib\SDL3-static.lib".
    echo Build one first:  msvc\build-sdl3-static.bat
    exit /b 1
  )
) else if not "%MODE%"=="clean" (
  echo SDL3   : %SDL3_DIR%
  if not exist "%SDL3_DIR%\include\SDL3\SDL.h" (
    echo.
    echo ERROR: no SDL3 headers under "%SDL3_DIR%".
    echo Set SDL3_DIR to your SDL3 SDK.
    exit /b 1
  )
)
echo.

rem --- keep the four build files in step -----------------------------------
if not "%MODE%"=="clean" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\check-sources.ps1"
  if errorlevel 1 echo WARNING: build files disagree about the source list ^(continuing^).
  echo.
)

rem --- build ---------------------------------------------------------------
if /i "%MODE%"=="ide" (
  msbuild msvc\SMACK.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
  if errorlevel 1 goto :failed
  echo.
  echo Built run\smack.exe  ^(VS2019 solution, Release^)
  goto :done
)

if /i "%MODE%"=="clean" (
  nmake /nologo /f Makefile.msvc clean
  if errorlevel 1 goto :failed
  echo Cleaned.
  goto :done
)

if defined DOCLEAN nmake /nologo /f Makefile.msvc clean

nmake /nologo /f Makefile.msvc %NMAKEARGS%
if errorlevel 1 goto :failed

echo.
if /i "%MODE%"=="static" (
  echo Built run\smack.exe  ^(standalone: no SDL3.dll, no VC++ redistributable^)
) else (
  echo Built run\smack.exe  ^(%MODE%; needs SDL3.dll beside it^)
)
goto :done

:failed
echo.
echo BUILD FAILED
exit /b 1

:done
endlocal
