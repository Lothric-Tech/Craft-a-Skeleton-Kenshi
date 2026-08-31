param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot 'CraftASkeletonPlugin.vcxproj'

if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "Project file not found: $Project"
}

foreach ($Name in @(
    'KENSHILIB_DIR',
    'KENSHILIB_DEPS_DIR',
    'BOOST_INCLUDE_PATH'
)) {
    $Value = [Environment]::GetEnvironmentVariable($Name)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "Required environment variable is not set: $Name"
    }
}

$Candidates = @(
    'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
)

$MSBuild = @(
    $Candidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
) | Select-Object -First 1

if (-not $MSBuild) {
    $VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'

    if (Test-Path -LiteralPath $VsWhere -PathType Leaf) {
        $InstallPath = & $VsWhere `
            -latest `
            -products '*' `
            -requires Microsoft.Component.MSBuild `
            -property installationPath

        if ($InstallPath) {
            $Candidate = Join-Path $InstallPath 'MSBuild\Current\Bin\MSBuild.exe'

            if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
                $MSBuild = $Candidate
            }
        }
    }
}

if (-not $MSBuild) {
    throw 'MSBuild.exe could not be located.'
}

& $MSBuild `
    $Project `
    '/t:Rebuild' `
    '/p:Configuration=Release' `
    '/p:Platform=x64' `
    '/m'

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$Dll = Join-Path $RepoRoot '_builds\Release\CraftASkeleton.dll'

if (-not (Test-Path -LiteralPath $Dll -PathType Leaf)) {
    throw "Build completed but the DLL was not found: $Dll"
}

$Hash = (Get-FileHash -LiteralPath $Dll -Algorithm SHA256).Hash

Write-Host ''
Write-Host 'Build complete.'
Write-Host "DLL:    $Dll"
Write-Host "SHA256: $Hash"
