cmake_minimum_required(VERSION 3.14)
project(forge-studio VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(forge-studio
    src/main.cpp
    src/terminal.cpp
    src/syntax.cpp
    src/editor.cpp
    src/file_explorer.cpp
    src/repl.cpp
    src/theme.cpp
)

target_include_directories(forge-studio PRIVATE src)
