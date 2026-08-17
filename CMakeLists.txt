cmake_minimum_required (VERSION 3.25) # Debian Trixie 3.31

project(libmcp2221a_hid VERSION 0.0.1)

# C++17 — public buffers use ConstByteSpan / MutableByteSpan (mcp2221a_hid_types.hpp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_VERBOSE_MAKEFILE 1)

set(MAIN_SRCS
   src/mcp2221a/hid/mcp2221a_hid.cpp
)

set(MAIN_HDRS
   src/mcp2221a/hid/mcp2221a_hid.hpp
   src/mcp2221a/hid/mcp2221a_hid_types.hpp
)

# GUID fixe d'une compilation à une autre
set(${PROJECT_NAME}_GUID_CMAKE "8BC9CEB8-8B4A-11D0-8D11-00A0C56BC942" CACHE INTERNAL "Stored GUID")

# Publication: dist/<cible>/ like Sample (lib/, include/, DLL at root — no bin/)
include(tools/LibDist.cmake)
set(_DIST_ROOT "${MCP2221A_HID_DIST_ROOT}")

################################################################################
# Library build
################################################################################
add_library(${PROJECT_NAME} STATIC ${MAIN_SRCS} ${MAIN_HDRS})

set_source_files_properties(${MAIN_HDRS} PROPERTIES HEADER_FILE_ONLY TRUE)

# Arborescence VS : mcp2221a/hid (aligné sur #include <mcp2221a/hid/...> et src/)
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${MAIN_SRCS}
)
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Header Files"
    FILES ${MAIN_HDRS}
)

# API publique : #include <mcp2221a/hid/mcp2221a_hid.hpp> (aligné sur src/mcp2221a/hid)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

if ($ENV{BUILD_TARGET} MATCHES "linux-x86.*")
    set(CMAKE_C_FLAGS   "-O0 -g3 -Wall -Wextra")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6)
        set(CMAKE_CXX_FLAGS "-O0 -g3 -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wno-overloaded-virtual -Wlogical-op -Wdouble-promotion -Wshadow -Wpedantic -Wmisleading-indentation -Wduplicated-cond -Wnull-dereference ")
    else()
        set(CMAKE_CXX_FLAGS "-O0 -g3 -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wno-overloaded-virtual -Wlogical-op -Wdouble-promotion")
    endif()
elseif ($ENV{BUILD_TARGET} STREQUAL "mingw64")
    # Aligné sur linux-x86 : debug avec -O0 -g3 ; NOMINMAX/NOGDI comme le reste du projet Windows (libusb → windows.h)
    add_compile_definitions(NOMINMAX NOGDI)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0;-g3>
        $<$<CONFIG:Release>:-O3;-DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2;-g;-DNDEBUG>
        $<$<CONFIG:MinSizeRel>:-Os;-DNDEBUG>
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wno-overloaded-virtual
        -Wdouble-promotion
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6)
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wlogical-op -Wshadow -Wpedantic -Wmisleading-indentation -Wduplicated-cond -Wnull-dereference
        )
    endif()
elseif ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+")
    # MinGW/MSVC : NOMINMAX (évite macros min/max), NOGDI (évite wingdi.h et #define ERROR 0)
    add_compile_definitions(NOMINMAX NOGDI)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP") # Enable multi-core compilation
    add_compile_definitions(_CRTDBG_MAP_ALLOC _BIND_TO_CURRENT_CRT_VERSION)
    if($ENV{BUILD_TARGET} STREQUAL "msvc9" OR $ENV{BUILD_TARGET} MATCHES "msvc[0-9]+_xp")
        add_compile_definitions(WINVER=0x0501) # Cible Windows XP sous Visual Studio 2008 ou si on utilise le SDK pour Windows XP
    endif()
    # respect plus strict du standard C++ a partir de Visual Studio 2017 (sauf si on compile pour Windows XP)
    if(NOT (MSVC_VERSION LESS 1910) AND NOT ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+_xp"))
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:
                /std:c++17
                /permissive-
                /Zc:__cplusplus
                /EHsc
                /W4
            >
        )
    endif()
endif()

# TODO - Description.xml Include sub-CMakeFiles of external libraries
include(tools/Fetchlibusb.cmake)

# libusb-1.0 (headers : <libusb-1.0/libusb.h>)
# PUBLIC: static lib — consumers must link libusb-1.0 (symbols not re-exported from .a/.lib alone)
if($ENV{BUILD_TARGET} MATCHES "linux-x86.*")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBUSB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})
elseif($ENV{BUILD_TARGET} STREQUAL "mingw64")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUSB libusb-1.0)
    endif()
    if(LIBUSB_FOUND)
        target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBUSB_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})
    else()
        target_link_libraries(${PROJECT_NAME} PUBLIC usb-1.0)
    endif()
elseif($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+")
    target_link_libraries(${PROJECT_NAME} PUBLIC libusb::libusb)
endif()

install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION ${_DIST_ROOT}/lib
        LIBRARY DESTINATION ${_DIST_ROOT}/lib
        RUNTIME DESTINATION ${_DIST_ROOT})

install(FILES
        src/mcp2221a/hid/mcp2221a_hid.hpp
        src/mcp2221a/hid/mcp2221a_hid_types.hpp
        DESTINATION ${_DIST_ROOT}/include/mcp2221a/hid)

if ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+" OR MSVC)
    if(TARGET libusb::libusb)
        get_target_property(_LIBUSB_RUNTIME_DLL libusb::libusb IMPORTED_LOCATION_RELEASE)
        if(NOT _LIBUSB_RUNTIME_DLL OR _LIBUSB_RUNTIME_DLL MATCHES "NOTFOUND")
            get_target_property(_LIBUSB_RUNTIME_DLL libusb::libusb IMPORTED_LOCATION)
        endif()
        if(_LIBUSB_RUNTIME_DLL AND NOT _LIBUSB_RUNTIME_DLL MATCHES "NOTFOUND" AND EXISTS "${_LIBUSB_RUNTIME_DLL}")
            install(FILES "${_LIBUSB_RUNTIME_DLL}"
                    DESTINATION ${_DIST_ROOT} COMPONENT LIBRARIES)
        endif()
    endif()
elseif($ENV{BUILD_TARGET} STREQUAL "mingw64" OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    find_library(_MCP_LIBUSB_IMPLIB NAMES usb-1.0 libusb-1.0 NO_CMAKE_FIND_ROOT_PATH)
    if(_MCP_LIBUSB_IMPLIB)
        get_filename_component(_libusb_lib_dir "${_MCP_LIBUSB_IMPLIB}" DIRECTORY)
        find_file(_MCP_LIBUSB_RUNTIME_DLL NAMES libusb-1.0.dll
            PATHS "${_libusb_lib_dir}" "${_libusb_lib_dir}/../bin" NO_DEFAULT_PATH)
        if(_MCP_LIBUSB_RUNTIME_DLL)
            install(FILES "${_MCP_LIBUSB_RUNTIME_DLL}"
                    DESTINATION ${_DIST_ROOT} COMPONENT LIBRARIES)
        endif()
    endif()
endif()
