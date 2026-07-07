[CmdletBinding()]
param(
    [string] $VSInstallDir,
    [ValidateSet("x64")]
    [string] $Arch = "x64",
    [switch] $Quiet
)

$script = Join-Path $PSScriptRoot "script\msvc-init-core.ps1"
. $script -VSInstallDir $VSInstallDir -Arch $Arch -Quiet:$Quiet
