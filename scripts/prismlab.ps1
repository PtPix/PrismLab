# Builds and runs a PrismLab experiment. Single entry point shared by Code Runner,
# VS Code tasks, launch configurations and plain terminals.
#
# NOTE: this file is intentionally ASCII-only. Windows PowerShell 5.1 reads a script
# without a UTF-8 BOM as ANSI, which corrupts non-ASCII literals and can break parsing.
#
# What it does:
#   1. finds cmake (PATH -> vswhere -> the VS 2022 install that ships CMake);
#   2. configures the preset only when the build directory does not exist yet;
#   3. builds the requested target(s);
#   4. starts the matching executable: headless flags (--smoke-test / --bench /
#      --capture / --reference / --write-reference) wait for the process and return
#      its exit code, while an interactive run starts in the background so that the
#      terminal is not blocked.
#
# Examples:
#   powershell -NoProfile -File scripts/prismlab.ps1 -SourceFile samples/lab_forward/forward_lab.cpp
#   powershell -NoProfile -File scripts/prismlab.ps1 -Target PrismLabContract -SmokeTest 30 -TailLog
#   powershell -NoProfile -File scripts/prismlab.ps1 -Target PrismLabForward -Bench 120 -Metrics forward_bench.csv -TailLog
#   powershell -NoProfile -File scripts/prismlab.ps1 -Target PrismLabForward -Reference forward_ref.f32 -Tolerance 0.0001

[CmdletBinding()]
param(
    # Source file that triggered the run (Code Runner passes $fullFileName).
    # Used only to pick a target when -Target is not given.
    [string]$SourceFile = '',

    [ValidateSet('', 'PrismLabForward', 'PrismLabContract', 'PrismLabStarter', 'All')]
    [string]$Target = '',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',

    [string]$Preset = 'my-project',

    [switch]$ConfigureOnly,
    [switch]$NoBuild,
    [switch]$NoRun,
    [switch]$Wait,
    [switch]$TailLog,

    # Host command line shortcuts
    [int]$SmokeTest = 0,
    [int]$Bench = 0,
    [int]$BenchWarmup = 30,
    [string]$Metrics = '',
    [string]$Capture = '',
    [int]$CaptureFrame = 0,
    [int]$DebugView = 0,
    [string]$Reference = '',
    [string]$WriteReference = '',
    [string]$Tolerance = '',

    # Extra arguments passed to the executable verbatim
    [string[]]$AppArgs = @()
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root ('build/' + $Preset)
$binDir = Join-Path $buildDir 'bin'

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $installation = & $vswhere -latest -products * -property installationPath 2>$null
        if ($installation) {
            $candidate = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $fallbacks = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )

    foreach ($candidate in $fallbacks) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw 'cmake was not found. Install CMake or the Visual Studio C++ workload, or run from a Developer PowerShell.'
}

function Resolve-TargetFromSource([string]$file) {
    if ([string]::IsNullOrEmpty($file)) {
        return 'PrismLabForward'
    }

    $normalized = $file -replace '\\', '/'

    if ($normalized -match '/samples/lab_forward/') { return 'PrismLabForward' }
    if ($normalized -match '/samples/lab_contract/') { return 'PrismLabContract' }
    if ($normalized -match '/samples/starter/') { return 'PrismLabStarter' }

    # Framework code (host/, pipelines/, adapters/, backends/, include/, algorithms/):
    # run the baseline experiment.
    return 'PrismLabForward'
}

$cmake = Find-CMake
Write-Host ('[prismlab] cmake      : ' + $cmake)
Write-Host ('[prismlab] workspace  : ' + $root)

# --- configure ------------------------------------------------------------
if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    Write-Host ('[prismlab] configuring preset ' + $Preset + ' ...')
    Push-Location $root
    try {
        & $cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) {
            throw ('cmake configure failed with exit code ' + $LASTEXITCODE)
        }
    }
    finally {
        Pop-Location
    }
}
else {
    Write-Host ('[prismlab] build dir  : ' + $buildDir)
}

if ($ConfigureOnly) {
    Write-Host '[prismlab] configure done.'
    exit 0
}

# --- pick target ----------------------------------------------------------
if ([string]::IsNullOrEmpty($Target)) {
    $Target = Resolve-TargetFromSource $SourceFile
    Write-Host ('[prismlab] target     : ' + $Target + ' (inferred from ' + $SourceFile + ')')
}
else {
    Write-Host ('[prismlab] target     : ' + $Target)
}

