#requires -Version 5.1
<#
.SYNOPSIS
    Wrapper around arduino-cli for the Gardener firmware.

.DESCRIPTION
    -Task build    compile the sketch (uses the project local libraries folder)
    -Task upload   compile and upload, auto-detecting the serial port
    -Task data     pack firmware/gardener/data and write it to the LittleFS partition
    -Task monitor  open the serial monitor at 115200 baud
    -Task libs     install the third party libraries the firmware needs
    -Task clean    remove the build output

.EXAMPLE
    powershell -File scripts/arduino.ps1 -Task upload -Port COM5

.EXAMPLE
    powershell -File scripts/arduino.ps1 -Task data -Port COM5
#>
[CmdletBinding()]
param(
    [ValidateSet('build', 'upload', 'data', 'monitor', 'libs', 'clean')]
    [string]$Task = 'build',

    [string]$Port = '',
    # USBMode=hwcdc puts USB on the chip's USB-Serial-JTAG CDC and CDCOnBoot=cdc maps Serial
    # to it, which is what puts the interface on the USB-C socket while leaving the ROM
    # downloader on the same port. Without CDCOnBoot Serial is UART0 on GPIO43/44 and the
    # interface is invisible to a USB cable.
    #
    # FlashSize and PartitionScheme are pinned because -Task data writes the LittleFS partition
    # at the offset the table names: a different scheme would put the image somewhere else.
    [string]$Fqbn = 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PartitionScheme=default',
    [switch]$VerboseOutput
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sketch = Join-Path $root 'firmware\gardener'
$projectLibs = Join-Path $root 'libraries'

$thirdPartyLibs = @(
    'Adafruit SH110X',
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

    'data' {
        $device = Get-DevicePort
        if (-not $device) { throw 'no serial port found, pass -Port COMx' }

        $dataDir = Join-Path $sketch 'data'
        if (-not (Test-Path $dataDir)) {
            New-Item -ItemType Directory -Force $dataDir | Out-Null
            Write-Host "created $dataDir; put the files to upload there" -ForegroundColor Yellow
        }

        # The ESP32 core ships both tools. The newest copy is the one arduino-cli builds with.
        $arduinoData = if ($env:ARDUINO_DIRECTORIES_DATA) {
            $env:ARDUINO_DIRECTORIES_DATA
        } else {
            Join-Path $env:LOCALAPPDATA 'Arduino15'
        }
        $esp32Tools = Join-Path $arduinoData 'packages\esp32\tools'
        $mklittlefs = Get-ChildItem (Join-Path $esp32Tools 'mklittlefs') -Recurse -Filter 'mklittlefs.exe' `
            -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        $esptool = Get-ChildItem (Join-Path $esp32Tools 'esptool_py') -Recurse -Filter 'esptool.exe' `
            -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if (-not $mklittlefs) { throw "mklittlefs.exe not found under $esp32Tools" }
        if (-not $esptool) { throw "esptool.exe not found under $esp32Tools" }

        # The offset and size come from the table the build pins, so the image lands on the
        # partition the firmware mounts rather than on a number copied into this script.
        $coreRoot = Join-Path $arduinoData 'packages\esp32\hardware\esp32'
        $core = Get-ChildItem $coreRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
        $table = Join-Path $core.FullName 'tools\partitions\default.csv'
        $row = Get-Content $table | Where-Object { $_ -match '^\s*spiffs\s*,' } | Select-Object -First 1
        if (-not $row) { throw "no spiffs partition in $table" }
        $cells = @($row -split ',' | ForEach-Object { $_.Trim() })
        $size = [Convert]::ToInt64(($cells[4] -replace '^0x', ''), 16)
        $offset = '0x{0:x}' -f [Convert]::ToInt64(($cells[3] -replace '^0x', ''), 16)

        $image = Join-Path $sketch 'build\littlefs.bin'
        New-Item -ItemType Directory -Force (Split-Path -Parent $image) | Out-Null

        Write-Host "packing $dataDir" -ForegroundColor Cyan
        & $mklittlefs.FullName -c $dataDir -b 4096 -p 256 -s $size $image
        if ($LASTEXITCODE -ne 0) { throw "mklittlefs exited with $LASTEXITCODE" }

        Write-Host "writing $image to $device at $offset" -ForegroundColor Cyan
        & $esptool.FullName --chip esp32s3 --port $device --baud 921600 --before default_reset `
            --after hard_reset write_flash $offset $image
        if ($LASTEXITCODE -ne 0) { throw "esptool exited with $LASTEXITCODE" }
    }

    'monitor' {
        $device = Get-DevicePort
        if (-not $device) { throw 'no serial port found, pass -Port COMx' }
        Write-Host "monitoring $device at 115200 baud, Ctrl+C to stop" -ForegroundColor Cyan
        Invoke-ArduinoCli @('monitor', '-p', $device, '--config', 'baudrate=115200')
    }
}
