cmake_minimum_required(VERSION 3.14)
project(forge VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main forge interpreter (direct .fge execution) - uses FVM
add_executable(forge
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/optimizer.cpp
    src/fvm/runtime.cpp
    src/fvm/sdl2_ui.cpp
    src/fvm/sdl_fvm_ui.cpp
    src/pkg_manager.cpp
)

target_include_directories(forge PRIVATE src)
target_compile_definitions(forge PRIVATE FORGE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# FVM - Forge Virtual Machine (production VM with GC, threading, FFI).
# Ships together with its SDL2 UI frontend (src/fvm/sdl_fvm_ui.cpp) so the
# VM and its cross-platform windowing layer are one package -- launch with
# `forgevm --sdl` / `forge --sdl`, exactly like a .jar runs on its JVM.
add_executable(forgevm
    src/forgevm_main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/optimizer.cpp
    src/fvm/runtime.cpp
    src/fvm/sdl2_ui.cpp
    src/fvm/sdl_fvm_ui.cpp
    src/pkg_manager.cpp
    src/forge_build.cpp
)

target_include_directories(forgevm PRIVATE src)
target_compile_definitions(forgevm PRIVATE FVM_BUILD FORGE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

find_package(SDL2 REQUIRED)
target_link_libraries(forge PRIVATE dl ${SDL2_LIBRARIES})
target_link_libraries(forgevm PRIVATE dl pthread ${SDL2_LIBRARIES})
target_include_directories(forgevm PRIVATE ${SDL2_INCLUDE_DIRS})

add_subdirectory(tools)
