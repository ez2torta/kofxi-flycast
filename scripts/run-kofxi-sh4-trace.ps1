[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$Executable = 'build-windows-msvc-lab/Debug/flycast.exe',
    [string]$ScratchRoot = (Join-Path $env:LOCALAPPDATA 'Temp/opencode/kofxi-sh4-traces'),
    [string]$NvmemFixture,
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 30,
    [string]$ReadStart,
    [string]$ReadEnd,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exePath = if ([IO.Path]::IsPathRooted($Executable)) { $Executable } else { Join-Path $repo $Executable }
$exePath = (Resolve-Path -LiteralPath $exePath).Path
$romPath = (Resolve-Path -LiteralPath $RomPath).Path
$fixturePath = $null
$fixtureHash = $null
if (![string]::IsNullOrWhiteSpace($NvmemFixture)) {
    $fixturePath = (Resolve-Path -LiteralPath $NvmemFixture).Path
    $fixtureInfo = Get-Item -LiteralPath $fixturePath
    $fixtureHash = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash
    if ($fixtureInfo.Length -ne 131072 -or $fixtureHash -ne 'F0B006158F826A5394B47434AA0453AFD476D1C339EAE8DA746C36AA5D957DCA') {
        throw 'Only the explicitly approved 128 KiB KOF XI fixture is accepted; refusing any other NVRAM.'
    }
}

$hasReadStart = ![string]::IsNullOrWhiteSpace($ReadStart)
$hasReadEnd = ![string]::IsNullOrWhiteSpace($ReadEnd)
if ($hasReadStart -ne $hasReadEnd) {
    throw 'ReadStart and ReadEnd must be specified together.'
}
if ($hasReadStart) {
    [uint32]$parsedStart = 0
    [uint32]$parsedEnd = 0
    $startOk = if ($ReadStart.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        [uint32]::TryParse($ReadStart.Substring(2), [Globalization.NumberStyles]::HexNumber, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsedStart)
    } else {
        [uint32]::TryParse($ReadStart, [Globalization.NumberStyles]::Integer, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsedStart)
    }
    $endOk = if ($ReadEnd.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        [uint32]::TryParse($ReadEnd.Substring(2), [Globalization.NumberStyles]::HexNumber, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsedEnd)
    } else {
        [uint32]::TryParse($ReadEnd, [Globalization.NumberStyles]::Integer, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsedEnd)
    }
    if (!$startOk -or !$endOk -or $parsedStart -ge $parsedEnd) {
        throw 'Read range must be a valid increasing pair of uint32 addresses.'
    }
    $ReadStart = '0x{0:x8}' -f $parsedStart
    $ReadEnd = '0x{0:x8}' -f $parsedEnd
}

$exeBytes = [IO.File]::ReadAllBytes($exePath)
$exeMarkers = [Text.Encoding]::ASCII.GetString($exeBytes)
if (!$exeMarkers.Contains('KOFXI_SH4_READ_TRACE_START') -or !$exeMarkers.Contains('KOFXI_MAPLE_DMA')) {
    throw 'Selected executable does not contain both Debug laboratory trace hooks.'
}
$exeHash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash
$romHash = (Get-FileHash -LiteralPath $romPath -Algorithm SHA256).Hash
$runName = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$runDirectory = Join-Path $ScratchRoot $runName
if (Test-Path -LiteralPath $runDirectory) {
    throw "Run directory already exists; refusing to reuse it: $runDirectory"
}

$arguments = '-config "log:LogToFile=yes" -config "log:Verbosity=4" -config "network:GGPO=no" ' +
    '-config "config:UseReios=no,Dreamcast.AutoLoadState=no,Dreamcast.AutoSaveState=no" ' +
    '"' + $romPath.Replace('"', '\"') + '"'
$metadata = [ordered]@{
    executable_source = $exePath
    executable_sha256 = $exeHash
    rom_source = $romPath
    rom_sha256 = $romHash
    nvram_fixture_source = $fixturePath
    nvram_fixture_sha256 = $fixtureHash
    read_start = $ReadStart
    read_end = $ReadEnd
    duration_seconds = $DurationSeconds
    arguments = $arguments
    scratch_directory = $runDirectory
    note = if ($null -eq $fixturePath) { 'Debug diagnostic run only. Fresh executable directory isolates Flycast data; no state import. This is not replay or B/D acceptance.' } else { 'Debug diagnostic run only. Only the approved NVRAM fixture is copied to scratch data/kofxi.zip.nvmem; no .nvmem2 or savestate is imported. This is not replay or B/D acceptance.' }
}

if ($DryRun) {
    [pscustomobject]$metadata
    return
}
if (Get-Process -Name flycast -ErrorAction SilentlyContinue) {
    throw 'A Flycast process is already running; refusing to start another instance.'
}

New-Item -ItemType Directory -Path $runDirectory | Out-Null
$runExe = Join-Path $runDirectory 'flycast.exe'
Copy-Item -LiteralPath $exePath -Destination $runExe
if ((Get-FileHash -LiteralPath $runExe -Algorithm SHA256).Hash -ne $exeHash) {
    throw 'Scratch executable hash mismatch.'
}
$dataDirectory = Join-Path $runDirectory 'data'
if ($null -ne $fixturePath) {
    New-Item -ItemType Directory -Path $dataDirectory | Out-Null
    $nvramDestination = Join-Path $dataDirectory 'kofxi.zip.nvmem'
    Copy-Item -LiteralPath $fixturePath -Destination $nvramDestination
    if ((Get-FileHash -LiteralPath $nvramDestination -Algorithm SHA256).Hash -ne $fixtureHash) {
        throw 'Scratch NVRAM fixture hash mismatch; refusing to launch.'
    }
}
$metadata.executable = $runExe
$metadata.log = Join-Path $runDirectory 'flycast.log'
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDirectory 'run.json') -Encoding UTF8

