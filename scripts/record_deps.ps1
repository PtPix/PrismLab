# Writes the currently pinned dependency commits back into docs/dependencies.md.
#
# NOTE: this file is intentionally ASCII-only. Windows PowerShell 5.1 reads a
# script without a UTF-8 BOM as ANSI, which corrupts any non-ASCII literal and
# can break parsing. The document itself stays UTF-8 (read/written explicitly).
#
# Usage: powershell -ExecutionPolicy Bypass -File scripts/record_deps.ps1

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'docs/dependencies.md'

if (-not (Test-Path $output)) {
    Write-Error ('Missing file: ' + $output)
    exit 1
}

function Get-Commit($relativePath) {
    if ([string]::IsNullOrEmpty($relativePath)) {
        $path = $root
    }
    else {
        $path = Join-Path $root $relativePath
    }

    if (-not (Test-Path (Join-Path $path '.git'))) {
        return '<not-a-git-checkout>'
    }

    return (git -C $path rev-parse HEAD).Trim()
}

$components = @(
    @{ Name = 'PrismLab';      Path = '' },
    @{ Name = 'Donut-Samples'; Path = 'external/Donut-Samples' },
    @{ Name = 'donut';         Path = 'external/Donut-Samples/donut' },
    @{ Name = 'nvrhi';         Path = 'external/Donut-Samples/donut/nvrhi' },
    @{ Name = 'ShaderMake';    Path = 'external/Donut-Samples/thirdparty/ShaderMake' },
    @{ Name = 'glfw';          Path = 'external/Donut-Samples/thirdparty/glfw' },
    @{ Name = 'imgui';         Path = 'external/Donut-Samples/thirdparty/imgui' },
    @{ Name = 'cgltf';         Path = 'external/Donut-Samples/thirdparty/cgltf' }
)

$lines = @(Get-Content -Path $output -Encoding UTF8)

foreach ($component in $components) {
    $name = $component.Name
    $commit = Get-Commit $component.Path
    $pattern = '^\|\s*' + [regex]::Escape($name) + '\s*\|\s*.*?\s*\|\s*$'
    $replaced = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $pattern) {
            $lines[$i] = '| ' + $name + ' | ' + $commit + ' |'
            $replaced = $true
            break
        }
    }

    if ($replaced) {
        Write-Host ($name + ' = ' + $commit)
    }
    else {
        Write-Warning ('No table row found for component: ' + $name)
    }
}

$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '<!-- generated: .* -->') {
        $lines[$i] = '<!-- generated: ' + $timestamp + ' -->'
        break
    }
}

[System.IO.File]::WriteAllText(
    $output,
    (($lines -join "`r`n") + "`r`n"),
    (New-Object System.Text.UTF8Encoding($true)))

Write-Host ('Updated ' + $output)
