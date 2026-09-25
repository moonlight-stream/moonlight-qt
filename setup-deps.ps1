$ErrorActionPreference = 'Stop'

$Organization = "moonlight-stream"
$PrebuiltRepo = "moonlight-qt-deps"
$TargetDir = Join-Path $PSScriptRoot "libs\windows"
$Assets = @("windows-x64.zip", "windows-ARM64.zip")
$Tag = "v17"

if (Test-Path $TargetDir) {
    Write-Host "Cleaning target directory..." -ForegroundColor Cyan
    Remove-Item -Path "$TargetDir\*" -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $TargetDir | Out-Null
}

foreach ($AssetName in $Assets) {
    $Url = "https://github.com/$Organization/$PrebuiltRepo/releases/download/$Tag/$AssetName"
    $ArchivePath = Join-Path $env:TEMP $AssetName

    Write-Host "Downloading $AssetName..." -ForegroundColor Cyan
    curl.exe -s -L -f -o "$ArchivePath" "$Url"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Write-Host "Extracting $AssetName..." -ForegroundColor Cyan
    Expand-Archive -Path $ArchivePath -DestinationPath $TargetDir -Force
    Remove-Item $ArchivePath
}

# OpenSSL MASM assembly routines lack CFG metadata, causing 0xc0000409 crashes on Windows 11 24H2.
# Strip IMAGE_DLLCHARACTERISTICS_GUARD_CF (0x4000) from libcrypto DLLs to fix CFG violation on AES_encrypt.
Get-ChildItem -Path "$TargetDir\lib" -Filter "libcrypto*.dll" -Recurse | ForEach-Object {
    $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    $dllCharOffset = $peOffset + 4 + 20 + 70 # PE32+ OptionalHeader DllCharacteristics
    $val = [BitConverter]::ToUInt16($bytes, $dllCharOffset)
    if ($val -band 0x4000) {
        $newVal = $val -band (-bnot 0x4000)
        [BitConverter]::GetBytes([uint16]$newVal).CopyTo($bytes, $dllCharOffset)
        [System.IO.File]::WriteAllBytes($_.FullName, $bytes)
        Write-Host "Patched CFG flag on $($_.Name) (0x$("{0:X4}" -f $val) -> 0x$("{0:X4}" -f $newVal))" -ForegroundColor Yellow
    }
}

Write-Host "Dependencies successfully deployed and patched" -ForegroundColor Green