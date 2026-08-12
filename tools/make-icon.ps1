<#
    Generate the application icon artefacts from tools/appicon.png.

        powershell -ExecutionPolicy Bypass -File tools\make-icon.ps1

    Produces:
      res\smack.ico       multi-size Windows icon (16/24/32/48/64/128/256),
                          32-bit BGRA DIB entries with a real AND mask, so it
                          works everywhere rather than only on PNG-in-ICO-aware
                          shells.
      res\icon_rgba.h     the same artwork as a 64x64 RGBA byte array, handed to
                          SDL_SetWindowIcon at runtime. That is what gives the
                          window and taskbar an icon on Linux, where there is no
                          resource section to read one from.

    Re-run after changing appicon.png; both outputs are committed so a normal
    build needs neither PowerShell nor this script.
#>
[CmdletBinding()]
param(
    [string]$Source,
    [string]$OutDir
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# Resolved here rather than as param defaults: $PSScriptRoot is not reliably
# populated while parameter defaults are being bound.
$toolsDir = Split-Path -Parent $PSCommandPath
$repoRoot = Split-Path -Parent $toolsDir
if (-not $Source) { $Source = Join-Path $toolsDir 'appicon.png' }
if (-not $OutDir) { $OutDir = Join-Path $repoRoot 'res' }

if (-not (Test-Path $Source)) { throw "source image not found: $Source" }
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$src = [System.Drawing.Image]::FromFile($Source)
Write-Host ("source: {0} ({1}x{2})" -f $Source, $src.Width, $src.Height)

# Render the source at one size, premultiplied-free straight BGRA.
function Get-Resized([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CompositingMode = 'SourceCopy'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.SmoothingMode = 'HighQuality'
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $size, $size))
    $g.Dispose()
    return $bmp
}

# Raw BGRA bytes, top-down.
function Get-Bgra([System.Drawing.Bitmap]$bmp) {
    $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($bmp.Width * $bmp.Height * 4)
    # copy row by row: Stride may exceed width*4
    for ($y = 0; $y -lt $bmp.Height; $y++) {
        $srcPtr = [IntPtr]::Add($data.Scan0, $y * $data.Stride)
        [System.Runtime.InteropServices.Marshal]::Copy($srcPtr, $bytes, $y * $bmp.Width * 4, $bmp.Width * 4)
    }
    $bmp.UnlockBits($data)
    return $bytes
}

# ---------------------------------------------------------------- the .ico ---
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()

foreach ($s in $sizes) {
    $bmp = Get-Resized $s
    $bgra = Get-Bgra $bmp
    $bmp.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    # BITMAPINFOHEADER -- height is doubled to cover the XOR image + AND mask
    $bw.Write([uint32]40)
    $bw.Write([int32]$s)
    $bw.Write([int32]($s * 2))
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]0)            # BI_RGB
    $bw.Write([uint32]($s * $s * 4))
    $bw.Write([int32]0); $bw.Write([int32]0)
    $bw.Write([uint32]0); $bw.Write([uint32]0)

    # XOR image, bottom-up
    for ($y = $s - 1; $y -ge 0; $y--) {
        $bw.Write($bgra, $y * $s * 4, $s * 4)
    }

    # AND mask, bottom-up, 1bpp, rows padded to 4 bytes. A set bit means
    # "transparent"; 32-bit icons are drawn from the alpha channel, but a
    # correct mask keeps older shells from drawing a black box behind the art.
    $maskRow = [int][Math]::Ceiling($s / 8.0)
    $maskPad = (4 - ($maskRow % 4)) % 4
    for ($y = $s - 1; $y -ge 0; $y--) {
        $row = New-Object byte[] $maskRow
        for ($x = 0; $x -lt $s; $x++) {
            # [int](x/8) would *round* in PowerShell, overrunning the row
            $byte = [int][Math]::Floor($x / 8)
            $a = $bgra[($y * $s + $x) * 4 + 3]
            if ($a -eq 0) { $row[$byte] = $row[$byte] -bor (0x80 -shr ($x % 8)) }
        }
        $bw.Write($row, 0, $maskRow)
        if ($maskPad) { $bw.Write((New-Object byte[] $maskPad), 0, $maskPad) }
    }

    $bw.Flush()
    $images += , @{ size = $s; bytes = $ms.ToArray() }
    $bw.Dispose(); $ms.Dispose()
}

