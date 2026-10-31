[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port,
    [ValidateRange(115200, 921600)]
    [int]$Baud = 460800,
    [switch]$EraseAll,
    [switch]$ApplicationOnly
)
$ErrorActionPreference = 'Stop'
if ($EraseAll -and $ApplicationOnly) {
    throw 'EraseAll und ApplicationOnly duerfen nicht kombiniert werden.'
}
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.chip -ne 'esp32h2' -or $manifest.flash_bytes -ne 4194304) {
    throw 'Dieses Paket ist nicht fuer den ESP32-H2 mit 4 MB Flash vorgesehen.'
}
$fileName = if ($ApplicationOnly) { 'esp_matter_base.bin' } else { 'factory.bin' }
$offset = if ($ApplicationOnly) { $manifest.app_offset } else { $manifest.factory_offset }
$image = Join-Path $PSScriptRoot $fileName
$expected = $manifest.files.PSObject.Properties[$fileName].Value
if (-not $expected -or -not (Test-Path -LiteralPath $image)) { throw 'Firmware fehlt.' }
if ((Get-Item -LiteralPath $image).Length -ne $expected.bytes -or
    (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash -ne $expected.sha256) {
    throw 'Pruefsumme oder Dateigroesse stimmt nicht. Paket erneut herunterladen.'
}
& python -m esptool version
if ($LASTEXITCODE -ne 0) {
    throw 'esptool fehlt. Installieren: python -m pip install "esptool>=4.8,<5"'
}
if (-not $ApplicationOnly) {
    Write-Warning 'Erstinstallation: das Factory-Image ueberschreibt auch bisherige Kopplungsdaten.'
}
if ($EraseAll) {
    & python -m esptool --chip esp32h2 --port $Port erase_flash
    if ($LASTEXITCODE -ne 0) { throw 'Flash loeschen fehlgeschlagen.' }
}
$settings = $manifest.flash_settings
& python -m esptool --chip esp32h2 --port $Port --baud $Baud write_flash --flash_mode $settings.flash_mode --flash_freq $settings.flash_freq --flash_size 4MB $offset $image
if ($LASTEXITCODE -ne 0) { throw 'Flashen fehlgeschlagen. Gegebenenfalls mit -Baud 115200 wiederholen.' }
Write-Host 'Firmware geschrieben und von esptool verifiziert. Falls noetig RST kurz druecken.'
