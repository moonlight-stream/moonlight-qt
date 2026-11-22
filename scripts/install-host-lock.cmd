@echo off
setlocal DisableDelayedExpansion
set "MOONLIGHT_HOST_LOCK_SETUP=%~f0"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -Command "$ErrorActionPreference = 'Stop'; $file = [IO.File]::ReadAllText($env:MOONLIGHT_HOST_LOCK_SETUP); & ([ScriptBlock]::Create(($file -split '(?m)^:PowerShell\r?$', 2)[1]))"
set "result=%errorlevel%"
pause
exit /b %result%

:PowerShell
# Install in the interactive host user's Start menu so Explorer owns the hotkey.
$programs = [Environment]::GetFolderPath([Environment+SpecialFolder]::Programs)
$path = Join-Path $programs 'Moonlight - Lock this PC.lnk'
$target = Join-Path $PSHOME 'powershell.exe'
$lockCommand = @'
Add-Type -Namespace Moonlight -Name Session -MemberDefinition '[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool LockWorkStation();'
if (-not [Moonlight.Session]::LockWorkStation()) { exit 1 }
'@
# EncodedCommand preserves the DllImport quotes when Explorer launches PowerShell.
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($lockCommand.Replace("`r`n", "`n")))
$arguments = "-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand $encoded"
$description = 'Lock this Windows session from Moonlight'
$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($path)
if (Test-Path -LiteralPath $path) {
    if ($link.Description -cne $description -or $link.TargetPath -ne $target -or $link.Arguments -cne $arguments) {
        throw "A different shortcut already exists at $path. Choose another name for that shortcut before installing."
    }
}
$link.TargetPath = $target
$link.Arguments = $arguments
$link.WorkingDirectory = $PSHOME
$link.WindowStyle = 7
$link.Description = $description
$link.Hotkey = 'CTRL+ALT+F10'
$link.Save()
Write-Output "Installed Ctrl+Alt+F10 for this user. To remove it, delete this shortcut:"
Write-Output $path
