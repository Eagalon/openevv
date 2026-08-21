@echo off
rem Uninstall the OpenEloquence SAPI5 engine from where this script sits.
rem Must run elevated: registration lives in HKLM.

setlocal
set HERE=%~dp0

if exist "%HERE%OpenEloquence.dll" (
    regsvr32 /s /u "%HERE%OpenEloquence.dll"
    echo Unregistered 64-bit engine.
)

if exist "%HERE%OpenEloquence32.dll" (
    regsvr32 /s /u "%HERE%OpenEloquence32.dll"
    echo Unregistered 32-bit engine.
)

echo.
echo Done. The "Open Eloquence" voices are gone from SAPI5.
exit /b 0
