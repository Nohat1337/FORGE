@echo off
REM Build Forge natively on Windows - no vcpkg required
setlocal ENABLEDELAYEDEXPANSION

title Forge Builder
color 0B

echo.
echo  ███████╗ ██████╗ ██████╗  ██████╗ ███████╗
echo  ██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
echo  █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
echo  ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
echo  ██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
echo  ╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝
echo.
echo                BUILDING ALL TOOLS
echo.

REM Setup VS environment
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo [!] Visual Studio Build Tools not found
    echo [!] Run install_forge_windows.bat as Administrator first
    pause
    exit /b 1
)

echo [*] Configuring with CMake (native, no vcpkg)...
cmake -B build-win -S . -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DFORGE_WITH_LSP=ON 2>nul
if %ERRORLEVEL% NEQ 0 (
    cmake -B build-win -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DFORGE_WITH_LSP=ON
)

if %ERRORLEVEL% NEQ 0 (
    echo [!] CMake configuration failed
    pause
    exit /b 1
)

echo [*] Building forge, forgevm, forge-lsp, decompilers, c2forge, forge2c...
cmake --build build-win --config Release --parallel %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo [!] Build failed
    pause
    exit /b 1
)

echo.
echo [✓] BUILD COMPLETE!
echo.
echo Binaries in: build-win\Release\
echo.
dir /b build-win\Release\forge.exe build-win\Release\forgevm.exe build-win\Release\forge-lsp.exe build-win\Release\forge-decompiler.exe build-win\Release\forge-fclass-decompiler.exe build-win\Release\c2forge.exe build-win\Release\forge2c.exe 2>nul
echo.
echo Usage:
echo   forgevm.exe - Run FVM
echo   forge.exe    - Run Forge interpreter  
echo   c2forge.exe  - Translate C/C++ to Forge
echo   forge2c.exe  - Translate Forge to C/C++
echo.
pause
endlocal
