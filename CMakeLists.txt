# CMakeList.txt : CMake project for COWStrings, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 17)

project ("COWStrings")

# Add source to this project's executable.
add_executable (COWStrings "COWStrings.cpp" "COWStrings.h" "mem.h" "util.h" "forward.h" "test.h" "test.cpp" "string.h" "string.cpp" "character.h" "character.cpp")

# TODO: Add tests and install targets if needed.
