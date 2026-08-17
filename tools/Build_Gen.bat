@ECHO OFF
SETLOCAL enabledelayedexpansion
SET TARGET=libmcp2221a_hid

REM Construction de l'application
IF NOT EXIST "%~dp0..\build" MKDIR "%~dp0..\build"
IF NOT EXIST "%~dp0..\dist" MKDIR "%~dp0..\dist"

REM DEBUG / RELEASE / DEBUGWITHRELEASEINFO
IF [%BUILD_CONFIG%] == [] (
    REM SET BUILD_CONFIG=Release
    REM SET BUILD_CONFIG=Debug
    REM SET BUILD_CONFIG=DebugWithReleaseInfo
    SET BUILD_CONFIG=Release
    ECHO unspecified BUILD_CONFIG using [!BUILD_CONFIG!]
)

IF [%BUILD_TARGET%] == [] (
    REM SET BUILD_TARGET=msvc16-x86_64
    REM SET BUILD_TARGET=linux-x86_64-trixie
    REM SET BUILD_TARGET=mingw64
    SET BUILD_TARGET=msvc16-x86_64
    ECHO unspecified BUILD_TARGET using [!BUILD_TARGET!]
)

REM Locate cmake.exe (prefer full Program Files path), fallback to 'cmake' in PATH
IF EXIST "%PROGRAMFILES(X86)%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES(X86)%\CMake\bin\cmake.exe"
) ELSE IF EXIST "%PROGRAMFILES%\CMake" (
    SET CMAKE_PROGRAM="%PROGRAMFILES%\CMake\bin\cmake.exe"
) ELSE (
    SET CMAKE_PROGRAM="cmake.exe"
)
ECHO CMAKE_PROGRAM=%CMAKE_PROGRAM%

IF [%BUILD_TARGET%] == [mingw64] (
    REM TODO - Ensure MinGW-w64 is in PATH for CMake to find mingw32-make
    REM CMake Error: CMake was unable to find a build program corresponding to "MinGW Makefiles". 
    REM CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
    SET "PATH=C:/msys64/ucrt64/bin;%PATH%"
    ECHO PATH=!PATH!

    REM toolchain
    SET TOOLCHAIN_FILE="%~dp0toolchain-windows-mingw64-clang.cmake"
    ECHO TOOLCHAIN_FILE=!TOOLCHAIN_FILE!
)

REM Construction de l'application pour msvc
IF /I [%BUILD_TARGET:~0,4%] == [msvc] (

    SET GENERATOR_MAP=msvc9-x86:"Visual Studio 9 2008" -A Win32;msvc10-x86:"Visual Studio 10 2010" -A Win32;msvc16-x86:"Visual Studio 16 2019" -A Win32;msvc16-x86_64:"Visual Studio 16 2019" -A x64
    FOR /F "delims=;" %%a IN ("!GENERATOR_MAP:*%BUILD_TARGET%:=!") DO SET GENERATOR=%%a

    PUSHD "%~dp0..\build"
    IF NOT EXIST "CMakeCache.txt" (
        ECHO %CMAKE_PROGRAM% -G !GENERATOR! ..
        %CMAKE_PROGRAM% -G !GENERATOR! ..
    ) ELSE (
        ECHO [INFO] Using cached dependencies, skipping FetchContent download...
    )
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target %TARGET% --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    %CMAKE_PROGRAM% --build . --target INSTALL --config %BUILD_CONFIG%
    IF ERRORLEVEL 1 (
        POPD
        EXIT /B 1
    )

    POPD
    EXIT /B 0
)

REM Construction de l'application pour mingw64
IF [%BUILD_TARGET%] == [mingw64] (
    PUSHD %~dp0..\build

    IF NOT EXIST "CMakeCache.txt" (
        ECHO %CMAKE_PROGRAM% -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=!TOOLCHAIN_FILE! ..
        %CMAKE_PROGRAM% -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=!TOOLCHAIN_FILE! ..
    ) ELSE (
        REM ECHO [INFO] Using cached dependencies, skipping FetchContent download...
    )
    IF ERRORLEVEL 1 EXIT /B 1
    %CMAKE_PROGRAM% --build . --target %TARGET% --parallel %NUMBER_OF_PROCESSORS%
    IF ERRORLEVEL 1 EXIT /B 1

    %CMAKE_PROGRAM% --install .
    IF ERRORLEVEL 1 EXIT /B 1

    POPD
    EXIT /B 0
)

ECHO Unknown or unspecified BUILD_TARGET = [%BUILD_TARGET%]
EXIT /B 1

