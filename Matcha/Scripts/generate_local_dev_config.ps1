[CmdletBinding()]
param(
    [string] $ConfigPath = "config/local-dev.json",
    [string] $CMakeUserPresetsPath = "CMakeUserPresets.json",
    [string] $VSCodeSettingsPath = ".vscode/settings.json",
    [switch] $SkipVSCodeSettings
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ProjectPath {
    param([string] $Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path (Get-Location).Path $Path
}

function Assert-FileExists {
    param(
        [string] $Path,
        [string] $Name
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Name was not found: $Path"
    }
}

function Assert-DirectoryExists {
    param(
        [string] $Path,
        [string] $Name
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Name was not found: $Path"
    }
}

function Get-RequiredString {
    param(
        [object] $Object,
        [string] $Name
    )

    $value = $Object.$Name
    if ($null -eq $value -or [string]::IsNullOrWhiteSpace([string] $value)) {
        throw "Missing required config value: $Name"
    }

    return [string] $value
}

function Add-PathEntry {
    param([string] $Entry)

    if ([string]::IsNullOrWhiteSpace($Entry)) {
        return
    }

    $entries = New-Object "System.Collections.Generic.List[string]"
    $normalized = $Entry.Trim().TrimEnd("\")
    $exists = $false

    foreach ($pathEntry in $env:PATH.Split(";", [System.StringSplitOptions]::RemoveEmptyEntries)) {
        $current = $pathEntry.Trim().TrimEnd("\")
        if ($current.Equals($normalized, [StringComparison]::OrdinalIgnoreCase)) {
            $exists = $true
        }
        $entries.Add($current) | Out-Null
    }

    if (-not $exists) {
        $env:PATH = $normalized + ";" + ($entries -join ";")
    }
}

function Write-JsonFile {
    param(
        [string] $Path,
        [object] $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $json = $Value | ConvertTo-Json -Depth 30
    Set-Content -Encoding UTF8 -LiteralPath $Path -Value $json
}

$resolvedConfigPath = Resolve-ProjectPath $ConfigPath
if (-not (Test-Path -LiteralPath $resolvedConfigPath -PathType Leaf)) {
    $examplePath = Resolve-ProjectPath "config/local-dev.example.json"
    if (Test-Path -LiteralPath $examplePath -PathType Leaf) {
        throw "Local development config was not found: $resolvedConfigPath. Copy config/local-dev.example.json to config/local-dev.json and edit local paths."
    }

    throw "Local development config was not found: $resolvedConfigPath"
}

$config = Get-Content -Encoding UTF8 -LiteralPath $resolvedConfigPath | ConvertFrom-Json

$msvcInitScript = Resolve-ProjectPath (Get-RequiredString -Object $config.toolchain -Name "msvcInitScript")
$arch = Get-RequiredString -Object $config.toolchain -Name "arch"
$ninja = Resolve-ProjectPath (Get-RequiredString -Object $config.tools -Name "ninja")
$qtPrefix = Resolve-ProjectPath (Get-RequiredString -Object $config.thirdParty -Name "qtPrefix")

Assert-FileExists -Path $msvcInitScript -Name "MSVC init script"
Assert-FileExists -Path $ninja -Name "Ninja executable"
Assert-DirectoryExists -Path $qtPrefix -Name "Qt prefix"

$msvcArgs = @{
    Arch = $arch
    Quiet = $true
}

if ($config.toolchain.PSObject.Properties.Name -contains "vsInstallDir") {
    if (-not [string]::IsNullOrWhiteSpace([string] $config.toolchain.vsInstallDir)) {
        $msvcArgs.VSInstallDir = [string] $config.toolchain.vsInstallDir
    }
}

. $msvcInitScript @msvcArgs
Add-PathEntry -Entry (Split-Path -Parent $ninja)

$prefixes = New-Object "System.Collections.Generic.List[string]"
$prefixes.Add($qtPrefix) | Out-Null

if ($config.thirdParty.PSObject.Properties.Name -contains "extraPrefixes") {
    foreach ($prefix in $config.thirdParty.extraPrefixes) {
        if (-not [string]::IsNullOrWhiteSpace([string] $prefix)) {
            $resolvedPrefix = Resolve-ProjectPath ([string] $prefix)
            Assert-DirectoryExists -Path $resolvedPrefix -Name "Extra CMake prefix"
            $prefixes.Add($resolvedPrefix) | Out-Null
        }
    }
}

$prefixPath = $prefixes -join ";"
$configurePreset = Get-RequiredString -Object $config.presets -Name "configurePreset"
$buildPreset = Get-RequiredString -Object $config.presets -Name "buildPreset"
$inherits = Get-RequiredString -Object $config.presets -Name "inherits"
$binaryDir = Get-RequiredString -Object $config.presets -Name "binaryDir"

$environmentNames = @(
    "PATH",
    "INCLUDE",
    "LIB",
    "LIBPATH",
    "VSHomeDir",
    "VSINSTALLDIR",
    "VCToolsInstallDir",
    "VCINSTALLDIR",
    "WindowsSdkDir",
    "WindowsSDKVersion",
    "UniversalCRTSdkDir",
    "UCRTVersion",
    "VisualStudioVersion",
    "VSCMD_ARG_TGT_ARCH",
    "VSCMD_VER"
)

$environment = [ordered] @{}
foreach ($name in $environmentNames) {
    $value = [Environment]::GetEnvironmentVariable($name, "Process")
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $environment[$name] = $value
    }
}

$cmakeUserPresets = [ordered] @{
    version = 6
    cmakeMinimumRequired = [ordered] @{
        major = 3
        minor = 28
        patch = 0
    }
    configurePresets = @(
        [ordered] @{
            name = $configurePreset
            displayName = "Local Windows MSVC Debug"
            inherits = $inherits
            binaryDir = $binaryDir
            cacheVariables = [ordered] @{
                CMAKE_MAKE_PROGRAM = $ninja
                CMAKE_PREFIX_PATH = $prefixPath
            }
            environment = $environment
        }
    )
    buildPresets = @(
        [ordered] @{
            name = $buildPreset
            configurePreset = $configurePreset
            inheritConfigureEnvironment = $true
        }
    )
    testPresets = @(
        [ordered] @{
            name = $buildPreset
            configurePreset = $configurePreset
            inheritConfigureEnvironment = $true
            output = [ordered] @{
                outputOnFailure = $true
            }
            environment = [ordered] @{
                QT_QPA_PLATFORM = "offscreen"
            }
        }
    )
}

Write-JsonFile -Path (Resolve-ProjectPath $CMakeUserPresetsPath) -Value $cmakeUserPresets

$shouldGenerateVSCode = -not $SkipVSCodeSettings
if ($config.PSObject.Properties.Name -contains "vscode") {
    if ($config.vscode.PSObject.Properties.Name -contains "generateSettings") {
        $shouldGenerateVSCode = $shouldGenerateVSCode -and [bool] $config.vscode.generateSettings
    }
}

if ($shouldGenerateVSCode) {
    $configureOnOpen = $false
    if ($config.PSObject.Properties.Name -contains "vscode" -and
        $config.vscode.PSObject.Properties.Name -contains "configureOnOpen") {
        $configureOnOpen = [bool] $config.vscode.configureOnOpen
    }

    $settings = [ordered] @{
        "cmake.useCMakePresets" = "always"
        "cmake.configurePreset" = $configurePreset
        "cmake.buildPreset" = $buildPreset
        "cmake.configureOnOpen" = $configureOnOpen
    }

    Write-JsonFile -Path (Resolve-ProjectPath $VSCodeSettingsPath) -Value $settings
}

Write-Host "Generated $CMakeUserPresetsPath"
if ($shouldGenerateVSCode) {
    Write-Host "Generated $VSCodeSettingsPath"
}
