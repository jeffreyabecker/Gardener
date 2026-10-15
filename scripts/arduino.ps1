#requires -Version 5.1
<#
.SYNOPSIS
    Wrapper around arduino-cli for the Eggplan Vacuum firmware.

.DESCRIPTION
    -Task build    compile the sketch (uses the project local libraries folder)
    -Task upload   compile and upload, auto-detecting the serial port
    -Task monitor  open the serial monitor at 115200 baud
    -Task libs     install the third party libraries the firmware needs
    -Task link     junction the project library into the arduino sketchbook
    -Task clean    remove the build output

.EXAMPLE
    powershell -File scripts/arduino.ps1 -Task upload -Port COM5
#>
[CmdletBinding()]
param(
    [ValidateSet('build', 'upload', 'monitor', 'libs', 'link', 'clean')]
    [string]$Task = 'build',

    [string]$Port = '',
    [string]$Fqbn = 'esp32:esp32:esp32c3',
    [switch]$VerboseOutput
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sketch = Join-Path $root 'firmware\eggplan_vacuum'
$projectLibs = Join-Path $root 'libraries'

$thirdPartyLibs = @(
    'NimBLE-Arduino',
    'Adafruit SH110X',
    'Adafruit MPRLS Library',
    'Adafruit BusIO',
    'Adafruit GFX Library'
)

function Invoke-ArduinoCli {
    param([string[]]$Arguments)
    if ($VerboseOutput) { Write-Host "arduino-cli $($Arguments -join ' ')" -ForegroundColor DarkGray }
    & arduino-cli @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "arduino-cli exited with $LASTEXITCODE"
    }
}

function Get-DevicePort {
    if ($Port) { return $Port }
    $json = & arduino-cli board list --format json 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $json) { return '' }
    $parsed = $json | ConvertFrom-Json
    $ports = @($parsed.detected_ports)
    $esp = $ports | Where-Object {
        $_.matching_boards -and ($_.matching_boards | Where-Object { $_.fqbn -like 'esp32*' })
    }
    if ($esp) { return $esp[0].port.address }
    if ($ports) { return $ports[0].port.address }
    return ''
}

switch ($Task) {
    'libs' {
        foreach ($lib in $thirdPartyLibs) {
            Write-Host "installing $lib" -ForegroundColor Cyan
            Invoke-ArduinoCli @('lib', 'install', $lib)
        }
    }

    'link' {
        $sketchbook = (& arduino-cli config get directories.user).Trim().Trim('"')
        if (-not (Test-Path $sketchbook)) { New-Item -ItemType Directory -Force -Path $sketchbook | Out-Null }
        $target = Join-Path $sketchbook 'libraries\VacuumCore'
        $source = Join-Path $projectLibs 'VacuumCore'
        if (Test-Path $target) {
            Write-Host "already linked: $target" -ForegroundColor Yellow
        }
        else {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            New-Item -ItemType Junction -Path $target -Target $source | Out-Null
            Write-Host "linked $target -> $source" -ForegroundColor Green
        }
    }

    'clean' {
        Remove-Item -Recurse -Force (Join-Path $sketch 'build') -ErrorAction SilentlyContinue
        Write-Host 'build output removed' -ForegroundColor Green
    }

    'build' {
        Invoke-ArduinoCli @('compile', '--fqbn', $Fqbn, '--libraries', $projectLibs, $sketch)
    }

    'upload' {
        $device = Get-DevicePort
        if (-not $device) { throw 'no serial port found, pass -Port COMx' }
        Write-Host "uploading to $device" -ForegroundColor Cyan
        Invoke-ArduinoCli @('compile', '--fqbn', $Fqbn, '--libraries', $projectLibs,
            '--upload', '-p', $device, $sketch)
    }

    'monitor' {
        $device = Get-DevicePort
        if (-not $device) { throw 'no serial port found, pass -Port COMx' }
        Write-Host "monitoring $device at 115200 baud, Ctrl+C to stop" -ForegroundColor Cyan
        Invoke-ArduinoCli @('monitor', '-p', $device, '--config', 'baudrate=115200')
    }
}
