#requires -Version 5.1
<#
.SYNOPSIS
    Wrapper around arduino-cli for the Eggplan Vacuum firmware.

.DESCRIPTION
    -Task build    compile the sketch (uses the project local libraries folder)
    -Task upload   compile and upload, auto-detecting the serial port
    -Task monitor  open the serial monitor at 115200 baud
    -Task libs     install the third party libraries the firmware needs
    -Task clean    remove the build output

.EXAMPLE
    powershell -File scripts/arduino.ps1 -Task upload -Port COM5
#>
[CmdletBinding()]
param(
    [ValidateSet('build', 'upload', 'monitor', 'libs', 'clean')]
    [string]$Task = 'build',

    [string]$Port = '',
    # CDCOnBoot=cdc maps Serial to the native USB CDC, which is what puts the interface on
    # the USB-C socket. Without it Serial is UART0 on GPIO20/21 and the interface is
    # invisible to a USB cable.
    [string]$Fqbn = 'esp32:esp32:esp32c3:CDCOnBoot=cdc',
    [switch]$VerboseOutput
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sketch = Join-Path $root 'firmware\eggplan_vacuum'
$projectLibs = Join-Path $root 'libraries'

$thirdPartyLibs = @(
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

function Assert-LayerRule {
    $check = Join-Path $root 'scripts\check-layers.ps1'
    if (-not (Test-Path $check)) { return }
    & powershell -NoProfile -ExecutionPolicy Bypass -File $check
    if ($LASTEXITCODE -ne 0) { throw 'layer rule violated, refusing to build' }
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

    'clean' {
        Remove-Item -Recurse -Force (Join-Path $sketch 'build') -ErrorAction SilentlyContinue
        Write-Host 'build output removed' -ForegroundColor Green
    }

    'build' {
        Assert-LayerRule
        Invoke-ArduinoCli @('compile', '--fqbn', $Fqbn, '--libraries', $projectLibs, $sketch)
    }

    'upload' {
        Assert-LayerRule
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
