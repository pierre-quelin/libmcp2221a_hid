@ECHO OFF
SETLOCAL enabledelayedexpansion

REM Aligné sur Build_Gen.bat : défaut MSVC si BUILD_TARGET non défini
IF [%BUILD_TARGET%] == [] (
    SET BUILD_TARGET=msvc16-x86_64
    ECHO unspecified BUILD_TARGET using !BUILD_TARGET!
)

REM Prefer Description.xml when Python + Dependencies.py are available
IF EXIST "%~dp0..\Description.xml" (
    SET "_DEPS_PY="
    WHERE python >NUL 2>&1
    IF NOT ERRORLEVEL 1 (
        SET "_DEPS_PY=python"
    ) ELSE (
        WHERE py >NUL 2>&1
        IF NOT ERRORLEVEL 1 SET "_DEPS_PY=py -3"
    )
    SET "_DEPS_SCRIPT="
    IF EXIST "%~dp0Dependencies.py" SET "_DEPS_SCRIPT=%~dp0Dependencies.py"
    IF "!_DEPS_SCRIPT!"=="" IF EXIST "%~dp0..\..\Foundation\tools\Dependencies.py" SET "_DEPS_SCRIPT=%~dp0..\..\Foundation\tools\Dependencies.py"
    IF "!_DEPS_SCRIPT!"=="" IF EXIST "%~dp0..\..\tools\Dependencies.py" SET "_DEPS_SCRIPT=%~dp0..\..\tools\Dependencies.py"

    IF NOT "!_DEPS_PY!"=="" IF NOT "!_DEPS_SCRIPT!"=="" (
        ECHO [INFO] Description fetch via !_DEPS_SCRIPT!
        !_DEPS_PY! "!_DEPS_SCRIPT!" -d "%~dp0..\Description.xml" fetch
        IF NOT ERRORLEVEL 1 EXIT /B 0
        ECHO [WARN] Description fetch failed — fallback Fetchlibusb_deps.bat
    )
)

CALL "%~dp0Fetchlibusb_deps.bat"
IF ERRORLEVEL 1 EXIT /B 1
EXIT /B 0
