# Builds and runs a Prism experiment: the single entry point for VS Code tasks, F5 and terminals.
#
# ASCII-only on purpose: Windows PowerShell 5.1 reads a BOM-less script as ANSI and would corrupt
# non-ASCII literals.
#
#   powershell -NoProfile -File prism.ps1                                    # ForwardExperiment, Debug
#   powershell -NoProfile -File prism.ps1 -Target All -Config Release -NoRun
#   powershell -NoProfile -File prism.ps1 -Target PrismContract -AppArgs --smoke-test=30 -TailLog
#   powershell -NoProfile -File prism.ps1 -Target PrismForward -AppArgs --bench=120 --metrics forward_bench.csv

param(
    # Source file that triggered the run (Code Runner passes $fullFileName); picks a target when
    # -Target is empty.
    [string]$SourceFile = '',

    [ValidateSet('', 'PrismForward', 'PrismContract', 'PrismStarter', 'All')]
    [string]$Target = '',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',

    [string]$Preset = 'my-project',

    [switch]$ConfigureOnly,
    [switch]$NoBuild,
    [switch]$NoRun,
    [switch]$TailLog,

    # Passed to the executable verbatim: --smoke-test=30 / --bench=120 / --capture out.png ...
    [string[]]$AppArgs = @()
)

$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$buildDir = Join-Path $root ('build/' + $Preset)
$binDir = Join-Path $buildDir ('bin/' + $Config)

# cmake is not on PATH outside a Developer PowerShell; fall back to the VS installation.
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

    throw 'cmake not found. Install CMake or the VS C++ workload, or run from a Developer PowerShell.'
}

function Resolve-Target([string]$file) {
    if ([string]::IsNullOrEmpty($file)) {
        return 'PrismForward'
    }

    switch -Regex ($file -replace '\\', '/') {
        '/samples/forward/'  { return 'PrismForward' }
        '/tests/gpu/contract/' { return 'PrismContract' }
        '/samples/starter/'      { return 'PrismStarter' }
        default                  { return 'PrismForward' }
    }
}

$cmake = Find-CMake
Write-Host ('[prism] cmake : ' + $cmake)

if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    Write-Host ('[prism] configuring ' + $Preset)
    Push-Location $root
    try {
        & $cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) {
            throw ('configure failed with exit code ' + $LASTEXITCODE)
        }
    }
    finally {
        Pop-Location
    }
}

if ($ConfigureOnly) {
    exit 0
}

if ([string]::IsNullOrEmpty($Target)) {
    $Target = Resolve-Target $SourceFile
}

if (-not $NoBuild) {
    $targets = if ($Target -eq 'All') { @('PrismForward', 'PrismContract', 'PrismStarter') } else { @($Target) }

    Write-Host ('[prism] building ' + ($targets -join ', ') + ' (' + $Config + ')')
    & $cmake --build $buildDir --config $Config --target $targets --parallel
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($NoRun) {
    exit 0
}

$runTarget = if ($Target -eq 'All') { 'PrismForward' } else { $Target }
$executable = Join-Path $binDir ($runTarget + '.exe')
if (-not (Test-Path $executable)) {
    Write-Error ('executable not found: ' + $executable)
    exit 1
}

# Headless flags make the process exit on its own; an interactive run starts in the background.
$headless = $false
foreach ($argument in $AppArgs) {
    if ($argument -match '^--(smoke-test|bench|capture|reference|write-reference)') {
        $headless = $true
        break
    }
}

Write-Host ('[prism] running ' + $runTarget + '.exe ' + ($AppArgs -join ' '))

$startParameters = @{ FilePath = $executable; WorkingDirectory = $binDir; PassThru = $true }
if ($AppArgs.Count -gt 0) {
    $startParameters.ArgumentList = $AppArgs
}
if ($headless) {
    $startParameters.Wait = $true
}

$process = Start-Process @startParameters

$exitCode = 0
if ($headless) {
    $exitCode = $process.ExitCode
    Write-Host ('[prism] exit code : ' + $exitCode)
}

# Headless runs write the host log next to the executable.
if ($TailLog -or $headless) {
    $logPath = Join-Path $binDir 'prism.log'
    if (Test-Path $logPath) {
        Write-Host '[prism] --- prism.log (tail) ---'
        Get-Content $logPath -Tail 15 | ForEach-Object { Write-Host ('    ' + $_) }
    }
}

exit $exitCode
