#requires -Version 5.1
<#
.SYNOPSIS
    Enforces the one-way layer rule for the VacuumCore library.

.DESCRIPTION
    Every layer may only include from itself and the layers below it:

        app  ->  ui, hosts, services, hal, core
        ui   ->  hal, core
        hosts->  services, hal, core   (and inside hosts: ble -> commands -> script)
        services -> hal, core
        hal  ->  core
        core ->  core

    Exits 1 and lists offenders when a lower layer reaches upwards.

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
    'core'     = @('core')
    'hal'      = @('hal', 'core')
    'services' = @('services', 'hal', 'core')
    'hosts'    = @('hosts', 'services', 'hal', 'core')
    'ui'       = @('ui', 'hal', 'core')
    'app'      = @('app', 'ui', 'hosts', 'services', 'hal', 'core')
    ''         = @('app', 'ui', 'hosts', 'services', 'hal', 'core')   # umbrella headers
}

# Within hosts/ the subfolders are one-way too.
$hostsOrder = @{ 'ble' = @('ble', 'commands'); 'commands' = @('commands') }

if (-not (Test-Path $Src)) { throw "source folder not found: $Src" }

$violations = @()
$files = Get-ChildItem -Path $Src -Recurse -File -Include *.h, *.cpp, *.ino

foreach ($file in $files) {
    $relative = $file.FullName.Substring($Src.Length + 1) -replace '\\', '/'
    $segments = $relative -split '/'
    $layer = if ($segments.Count -gt 1) { $segments[0] } else { '' }
    $sub = @($segments | Select-Object -Skip 1) -join '/'
    $subFolder = if ($sub -match '/') { ($sub -split '/')[0] } else { '' }

    $permitted = $allowed[$layer]
    if ($null -eq $permitted) { continue }

    Select-String -Path $file.FullName -Pattern '^\s*#include\s+"([^"]+)"' | ForEach-Object {
        $include = $_.Matches[0].Groups[1].Value
        if ($include -notmatch '/') { return }   # third party or same-folder include
        $includeLayer = ($include -split '/')[0]
        $line = $_.Line.Trim()

        if ($permitted -notcontains $includeLayer) {
            $violations += "$relative`:$($_.LineNumber)  includes  $include  [$layer -> $includeLayer]"
            return
        }
        if ($layer -eq 'hosts' -and $includeLayer -eq 'hosts' -and $subFolder) {
            $includeSub = ($include -split '/')[1]
            if ($hostsOrder.ContainsKey($subFolder) -and ($hostsOrder[$subFolder] -notcontains $includeSub)) {
                $violations += "$relative`:$($_.LineNumber)  includes  $include  [hosts/$subFolder -> hosts/$includeSub]"
            }
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
