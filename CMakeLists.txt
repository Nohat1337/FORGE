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
    src/fvm/liquid_glass.cpp
    src/fvm/gui_kit.cpp
    src/fvm/sdl_fvm_ui.cpp
    src/fvm/forge_ide.cpp
    src/fvm/ffi.cpp
    src/fvm/actors.cpp
    src/fvm/llm.cpp
    src/fvm/gguf_loader.cpp
    src/fvm/faw.cpp
    src/fvm/forge_compute.cpp
    src/fvm/hf_converter.cpp
    src/pkg_manager.cpp
    src/java_backend.cpp
    src/lib_cmd.cpp
    src/android_cmd.cpp
)

# Embed the Android project scaffold (gradle/manifest/java templates)
# into the binary so `forgevm android new` needs no external assets.
set(ANDROID_TPL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/android_templates)
file(GLOB ANDROID_TPL_FILES
    ${ANDROID_TPL_DIR}/*.java
    ${ANDROID_TPL_DIR}/*.kt
    ${ANDROID_TPL_DIR}/*.xml
    ${ANDROID_TPL_DIR}/*.kts
    ${ANDROID_TPL_DIR}/*.properties
    ${ANDROID_TPL_DIR}/res/values/*.xml)
foreach(_f ${ANDROID_TPL_FILES})
    file(READ ${_f} _content)
    get_filename_component(_base ${_f} NAME)
    string(MAKE_C_IDENTIFIER ${_base} _ident)
    set(ANDROID_TPL_${_ident} "${_content}")
    # re-run configure when a template changes so the embedded copy stays fresh
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_f})
endforeach()
configure_file(src/android_templates.h.in
    ${CMAKE_BINARY_DIR}/generated/android_templates.h @ONLY)
set_property(SOURCE src/android_cmd.cpp APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/generated/android_templates.h)
include_directories(${CMAKE_BINARY_DIR}/generated)

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

# ---------------------------------------------------------------------------
# forge_core — the Forge runtime as a linkable static library.
#
# Everything except the CLI front-ends, so a Forge program can be linked into
# a native binary or a shared library (forgevm build [--shared]).
# ---------------------------------------------------------------------------
add_library(forge_core STATIC
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/optimizer.cpp
    src/fvm/runtime.cpp
    src/fvm/classfile.cpp
    src/fvm/sdl2_ui.cpp
    src/fvm/liquid_glass.cpp
    src/fvm/gui_kit.cpp
    src/fvm/sdl_fvm_ui.cpp
    src/fvm/forge_ide.cpp
    src/fvm/ffi.cpp
    src/fvm/actors.cpp
    src/fvm/llm.cpp
    src/fvm/gguf_loader.cpp
    src/fvm/faw.cpp
    src/fvm/forge_compute.cpp
    src/fvm/hf_converter.cpp
    src/fvm/forge_export.cpp
)

set_target_properties(forge_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(forge_core PUBLIC src)
target_link_libraries(forge_core PUBLIC dl pthread ${SDL2_LIBRARIES})
target_include_directories(forge_core PUBLIC ${SDL2_INCLUDE_DIRS})

# Flags used by `forgevm build` when it links a generated runner against
# forge_core (baked in so projects build without a CMake project file).
get_target_property(_forge_sdl2_loc SDL2::SDL2 IMPORTED_LOCATION)
if(NOT _forge_sdl2_loc)
    get_target_property(_forge_sdl2_loc SDL2::SDL2 IMPORTED_LOCATION_RELEASE)
endif()
if(_forge_sdl2_loc)
    get_filename_component(_forge_sdl2_dir "${_forge_sdl2_loc}" DIRECTORY)
    set(FORGE_SDL2_LINK_FLAGS "-L${_forge_sdl2_dir} -lSDL2")
else()
    set(FORGE_SDL2_LINK_FLAGS "-lSDL2")
endif()
foreach(_dir ${SDL2_INCLUDE_DIRS})
    set(FORGE_SDL2_INCLUDE_FLAGS "${FORGE_SDL2_INCLUDE_FLAGS} -I${_dir}")
endforeach()

target_compile_definitions(forgevm PRIVATE
    FORGE_CORE_LIB="${CMAKE_BINARY_DIR}/libforge_core.a"
    FORGE_SDL2_LINK_FLAGS="${FORGE_SDL2_LINK_FLAGS}"
    FORGE_SDL2_INCLUDE_FLAGS="${FORGE_SDL2_INCLUDE_FLAGS}")

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
install(TARGETS forge_core ARCHIVE DESTINATION lib)
if(FORGE_WITH_LSP)
    install(TARGETS forge-lsp DESTINATION bin)
endif()