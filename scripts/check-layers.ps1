#requires -Version 5.1
<#
.SYNOPSIS
    Enforces the one-way layer rule for the VacuumCore library.

.DESCRIPTION
    Three layers, and dependencies only ever point downwards:

        app/   how the device is driven: the serial interface, the composition root
        hal/   the device: the panel, the battery, the pin map, the plant
        core/  shared foundations: config, math, the machine view

    hal/ never sees app/, which is the rule that keeps the device independent of the
    interface in front of it. Exits 1 and lists offenders when one points up.

.EXAMPLE
    powershell -File scripts/check-layers.ps1
#>
[CmdletBinding()]
param(
    [string]$Src = ''
)

$ErrorActionPreference = 'Stop'

if (-not $Src) {
    $root = if ($PSScriptRoot) { Split-Path -Parent $PSScriptRoot } else { (Get-Location).Path }
    $Src = Join-Path $root 'libraries\VacuumCore\src'
}

$allowed = @{
    'core' = @('core')
    'hal'  = @('hal', 'core')
    'app'  = @('app', 'hal', 'core')
    ''     = @('app', 'hal', 'core')   # umbrella headers
}

if (-not (Test-Path $Src)) { throw "source folder not found: $Src" }

$violations = @()
$files = Get-ChildItem -Path $Src -Recurse -File -Include *.h, *.cpp, *.ino

foreach ($file in $files) {
    $relative = $file.FullName.Substring($Src.Length + 1) -replace '\\', '/'
    $layer = ($relative -split '/')[0] -replace '\..*$', ''
    $permitted = $allowed[$layer]
    if ($null -eq $permitted) { continue }   # a file at the src root

    Select-String -Path $file.FullName -Pattern '^\s*#include\s+"([^"]+)"' | ForEach-Object {
        $include = $_.Matches[0].Groups[1].Value
        if ($include -notmatch '/') { return }   # third party or same-folder include
        $includeLayer = ($include -split '/')[0]
        if ($permitted -notcontains $includeLayer) {
            $violations += "$relative`:$($_.LineNumber)  includes  $include  [$layer -> $includeLayer]"
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "layer rule violations ($($violations.Count)):" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

Write-Host "layer rule ok ($($files.Count) files)"
exit 0

