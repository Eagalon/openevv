@echo off
rem Install the OpenEloquence SAPI5 engine from where this script sits.
rem Must run elevated: registration writes to HKLM.

setlocal
set HERE=%~dp0

if exist "%HERE%OpenEloquence.dll" (
    regsvr32 /s "%HERE%OpenEloquence.dll"
    if errorlevel 1 goto failed
    echo Registered 64-bit engine: %HERE%OpenEloquence.dll
)

if exist "%HERE%OpenEloquence32.dll" (
    regsvr32 /s "%HERE%OpenEloquence32.dll"
    if errorlevel 1 goto failed
    echo Registered 32-bit engine: %HERE%OpenEloquence32.dll
)

echo.
echo Done. The "Open Eloquence" voices are available to SAPI5 now.
exit /b 0

:failed
echo Registration failed. Run this script as administrator.
exit /b 1
