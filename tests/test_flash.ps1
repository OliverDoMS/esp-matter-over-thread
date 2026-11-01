# Tests intercept every python invocation: no serial port or device is accessed.
$ErrorActionPreference = 'Stop'
$tempPath = Join-Path ([System.IO.Path]::GetTempPath()) ('h2-flash-test-' + [guid]::NewGuid())
$global:H2FlashTestCalls = @()
function python {
    $global:H2FlashTestCalls += ,@($args)
    $global:LASTEXITCODE = 0
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

    $global:H2FlashTestCalls = @()
    Assert-Rejected { & $flash -Port COM99 -EraseAll -ApplicationOnly } 'kombiniert'
    Assert-True ($global:H2FlashTestCalls.Count -eq 0) 'Invalid flags must not access a device'
    Set-Content -LiteralPath (Join-Path $tempPath 'factory.bin') -Value 'corrupted fixture'
    Assert-Rejected { & $flash -Port COM99 } 'Pruefsumme'
    Assert-True ($global:H2FlashTestCalls.Count -eq 0) 'Invalid checksum must not access a device'
    Write-Output 'Flash script tests OK: offsets, selected image, conflicting flags, corruption rejection'
} finally {
    # Only the explicitly created fixture files are removed; no recursive delete.
    foreach ($name in @('flash.ps1', 'factory.bin', 'esp_matter_base.bin', 'manifest.json')) {
        $file = Join-Path $tempPath $name
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    if (Test-Path -LiteralPath $tempPath) { Remove-Item -LiteralPath $tempPath }
    Remove-Item Function:\python
    Remove-Variable H2FlashTestCalls -Scope Global
}
