@ECHO OFF
SETLOCAL enabledelayedexpansion

REM Telecharge et extrait libusb (MSVC, binaires 7z) — contourne FetchContent/CMake
REM (SSL via proxy Fortinet + extraction .7z non fiable sous generateur VS).
IF /I [%BUILD_TARGET:~0,4%] NEQ [msvc] (
    ECHO [INFO] Fetchlibusb: skip for BUILD_TARGET=%BUILD_TARGET%
    EXIT /B 0
)

SET LIBUSB_VER=1.0.30
SET DEPS_ROOT=%~dp0..\deps
SET DEPS_DIR=%DEPS_ROOT%\libusb-%LIBUSB_VER%
SET ARCHIVE=%DEPS_ROOT%\libusb-%LIBUSB_VER%.7z
SET LIBUSB_URL=https://github.com/libusb/libusb/releases/download/v%LIBUSB_VER%/libusb-%LIBUSB_VER%.7z

IF EXIST "%DEPS_DIR%\include\libusb.h" GOTO :AlreadyPresent

IF NOT EXIST "%DEPS_ROOT%" MKDIR "%DEPS_ROOT%"

ECHO [INFO] Telechargement libusb %LIBUSB_VER% ...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ProgressPreference='SilentlyContinue'; $p=@{}; if ($env:HTTPS_PROXY) { $p['Proxy']=$env:HTTPS_PROXY } elseif ($env:HTTP_PROXY) { $p['Proxy']=$env:HTTP_PROXY }; try { Invoke-WebRequest -Uri '%LIBUSB_URL%' -OutFile '%ARCHIVE%' @p -UseBasicParsing; exit 0 } catch { Write-Error $_; exit 1 }"
IF ERRORLEVEL 1 (
    ECHO [ERROR] Echec telechargement libusb. Verifier proxy/reseau.
    EXIT /B 1
)

IF EXIST "%ProgramFiles%\7-Zip\7z.exe" (
    SET "SEVENZ=%ProgramFiles%\7-Zip\7z.exe"
) ELSE IF EXIST "%ProgramFiles(x86)%\7-Zip\7z.exe" (
    SET "SEVENZ=%ProgramFiles(x86)%\7-Zip\7z.exe"
) ELSE (
    WHERE 7z >NUL 2>&1
    IF ERRORLEVEL 1 (
        ECHO [ERROR] 7-Zip introuvable pour extraire libusb-%LIBUSB_VER%.7z
        EXIT /B 1
    )
    SET SEVENZ=7z
)

IF EXIST "%DEPS_DIR%" RD /S /Q "%DEPS_DIR%"
MKDIR "%DEPS_DIR%"

ECHO [INFO] Extraction vers %DEPS_DIR% ...
"%SEVENZ%" x "%ARCHIVE%" -o"%DEPS_DIR%" -y >NUL
IF ERRORLEVEL 1 (
    ECHO [ERROR] Extraction 7z echouee
    EXIT /B 1
)

IF NOT EXIST "%DEPS_DIR%\include\libusb.h" (
    ECHO [ERROR] Archive libusb invalide (include\libusb.h absent)
    EXIT /B 1
)

ECHO [INFO] libusb %LIBUSB_VER% pret: %DEPS_DIR%
EXIT /B 0

:AlreadyPresent
ECHO [INFO] libusb %LIBUSB_VER% deja present: %DEPS_DIR%
EXIT /B 0
