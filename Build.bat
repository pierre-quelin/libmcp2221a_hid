@ECHO OFF
SETLOCAL enabledelayedexpansion

SET _FETCH=0
SET _GEN=0
SET _CLEAN=0
SET _CHECK=0
SET _RECURSE=1
SET _PAUSE=0
SET "_ROOT=%~dp0"

REM ===================== Select Service =======================================
IF /I NOT [%1]==[] GOTO :NOT_INTERACTIVE
SET _PAUSE=1
ECHO.--------------------
ECHO.-^> Select service ^<-
ECHO.--------------------
ECHO.[ ] - all     (fetch + gen)
ECHO.[1] - fetch   (retrieve all dependencies)
ECHO.[2] - gen     (build the target)
ECHO.[3] - check   (run cppcheck)
ECHO.[9] - clean
ECHO.
ECHO.[0] - Quit
ECHO.
SET /P _USRINPUT=
IF /I [%_USRINPUT%]==[0] (
   ECHO 0 -^> Bye
   GOTO :End
) ELSE IF /I [%_USRINPUT%]==[1] (
   SET _FETCH=1
) ELSE IF /I [%_USRINPUT%]==[2] (
   SET _GEN=1
) ELSE IF /I [%_USRINPUT%]==[3] (
   SET _CHECK=1
) ELSE IF /I [%_USRINPUT%]==[9] (
   SET _CLEAN=1
) ELSE IF /I [%_USRINPUT%]==[all] (
   SET _FETCH=1
   SET _GEN=1
) ELSE IF /I [%_USRINPUT%]==[] (
   SET _FETCH=1
   SET _GEN=1
) ELSE (
   ECHO ERROR : %_USRINPUT% -^> Bad choice -^> Bye
   GOTO :End
)
:NOT_INTERACTIVE

FOR %%a IN (%*) DO (
  IF /I [%%a]==[all] (
    SET _FETCH=1
    SET _GEN=1
  )
  IF /I [%%a]==[clean] SET _CLEAN=1
  IF /I [%%a]==[fetch] SET _FETCH=1
  IF /I [%%a]==[gen] SET _GEN=1
  IF /I [%%a]==[check] SET _CHECK=1
  IF /I [%%a]==[-n] SET _RECURSE=0
)

REM ===================== Clean ================================================
IF [%_CLEAN%]==[1] (
  ECHO [INFO] === clean ===
  IF NOT EXIST "%_ROOT%tools\Build_Clean.bat" (
    ECHO [ERROR] Missing "%_ROOT%tools\Build_Clean.bat"
    GOTO :Fail
  )
  CALL "%_ROOT%tools\Build_Clean.bat"
  IF ERRORLEVEL 1 GOTO :Fail
)

REM ===================== Fetch ================================================
IF [%_FETCH%]==[1] (
  ECHO [INFO] === fetch ===
  IF NOT EXIST "%_ROOT%tools\Build_Fetch.bat" (
    ECHO [ERROR] Missing "%_ROOT%tools\Build_Fetch.bat"
    GOTO :Fail
  )
  CALL "%_ROOT%tools\Build_Fetch.bat"
  IF ERRORLEVEL 1 GOTO :Fail
)

REM ===================== Gen ==================================================
IF [%_GEN%]==[1] (
  ECHO [INFO] === gen ===
  IF NOT EXIST "%_ROOT%tools\Build_Gen.bat" (
    ECHO [ERROR] Missing "%_ROOT%tools\Build_Gen.bat"
    GOTO :Fail
  )
  CALL "%_ROOT%tools\Build_Gen.bat"
  IF ERRORLEVEL 1 GOTO :Fail
)

REM ===================== Check ==================================================
IF [%_CHECK%]==[1] (
  ECHO [INFO] === check ===
  IF NOT EXIST "%_ROOT%tools\Build_Check.bat" (
    ECHO [ERROR] Missing "%_ROOT%tools\Build_Check.bat"
    GOTO :Fail
  )
  CALL "%_ROOT%tools\Build_Check.bat"
  IF ERRORLEVEL 1 GOTO :Fail
)

GOTO :End

:Fail
ECHO [ERROR] Build.bat failed
IF [%_PAUSE%]==[1] PAUSE
EXIT /B 1

:End
IF [%_PAUSE%]==[1] PAUSE
EXIT /B 0
