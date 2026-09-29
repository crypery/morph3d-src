@echo off
setlocal
cd /d "%~dp0"

set "INNO_DIR=C:\Program Files\Inno Setup 7"
set "ISCC=%INNO_DIR%\ISCC.exe"

echo [1/2] Preparing screensaver file (morph3d.scr)...
copy /y morph3d.exe morph3d.scr >nul
if errorlevel 1 (
    echo [ERROR] Failed to create morph3d.scr.
    exit /b 1
)

echo [2/2] Building installer with Inno Setup 7...
if not exist "%ISCC%" (
    echo [ERROR] Inno Setup 7 not found at "%INNO_DIR%".
    exit /b 1
)
"%ISCC%" InstallMorph3D.iss
if errorlevel 1 (
    echo [ERROR] Inno Setup compilation failed.
    exit /b 1
)

echo.
echo Build OK: output\InstallMorph3D.exe
endlocal
