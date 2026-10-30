param([string]$Usb = "", [string]$CoreDir = "", [switch]$NoFlash, [switch]$Monitor)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$Pio=Join-Path $PSScriptRoot '.venv\Scripts\pio.exe'
if (-not (Test-Path $Pio)) { throw 'Ejecuta primero setup.bat.' }
if (-not $CoreDir -and (Test-Path '.pio-core')) { $CoreDir=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '.pio-core')).Trim() }
if ($CoreDir) { $env:PLATFORMIO_CORE_DIR=$CoreDir }
if (-not $NoFlash) {
    $UploadOptions=@(); if ($Usb) { $UploadOptions=@('--upload-port',$Usb) }
    & $Pio run -t upload @UploadOptions
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
if ($Monitor) {
    $MonitorOptions=@(); if ($Usb) { $MonitorOptions=@('--port',$Usb) }
    & $Pio device monitor @MonitorOptions
}
