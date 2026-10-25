@echo off
REM Native Forge Installer for Windows 10/11 x64
REM Installs Visual Studio Build Tools, CMake, Git natively without winget complexity
setlocal ENABLEDELAYEDEXPANSION

title Forge Native Installer
color 0E

echo.
echo  ███████╗ ██████╗ ██████╗  ██████╗ ███████╗
echo  ██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
echo  █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
echo  ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
echo  ██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
echo  ╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
echo.
echo            NATIVE INSTALLER FOR WINDOWS
echo.

NET SESSION >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] This installer requires Administrator privileges
    echo [!] Right-click and select "Run as Administrator"
    pause
    exit /b 1
)

echo [*] Checking system...
systeminfo | findstr /C:"OS Name" /C:"System Type"

echo.
echo [1/4] Downloading Visual Studio 2022 Build Tools...
if not exist "%TEMP%\vs_BuildTools.exe" (
    curl -L -o "%TEMP%\vs_BuildTools.exe" "https://aka.ms/vs/17/release/vs_BuildTools.exe"
)

echo [2/4] Installing MSVC v143 + Windows SDK (native)...
"%TEMP%\vs_BuildTools.exe" --quiet --wait --norestart --nocache ^
    --installPath "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" ^
    --add Microsoft.VisualStudio.Workload.VCTools ^
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    --add Microsoft.VisualStudio.Component.Windows10SDK.19041 ^
    --add Microsoft.VisualStudio.Component.Windows11SDK.22000

echo [3/4] Installing CMake and Git via direct installers...
if not exist "%TEMP%\cmake.msi" (
    curl -L -o "%TEMP%\cmake.msi" "https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-windows-x86_64.msi"
)
msiexec /i "%TEMP%\cmake.msi" /quiet /norestart ADD_CMAKE_TO_PATH=System

if not exist "%TEMP%\Git.exe" (
    curl -L -o "%TEMP%\Git.exe" "https://github.com/git-for-windows/git/releases/download/v2.43.0.windows.1/Git-2.43.0-64-bit.exe"
)
"%TEMP%\Git.exe" /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS="icons,ext\reg\shellhere,assoc,assoc_sh"

echo [4/4] Setting up environment...
setx PATH "%PATH%;C:\Program Files\CMake\bin" /M

echo.
echo [✓] Dependencies installed successfully!
echo.
echo Next steps:
echo 1. Close this terminal
echo 2. Open "Developer Command Prompt for VS 2022" or run build_forge.bat
echo 3. Run: build_forge.bat
echo.
pause
