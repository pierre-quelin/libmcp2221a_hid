# Binaires précompilés MSVC uniquement (mingw64 : package MinGW ; Linux : pkg-config distro)
if(NOT ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+"))
    return()
endif()

set(LIBUSB_VERSION "1.0.30")
set(_libusb_deps_dir "${CMAKE_CURRENT_LIST_DIR}/../deps/libusb-${LIBUSB_VERSION}")

if(NOT EXISTS "${_libusb_deps_dir}/include/libusb.h")
    message(FATAL_ERROR
        "libusb ${LIBUSB_VERSION} introuvable dans ${_libusb_deps_dir}.\n"
        "Exécuter Build.bat fetch (tools/Fetchlibusb_deps.bat) avant gen.")
endif()

set(libusb_SOURCE_DIR "${_libusb_deps_dir}")

# Architecture : MS64 / MS32 (binaires précompilés dans l'archive 7z officielle)
if($ENV{BUILD_TARGET} MATCHES "x86_64|x64")
    set(_libusb_arch MS64)
elseif($ENV{BUILD_TARGET} MATCHES "x86[^_]|Win32|-x86$")
    set(_libusb_arch MS32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_libusb_arch MS64)
else()
    set(_libusb_arch MS32)
endif()

# Dossier toolchain VS (import des .lib/.dll fournis — pas de compilation)
if($ENV{BUILD_TARGET} MATCHES "msvc9|msvc10|msvc11|msvc12")
    set(_libusb_vs VS2013)
elseif($ENV{BUILD_TARGET} MATCHES "msvc14")
    set(_libusb_vs VS2015)
elseif($ENV{BUILD_TARGET} MATCHES "msvc15")
    set(_libusb_vs VS2017)
elseif($ENV{BUILD_TARGET} MATCHES "msvc16")
    set(_libusb_vs VS2019)
elseif($ENV{BUILD_TARGET} MATCHES "msvc17")
    set(_libusb_vs VS2022)
elseif($ENV{BUILD_TARGET} MATCHES "msvc18")
    set(_libusb_vs VS2025)
elseif(MSVC_VERSION LESS 1900)
    set(_libusb_vs VS2013)
elseif(MSVC_VERSION LESS 1910)
    set(_libusb_vs VS2015)
elseif(MSVC_VERSION LESS 1920)
    set(_libusb_vs VS2017)
elseif(MSVC_VERSION LESS 1930)
    set(_libusb_vs VS2019)
elseif(MSVC_VERSION LESS 1940)
    set(_libusb_vs VS2022)
else()
    set(_libusb_vs VS2025)
endif()

set(_libusb_prefix "${libusb_SOURCE_DIR}/${_libusb_vs}/${_libusb_arch}")
get_filename_component(_libusb_implib "${_libusb_prefix}/dll/libusb-1.0.lib" ABSOLUTE)
get_filename_component(_libusb_dll "${_libusb_prefix}/dll/libusb-1.0.dll" ABSOLUTE)

if(NOT EXISTS "${_libusb_implib}")
    message(FATAL_ERROR "libusb prebuilt introuvable pour ${_libusb_vs}/${_libusb_arch}: ${_libusb_implib}")
endif()
if(NOT EXISTS "${_libusb_dll}")
    message(FATAL_ERROR "libusb-1.0.dll introuvable pour ${_libusb_vs}/${_libusb_arch}: ${_libusb_dll}")
endif()

# L'archive fournit include/libusb.h ; le projet inclut <libusb-1.0/libusb.h>
set(_libusb_include_nested "${libusb_SOURCE_DIR}/include/libusb-1.0")
if(NOT EXISTS "${_libusb_include_nested}/libusb.h")
    file(MAKE_DIRECTORY "${_libusb_include_nested}")
    file(COPY "${libusb_SOURCE_DIR}/include/libusb.h" DESTINATION "${_libusb_include_nested}")
endif()

if(NOT TARGET libusb::libusb)
    add_library(libusb::libusb SHARED IMPORTED GLOBAL)
    # Multi-config (VS): set per-config locations so GetPrerequisites / install see absolute paths
    set_target_properties(libusb::libusb PROPERTIES
        IMPORTED_IMPLIB "${_libusb_implib}"
        IMPORTED_LOCATION "${_libusb_dll}"
        IMPORTED_IMPLIB_RELEASE "${_libusb_implib}"
        IMPORTED_LOCATION_RELEASE "${_libusb_dll}"
        IMPORTED_IMPLIB_DEBUG "${_libusb_implib}"
        IMPORTED_LOCATION_DEBUG "${_libusb_dll}"
        IMPORTED_IMPLIB_RELWITHDEBINFO "${_libusb_implib}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${_libusb_dll}"
        IMPORTED_IMPLIB_MINSIZEREL "${_libusb_implib}"
        IMPORTED_LOCATION_MINSIZEREL "${_libusb_dll}"
        INTERFACE_INCLUDE_DIRECTORIES "${libusb_SOURCE_DIR}/include"
    )
endif()

message(STATUS "libusb ${LIBUSB_VERSION} prebuilt (${_libusb_vs}/${_libusb_arch}): ${_libusb_dll}")
