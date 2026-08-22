@echo off
rem Register the OpenEloquence SAPI5 engine from wherever this script sits.
rem
rem Both word sizes matter on a 64-bit Windows: a 32-bit program asking for
rem voices sees only 32-bit engines, so the 32-bit DLL is registered too when
rem it is here, and with the 32-bit regsvr32 -- the 64-bit one refuses it.
rem That also puts its entries under Wow6432Node, which is where a 32-bit
rem host looks for them.

setlocal
set HERE=%~dp0
set DONE=0
set FAILED=0

rem Registration writes to HKLM, so say plainly what is wrong rather than
rem letting eight silent failures look like a success.
reg query "HKU\S-1-5-19" >nul 2>&1
if errorlevel 1 (
    echo This must run as administrator: registration writes to HKLM.
    echo Right-click install.bat and choose "Run as administrator".
    exit /b 1
)

rem SysWOW64 exists only on 64-bit Windows; on a 32-bit one the plain
rem regsvr32 is already the 32-bit one.
set REGSVR32=%SystemRoot%\System32\regsvr32.exe
set REGSVR64=%SystemRoot%\System32\regsvr32.exe
if exist "%SystemRoot%\SysWOW64\regsvr32.exe" set REGSVR32=%SystemRoot%\SysWOW64\regsvr32.exe

if exist "%HERE%OpenEloquence.dll" call :reg "%REGSVR64%" "%HERE%OpenEloquence.dll" 64-bit
if exist "%HERE%OpenEloquence32.dll" call :reg "%REGSVR32%" "%HERE%OpenEloquence32.dll" 32-bit

if %DONE%==0 (
    echo No engine found next to this script.
    echo Expected OpenEloquence.dll or OpenEloquence32.dll in %HERE%
    exit /b 1
)
if not %FAILED%==0 exit /b 1

rem Registering is not the same as being found. Ask the registry whether the
rem voices are actually there before promising they are.
reg query "HKLM\SOFTWARE\Microsoft\Speech\Voices\Tokens\OpenEloquence.1" >nul 2>&1
if errorlevel 1 (
    reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\Speech\Voices\Tokens\OpenEloquence.1" >nul 2>&1
    if errorlevel 1 (
        echo regsvr32 reported success but no voice token was written.
        exit /b 1
    )
)

echo.
echo Done. The eight "Open Eloquence" voices are available to SAPI5 now.
exit /b 0

:reg
set DONE=1
"%~1" /s "%~2"
if errorlevel 1 (
    echo FAILED to register %~3 engine: %~2
    set FAILED=1
    goto :eof
)
echo Registered %~3 engine: %~2
goto :eof
