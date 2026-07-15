cmake_minimum_required(VERSION 3.14)
project(forge VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional features
option(FORGE_WITH_LLVM "Enable LLVM JIT compiler" OFF)
option(FORGE_WITH_LSP "Enable Language Server Protocol" ON)
option(FORGE_WASM "Build for WebAssembly (Emscripten)" OFF)
option(FORGE_WITH_ROCM "Enable AMD ROCm/HIP support" ON)
option(FORGE_WITH_CUDA "Enable NVIDIA CUDA support" OFF)

# Find ROCm/HIP if requested
if(FORGE_WITH_ROCM)
    find_package(HIP QUIET)
    if(HIP_FOUND)
        message(STATUS "Found HIP: ${HIP_VERSION}")
        add_definitions(-DFORGE_HAS_HIP)
    else()
        message(STATUS "HIP not found, will use CPU fallback")
    endif()
endif()

# Find CUDA if requested
if(FORGE_WITH_CUDA)
    find_package(CUDA QUIET)
    if(CUDA_FOUND)
        message(STATUS "Found CUDA: ${CUDA_VERSION}")
        add_definitions(-DFORGE_HAS_CUDA)
    endif()
endif()

# Find LLVM if requested
if(FORGE_WITH_LLVM)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM: ${LLVM_VERSION}")
    set(LLVM_DEFINITIONS ${LLVM_DEFINITIONS})
    include_directories(${LLVM_INCLUDE_DIRS})
    add_compile_options(${LLVM_COMPILE_OPTIONS})
    add_definitions(-DFORGE_HAS_LLVM)
endif()

# Find Emscripten for WASM
if(FORGE_WASM)
    find_package(Emscripten REQUIRED)
    add_definitions(-DFORGE_WASM)
endif()

# Common sources
set(COMMON_SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/optimizer.cpp
    src/fvm/runtime.cpp
    src/fvm/classfile.cpp
    src/fvm/sdl2_ui.cpp
    src/fvm/sdl_fvm_ui.cpp
    src/fvm/forge_ide.cpp
    src/fvm/ffi.cpp
    src/fvm/actors.cpp
    src/fvm/llm.cpp
    src/fvm/forge_compute.cpp
    src/pkg_manager.cpp
)

# Main forge interpreter (direct .fge execution) - uses FVM
add_executable(forge
    src/main.cpp
    ${COMMON_SOURCES}
)

target_include_directories(forge PRIVATE src)
target_compile_definitions(forge PRIVATE FORGE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# FVM - Forge Virtual Machine (production VM with GC, threading, FFI).
add_executable(forgevm
    src/forgevm_main.cpp
    ${COMMON_SOURCES}
    src/forge_build.cpp
)

target_include_directories(forgevm PRIVATE src)
target_compile_definitions(forgevm PRIVATE FVM_BUILD FORGE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# LSP Server is built in tools/

find_package(SDL2 REQUIRED)

# Link libraries
target_link_libraries(forge PRIVATE dl ${SDL2_LIBRARIES})
target_link_libraries(forgevm PRIVATE dl pthread ${SDL2_LIBRARIES})
target_include_directories(forgevm PRIVATE ${SDL2_INCLUDE_DIRS})

# ROCm/HIP linking
if(FORGE_WITH_ROCM AND HIP_FOUND)
    target_link_libraries(forge PRIVATE ${HIP_LIBRARIES})
    target_link_libraries(forgevm PRIVATE ${HIP_LIBRARIES})
    target_include_directories(forge PRIVATE ${HIP_INCLUDE_DIRS})
    target_include_directories(forgevm PRIVATE ${HIP_INCLUDE_DIRS})
    add_definitions(-DFORGE_HAS_HIP)
endif()

# CUDA linking
if(FORGE_WITH_CUDA AND CUDA_FOUND)
    target_link_libraries(forge PRIVATE ${CUDA_LIBRARIES})
    target_link_libraries(forgevm PRIVATE ${CUDA_LIBRARIES})
    add_definitions(-DFORGE_HAS_CUDA)
endif()

# LLVM linking for JIT
if(FORGE_WITH_LLVM)
    target_link_libraries(forge PRIVATE ${LLVM_LIBRARIES})
    target_link_libraries(forgevm PRIVATE ${LLVM_LIBRARIES})
    target_link_libraries(forge-lsp PRIVATE ${LLVM_LIBRARIES})
    # Link LLVM JIT components
    target_link_libraries(forge PRIVATE LLVMJIT LLVMExecutionEngine LLVMOrcJIT LLVMOrcShared)
    target_link_libraries(forgevm PRIVATE LLVMJIT LLVMExecutionEngine LLVMOrcJIT LLVMOrcShared)
endif()

# Emscripten settings
if(FORGE_WASM)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    target_link_options(forge PRIVATE -s WASM=1 -s MODULARIZE=1 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]')
    target_link_options(forgevm PRIVATE -s WASM=1 -s MODULARIZE=1 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]')
endif()

# Tools subdirectory
add_subdirectory(tools)

# Install targets
install(TARGETS forge forgevm DESTINATION bin)
if(FORGE_WITH_LSP)
    install(TARGETS forge-lsp DESTINATION bin)
endif()