@echo off
REM Run as Administrator
NET SESSION >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] Run this as Administrator
    pause
    exit /b 1
)

echo === Forge Windows Dependencies Installer ===
powershell -ExecutionPolicy Bypass -File "%~dp0setup_windows_deps.ps1"
pause
