[CmdletBinding()]
param(
    [string] $VSInstallDir,
    [ValidateSet("x64")]
    [string] $Arch = "x64",
    [switch] $Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Step {
    param([string] $Message)
    if (-not $Quiet) {
        Write-Host "[msvc] $Message"
    }
}

function Find-VsWhere {
    $candidate = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $candidate) {
        return $candidate
    }
    $command = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    return $null
}

function Find-VisualStudio {
    param([string] $ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path $ExplicitPath)) {
            throw "VS install directory not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $vswhere = Find-VsWhere
    if ($vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($path)) {
            return $path.Trim()
        }
    }

    $fallbacks = @(
        ${env:VSINSTALLDIR},
        ${env:VSHomeDir},
        "C:\Program Files\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
    )

    foreach ($path in $fallbacks) {
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path $path)) {
            return (Resolve-Path $path).Path
        }
    }

    throw "Visual Studio with C++ tools was not found. Install Visual Studio Build Tools or pass -VSInstallDir."
}

function Split-PathList {
    param([string] $PathList)

    if ([string]::IsNullOrWhiteSpace($PathList)) {
        return @()
    }

    return $PathList.Split(";", [System.StringSplitOptions]::RemoveEmptyEntries)
}

function Add-UniquePathEntry {
    param(
        [System.Collections.Generic.List[string]] $Entries,
        [string] $Entry
    )

    if ([string]::IsNullOrWhiteSpace($Entry)) {
        return
    }

    $normalized = $Entry.Trim().TrimEnd("\")
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        return
    }

    foreach ($existing in $Entries) {
        if ($existing.TrimEnd("\").Equals($normalized, [StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }

    $Entries.Add($normalized) | Out-Null
}

function Get-MinimalCmdPath {
    $entries = New-Object "System.Collections.Generic.List[string]"
    $windowsRoot = $env:SystemRoot
    if ([string]::IsNullOrWhiteSpace($windowsRoot)) {
        $windowsRoot = Join-Path $env:SystemDrive "Windows"
    }

    Add-UniquePathEntry -Entries $entries -Entry (Join-Path $windowsRoot "System32")
    Add-UniquePathEntry -Entries $entries -Entry $windowsRoot
    Add-UniquePathEntry -Entries $entries -Entry (Join-Path $windowsRoot "System32\Wbem")
    Add-UniquePathEntry -Entries $entries -Entry (Join-Path $windowsRoot "System32\WindowsPowerShell\v1.0")

    return ($entries -join ";")
}

function Merge-ImportedPath {
    param(
        [string] $ImportedPath,
        [string] $OriginalPath
    )

    $entries = New-Object "System.Collections.Generic.List[string]"
    foreach ($entry in Split-PathList $ImportedPath) {
        Add-UniquePathEntry -Entries $entries -Entry $entry
    }

    $importantCommands = @("cmake.exe", "git.exe")
    foreach ($commandName in $importantCommands) {
        $command = Get-Command $commandName -ErrorAction SilentlyContinue
        if ($command) {
            Add-UniquePathEntry -Entries $entries -Entry (Split-Path -Parent $command.Source)
        }
    }

    $maxPathLength = 7000
    foreach ($entry in Split-PathList $OriginalPath) {
        $candidateEntries = New-Object "System.Collections.Generic.List[string]"
        foreach ($existing in $entries) {
            $candidateEntries.Add($existing) | Out-Null
        }
        Add-UniquePathEntry -Entries $candidateEntries -Entry $entry
        $candidatePath = $candidateEntries -join ";"
        if ($candidatePath.Length -gt $maxPathLength) {
            continue
        }
        $entries = $candidateEntries
    }

    return ($entries -join ";")
}

function Import-BatchEnvironment {
    param(
        [string] $BatchFile,
        [string[]] $Arguments
    )

    if (-not (Test-Path $BatchFile)) {
        throw "Batch file not found: $BatchFile"
    }

    $argLine = ($Arguments | ForEach-Object { '"' + $_ + '"' }) -join " "
    $tempFile = Join-Path ([IO.Path]::GetTempPath()) ("CWB_CAD-msvc-env-" + [Guid]::NewGuid().ToString("N") + ".cmd")
    $script = @(
        "@echo off",
        "call `"$BatchFile`" $argLine >nul",
        "if errorlevel 1 exit /b %errorlevel%",
        "set"
    )
    Set-Content -LiteralPath $tempFile -Value $script -Encoding ASCII
    $originalPath = $env:PATH
    $env:PATH = Get-MinimalCmdPath
    try {
        $lines = & cmd.exe /d /c "`"$tempFile`""
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to initialize MSVC environment with $BatchFile"
        }
    }
    finally {
        Remove-Item -LiteralPath $tempFile -Force -ErrorAction SilentlyContinue
    }

    $environment = New-Object "System.Collections.Generic.Dictionary[string,string]" ([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $lines) {
        $index = $line.IndexOf("=")
        if ($index -le 0) {
            continue
        }
        $name = $line.Substring(0, $index)
        $value = $line.Substring($index + 1)
        if ($environment.ContainsKey($name)) {
            if ($name.Equals("Path", [StringComparison]::OrdinalIgnoreCase) -and
                $environment[$name].Length -gt $value.Length) {
                continue
            }
            $environment[$name] = $value
        } else {
            $environment.Add($name, $value)
        }
    }

    foreach ($item in $environment.GetEnumerator()) {
        if ($item.Key.Equals("Path", [StringComparison]::OrdinalIgnoreCase)) {
            Set-Item -Path ("Env:" + $item.Key) -Value (Merge-ImportedPath -ImportedPath $item.Value -OriginalPath $originalPath)
        } else {
            Set-Item -Path ("Env:" + $item.Key) -Value $item.Value
        }
    }
}

$vsRoot = Find-VisualStudio -ExplicitPath $VSInstallDir
$vsDevCmd = Join-Path $vsRoot "Common7\Tools\VsDevCmd.bat"
$vcvars64 = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"

if (Test-Path $vcvars64) {
    Import-BatchEnvironment -BatchFile $vcvars64 -Arguments @()
} elseif (Test-Path $vsDevCmd) {
    Import-BatchEnvironment -BatchFile $vsDevCmd -Arguments @("-arch=$Arch", "-host_arch=$Arch")
} else {
    throw "Neither VsDevCmd.bat nor vcvars64.bat was found under $vsRoot"
}

$env:VSHomeDir = $vsRoot.TrimEnd("\")
if (-not [string]::IsNullOrWhiteSpace($env:VCToolsInstallDir)) {
    $env:VSVersion = Split-Path $env:VCToolsInstallDir.TrimEnd("\") -Leaf
}

$ninjaDir = Resolve-Path (Join-Path $PSScriptRoot "..\ninja") -ErrorAction SilentlyContinue
if ($ninjaDir -and ($env:PATH -notlike "*" + $ninjaDir.Path + "*")) {
    $env:PATH = $ninjaDir.Path + ";" + $env:PATH
}

Write-Step "VSHomeDir=$env:VSHomeDir"
Write-Step "VSVersion=$env:VSVersion"
Write-Step "VCToolsInstallDir=$env:VCToolsInstallDir"
Write-Step "WindowsSdkDir=$env:WindowsSdkDir"
