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
if ($ApplicationOnly -and $manifest.ota_supported) {
    # Initial USB upgrade only: never overwrite a slot selected by a previous OTA.
    $table = Join-Path $PSScriptRoot 'partition-table.bin'
    $tableInfo = $manifest.files.PSObject.Properties['partition-table.bin'].Value
    if (-not $tableInfo -or -not (Test-Path -LiteralPath $table) -or
        (Get-Item -LiteralPath $table).Length -ne $tableInfo.bytes -or
        (Get-FileHash -LiteralPath $table -Algorithm SHA256).Hash -ne $tableInfo.sha256) {
        throw 'Partitionstabelle fehlt oder Pruefsumme stimmt nicht.'
    }
    $tableRead = [System.IO.Path]::GetTempFileName()
    $otaRead = [System.IO.Path]::GetTempFileName()
    try {
        & python -m esptool --chip esp32h2 --port $Port --baud $Baud read_flash $manifest.partition_table_offset $tableInfo.bytes $tableRead
        if ($LASTEXITCODE -ne 0) { throw 'Partitionstabelle konnte nicht gelesen werden.' }
        if ((Get-Item -LiteralPath $tableRead).Length -ne $tableInfo.bytes -or
            (Get-FileHash -LiteralPath $tableRead -Algorithm SHA256).Hash -ne $tableInfo.sha256) {
            throw 'Andere Partitionstabelle auf dem Geraet; Anwendungsupdate abgebrochen.'
        }
        & python -m esptool --chip esp32h2 --port $Port --baud $Baud read_flash $manifest.ota_data_offset $manifest.ota_data_bytes $otaRead
        if ($LASTEXITCODE -ne 0) { throw 'OTA-Bootauswahl konnte nicht gelesen werden.' }
        $otaBytes = [System.IO.File]::ReadAllBytes($otaRead)
        if ($otaBytes.Length -ne $manifest.ota_data_bytes -or
            @($otaBytes | Where-Object { $_ -ne 255 }).Count -ne 0) {
            throw 'OTA-Bootauswahl ist bereits gesetzt. Bitte per Matter OTA aktualisieren; kein festes ota_0-USB-Update.'
        }
    } finally {
        Remove-Item -LiteralPath $tableRead, $otaRead -ErrorAction SilentlyContinue
    }
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
