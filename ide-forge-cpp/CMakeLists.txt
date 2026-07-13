cmake_minimum_required(VERSION 3.15)
project(forge-studio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(forge-studio
    src/main.cpp
    src/terminal.cpp
    src/syntax.cpp
    src/editor.cpp
    src/file_explorer.cpp
    src/repl.cpp
    src/theme.cpp
)

target_link_libraries(forge-studio PRIVATE Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(forge-studio PRIVATE dl)
endif()

# Install
install(TARGETS forge-studio
    RUNTIME DESTINATION bin
)

# CPack configuration
include(CPack)
set(CPACK_PACKAGE_NAME "forge-studio")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Forge Studio - Terminal IDE for Forge Language")
set(CPACK_PACKAGE_VENDOR "Forge Language Team")
set(CPACK_GENERATOR "TGZ;DEB;RPM")