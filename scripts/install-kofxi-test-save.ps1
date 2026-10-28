[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'Both')]
    [string]$Configuration = 'Release',

    [string]$SourcePath = (Join-Path $PSScriptRoot '..\tests\fixtures\kofxi\unlocked-spanish.nvmem'),

    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$expectedSize = 131072
$destinationName = 'kofxi.zip.nvmem'

if (Get-Process -Name 'flycast' -ErrorAction SilentlyContinue) {
    throw 'Flycast is running. Exit Flycast normally before importing NVRAM; the running emulator can rewrite the save.'
}

$sourceInfo = Get-Item -LiteralPath $source
if ($sourceInfo.Length -ne $expectedSize) {
    throw "Unexpected NVRAM size ($($sourceInfo.Length) bytes). Expected $expectedSize; refusing to copy."
}

$configurations = if ($Configuration -eq 'Both') { @('Debug', 'Release') } else { @($Configuration) }
$destinations = foreach ($configurationName in $configurations) {
    Join-Path $repoRoot "build-windows-msvc\$configurationName\data\$destinationName"
}

Write-Host "Source: $source"
Write-Host "SHA-256: $((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash)"
Write-Host 'The separate kofxi.zip.nvmem2 file will not be changed.'
foreach ($destination in $destinations) {
    Write-Host "Destination: $destination"
}

if (-not $Yes) {
    $answer = Read-Host 'Proceed? Type Y to continue'
    if ($answer -cne 'Y') {
        Write-Host 'Cancelled; no files changed.'
        exit 0
    }
}

$backupStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
foreach ($destination in $destinations) {
    $destinationDirectory = Split-Path -Parent $destination
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null

    if (Test-Path -LiteralPath $destination) {
        $backupDirectory = Join-Path $repoRoot "local-saves\kofxi-unlocked-spanish\backups\$backupStamp\$configurationName"
        New-Item -ItemType Directory -Force -Path $backupDirectory | Out-Null
        Copy-Item -LiteralPath $destination -Destination (Join-Path $backupDirectory $destinationName)
    }

    $temporary = "$destination.importing"
    Copy-Item -LiteralPath $source -Destination $temporary -Force
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $temporaryHash = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
    if ($sourceHash -ne $temporaryHash) {
        Remove-Item -LiteralPath $temporary -Force
        throw "Hash verification failed for $destination; original destination was not replaced."
    }

    Move-Item -LiteralPath $temporary -Destination $destination -Force
    Write-Host "Installed $destination (SHA-256 $sourceHash)"
}
