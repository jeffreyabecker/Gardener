#requires -Version 5.1
<#
.SYNOPSIS
    Convert a BMP into the monochrome RLE asset the firmware draws.

.DESCRIPTION
    Any uncompressed BMP -- 1, 4, 8, 24 or 32 bits per pixel -- becomes the one-bit image the panel
    can show. The output is the container docs/specs/assets.md describes: a ten-byte header, a two-entry
    palette and the runs RLEBitmap walks.

    Colour or grey is reduced by luminance: a pixel darker than -Threshold is ink and everything else
    is paper, which the renderer skips so the page keeps what was under the image. -Invert turns that
    round, for art drawn light on dark. -Dither spreads the error of that cut, which is what a photo
    or a gradient needs to survive on one bit; line art is better without it.

    The two rules the decoder imposes are kept here: a run never crosses the end of a row (the
    renderer discards the overshoot), and no run is longer than 255 pixels.

.EXAMPLE
    powershell -File scripts/asset.ps1 -Source firmware/gardener/assets/eggplant.bmp `
                      -Out firmware/gardener/data/assets/eggplant.rle

.EXAMPLE
    powershell -File scripts/asset.ps1 -Source art/logo.bmp -Out data/assets/logo.rle -Dither

.EXAMPLE
    powershell -File scripts/asset.ps1 -Source art/mark.bmp -Out data/assets/mark.rle -Invert
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Out,

    # The luminance a pixel has to fall below to be ink, 0 to 255. The default sits in the middle,
    # which is where a black-on-white drawing lands either side of.
    [ValidateRange(1, 254)][int]$Threshold = 128,

    # The drawing is the light part, for art made light on dark.
    [switch]$Invert,

    # Spread the error of the cut over the neighbours the scan has not reached, so a gradient becomes
    # a dot pattern rather than a hard edge. Photographs need it; line art does not.
    [switch]$Dither
)

$ErrorActionPreference = 'Stop'

# The container, and the only palette it ever holds: paper is colour 0, which the renderer treats as
# transparent, and ink is anything else.
$kMagic = [byte[]](0x47, 0x52, 0x4C, 0x31)   # "GRL1"
$kFlags = 1                                  # one-byte runs are legal: the palette has 2 entries
$kPalettePaper = 0x0000
$kPaletteInk = 0xFFFF

function Get-Int16([byte[]]$bytes, [int]$offset) {
    return [int]($bytes[$offset] -bor ($bytes[$offset + 1] -shl 8))
}

function Get-Int32([byte[]]$bytes, [int]$offset) {
    return [int]($bytes[$offset] -bor ($bytes[$offset + 1] -shl 8) -bor
        ($bytes[$offset + 2] -shl 16) -bor ($bytes[$offset + 3] -shl 24))
}

# ITU-R BT.601, which is how a colour becomes the one number the panel's bit has to come from.
function Get-Luma([int]$b, [int]$g, [int]$r) {
    return 0.299 * $r + 0.587 * $g + 0.114 * $b
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

if (@(1, 4, 8, 24, 32) -notcontains $bitsPerPixel) {
    throw "$Source is $bitsPerPixel bits per pixel; this tool reads 1, 4, 8, 24 and 32"
}
if ($compression -ne 0) {
    throw "$Source is compressed (mode $compression); save it as an uncompressed BMP"
}

# A negative height means the rows are stored top down, which the format allows.
$topDown = $height -lt 0
if ($topDown) { $height = -$height }
if ($width -le 0 -or $height -le 0) { throw "$Source has no pixels" }
if ($width -gt 4096 -or $height -gt 4096) { throw "$Source is $width x $height, which is not a panel image" }

# 1, 4 and 8 bits are indices into a palette; the 24 and 32 bit forms are colours in the pixels.
$paletteBase = 14 + 40
$paletteLuma = @()
if ($bitsPerPixel -le 8) {
    if ($paletteCount -eq 0) { $paletteCount = 1 -shl $bitsPerPixel }
    if ($paletteBase + $paletteCount * 4 -gt $bmp.Length) { throw "$Source has a truncated palette" }
    for ($i = 0; $i -lt $paletteCount; $i++) {
        $paletteLuma += Get-Luma $bmp[$paletteBase + $i * 4] $bmp[$paletteBase + $i * 4 + 1] $bmp[$paletteBase + $i * 4 + 2]
    }
}

# Every pixel as a luminance, top row first, whatever the file's own layout was.
$rowBytes = [int][math]::Floor((($bitsPerPixel * $width + 31) / 32)) * 4
$luma = New-Object 'double[]' ($width * $height)
$inkPixels = 0

for ($row = 0; $row -lt $height; $row++) {
    $sourceRow = if ($topDown) { $row } else { $height - 1 - $row }
    $rowBase = $pixelOffset + $sourceRow * $rowBytes
    $target = $row * $width

    if ($bitsPerPixel -eq 24 -or $bitsPerPixel -eq 32) {
        $step = $bitsPerPixel / 8
        for ($x = 0; $x -lt $width; $x++) {
            $p = $rowBase + $x * $step
            $luma[$target + $x] = Get-Luma $bmp[$p] $bmp[$p + 1] $bmp[$p + 2]
        }
        continue
    }

    for ($x = 0; $x -lt $width; $x++) {
        $byte = $bmp[$rowBase + [int][math]::Floor($x * $bitsPerPixel / 8)]
        switch ($bitsPerPixel) {
            1 { $index = ($byte -shr (7 - ($x -band 7))) -band 1 }
            4 {
                $index = if (($x -band 1) -eq 0) { ($byte -shr 4) -band 0x0F } else { $byte -band 0x0F }
            }
            8 { $index = $byte }
        }
        $luma[$target + $x] = $paletteLuma[$index]
    }
}

# Floyd-Steinberg: the error of rounding a pixel to paper or ink is carried into the neighbours the
# scan has not reached yet, which is what turns a gradient into a dot pattern.
if ($Dither) {
    for ($y = 0; $y -lt $height; $y++) {
        # Serpentine, so the pattern has no directional streak.
        $leftToRight = ($y % 2) -eq 0
        $target = $y * $width
        for ($step = 0; $step -lt $width; $step++) {
            $x = if ($leftToRight) { $step } else { $width - 1 - $step }
            $old = $luma[$target + $x]
            $new = if ($old -lt $Threshold) { 0.0 } else { 255.0 }
            $carry = $old - $new
            $luma[$target + $x] = $new

            $ahead = if ($leftToRight) { 1 } else { -1 }
            $behind = -$ahead
            if ($x + $ahead -ge 0 -and $x + $ahead -lt $width) {
                $luma[$target + $x + $ahead] += $carry * 7 / 16
            }
            if ($y + 1 -lt $height) {
                $below = $target + $width
                if ($x + $behind -ge 0 -and $x + $behind -lt $width) {
                    $luma[$below + $x + $behind] += $carry * 3 / 16
                }
                $luma[$below + $x] += $carry * 5 / 16
                if ($x + $ahead -ge 0 -and $x + $ahead -lt $width) {
                    $luma[$below + $x + $ahead] += $carry * 1 / 16
                }
            }
        }
    }
}

# Runs: palette index 1 is ink, 0 is paper. One byte holds a four-bit length and a four-bit index;
# anything longer is the two-byte form -- index, then length -- and never crosses a row.
$runs = New-Object System.Collections.Generic.List[byte]

for ($y = 0; $y -lt $height; $y++) {
    $target = $y * $width
    $x = 0
    while ($x -lt $width) {
        $isInk = if ($Invert) { $luma[$target + $x] -ge $Threshold } else { $luma[$target + $x] -lt $Threshold }
        $length = 1
        while ($x + $length -lt $width -and $length -lt 255) {
            $nextInk = if ($Invert) {
                $luma[$target + $x + $length] -ge $Threshold
            } else {
                $luma[$target + $x + $length] -lt $Threshold
            }
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
$mode = if ($Invert) { 'light is ink' } else { 'dark is ink' }
if ($Dither) { $mode += ', dithered' }
Write-Host ("{0}: {1}x{2}, {3} bits per pixel, {4}, threshold {5}" -f
    (Split-Path -Leaf $Source), $width, $height, $bitsPerPixel, $mode, $Threshold)
Write-Host ("  {0} ink pixels, {1} bytes of runs, {2} bytes in the file (1-bit BMP would be {3})" -f
    $inkPixels, $runs.Count, $asset.Count, $packed)
Write-Host ("  wrote {0}" -f $Out)
