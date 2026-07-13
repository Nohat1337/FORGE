cmake_minimum_required(VERSION 3.14)
project(forge VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(forge
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/value.cpp
    src/chunk.cpp
    src/compiler.cpp
    src/vm.cpp
    src/optimizer.cpp
)

target_include_directories(forge PRIVATE src)

if(UNIX AND NOT APPLE)
    target_link_libraries(forge PRIVATE dl)
endif()

add_subdirectory(tools)
