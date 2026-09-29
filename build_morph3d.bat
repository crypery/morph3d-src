@echo off
setlocal
cd /d "%~dp0"

set COMPILER=g++

where %COMPILER% >nul 2>nul
if errorlevel 1 (
    echo [ERROR] %COMPILER% not found. Install MinGW-w64 and add its bin folder to PATH.
    exit /b 1
)

where windres >nul 2>nul
if errorlevel 1 (
    echo [ERROR] windres not found. Install MinGW-w64 and add its bin folder to PATH.
    exit /b 1
)

echo [1/2] Compiling resources (icon, version info, manifest)...
windres --output-format=coff morph3d.rc morph3d.o
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

echo [2/2] Compiling and linking with %COMPILER% ...
%COMPILER% -O2 -mwindows -D_CRT_SECURE_NO_WARNINGS -o morph3d.exe morph3d.c morph3d.o -lopengl32 -lgdi32 -lcomctl32
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo Build OK: morph3d.exe (compiler: %COMPILER%)
