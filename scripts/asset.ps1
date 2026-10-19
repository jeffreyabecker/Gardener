#requires -Version 5.1
<#
.SYNOPSIS
    Convert a 1-bit BMP into the RLE asset the firmware draws.

.DESCRIPTION
    The output is the container docs/assets.md describes: a ten-byte header, a two-entry palette and
    the runs RLEBitmap walks. The drawing is whichever colour -Ink names and everything else is
    paper, which the renderer skips, so the panel keeps what was under the image.

    The two rules the decoder imposes are kept here: a run never crosses the end of a row (the
    renderer discards the overshoot), and no run is longer than 255 pixels.

.EXAMPLE
    powershell -File scripts/asset.ps1 -Source firmware/gardener/assets/eggplant.bmp `
                      -Out firmware/gardener/data/assets/eggplant.rle

.EXAMPLE
    powershell -File scripts/asset.ps1 -Source art/logo.bmp -Out data/assets/logo.rle -Ink White
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Out,

    # Which colour in the drawing is the ink. Art drawn dark-on-light uses the default; art drawn
    # light-on-dark -- what a one-bit panel shows natively -- says White.
    [ValidateSet('Black', 'White')][string]$Ink = 'Black'
)

$ErrorActionPreference = 'Stop'

# The container, and the only palette it ever holds: paper is colour 0, which the renderer treats as
# transparent, and ink is anything else.
$kMagic = [byte[]](0x47, 0x52, 0x4C, 0x31)   # "GRL1"
$kHeaderBytes = 10
$kFlags = 1                                  # one-byte runs are legal: the palette is 16 or fewer
$kPalettePaper = 0x0000
$kPaletteInk = 0xFFFF

function Get-Int16([byte[]]$bytes, [int]$offset) {
    return [int]($bytes[$offset] -bor ($bytes[$offset + 1] -shl 8))
}

function Get-Int32([byte[]]$bytes, [int]$offset) {
    return [int]($bytes[$offset] -bor ($bytes[$offset + 1] -shl 8) -bor
        ($bytes[$offset + 2] -shl 16) -bor ($bytes[$offset + 3] -shl 24))
}

if (-not (Test-Path $Source)) { throw "no such file: $Source" }
$bmp = [System.IO.File]::ReadAllBytes((Resolve-Path $Source))

if ($bmp.Length -lt 62 -or $bmp[0] -ne 0x42 -or $bmp[1] -ne 0x4D) { throw "$Source is not a BMP" }
$pixelOffset = Get-Int32 $bmp 10
$width = Get-Int32 $bmp 18
$height = Get-Int32 $bmp 22
$bitsPerPixel = Get-Int16 $bmp 28
$compression = Get-Int32 $bmp 30
$paletteCount = Get-Int32 $bmp 46

if ($bitsPerPixel -ne 1) { throw "$Source is $bitsPerPixel bits per pixel; this tool reads 1-bit BMPs" }
if ($compression -ne 0) { throw "$Source is compressed; save it as an uncompressed 1-bit BMP" }
if ($paletteCount -eq 0) { $paletteCount = 2 }

# A negative height means the rows are stored top down, which the format allows.
$topDown = $height -lt 0
if ($topDown) { $height = -$height }
if ($width -le 0 -or $height -le 0) { throw "$Source has no pixels" }
if ($width -gt 4096 -or $height -gt 4096) { throw "$Source is $width x $height, which is not a panel image" }

# The palette is RGBQUADs starting after the 40-byte header; luminance decides which entry is ink.
$paletteBase = 14 + 40
$inkIndex = 0
$inkLuma = if ($Ink -eq 'Black') { 1e9 } else { -1e9 }
for ($i = 0; $i -lt $paletteCount; $i++) {
    $b = $bmp[$paletteBase + $i * 4]
    $g = $bmp[$paletteBase + $i * 4 + 1]
    $r = $bmp[$paletteBase + $i * 4 + 2]
    $luma = 0.299 * $r + 0.587 * $g + 0.114 * $b
    if ($Ink -eq 'Black') {
        if ($luma -lt $inkLuma) { $inkLuma = $luma; $inkIndex = $i }
    } else {
        if ($luma -gt $inkLuma) { $inkLuma = $luma; $inkIndex = $i }
    }
}

# Runs: palette index 1 is ink, 0 is paper. One byte holds a four-bit length and a four-bit index;
# anything longer is the two-byte form -- index, then length -- and never crosses a row.
$runs = New-Object System.Collections.Generic.List[byte]
$rowBytes = [int]([math]::Floor((($width + 31) / 32))) * 4
$inkPixels = 0

for ($row = 0; $row -lt $height; $row++) {
    $sourceRow = if ($topDown) { $row } else { $height - 1 - $row }
    $rowBase = $pixelOffset + $sourceRow * $rowBytes

    $x = 0
    while ($x -lt $width) {
        $isInk = ((($bmp[$rowBase + ($x -shr 3)] -shr (7 - ($x -band 7))) -band 1) -eq $inkIndex)
        $length = 1
        while ($x + $length -lt $width -and $length -lt 255) {
            $next = $x + $length
            $nextInk = ((($bmp[$rowBase + ($next -shr 3)] -shr (7 - ($next -band 7))) -band 1) -eq $inkIndex)
            if ($nextInk -ne $isInk) { break }
            $length++
        }
        if ($isInk) { $inkPixels += $length }

        $index = if ($isInk) { 1 } else { 0 }
        if ($length -le 15) {
            $runs.Add([byte](($length -shl 4) -bor $index))
        } else {
            $runs.Add([byte]$index)
            $runs.Add([byte]$length)
        }
        $x += $length
    }
}

# "GRL1", the size, the flags, the palette length, then the palette.
$asset = New-Object System.Collections.Generic.List[byte]
$asset.AddRange($kMagic)
$asset.Add([byte]($width -band 0xFF)); $asset.Add([byte](($width -shr 8) -band 0xFF))
$asset.Add([byte]($height -band 0xFF)); $asset.Add([byte](($height -shr 8) -band 0xFF))
$asset.Add([byte]$kFlags)
$asset.Add([byte]2)
$asset.Add([byte]($kPalettePaper -band 0xFF)); $asset.Add([byte](($kPalettePaper -shr 8) -band 0xFF))
$asset.Add([byte]($kPaletteInk -band 0xFF)); $asset.Add([byte](($kPaletteInk -shr 8) -band 0xFF))
$asset.AddRange($runs)

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }
[System.IO.File]::WriteAllBytes([System.IO.Path]::GetFullPath($Out), $asset.ToArray())

$packed = [int]($width * $height / 8)
Write-Host ("{0}: {1}x{2}, ink {3} index {4}" -f (Split-Path -Leaf $Source), $width, $height, $Ink, $inkIndex)
Write-Host ("  {0} ink pixels, {1} bytes of runs, {2} bytes in the file (1-bit BMP would be {3})" -f
    $inkPixels, $runs.Count, $asset.Count, $packed)
Write-Host ("  wrote {0}" -f $Out)
