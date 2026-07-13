cmake_minimum_required(VERSION 3.15)
project(ForgeTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(forge-format format.cpp ../src/lexer.cpp)
target_include_directories(forge-format PRIVATE ../src)

add_executable(forge-lint linter.cpp ../src/lexer.cpp)
target_include_directories(forge-lint PRIVATE ../src)

add_executable(forge-debug forge-debug.cpp ../src/lexer.cpp ../src/parser.cpp ../src/compiler.cpp ../src/value.cpp ../src/chunk.cpp ../src/vm.cpp)
target_include_directories(forge-debug PRIVATE ../src)
if(UNIX AND NOT APPLE)
    target_link_libraries(forge-debug PRIVATE dl)
endif()