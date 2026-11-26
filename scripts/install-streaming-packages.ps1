param(
    [Parameter(Mandatory)][string]$Stage,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$SunshineSha256,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$MoonlightSha256,
    [string]$SunshineTarget = 'D:\Soft\Sunshine',
    [string]$MoonlightTarget = 'D:\Soft\moonlight',
    [switch]$StageOnly,
    [string]$ExpectedComputerName,
    [ValidateRange(0, 2147483647)][int]$KeepBackups
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
if ($ExpectedComputerName -and $env:COMPUTERNAME -ine $ExpectedComputerName) { throw "Expected host $ExpectedComputerName, reached $env:COMPUTERNAME." }
$Stage = [IO.Path]::GetFullPath($Stage).TrimEnd('\')
$SunshineTarget = [IO.Path]::GetFullPath($SunshineTarget).TrimEnd('\')
$MoonlightTarget = [IO.Path]::GetFullPath($MoonlightTarget).TrimEnd('\')
$moonExe = Join-Path $MoonlightTarget 'Moonlight.exe'
$sunExe = Join-Path $SunshineTarget 'sunshine.exe'
$sunConf = Join-Path $SunshineTarget 'config\sunshine.conf'
function RequireUnder([string]$path, [string]$base) {
    if (![IO.Path]::GetFullPath($path).StartsWith($base + '\',[StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected path outside $base"
    }
}
function NoReparse([string]$path) {
    while ($path) {
        if ((Test-Path -LiteralPath $path) -and
            ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to traverse reparse point $path"
        }
        $path = [IO.Path]::GetDirectoryName($path)
    }
}
function CheckBackupTree([string]$path) {
    foreach ($entry in Get-ChildItem -LiteralPath $path -Force) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to traverse reparse point $($entry.FullName)"
        }
        if ($entry.PSIsContainer) { CheckBackupTree $entry.FullName }
    }
}
function ClearDeploymentBackups([string]$stagePath, [int]$retain, [bool]$preview) {
    if ([IO.Path]::GetFileName($stagePath) -notmatch '^deploy-streaming-[0-9a-f]{32}$') {
        throw 'Unexpected deployment staging directory.'
    }
    $root = [IO.Path]::GetDirectoryName($stagePath)
    $backups = @(
        foreach ($dir in Get-ChildItem -LiteralPath $root -Directory -Force) {
            if ($dir.Name -notmatch '^deploy-streaming-[0-9a-f]{32}$' -or
                ($dir.Attributes -band [IO.FileAttributes]::ReparsePoint)) { continue }
            foreach ($backupDir in Get-ChildItem -LiteralPath $dir.FullName -Directory -Force) {
                if ($backupDir.Name -notmatch '^backup-[0-9a-f]{32}$' -or
                    ($backupDir.Attributes -band [IO.FileAttributes]::ReparsePoint)) { continue }
                $manifest = Get-Item -LiteralPath (Join-Path $backupDir.FullName 'changes.json') -Force -ErrorAction SilentlyContinue
                if ($manifest -and !$manifest.PSIsContainer -and
                    !($manifest.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                    [pscustomobject]@{ Path = $backupDir.FullName; Timestamp = $manifest.LastWriteTimeUtc }
                }
            }
        }
    )
    $older = @($backups | Sort-Object Timestamp, Path -Descending | Select-Object -Skip $retain)
    foreach ($backupEntry in $older) {
        $actual = (Resolve-Path -LiteralPath $backupEntry.Path).ProviderPath
        RequireUnder $actual $root
        NoReparse $actual
        CheckBackupTree $actual
        $backupEntry.Path = $actual
    }
    if (!$preview) {
        foreach ($backupEntry in $older) {
            Remove-Item -LiteralPath $backupEntry.Path -Recurse -Force
            $parent = [IO.Path]::GetDirectoryName($backupEntry.Path)
            try {
                if (![IO.Directory]::GetFileSystemEntries($parent).Count) {
                    [IO.Directory]::Delete($parent)
                }
            } catch [IO.DirectoryNotFoundException] {}
        }
    }
    [pscustomobject]@{
        Kept = $backups.Count - $older.Count
        Backups = @($older | ForEach-Object { $_.Path })
    }
}
function WriteDeploymentResult([hashtable]$result) {
    if ($retainBackups) {
        try {
            $cleanup = ClearDeploymentBackups $Stage $KeepBackups $StageOnly.IsPresent
            $result.BackupCleanup = $cleanup
            if ($result.Backup -in $cleanup.Backups) { $result.Backup = $null }
        } catch {
            $result.BackupCleanupError = $_.Exception.Message
        }
    }
    $result | ConvertTo-Json -Depth 3
}
NoReparse $Stage
$lock = [Threading.Mutex]::new($false, 'Global\MoonlightSunshineDeployment')
try { $owned = $lock.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned = $true }
if (!$owned) { throw 'Another deployment is running on this host.' }
try {
$retainBackups = $PSBoundParameters.ContainsKey('KeepBackups')
$products = @()
if ($SunshineSha256) { $products += 'Sunshine' }
if ($MoonlightSha256) { $products += 'Moonlight' }
if (!$products.Count) {
    if (!$retainBackups) { throw 'At least one deployment package is required.' }
    ClearDeploymentBackups $Stage $KeepBackups $StageOnly.IsPresent | ConvertTo-Json
    return
}
$required = @()
if ($SunshineSha256) { $required += @($sunExe, $sunConf) }
if ($MoonlightSha256) { $required += $moonExe }
foreach ($path in $required) { NoReparse $path }
if ($Stage -eq $SunshineTarget -or $Stage -eq $MoonlightTarget -or $MoonlightTarget -eq $SunshineTarget) { throw 'Stage and targets must be separate directories.' }
foreach ($folder in @($SunshineTarget,$MoonlightTarget)) {
    if ($Stage.StartsWith($folder+'\',[StringComparison]::OrdinalIgnoreCase) -or $folder.StartsWith($Stage+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Stage must be separate from the installations.' }
}
foreach ($path in $required) {
    if (![IO.File]::Exists($path)) { throw "Installation not found: $path" }
}
if ($SunshineSha256) {
    $svc = Get-CimInstance Win32_Service -Filter "Name='SunshineService'"
    if (!$svc -or $svc.PathName.IndexOf((Join-Path $SunshineTarget 'tools\sunshinesvc.exe'),[StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw 'Sunshine service does not point to requested installation.'
    }
}
$stamp = [Guid]::NewGuid().ToString('N')
$work = Join-Path $Stage "extracted-$stamp"
$backup = Join-Path $Stage "backup-$stamp"
$changes = [Collections.Generic.List[object]]::new()
$installedHashes = @{}
foreach ($product in $products) {
    $archive = Join-Path $Stage "$product.zip"
    $expected = if ($product -eq 'Sunshine') { $SunshineSha256 } else { $MoonlightSha256 }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash -ne $expected) { throw "Invalid $product package hash." }
    $unpack = Join-Path $work $product
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($archive,$unpack)
    $binaries = @(Get-ChildItem -LiteralPath $unpack -File -Recurse -Filter "$product.exe")
    if ($binaries.Count -ne 1) { throw "Expected one $product executable in the package" }
    $source = $binaries[0].Directory.FullName
    $target = if ($product -eq 'Sunshine') { $SunshineTarget } else { $MoonlightTarget }
    if (![IO.File]::Exists((Join-Path $source "$product.exe"))) { throw "Missing $product binary." }
    $installedHashes[$product] = (Get-FileHash -LiteralPath (Join-Path $source "$product.exe")).Hash
    foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse -Force) {
        $rel = $file.FullName.Substring($source.Length+1)
        if ($rel -match '^(?:config\\|Moonlight Game Streaming Project\\|cache\\|boxart\\|logs\\|dumps\\|portable\.dat$)') { continue }
        $dest = Join-Path $target $rel
        RequireUnder $dest $target
        NoReparse $dest
        $hash = (Get-FileHash -LiteralPath $file.FullName).Hash
        if ([IO.File]::Exists($dest) -and (Get-FileHash -LiteralPath $dest).Hash -eq $hash) { continue }
        $changes.Add([pscustomobject]@{Source=$file.FullName; Target=$dest; Backup=(Join-Path $backup "$product\$rel"); Hash=$hash})
    }
}
if ($StageOnly) { WriteDeploymentResult @{Verified=$true; FilesToUpdate=$changes.Count}; return }
$runningClient = @()
if ($MoonlightSha256) { $runningClient = @(Get-Process -Name Moonlight -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq $moonExe }) }
$runningHost = @()
$wasRunning = $false
$basePort=47989
if ($SunshineSha256) {
    $runningHost = @(Get-Process -Name sunshine -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq $sunExe })
    $wasRunning = (Get-Service SunshineService).Status -eq 'Running'
    $portSetting=Select-String -LiteralPath $sunConf -Pattern '^\s*port\s*=\s*(\d+)\s*$' | Select-Object -Last 1
    if ($portSetting) { $basePort=[int]$portSetting.Matches[0].Groups[1].Value }
}
$updated = [Collections.Generic.List[object]]::new()
function SaveOld([string]$dest,[string]$saved) {
    $exists=[IO.File]::Exists($dest)
    if ($exists) {
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($saved))
        [IO.File]::Copy($dest,$saved,$false)
    }
    $updated.Add([pscustomobject]@{Target=$dest; Backup=$saved; Existed=$exists})
}
try {
    $restart = $wasRunning -and @($changes | Where-Object { $_.Target.StartsWith($SunshineTarget + '\',[StringComparison]::OrdinalIgnoreCase) }).Count -gt 0
    if ($restart) {
        Stop-Service SunshineService
        (Get-Service SunshineService).WaitForStatus('Stopped',[TimeSpan]::FromSeconds(30))
    }
    if (!@($changes | Where-Object { $_.Target.StartsWith($MoonlightTarget + '\',[StringComparison]::OrdinalIgnoreCase) }).Count) { $runningClient = @() }
    foreach ($p in $runningClient) {
        $null=$p.CloseMainWindow()
        if (!$p.WaitForExit(6000)) { Stop-Process -Id $p.Id; $p.WaitForExit() }
    }
    foreach ($p in $runningHost) {
        if ($restart -and !$p.HasExited -and !$p.WaitForExit(5000)) { throw 'Sunshine executable remained running after service shutdown.' }
    }
    foreach ($entry in $changes) {
        SaveOld $entry.Target $entry.Backup
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($entry.Target))
        [IO.File]::Copy($entry.Source,$entry.Target,$true)
        if ((Get-FileHash -LiteralPath $entry.Target).Hash -ne $entry.Hash) { throw "Installed file verification failed: $($entry.Target)" }
    }
    if ($updated.Count) { [void][IO.Directory]::CreateDirectory($backup); $updated | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $backup 'changes.json') }
    if ($MoonlightSha256) {
        $out=Join-Path $work 'moonlight-version.txt'
        $test=Start-Process -FilePath $moonExe -ArgumentList '--version' -WindowStyle Hidden -WorkingDirectory $MoonlightTarget -RedirectStandardOutput $out -Wait -PassThru
        if ($test.ExitCode -ne 0 -or (Get-Content -LiteralPath $out -Raw) -notmatch 'Moonlight') {throw 'Moonlight launch verification failed.'}
    }
    if ($wasRunning) {
        if ($restart) { Start-Service SunshineService }
        $ok=$false
        for ($i=0;$i -lt 30 -and !$ok;$i++) {
            $hostIds=@(Get-Process sunshine -ErrorAction SilentlyContinue | Where-Object {$_.Path -ieq $sunExe} | ForEach-Object Id)
            if (@(Get-NetTCPConnection -State Listen -LocalPort ($basePort-5) -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -in $hostIds}).Count) { $ok=$true } else { Start-Sleep -Seconds 1 }
        }
        if (!$ok) {throw 'Sunshine did not open its HTTPS stream listener.'}
    }
    $result = @{Updated=($changes.Count -gt 0); ChangedFiles=$changes.Count; Backup=$(if ($updated.Count) { $backup } else { $null }); SunshineSHA256=$installedHashes.Sunshine; MoonlightSHA256=$installedHashes.Moonlight; Service=$(if ($SunshineSha256) { (Get-Service SunshineService).Status.ToString() }); ClientWasOpen=($runningClient.Count -gt 0)}
} catch {
    $failure=$_
    if ($restart -and (Get-Service SunshineService).Status -ne 'Stopped') { Stop-Service SunshineService; (Get-Service SunshineService).WaitForStatus('Stopped',[TimeSpan]::FromSeconds(30)) }
    for ($i=$updated.Count-1;$i -ge 0;$i--) {
        $item=$updated[$i]
        if ($item.Existed) { [IO.File]::Copy($item.Backup,$item.Target,$true) } elseif ([IO.File]::Exists($item.Target)) { [IO.File]::Delete($item.Target) }
    }
    if ($restart) { Start-Service SunshineService }
    throw $failure
}
WriteDeploymentResult $result

} finally {
    try {
        if ($work -and (Test-Path -LiteralPath $work)) {
            $actual = (Resolve-Path -LiteralPath $work).ProviderPath
            if ([IO.Path]::GetDirectoryName($actual) -ne $Stage -or
                [IO.Path]::GetFileName($actual) -ne "extracted-$stamp") { throw 'Unexpected extraction path' }
            NoReparse $actual
            Remove-Item -LiteralPath $actual -Recurse -Force
        }
    } finally { $lock.ReleaseMutex(); $lock.Dispose() }
}
