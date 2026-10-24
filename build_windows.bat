@echo off
REM Build Forge on Windows 10 IoT LTSC 2021 (x64)
setlocal ENABLEDELAYEDEXPANSION

echo === Forge Windows Build ===

REM Try to find vcpkg
if not "%VCPKG_ROOT%"=="" (
    echo [*] Using VCPKG_ROOT: %VCPKG_ROOT%
    set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
) else if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo [*] Found vcpkg at C:\vcpkg
    set VCPKG_ROOT=C:\vcpkg
    set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
) else if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo [*] Found vcpkg at %USERPROFILE%\vcpkg
    set VCPKG_ROOT=%USERPROFILE%\vcpkg
    set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake
) else (
    echo [*] No vcpkg found, building without SDL2 (some features may be limited)
    set TOOLCHAIN=
)

cmake -B build-win -S . -G "Visual Studio 17 2022" -A x64 %TOOLCHAIN% -DCMAKE_BUILD_TYPE=Release -DFORGE_WITH_LSP=ON
if %ERRORLEVEL% NEQ 0 (
    echo [!] CMake configure failed
    echo [!] Make sure Visual Studio 2022 Build Tools with C++ are installed
    echo [!] Run setup_windows_deps.bat as Administrator
    exit /b 1
)

cmake --build build-win --config Release --parallel %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% NEQ 0 (
    echo [!] Build failed
    exit /b 1
)

echo.
echo [✓] Build complete. Binaries in build-win\Release\
echo.
dir /b build-win\Release\forgevm.exe build-win\Release\forge.exe build-win\Release\forge-lsp.exe build-win\Release\forge-decompiler.exe build-win\Release\forge-fclass-decompiler.exe 2>nul
echo.
endlocal
