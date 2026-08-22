@echo off
rem Unregister the OpenEloquence SAPI5 engine sitting next to this script.
rem
rem The mirror of install.bat: each word size has to be taken out by the
rem regsvr32 of its own word size, or the 32-bit entries under Wow6432Node
rem are left behind and SAPI goes on offering voices whose DLL is gone.

setlocal
set HERE=%~dp0
set DONE=0
set FAILED=0

reg query "HKU\S-1-5-19" >nul 2>&1
if errorlevel 1 (
    echo This must run as administrator: the registration lives in HKLM.
    echo Right-click uninstall.bat and choose "Run as administrator".
    exit /b 1
)

set REGSVR32=%SystemRoot%\System32\regsvr32.exe
set REGSVR64=%SystemRoot%\System32\regsvr32.exe
if exist "%SystemRoot%\SysWOW64\regsvr32.exe" set REGSVR32=%SystemRoot%\SysWOW64\regsvr32.exe

if exist "%HERE%OpenEloquence.dll" call :unreg "%REGSVR64%" "%HERE%OpenEloquence.dll" 64-bit
if exist "%HERE%OpenEloquence32.dll" call :unreg "%REGSVR32%" "%HERE%OpenEloquence32.dll" 32-bit

if %DONE%==0 (
    echo No engine found next to this script; nothing to unregister.
    echo Expected OpenEloquence.dll or OpenEloquence32.dll in %HERE%
    exit /b 1
)
if not %FAILED%==0 exit /b 1

echo.
echo Done. The "Open Eloquence" voices are gone from SAPI5.
exit /b 0

:unreg
set DONE=1
"%~1" /s /u "%~2"
if errorlevel 1 (
    echo FAILED to unregister %~3 engine: %~2
    set FAILED=1
    goto :eof
)
echo Unregistered %~3 engine: %~2
goto :eof
