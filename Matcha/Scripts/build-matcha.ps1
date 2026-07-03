<#
.SYNOPSIS
Configures, builds, deploys, and optionally runs Matcha with the shared CADViewer Windows toolchain.

.DESCRIPTION
This script intentionally does not rely on Matcha's existing CMake presets because those presets are
currently configured for a local Clang/Qt path. It uses the CADViewer MSVC/Ninja/Qt toolchain paths
provided by the workspace and writes to an isolated build directory:

  Matcha/build/windows-msvc-<config>

By default the legacy token schema validation is disabled because Light.json is now a consumer-level
token definition and no longer matches the old validator. Pass -EnableTokenValidation to opt back in.

By default the script suppresses MSVC warnings C4251 and C4458 so Debug builds are not blocked by
current public-header warning debt under the project's /WX policy. Pass -StrictWarnings to keep the
project warning policy unchanged.

.EXAMPLE
.\Scripts\build-matcha.ps1
Configures and builds the Debug configuration.

.EXAMPLE
.\Scripts\build-matcha.ps1 -Config Release -Target NyanCad
Builds only the NyanCad target in Release configuration.

.EXAMPLE
.\Scripts\build-matcha.ps1 -BuildOnly -Target Matcha
Builds an already-configured Debug tree without running CMake configure again.

.EXAMPLE
.\Scripts\build-matcha.ps1 -DeployQt -Run
Builds, deploys Qt runtime dependencies for NyanCad, and runs it.
#>
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Debug",

    [switch] $ConfigureOnly,
    [switch] $BuildOnly,
    [switch] $DeployQt,
    [switch] $Run,
    [switch] $EnableTokenValidation,
    [switch] $StrictWarnings,

    [string] $Target = "",
    [string] $RunTarget = "NyanCad",

    [string] $ToolchainRoot = "D:\CodeData\CADViewer\toolchain\windows",
    [string] $QtRoot = "",
    [string] $BuildDir = "",
    [string] $DeployDir = "",
    [string] $VSInstallDir = "",

    [int] $Jobs = 12
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FilePath,

        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]] $CommandArguments
    )

    & $FilePath @CommandArguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Resolve-RequiredPath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path,

        [Parameter(Mandatory = $true)]
        [string] $Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not found: $Path"
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

$ScriptRoot = $PSScriptRoot
$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $ScriptRoot "..")
$configLower = $Config.ToLowerInvariant()

if ($QtRoot.Trim().Length -eq 0) {
    $QtRoot = "D:\CodeData\CADViewer\third_party\install\qt\6.10.2\msvc-$configLower"
}

if ($BuildDir.Trim().Length -eq 0) {
    $BuildDir = Join-Path $ProjectRoot "build\windows-msvc-$configLower"
}

$ToolchainRoot = Resolve-RequiredPath $ToolchainRoot "CADViewer Windows toolchain root"
$QtRoot = Resolve-RequiredPath $QtRoot "Qt root"
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

$msvcInit = Resolve-RequiredPath (Join-Path $ToolchainRoot "msvc-init.ps1") "MSVC init script"
$ninja = Resolve-RequiredPath (Join-Path $ToolchainRoot "ninja\ninja.exe") "Ninja executable"
$windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
$qtBin = Resolve-RequiredPath (Join-Path $QtRoot "bin") "Qt bin directory"

Push-Location $ProjectRoot
try {
    if ($VSInstallDir.Trim().Length -gt 0) {
        . $msvcInit -VSInstallDir $VSInstallDir -Quiet
    }
    else {
        . $msvcInit -Quiet
    }

    $oldPath = $env:PATH
    try {
        $env:PATH = "$qtBin;$(Split-Path -Parent $ninja);$oldPath"

        if (-not $BuildOnly) {
            $configureArgs = @(
                "-S", $ProjectRoot,
                "-B", $BuildDir,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=$Config",
                "-DCMAKE_CXX_COMPILER=cl",
                "-DCMAKE_MAKE_PROGRAM=$ninja",
                "-DCMAKE_PREFIX_PATH=$QtRoot",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
            )

            if (-not $StrictWarnings) {
                $configureArgs += "-DCMAKE_CXX_FLAGS=/EHsc /wd4068 /wd4251 /wd4458 /wd4702 /wd5285"
            }

            if (-not $EnableTokenValidation) {
                $configureArgs += "-DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON"
            }

            Invoke-NativeCommand "cmake" @configureArgs
        }

        if (-not $ConfigureOnly) {
            $buildArgs = @("--build", $BuildDir, "--parallel", "$Jobs")
            if ($Target.Trim().Length -gt 0) {
                $buildArgs += @("--target", $Target)
            }

            Invoke-NativeCommand "cmake" @buildArgs
        }

        $runExe = Join-Path $BuildDir "$RunTarget.exe"
        if (-not (Test-Path -LiteralPath $runExe)) {
            $runExe = Join-Path $BuildDir "bin\$RunTarget.exe"
        }

        if ($DeployQt) {
            if (-not (Test-Path -LiteralPath $runExe)) {
                throw "Executable was not found for deployment: $runExe"
            }

            if (-not (Test-Path -LiteralPath $windeployqt)) {
                throw "windeployqt.exe was not found: $windeployqt"
            }

            if ($DeployDir.Trim().Length -eq 0) {
                $DeployDir = Split-Path -Parent $runExe
            }

            New-Item -ItemType Directory -Force -Path $DeployDir | Out-Null

            $deployArgs = @("--dir", $DeployDir)
            if ($Config -eq "Debug") {
                $deployArgs += "--debug"
            }
            else {
                $deployArgs += "--release"
            }
            $deployArgs += $runExe

            Invoke-NativeCommand $windeployqt @deployArgs
        }

        if ($Run) {
            if (-not (Test-Path -LiteralPath $runExe)) {
                throw "Executable was not found for run: $runExe"
            }

            Push-Location (Split-Path -Parent $runExe)
            try {
                & $runExe
                if ($LASTEXITCODE -ne 0) {
                    throw "$runExe failed with exit code $LASTEXITCODE"
                }
            }
            finally {
                Pop-Location
            }
        }
    }
    finally {
        $env:PATH = $oldPath
    }
}
finally {
    Pop-Location
}
