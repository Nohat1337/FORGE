@echo off
REM Build Forge on Windows 10 IoT LTSC 2021 (x64)
REM Prereqs: Visual Studio 2022 (MSVC), CMake, vcpkg with sdl2 (optional)

setlocal

if "%VCPKG_ROOT%"=="" (
    echo VCPKG_ROOT not set. Using system libraries.
    cmake -B build-win -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DFORGE_WITH_LSP=ON
) else (
    echo Using vcpkg: %VCPKG_ROOT%
    cmake -B build-win -S . -G "Visual Studio 17 2022" -A x64 ^
        -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DFORGE_WITH_LSP=ON
)

if %ERRORLEVEL% NEQ 0 (
    echo CMake configure failed
    exit /b 1
)

cmake --build build-win --config Release --parallel %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo Build failed
    exit /b 1
)

echo.
echo Build complete. Binaries in build-win\Release\
dir build-win\Release\*.exe 2>nul
echo.
echo Decompilers: forge-decompiler.exe, fclass-decompile.exe, fclass-structurer.exe
echo VS Code extension: editors\vscode\
