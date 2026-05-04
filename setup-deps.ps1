$ErrorActionPreference = 'Stop'

$Organization = "moonlight-stream"
$PrebuiltRepo = "moonlight-qt-deps"
$TargetDir = Join-Path $PSScriptRoot "libs\windows"
$Assets = @("windows-x64.zip", "windows-ARM64.zip")
$Tag = "v1.0.1"

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

Write-Host "Dependencies successfully deployed" -ForegroundColor Green