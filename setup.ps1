param([string]$Python = "", [string]$CoreDir = "")
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
if (-not (Test-Path '.venv\Scripts\python.exe')) {
    if ($Python) { & $Python -m venv .venv }
    elseif (Get-Command py -ErrorAction SilentlyContinue) { py -3 -m venv .venv }
    elseif (Get-Command python -ErrorAction SilentlyContinue) { python -m venv .venv }
    else { throw "Instala Python 3.10 o posterior desde python.org y vuelve a ejecutar setup.bat." }
    if ($LASTEXITCODE -or -not (Test-Path '.venv\Scripts\python.exe')) { throw "No se pudo crear .venv. Instala Python o usa -Python RUTA_EXE." }
}
& .venv\Scripts\python.exe -m pip install -r requirements.txt
if ($LASTEXITCODE) { throw "No se pudieron instalar las dependencias." }
# A short path avoids CreateProcess/path length errors in the ESP32 compiler on Windows.
if (-not $CoreDir) { $CoreDir = Join-Path $env:USERPROFILE '.pokemon-pio' }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot '.pio-core'),$CoreDir)
$env:PLATFORMIO_CORE_DIR=$CoreDir
& .venv\Scripts\pio.exe run
if ($LASTEXITCODE) { throw "La compilacion ha fallado." }
Write-Host 'Listo. Conecta el robot y ejecuta run.bat -Usb COM3 (cambia COM3 si es necesario).'
