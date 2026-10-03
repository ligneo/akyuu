# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [Parameter(Mandatory)][string]$Build,
    [Parameter(Mandatory)][string]$Qt,
    [Parameter(Mandatory)][string]$Licenses,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = & python "$PSScriptRoot/release-version.py"
if ($LASTEXITCODE) { throw 'Reading the application version failed.' }
$version = "$version".Trim()
if ($version -notmatch '^(?<numeric>[0-9]+\.[0-9]+\.[0-9]+)(?:-[0-9A-Za-z.-]+)?$') {
    throw 'The application version must be numeric with an optional prerelease suffix.'
}
$numericVersion = $Matches.numeric
$stage = Join-Path $Output 'windows-stage'
if (Test-Path $stage) { throw 'The Windows staging directory must not exist.' }
New-Item -ItemType Directory -Force $stage, $Output | Out-Null
$cache = Get-Content (Join-Path $Build 'CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^AKYUU_PORTABLE:BOOL=OFF\r?$') { throw 'Installer builds must keep user data outside the application directory.' }
Copy-Item (Join-Path $Build 'bin/Akyuu.exe') $stage
& "$Qt/bin/windeployqt.exe" --release --no-compiler-runtime --include-plugins qoffscreen --dir $stage "$stage/Akyuu.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed.' }
New-Item -ItemType Directory -Force "$stage/licenses" | Out-Null
Copy-Item "$Licenses/*" "$stage/licenses" -Recurse -Force
Copy-Item "$root/LICENSE" "$stage/LICENSE"
Invoke-WebRequest 'https://aka.ms/vs/17/release/vc_redist.x64.exe' -OutFile "$stage/vc_redist.x64.exe"
$signature = Get-AuthenticodeSignature "$stage/vc_redist.x64.exe"
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'The Visual C++ Redistributable must have a valid Microsoft signature.'
}
# Compile a removal manifest from the exact deployed payload, never from the install tree.
$stage = (Resolve-Path $stage).Path
$entries = @(Get-ChildItem -LiteralPath $stage -Recurse -Force)
if ($entries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw 'The Windows payload must not contain reparse points.'
}
if (Test-Path (Join-Path $stage 'Uninstall.exe')) { throw 'Uninstall.exe is reserved for the generated uninstaller.' }
function Get-NsisPayloadPath($entry) {
    $relative = [IO.Path]::GetRelativePath($stage, $entry.FullName).Replace('/', '\')
    # NSIS expands dollars even in quoted strings; protect literal payload names.
    return $relative.Replace('$', '$$').Replace('"', '$\"')
}
$removal = @('; Generated from the deployed payload. Do not edit.')
foreach ($file in ($entries | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)) {
    $removal += 'Delete "$INSTDIR\' + (Get-NsisPayloadPath $file) + '"'
}
foreach ($directory in ($entries | Where-Object PSIsContainer | Sort-Object { $_.FullName.Length } -Descending)) {
    $removal += 'RMDir "$INSTDIR\' + (Get-NsisPayloadPath $directory) + '"'
}
$manifest = Join-Path (Resolve-Path $Output).Path 'windows-uninstall.nsh'
Set-Content -LiteralPath $manifest -Value $removal -Encoding utf8
$installer = Join-Path (Resolve-Path $Output) "akyuu-$version-x86_64-setup.exe"
$nsis = "${env:ProgramFiles(x86)}/NSIS/makensis.exe"
& $nsis "/DSTAGING_DIR=$stage" "/DOUTPUT_FILE=$installer" "/DPRODUCT_VERSION=$numericVersion" "/DDISPLAY_VERSION=$version" "/DUNINSTALL_MANIFEST=$manifest" "$root/setup/Akyuu.nsi"
if ($LASTEXITCODE) { throw 'Windows installer creation failed.' }
Write-Output $installer
