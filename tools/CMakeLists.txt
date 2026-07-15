cmake_minimum_required(VERSION 3.15)
project(ForgeTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared sources for tools
set(TOOL_SOURCES ../src/lexer.cpp ../src/parser.cpp ../src/value.cpp ../src/chunk.cpp ../src/compiler.cpp ../src/optimizer.cpp)

add_executable(forge-format format.cpp ${TOOL_SOURCES})
target_include_directories(forge-format PRIVATE ../src)

add_executable(forge-lint linter.cpp ${TOOL_SOURCES})
target_include_directories(forge-lint PRIVATE ../src)

add_executable(forge-debug forge-debug.cpp ${TOOL_SOURCES} ../src/vm.cpp)
target_include_directories(forge-debug PRIVATE ../src)
if(UNIX AND NOT APPLE)
    target_link_libraries(forge-debug PRIVATE dl)
endif()

# LSP Server
add_executable(forge-lsp ../src/lsp/forge_lsp_main.cpp ../src/lsp/forge_lsp.cpp ${TOOL_SOURCES})
target_include_directories(forge-lsp PRIVATE ../src)