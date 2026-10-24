# Build Forge on Windows 10 IoT LTSC 2021 (x64)
param(
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Arch = "x64",
    [switch]$NoLSP
)

$ErrorActionPreference = "Stop"

$cmakeArgs = @("-B", "build-win", "-S", ".", "-G", $Generator, "-A", $Arch, "-DCMAKE_BUILD_TYPE=Release")
if ($env:VCPKG_ROOT) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$($env:VCPKG_ROOT)\scripts\buildsystems\vcpkg.cmake"
    Write-Host "Using vcpkg: $($env:VCPKG_ROOT)"
}
if ($NoLSP) {
    $cmakeArgs += "-DFORGE_WITH_LSP=OFF"
} else {
    $cmakeArgs += "-DFORGE_WITH_LSP=ON"
}

Write-Host "Configuring..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building..."
$jobs = $env:NUMBER_OF_PROCESSORS
& cmake --build build-win --config Release --parallel $jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`nBuild complete. Binaries in build-win\Release\"
Get-ChildItem build-win\Release\*.exe | ForEach-Object { Write-Host "  $($_.Name)" }
Write-Host "`nTools: forgevm, forge, forge-lsp, forge-decompiler, fclass-decompile, fclass-structurer"
Write-Host "VS Code extension: editors\vscode\"
