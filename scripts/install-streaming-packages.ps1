param(
    [Parameter(Mandatory)][string]$Stage,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$SunshineSha256,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$MoonlightSha256,
    [string]$SunshineTarget = 'D:\Soft\Sunshine',
    [string]$MoonlightTarget = 'D:\Soft\moonlight',
    [switch]$StageOnly
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Stage = [IO.Path]::GetFullPath($Stage).TrimEnd('\')
$SunshineTarget = [IO.Path]::GetFullPath($SunshineTarget).TrimEnd('\')
$MoonlightTarget = [IO.Path]::GetFullPath($MoonlightTarget).TrimEnd('\')
$moonExe = Join-Path $MoonlightTarget 'Moonlight.exe'
$sunExe = Join-Path $SunshineTarget 'sunshine.exe'
$sunConf = Join-Path $SunshineTarget 'config\sunshine.conf'
$portable = Test-Path -LiteralPath (Join-Path $MoonlightTarget 'portable.dat')
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
foreach ($path in @($Stage,$sunExe,$moonExe,$sunConf)) { NoReparse $path }
if ($Stage -eq $SunshineTarget -or $Stage -eq $MoonlightTarget -or $MoonlightTarget -eq $SunshineTarget) { throw 'Stage and targets must be separate directories.' }
foreach ($folder in @($SunshineTarget,$MoonlightTarget)) {
    if ($Stage.StartsWith($folder+'\',[StringComparison]::OrdinalIgnoreCase) -or $folder.StartsWith($Stage+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Stage must be separate from the installations.' }
}
foreach ($path in @($sunExe,$moonExe,$sunConf)) {
    if (![IO.File]::Exists($path)) { throw "Installation not found: $path" }
}
$svc = Get-CimInstance Win32_Service -Filter "Name='SunshineService'"
if (!$svc -or $svc.PathName.IndexOf((Join-Path $SunshineTarget 'tools\sunshinesvc.exe'),[StringComparison]::OrdinalIgnoreCase) -lt 0) {
    throw 'Sunshine service does not point to requested installation.'
}
$lock = [Threading.Mutex]::new($false, 'Global\MoonlightSunshineDeployment')
try { $owned = $lock.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned = $true }
if (!$owned) { throw 'Another deployment is running on this host.' }
try {
$stamp = [Guid]::NewGuid().ToString('N')
$work = Join-Path $Stage "extracted-$stamp"
$backup = Join-Path $Stage "backup-$stamp"
$changes = [Collections.Generic.List[object]]::new()
$installedHashes = @{}
foreach ($product in @('Sunshine','Moonlight')) {
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
        if ($product -eq 'Moonlight' -and !$portable -and $rel -eq 'install-host-lock.cmd') { continue }
        $dest = Join-Path $target $rel
        RequireUnder $dest $target
        NoReparse $dest
        $hash = (Get-FileHash -LiteralPath $file.FullName).Hash
        if ([IO.File]::Exists($dest) -and (Get-FileHash -LiteralPath $dest).Hash -eq $hash) { continue }
        $changes.Add([pscustomobject]@{Source=$file.FullName; Target=$dest; Backup=(Join-Path $backup "$product\$rel"); Hash=$hash})
    }
}
if ($StageOnly) { [pscustomobject]@{Verified=$true; FilesToUpdate=$changes.Count} | ConvertTo-Json; return }
$runningClient = @(Get-Process -Name Moonlight -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq $moonExe })
$runningHost = @(Get-Process -Name sunshine -ErrorAction SilentlyContinue | Where-Object { $_.Path -ieq $sunExe })
$wasRunning = (Get-Service SunshineService).Status -eq 'Running'
$basePort=47989
$portSetting=Select-String -LiteralPath $sunConf -Pattern '^\s*port\s*=\s*(\d+)\s*$' | Select-Object -Last 1
if ($portSetting) { $basePort=[int]$portSetting.Matches[0].Groups[1].Value }
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
    $out=Join-Path $work 'moonlight-version.txt'
    $test=Start-Process -FilePath $moonExe -ArgumentList '--version' -WindowStyle Hidden -WorkingDirectory $MoonlightTarget -RedirectStandardOutput $out -Wait -PassThru
    if ($test.ExitCode -ne 0 -or (Get-Content -LiteralPath $out -Raw) -notmatch 'Moonlight') {throw 'Moonlight launch verification failed.'}
    if ($wasRunning) {
        if ($restart) { Start-Service SunshineService }
        $ok=$false
        for ($i=0;$i -lt 30 -and !$ok;$i++) {
            $hostIds=@(Get-Process sunshine -ErrorAction SilentlyContinue | Where-Object {$_.Path -ieq $sunExe} | ForEach-Object Id)
            if (@(Get-NetTCPConnection -State Listen -LocalPort ($basePort-5) -ErrorAction SilentlyContinue | Where-Object {$_.OwningProcess -in $hostIds}).Count) { $ok=$true } else { Start-Sleep -Seconds 1 }
        }
        if (!$ok) {throw 'Sunshine did not open its HTTPS stream listener.'}
    }
    [pscustomobject]@{Updated=($changes.Count -gt 0); ChangedFiles=$changes.Count; Backup=$(if ($updated.Count) { $backup } else { $null }); SunshineSHA256=$installedHashes.Sunshine; MoonlightSHA256=$installedHashes.Moonlight; Service=(Get-Service SunshineService).Status.ToString(); ClientWasOpen=($runningClient.Count -gt 0)} | ConvertTo-Json
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
