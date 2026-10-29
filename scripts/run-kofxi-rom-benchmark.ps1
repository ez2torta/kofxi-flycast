[CmdletBinding()]
param(
    [ValidateSet('original', 'segmented', 'decrypted', 'extracted')]
    [string]$Backend = 'original',
    [string]$BuildDirectory = 'build-windows-msvc-lab',
    [string]$DecryptedPath = (Join-Path $PSScriptRoot '..\..\kofxi_aw_modding\decrypted'),
    [string]$ExtractedPath = (Join-Path $PSScriptRoot '..\..\kofxi_aw_modding\fs_extracted'),
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildPath = if ([IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $repo $BuildDirectory }
$exe = (Resolve-Path (Join-Path $buildPath 'Release\flycast.exe')).Path
$rom = (Resolve-Path (Join-Path $repo 'roms\kofxi.zip')).Path
if (Get-Process -Name flycast -ErrorAction SilentlyContinue) {
    throw 'Close Flycast normally before starting another benchmark.'
}
# Reject an old/stock binary rather than silently benchmarking the wrong backend.
$binaryText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
if (!$binaryText.Contains('all_chip_words_and_PIO_inverse') -or !$binaryText.Contains('AW host:')) {
    throw 'Rebuild the lab Release with AW_CARTRIDGE_PROFILE first (cmake --preset windows-msvc-lab; cmake --build --preset windows-msvc-lab-release).'
}
if (!$binaryText.Contains('Input replay terminated') -or !$binaryText.Contains('Automation time-out')) {
    throw 'The selected executable lacks TEST_AUTOMATION replay markers; use the windows-msvc-lab Release.'
}
if ($Backend -eq 'extracted' -and !$binaryText.Contains('all_extracted_bytes_and_G1_records')) {
    throw 'Rebuild the lab Release with the extracted backend first.'
}
$binaryText = $null
if ($Backend -eq 'decrypted') {
    $DecryptedPath = (Resolve-Path -LiteralPath $DecryptedPath).Path
}
if ($Backend -eq 'extracted') {
    $ExtractedPath = (Resolve-Path -LiteralPath $ExtractedPath).Path
}
if ($DecryptedPath.Contains('"') -or $DecryptedPath.Contains(',') -or $ExtractedPath.Contains('"') -or $ExtractedPath.Contains(',')) {
    throw 'The asset directories cannot contain quotes or commas (Flycast config syntax).'
}
$run = Join-Path (Split-Path $exe) ('rom-benchmark-runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $Backend)
$arguments = '-config "log:LogToFile=yes" -config "network:GGPO=no" ' +
    '-config "config:UseReios=no,Dreamcast.AutoLoadState=no,Dreamcast.AutoSaveState=no" ' +
    '-config "kofxi:RomBackend=' + $Backend + '"'
if ($Backend -eq 'decrypted') {
    $arguments += ' -config "kofxi:DecryptedPath=' + $DecryptedPath + '"'
}
if ($Backend -eq 'extracted') {
    $arguments += ' -config "kofxi:ExtractedPath=' + $ExtractedPath + '"'
}
$arguments += ' "' + $rom + '"'
if ($DryRun) {
    [pscustomobject]@{Executable=$exe; Arguments=$arguments; WorkingDirectory=$run; Log=(Join-Path $run 'flycast.log')}
    return
}
New-Item -ItemType Directory -Path $run | Out-Null
[ordered]@{
    backend=$Backend
    build_directory=(Split-Path (Split-Path $exe))
    profile='AW_CARTRIDGE_PROFILE + TEST_AUTOMATION'
    executable=$exe
    executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash
    rom_sha256=(Get-FileHash $rom -Algorithm SHA256).Hash
    decrypted_directory=if ($Backend -eq 'decrypted') {$DecryptedPath} else {$null}
    extracted_directory=if ($Backend -eq 'extracted') {$ExtractedPath} else {$null}
    arguments=$arguments
    note='Exploratory offline run; record teams/stage and phase times. No save restored by launcher.'
} | ConvertTo-Json | Set-Content (Join-Path $run 'run.json') -Encoding UTF8
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $run -PassThru
[pscustomobject]@{Backend=$Backend; PID=$process.Id; Log=(Join-Path $run 'flycast.log'); Metadata=(Join-Path $run 'run.json')}
