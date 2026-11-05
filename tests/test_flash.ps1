# Tests intercept every python invocation: no serial port or device is accessed.
$ErrorActionPreference = 'Stop'
$tempPath = Join-Path ([System.IO.Path]::GetTempPath()) ('h2-flash-test-' + [guid]::NewGuid())
$global:H2FlashTestCalls = @()
$global:H2TestOtaSelected = $false
$global:H2TestWrongTable = $false
function python {
    $global:H2FlashTestCalls += ,@($args)
    $global:LASTEXITCODE = 0
    if ($args -contains 'read_flash') {
        if ($args -contains '0xc000') {
            $value = if ($global:H2TestWrongTable) { 'wrong table' } else { 'test table' }
            [System.IO.File]::WriteAllText($args[-1], $value)
        } else {
            [byte[]]$bytes = @(255) * 8192
            if ($global:H2TestOtaSelected) { $bytes[0] = 1 }
            [System.IO.File]::WriteAllBytes($args[-1], $bytes)
        }
    }
}
function Assert-True($Condition, $Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected($ScriptBlock, $Pattern) {
    $caught = $false
    try { & $ScriptBlock } catch {
        $caught = $true
        Assert-True ($_.Exception.Message -match $Pattern) "Unexpected failure: $_"
    }
    Assert-True $caught "Expected rejection: $Pattern"
}
try {
    New-Item -ItemType Directory -Path $tempPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\scripts\flash.ps1') -Destination $tempPath
    Set-Content -LiteralPath (Join-Path $tempPath 'factory.bin') -Value 'temporary test fixture'
    Set-Content -LiteralPath (Join-Path $tempPath 'esp_matter_base.bin') -Value 'temporary test app'
    $files = @{}
    foreach ($name in @('factory.bin', 'esp_matter_base.bin')) {
        $file = Join-Path $tempPath $name
        $files[$name] = @{ bytes = (Get-Item -LiteralPath $file).Length; sha256 = (Get-FileHash -LiteralPath $file).Hash }
    }
    $manifest = @{ chip = 'esp32h2'; flash_bytes = 4194304; factory_offset = '0x0'; app_offset = '0x20000';
        flash_settings = @{ flash_mode = 'dio'; flash_freq = '48m' }; files = $files }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $tempPath 'manifest.json')
    $flash = Join-Path $tempPath 'flash.ps1'
    & $flash -Port COM99 -WarningAction SilentlyContinue | Out-Null
    Assert-True ($global:H2FlashTestCalls.Count -eq 2) 'Expected version check and one flash call'
    Assert-True ($global:H2FlashTestCalls[1] -contains '0x0') 'Factory image must start at zero'
    Assert-True ($global:H2FlashTestCalls[1] -contains (Join-Path $tempPath 'factory.bin')) 'Wrong factory file'

    $global:H2FlashTestCalls = @()
    & $flash -Port COM99 -ApplicationOnly | Out-Null
    Assert-True ($global:H2FlashTestCalls[1] -contains '0x20000') 'App must use the manifest offset'
    Assert-True ($global:H2FlashTestCalls[1] -contains (Join-Path $tempPath 'esp_matter_base.bin')) 'Wrong application file'

    $tableFile = Join-Path $tempPath 'partition-table.bin'
    [System.IO.File]::WriteAllText($tableFile, 'test table')
    $manifest.ota_supported = $true
    $manifest.partition_table_offset = '0xc000'
    $manifest.ota_data_offset = '0x1d000'
    $manifest.ota_data_bytes = 8192
    $manifest.files['partition-table.bin'] = @{ bytes = (Get-Item $tableFile).Length; sha256 = (Get-FileHash $tableFile).Hash }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $tempPath 'manifest.json')
    $global:H2FlashTestCalls = @()
    & $flash -Port COM99 -ApplicationOnly | Out-Null
    Assert-True ($global:H2FlashTestCalls.Count -eq 4) 'Initial OTA enable must read table and OTA selection before writing'
    Assert-True ($global:H2FlashTestCalls[3] -contains 'write_flash') 'Validated initial upgrade must flash'

    $global:H2TestOtaSelected = $true
    $global:H2FlashTestCalls = @()
    Assert-Rejected { & $flash -Port COM99 -ApplicationOnly } 'Bootauswahl ist bereits gesetzt'
    Assert-True (@($global:H2FlashTestCalls | Where-Object { $_ -contains 'write_flash' }).Count -eq 0) 'Selected OTA slot must never be overwritten'
    $global:H2TestOtaSelected = $false
    $global:H2TestWrongTable = $true
    $global:H2FlashTestCalls = @()
    Assert-Rejected { & $flash -Port COM99 -ApplicationOnly } 'Andere Partitionstabelle'
    Assert-True (@($global:H2FlashTestCalls | Where-Object { $_ -contains 'write_flash' }).Count -eq 0) 'Wrong layout must never be flashed'
    $global:H2TestWrongTable = $false

    $global:H2FlashTestCalls = @()
    Assert-Rejected { & $flash -Port COM99 -EraseAll -ApplicationOnly } 'kombiniert'
    Assert-True ($global:H2FlashTestCalls.Count -eq 0) 'Invalid flags must not access a device'
    Set-Content -LiteralPath (Join-Path $tempPath 'factory.bin') -Value 'corrupted fixture'
    Assert-Rejected { & $flash -Port COM99 } 'Pruefsumme'
    Assert-True ($global:H2FlashTestCalls.Count -eq 0) 'Invalid checksum must not access a device'
    Write-Output 'Flash script tests OK: offsets, images, flags, checksums, OTA boot selection and partition guards'
} finally {
    # Only the explicitly created fixture files are removed; no recursive delete.
    foreach ($name in @('flash.ps1', 'factory.bin', 'esp_matter_base.bin', 'manifest.json', 'partition-table.bin')) {
        $file = Join-Path $tempPath $name
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    if (Test-Path -LiteralPath $tempPath) { Remove-Item -LiteralPath $tempPath }
    Remove-Item Function:\python
    Remove-Variable H2FlashTestCalls -Scope Global
    Remove-Variable H2TestOtaSelected, H2TestWrongTable -Scope Global
}
