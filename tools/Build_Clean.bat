@ECHO OFF
SETLOCAL

REM Effacement des dependances
REM IF NOT EXIST %~dp0../Description.xml GOTO :SkipRecurse
REM IF [%_RECURSE%]==[0] GOTO :SkipRecurse
REM python2.7 "%~dp0Dependencies.py" -d "%~dp0../Description.xml" clean
REM IF ERRORLEVEL 1 GOTO :EOF
REM :SkipRecurse

REM Effacement des répertoire build et dist
IF EXIST "%~dp0..\build" (
ECHO Deleting %CD%\build
RMDIR /Q /S "%~dp0..\build"
)

REM IF EXIST "%~dp0..\dist" (
REM ECHO Deleting %CD%\dist
REM RMDIR /Q /S "%~dp0..\dist"
REM )

REM IF EXIST "%~dp0..\packages" (
REM ECHO Deleting %CD%\packages
REM RMDIR /Q /S "%~dp0..\packages"
REM )

REM IF EXIST "%~dp0..\paket-files" (
REM ECHO Deleting %CD%\paket-files
REM RMDIR /Q /S "%~dp0..\paket-files"
REM )
