# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory)][string]$Installer, [Parameter(Mandatory)][string]$Probe)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) {
    throw 'Run this installation test only on a disposable GitHub Actions runner.'
}
# Include spaces to exercise the installer's command-line path handling.
$work = Join-Path $env:RUNNER_TEMP 'akyuu installed test'
if (Test-Path $work) { throw 'The installation test directory must not exist.' }
New-Item -ItemType Directory -Force $work | Out-Null
$env:QT_QPA_PLATFORM = 'offscreen'
$env:PATH = "$env:SystemRoot/system32;$env:SystemRoot;${env:ProgramFiles}/Git/cmd"
$env:QT_PLUGIN_PATH = ''
$env:QML_IMPORT_PATH = ''
# Qt uses the Windows known folder API, not the APPDATA environment override.
$roaming = [Environment]::GetFolderPath([Environment+SpecialFolder]::ApplicationData)
if (-not $roaming) { throw 'The runner has no roaming application data directory.' }
$marker = Join-Path $roaming 'akyuu/data/packaging-test-marker'
if (Test-Path $marker) { throw 'The user-data test marker must not exist.' }
New-Item -ItemType Directory -Force (Split-Path $marker) | Out-Null
Set-Content $marker 'Preserve user data'
$expected = (Get-FileHash $marker).Hash
$install = Join-Path $work 'application'
$unrelated = Join-Path $install 'unrelated/subdirectory/keep.txt'
New-Item -ItemType Directory -Force (Split-Path $unrelated) | Out-Null
Set-Content $unrelated 'Preserve unrelated installation-directory files'
$unrelated = (Get-Item -LiteralPath $unrelated).FullName
$unrelatedHash = (Get-FileHash $unrelated).Hash
function Invoke-InstallerProcess([string]$File, [string[]]$Arguments) {
    Write-Output "Starting installation process: $File"
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -PassThru
    if (-not $process.WaitForExit(180000)) {
        Get-CimInstance Win32_Process | Where-Object {
            $_.Name -match 'akyuu|vc_redist|msiexec|setup|uninstall'
        } | Select-Object ProcessId, ParentProcessId, Name, CommandLine | Format-List | Out-Host
        Get-ChildItem $env:TEMP -Filter 'dd_vcredist*.log' | ForEach-Object {
            Write-Output "Runtime installer log: $($_.FullName)"
            Get-Content $_.FullName -Tail 60 | Out-Host
        }
        throw 'The installation process did not finish within three minutes.'
    }
    if ($process.ExitCode) { throw "Installation process failed: $($process.ExitCode)" }
    Write-Output 'Installation process completed.'
}
foreach ($attempt in 1..2) {
    Invoke-InstallerProcess $Installer @('/S', "/D=$install")
    Copy-Item $Probe "$install/akyuu-deployment-tests.exe"
    & "$install/akyuu-deployment-tests.exe" --network
    if ($LASTEXITCODE) { throw "Installed runtime verification failed (exit $LASTEXITCODE)." }
    Remove-Item "$install/akyuu-deployment-tests.exe"
    $process = Start-Process -FilePath "$install/Akyuu.exe" -ArgumentList '--debug' -PassThru
    Start-Sleep -Seconds 8
    if ($process.HasExited) { throw "Installed app exited early: $($process.ExitCode)" }
    Stop-Process -Id $process.Id
    Wait-Process -InputObject $process -ErrorAction SilentlyContinue
}
$packagedFiles = @(Get-ChildItem -LiteralPath $install -File -Recurse | Where-Object { $_.FullName -ne $unrelated } | Select-Object -ExpandProperty FullName)
Invoke-InstallerProcess "$install/Uninstall.exe" @('/S')
# NSIS's launcher returns before its copied uninstaller finishes; wait for removal.
$deadline = (Get-Date).AddSeconds(30)
do {
    $remaining = @($packagedFiles | Where-Object { Test-Path -LiteralPath $_ })
    if (-not $remaining.Count) { break }
    Start-Sleep -Milliseconds 200
} while ((Get-Date) -lt $deadline)
if ($remaining.Count) { throw "Uninstaller left packaged files: $($remaining -join ', ')" }
if ((Get-FileHash $unrelated).Hash -ne $unrelatedHash) { throw 'Uninstallation changed unrelated installation-directory files.' }
if ((Get-FileHash $marker).Hash -ne $expected) { throw 'Installation or removal changed user data.' }
Write-Output 'Passed installed startup, runtime, reinstall, user-data and unrelated-file preservation checks.'
