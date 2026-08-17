# Specify the MinGW-w64 toolchain root directory
set(TOOLCHAIN_PREFIX "C:/msys64/ucrt64")
message(STATUS "TOOLCHAIN_PREFIX: ${TOOLCHAIN_PREFIX}")
set(TOOLCHAIN_BIN "${TOOLCHAIN_PREFIX}/bin")
set(TOOLCHAIN_INCLUDE "${TOOLCHAIN_PREFIX}/include")
set(TOOLCHAIN_LIB "${TOOLCHAIN_PREFIX}/lib")

# Specify the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify compilers
set(arch x86_64-w64-mingw32)
set(CMAKE_C_COMPILER_TARGET ${arch})
set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/gcc.exe")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
set(CMAKE_CXX_COMPILER_TARGET ${arch})
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/g++.exe")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")

# CACHE FORCE so try_compile sub-builds get the make program (otherwise "Make command was:  -f Makefile")
set(CMAKE_MAKE_PROGRAM "${TOOLCHAIN_BIN}/mingw32-make.exe" CACHE FILEPATH "MinGW make" FORCE)
message(STATUS "CMAKE_MAKE_PROGRAM: ${CMAKE_MAKE_PROGRAM}")

# Specify the search paths for MinGW-w64 libraries and headers
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PREFIX}")
message(STATUS "CMAKE_FIND_ROOT_PATH: ${CMAKE_FIND_ROOT_PATH}")

# Compiler options
set(CMAKE_C_FLAGS "-O0 -g3 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "-O0 -g3 -Wall -Wextra")

# Modes for finding programs, libraries, and includes
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