$icoPath = Join-Path $OutDir 'smack.ico'
$fs = [System.IO.File]::Create($icoPath)
$bw = New-Object System.IO.BinaryWriter($fs)

$bw.Write([uint16]0)                 # reserved
$bw.Write([uint16]1)                 # type 1 = icon
$bw.Write([uint16]$images.Count)

$offset = 6 + 16 * $images.Count
foreach ($im in $images) {
    $dim = if ($im.size -ge 256) { 0 } else { $im.size }   # 256 is encoded as 0
    $bw.Write([byte]$dim); $bw.Write([byte]$dim)
    $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$im.bytes.Length)
    $bw.Write([uint32]$offset)
    $offset += $im.bytes.Length
}
foreach ($im in $images) { $bw.Write($im.bytes, 0, $im.bytes.Length) }
$bw.Flush(); $bw.Dispose(); $fs.Dispose()
Write-Host ("wrote {0} ({1} images, {2:N0} bytes)" -f $icoPath, $images.Count, (Get-Item $icoPath).Length)

# ------------------------------------------------------- the RGBA C array ---
$iconSize = 64
$bmp = Get-Resized $iconSize
$bgra = Get-Bgra $bmp
$bmp.Dispose()
$src.Dispose()

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('// Generated by tools/make-icon.ps1 from tools/appicon.png -- do not edit.')
[void]$sb.AppendLine('//')
[void]$sb.AppendLine('// The application icon as straight RGBA, handed to SDL_SetWindowIcon so the')
[void]$sb.AppendLine('// window and taskbar entry are branded on every platform. Windows also gets')
[void]$sb.AppendLine('// res/smack.ico linked in as a resource, which is what Explorer shows; Linux')
[void]$sb.AppendLine('// has no equivalent, so this array is the only icon there.')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#ifndef __SMACK_ICON_RGBA__')
[void]$sb.AppendLine('#define __SMACK_ICON_RGBA__')
[void]$sb.AppendLine('')
[void]$sb.AppendLine("#define SMACK_ICON_SIZE $iconSize")
[void]$sb.AppendLine('')
[void]$sb.AppendLine('static const unsigned char smack_icon_rgba[SMACK_ICON_SIZE * SMACK_ICON_SIZE * 4] = {')

$line = '  '
for ($i = 0; $i -lt $bgra.Length; $i += 4) {
    # BGRA (GDI+) -> RGBA (what SDL_PIXELFORMAT_RGBA32 expects)
    $r = $bgra[$i + 2]; $g = $bgra[$i + 1]; $b = $bgra[$i]; $a = $bgra[$i + 3]
    $line += ('{0},{1},{2},{3},' -f $r, $g, $b, $a)
    if ($line.Length -ge 76) { [void]$sb.AppendLine($line); $line = '  ' }
}
if ($line.Trim()) { [void]$sb.AppendLine($line) }
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#endif')

$hPath = Join-Path $OutDir 'icon_rgba.h'
[System.IO.File]::WriteAllText($hPath, $sb.ToString())
Write-Host ("wrote {0} ({1}x{1} RGBA, {2:N0} bytes)" -f $hPath, $iconSize, (Get-Item $hPath).Length)

# ------------------------------------------- 256px PNG for Linux desktops ---
# Freedesktop icon themes want a plain PNG on disk (hicolor/256x256/apps/),
# which res/smack.desktop points at.
$src2 = [System.Drawing.Image]::FromFile($Source)
$pngBmp = New-Object System.Drawing.Bitmap 256, 256, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g2 = [System.Drawing.Graphics]::FromImage($pngBmp)
$g2.CompositingMode = 'SourceCopy'
$g2.InterpolationMode = 'HighQualityBicubic'
$g2.PixelOffsetMode = 'HighQuality'
$g2.DrawImage($src2, (New-Object System.Drawing.Rectangle 0, 0, 256, 256))
$g2.Dispose()
$pngPath = Join-Path $OutDir 'smack.png'
$pngBmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$pngBmp.Dispose(); $src2.Dispose()
Write-Host ("wrote {0} (256x256, {1:N0} bytes)" -f $pngPath, (Get-Item $pngPath).Length)
