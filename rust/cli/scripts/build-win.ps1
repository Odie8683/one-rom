#!/usr/bin/env pwsh
#Requires -Version 5.0

# Builds One ROM CLI for Windows (x86_64 and arm64).
#
# Pre-requisites:
# - Rust:
#
# ```powershell
#   Invoke-WebRequest -Uri https://win.rustup.rs/ -OutFile rustup-init.exe
#   .\rustup-init.exe -y
# ```
#
# Note it is strongly recommended that you run this script from a Developer
# PowerShell prompt (e.g., "x64 Native Tools Command Prompt for VS 2022") so
# the various build tools are in your PATH.
#
# Note that signing DOES NOT WORK on arm64 Windows due to Certum not providing
# an arm64 minidriver.  https://deciphertools.com/blog/yubikey-5-parallels-arm
#
# The `sign-win.ps1` instead uses https://github.com/piersfinlayson/certum-code-signer.git
# a remote signing service for Certum certificates running on Intel Linux.
#
# When running with signing for the first time, you must first install the
# signing server certificate by running:
#
#   ..\studio\scripts\install-signing-cert.ps1
#
# This only needs to be done once per machine/user.

$ErrorActionPreference = "Stop"

# Parse command line arguments
$NoSign = $args -contains "nosign"
$NoDeps = $args -contains "nodeps"
$NoClean = $args -contains "noclean"

# Extract PIN from arguments (format: pin=VALUE)
$Pin = $null
foreach ($arg in $args) {
    if ($arg -like "pin=*") {
        $Pin = $arg.Substring(4)
    }
}

# Check for unexpected arguments
$ValidArgs = @("nosign", "nodeps", "noclean")
foreach ($arg in $args) {
    if ($arg -notin $ValidArgs -and $arg -notlike "pin=*") {
        Write-Error "Unknown argument: $arg. Valid arguments are: $($ValidArgs -join ', '), pin=VALUE"
        exit 1
    }
}

# Validate PIN if signing
if (-not $NoSign -and -not $Pin) {
    Write-Error "PIN required for signing. Use: pin=SMARTCARD_PIN"
    exit 1
}

# Log args
if ($NoSign) {
    Write-Host "!!!WARNING: Code signing disabled"
}
if ($NoDeps) {
    Write-Host "!!!WARNING: Dependency installation disabled"
}
if ($NoClean) {
    Write-Host "!!!WARNING: Clean disabled"
}

$Targets = @("x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc")

#
# Setup
#

# Extract version from Cargo.toml
$Version = (Get-Content "Cargo.toml" | Select-String -Pattern '^version\s*=\s*"([^"]+)"').Matches.Groups[1].Value
Write-Host "Building version: $Version"

if (-not $NoDeps) {
    foreach ($Target in $Targets) {
        rustup target add $Target
    }
}

#
# Clean previous builds
#

if (-not $NoClean) {
    foreach ($Target in $Targets) {
        cargo clean --target $Target
    }
    Remove-Item -Path "dist\*.exe" -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path dist | Out-Null

#
# Build for each target
#

foreach ($Target in $Targets) {
    Write-Host "`n=== Building for $Target ===`n"

    cargo build --bin onerom --release --target $Target | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Cargo build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    if (-not $NoSign) {
        Write-Host "Signing executable..."
        & "..\studio\scripts\sign-win.ps1" "..\target\$Target\release\onerom.exe" $Pin
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Signing failed with exit code $LASTEXITCODE"
            exit $LASTEXITCODE
        }
    }

    $Arch = if ($Target -eq "x86_64-pc-windows-msvc") { "x86_64" } else { "arm64" }
    $ZipPath = "dist\onerom-cli-win-${Version}-${Arch}.zip"
    Compress-Archive -Path "..\target\$Target\release\onerom.exe" -DestinationPath $ZipPath -Force
    Write-Host "Created: $ZipPath"
}

Write-Host "`nWindows builds complete."
Write-Host "`nArtifacts in dist\:"
Get-ChildItem dist\*.zip | Format-Table Name, Length