<#
    SMACK! - source-list drift check.

    Every build system in this repo carries its own list of source files:

        Makefile.sdl3        OBJS   (Linux / gcc)
        Makefile.mingw       OBJS   (Windows / mingw-w64)
        Makefile.msvc        OBJS   (Windows / MSVC, nmake)
        msvc\SMACK.vcxproj   ClCompile items (Visual Studio)

    Nothing enforces that they agree, so adding a .c file to one and forgetting
    the others produces a link error on a toolchain you were not using at the
    time -- often much later. This compares all four and reports the difference.

    Exit code 0 if they agree, 1 if they do not. build-mingw.bat and
    build-vs2019.bat run it before building and warn without stopping.

        powershell -ExecutionPolicy Bypass -File tools\check-sources.ps1
#>
[CmdletBinding()]
param([switch]$Quiet)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Get-List {
    param([string]$Path, [string]$Pattern, [int]$Group = 1)
    $full = Join-Path $root $Path
    if (-not (Test-Path $full)) { return $null }
    $text = Get-Content -Raw -LiteralPath $full
    $names = [regex]::Matches($text, $Pattern) | ForEach-Object { $_.Groups[$Group].Value }
    return , ($names | Sort-Object -Unique)
}

# $(O)/name.o  and  $(OUTDIR)\name.obj
$lists = [ordered]@{
    'Makefile.sdl3'      = Get-List 'Makefile.sdl3'  '\$\(O\)/([A-Za-z0-9_]+)\.o\b'
    'Makefile.mingw'     = Get-List 'Makefile.mingw' '\$\(O\)/([A-Za-z0-9_]+)\.o\b'
    'Makefile.msvc'      = Get-List 'Makefile.msvc'  '\$\(OUTDIR\)\\([A-Za-z0-9_]+)\.obj\b'
    'msvc\SMACK.vcxproj' = Get-List 'msvc\SMACK.vcxproj' 'ClCompile Include="\.\.\\(?:linux\\)?([A-Za-z0-9_]+)\.c"'
}

$missing = $lists.Keys | Where-Object { $null -eq $lists[$_] }
if ($missing) {
    Write-Host "check-sources: cannot read $($missing -join ', ')" -ForegroundColor Yellow
    exit 1
}

# Reference is the union; report what each list is missing or has extra.
$union = @($lists.Values | ForEach-Object { $_ }) | Sort-Object -Unique
$ok = $true

foreach ($name in $lists.Keys) {
    $have    = $lists[$name]
    $absent  = @($union | Where-Object { $have -notcontains $_ })
    if ($absent.Count) {
        $ok = $false
        Write-Host "check-sources: $name is missing:" -ForegroundColor Red
        $absent | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
    }
}

if ($ok) {
    if (-not $Quiet) {
        Write-Host "check-sources: all 4 build files list the same $($union.Count) sources." -ForegroundColor Green
    }
    exit 0
}

Write-Host ""
Write-Host "Add the missing sources to the build files listed above." -ForegroundColor Yellow
Write-Host "msvc\SMACK.vcxproj is generated from Makefile.msvc's OBJS." -ForegroundColor Yellow
exit 1
