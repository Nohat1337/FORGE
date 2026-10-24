# Auto-install Forge build dependencies on Windows 10 IoT LTSC 2021 via winget
# Run as Administrator in PowerShell
param(
    [switch]$SkipVcpkg
)

$ErrorActionPreference = "Continue"

function Install-WingetPkg {
    param([string]$Id, [string]$Name)
    Write-Host "[*] Installing $Name ($Id)..." -ForegroundColor Cyan
    winget install --id $Id --accept-package-agreements --accept-source-agreements --silent 2>&1 | Out-Null
}

Write-Host "=== Forge Windows Dependencies Installer ===" -ForegroundColor Yellow
Write-Host ""

# Core build tools
Install-WingetPkg "Microsoft.VisualStudio.2022.BuildTools" "Visual Studio 2022 Build Tools"
Install-WingetPkg "Kitware.CMake" "CMake"
Install-WingetPkg "Git.Git" "Git"

# Optional but useful
Install-WingetPkg "Python.Python.3.12" "Python 3.12"

Write-Host ""
Write-Host "[*] Checking winget..." -ForegroundColor Cyan
winget --version 2>&1 | Out-Null

if (-not $SkipVcpkg) {
    Write-Host ""
    Write-Host "[*] Setting up vcpkg for SDL2..." -ForegroundColor Cyan
    $vcpkgPath = "C:\vcpkg"
    if (-not (Test-Path $vcpkgPath)) {
        Write-Host "[*] Cloning vcpkg to $vcpkgPath..." -ForegroundColor Cyan
        git clone https://github.com/microsoft/vcpkg.git $vcpkgPath 2>&1 | Out-Null
        & "$vcpkgPath\bootstrap-vcpkg.bat" 2>&1 | Out-Null
    } else {
        Write-Host "[*] vcpkg already exists at $vcpkgPath" -ForegroundColor Green
    }
    
    Write-Host "[*] Installing sdl2:x64-windows (this may take a while)..." -ForegroundColor Cyan
    & "$vcpkgPath\vcpkg.exe" install sdl2:x64-windows 2>&1 | Tail -5
    
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgPath, "User")
    $env:VCPKG_ROOT = $vcpkgPath
    Write-Host "[+] VCPKG_ROOT set to $vcpkgPath" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "Close and reopen terminal, then run build_windows.bat" -ForegroundColor Yellow
