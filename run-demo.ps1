param(
    [string]$Demo = "demo_video.lua",
    [switch]$Release
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Config = if ($Release) { "Release" } else { "Debug" }
$ExePath = Join-Path $RepoRoot "build/windows-debug/$Config/bbfx.exe"
if ($Release) {
    $ExePath = Join-Path $RepoRoot "build/windows-release/$Config/bbfx.exe"
}

if (!(Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

$ScriptPath = Join-Path $RepoRoot ("lua/demos/" + $Demo)
if (!(Test-Path $ScriptPath)) {
    throw "Demo script not found: $ScriptPath"
}

Push-Location $RepoRoot
try {
    & $ExePath $ScriptPath
}
finally {
    Pop-Location
}