$oldStart = $env:KOFXI_SH4_READ_TRACE_START
$oldEnd = $env:KOFXI_SH4_READ_TRACE_END
try {
    if ($hasReadStart) {
        $env:KOFXI_SH4_READ_TRACE_START = $ReadStart
        $env:KOFXI_SH4_READ_TRACE_END = $ReadEnd
    } else {
        Remove-Item Env:KOFXI_SH4_READ_TRACE_START -ErrorAction SilentlyContinue
        Remove-Item Env:KOFXI_SH4_READ_TRACE_END -ErrorAction SilentlyContinue
    }
    $process = Start-Process -FilePath $runExe -ArgumentList $arguments -WorkingDirectory $runDirectory -PassThru
} finally {
    if ($null -eq $oldStart) { Remove-Item Env:KOFXI_SH4_READ_TRACE_START -ErrorAction SilentlyContinue } else { $env:KOFXI_SH4_READ_TRACE_START = $oldStart }
    if ($null -eq $oldEnd) { Remove-Item Env:KOFXI_SH4_READ_TRACE_END -ErrorAction SilentlyContinue } else { $env:KOFXI_SH4_READ_TRACE_END = $oldEnd }
}

$startedAt = Get-Date
Start-Sleep -Seconds $DurationSeconds
$process.Refresh()
if (!$process.HasExited) {
    [void]$process.CloseMainWindow()
    if (!$process.WaitForExit(15000)) {
        throw "Flycast did not close normally; PID=$($process.Id). No force termination was attempted. Scratch: $runDirectory"
    }
}

[pscustomobject]@{
    PID = $process.Id
    Started = $startedAt
    Exited = $process.HasExited
    ExitCode = $process.ExitCode
    Scratch = $runDirectory
    Log = $metadata.log
    Metadata = Join-Path $runDirectory 'run.json'
} | Format-List
