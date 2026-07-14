cmake_minimum_required(VERSION 3.14)
project(forge VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main forge interpreter (direct .fge execution)
add_executable(forge
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/vm.cpp
    src/optimizer.cpp
    src/pkg_manager.cpp
)

target_include_directories(forge PRIVATE src)

# FVM - Forge Virtual Machine (production VM with GC, threading, FFI)
add_executable(forgevm
    src/forgevm_main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/optimizer.cpp
    src/fvm/runtime.cpp
    src/pkg_manager.cpp
    src/forge_build.cpp
)

target_include_directories(forgevm PRIVATE src)
target_compile_definitions(forgevm PRIVATE FVM_BUILD FORGE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

if(UNIX AND NOT APPLE)
    target_link_libraries(forge PRIVATE dl)
    target_link_libraries(forgevm PRIVATE dl pthread)
elseif(APPLE)
    target_link_libraries(forgevm PRIVATE pthread)
endif()

add_subdirectory(tools)