# --- build ----------------------------------------------------------------
if (-not $NoBuild) {
    if ($Target -eq 'All') {
        $targets = @('PrismLabForward', 'PrismLabContract', 'PrismLabStarter')
    }
    else {
        $targets = @($Target)
    }

    Write-Host ('[prismlab] building   : ' + ($targets -join ', ') + ' (' + $Config + ')')

    & $cmake --build $buildDir --config $Config --target $targets --parallel
    if ($LASTEXITCODE -ne 0) {
        Write-Error ('build failed with exit code ' + $LASTEXITCODE)
        exit $LASTEXITCODE
    }
}

if ($NoRun) {
    exit 0
}

# --- run ------------------------------------------------------------------
$runTarget = $Target
if ($runTarget -eq 'All') {
    $runTarget = 'PrismLabForward'
}

$executable = Join-Path $binDir ($runTarget + '.exe')
if (-not (Test-Path $executable)) {
    Write-Error ('executable not found: ' + $executable)
    exit 1
}

$arguments = New-Object System.Collections.Generic.List[string]
foreach ($argument in $AppArgs) {
    [void]$arguments.Add($argument)
}

if ($SmokeTest -gt 0) {
    [void]$arguments.Add('--smoke-test=' + $SmokeTest)
}
if ($Bench -gt 0) {
    [void]$arguments.Add('--bench=' + $Bench)
    [void]$arguments.Add('--bench-warmup=' + $BenchWarmup)
}
if (-not [string]::IsNullOrEmpty($Metrics)) {
    [void]$arguments.Add('--metrics')
    [void]$arguments.Add($Metrics)
}
if (-not [string]::IsNullOrEmpty($Capture)) {
    [void]$arguments.Add('--capture')
    [void]$arguments.Add($Capture)
}
if ($CaptureFrame -gt 0) {
    [void]$arguments.Add('--capture-frame')
    [void]$arguments.Add($CaptureFrame)
}
if ($DebugView -gt 0) {
    [void]$arguments.Add('--debug-view')
    [void]$arguments.Add($DebugView)
}
if (-not [string]::IsNullOrEmpty($Reference)) {
    [void]$arguments.Add('--reference')
    [void]$arguments.Add($Reference)
}
if (-not [string]::IsNullOrEmpty($WriteReference)) {
    [void]$arguments.Add('--write-reference')
    [void]$arguments.Add($WriteReference)
}
if (-not [string]::IsNullOrEmpty($Tolerance)) {
    [void]$arguments.Add('--tolerance')
    [void]$arguments.Add($Tolerance)
}

$headless = ($SmokeTest -gt 0) -or ($Bench -gt 0) -or
    (-not [string]::IsNullOrEmpty($Capture)) -or
    (-not [string]::IsNullOrEmpty($Reference)) -or
    (-not [string]::IsNullOrEmpty($WriteReference))

$shouldWait = $Wait -or $headless

$displayArguments = if ($arguments.Count -gt 0) { $arguments -join ' ' } else { '(none)' }
Write-Host ('[prismlab] running    : ' + $runTarget + '.exe ' + $displayArguments)
Write-Host ('[prismlab] cwd        : ' + $binDir)

$startParameters = @{
    FilePath         = $executable
    WorkingDirectory = $binDir
    PassThru         = $true
}

if ($arguments.Count -gt 0) {
    $startParameters.ArgumentList = $arguments.ToArray()
}
if ($shouldWait) {
    $startParameters.Wait = $true
}

$process = Start-Process @startParameters

$exitCode = 0
if ($shouldWait) {
    $exitCode = $process.ExitCode
    Write-Host ('[prismlab] exit code  : ' + $exitCode)
}
else {
    Write-Host '[prismlab] started in the background; close the window to stop the experiment.'
}

# Headless runs write the host log next to the executable.
if ($TailLog -or $shouldWait) {
    $logPath = Join-Path $binDir 'renderlab.log'
    if (Test-Path $logPath) {
        Write-Host '[prismlab] --- renderlab.log (tail) ---'
        Get-Content $logPath -Tail 15 | ForEach-Object { Write-Host ('    ' + $_) }
    }
}

exit $exitCode
