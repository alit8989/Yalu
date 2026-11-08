@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo        Building Yalu Programming Language (C)
echo ========================================================

:: Check if clang or gcc is in PATH; if not, check standard install locations
where clang >nul 2>&1
if %ERRORLEVEL% neq 0 (
    where gcc >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        if defined LOCALAPPDATA (
            set "FALLBACK_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin"
            if exist "!FALLBACK_BIN!" (
                echo Adding LLVM-MinGW to PATH...
                set "PATH=!FALLBACK_BIN!;!PATH!"
            )
        )
        if exist "C:\Program Files\LLVM\bin" (
            set "PATH=C:\Program Files\LLVM\bin;!PATH!"
        ) else if exist "C:\msys64\ucrt64\bin" (
            set "PATH=C:\msys64\ucrt64\bin;!PATH!"
        ) else if exist "C:\msys64\mingw64\bin" (
            set "PATH=C:\msys64\mingw64\bin;!PATH!"
        )
    )
)

:: Select compiler
set COMPILER=
where clang >nul 2>&1 && set COMPILER=clang
if not defined COMPILER (
    where gcc >nul 2>&1 && set COMPILER=gcc
)

if not defined COMPILER (
    echo [ERROR] No C compiler clang or gcc found in PATH.
    echo Please install LLVM or MinGW-w64.
    exit /b 1
)

echo Using C compiler: %COMPILER%

set SRC=src\common.c src\errors.c src\value.c src\lexer.c src\ast.c src\parser.c src\runtime.c src\interpreter.c src\main.c

:: Compile Windows resources (Application Icon & Version Info)
set RES_OBJ=
where windres >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Compiling Windows resource script with windres...
    windres -I. -Isrc src\yalu.rc -O coff -o src\yalu_res.o
    if exist src\yalu_res.o (
        set RES_OBJ=src\yalu_res.o
        echo Embedded Windows icon: src\yalu.ico
    )
) else (
    where llvm-rc >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        echo Compiling Windows resource script with llvm-rc...
        llvm-rc /fo src\yalu_res.o src\yalu.rc
        if exist src\yalu_res.o (
            set RES_OBJ=src\yalu_res.o
            echo Embedded Windows icon: src\yalu.ico
        )
    )
)

echo [1/3] Compiling Release build (yalu.exe)...
%COMPILER% -Wall -Wextra -std=c11 -O2 -DNDEBUG %SRC% %RES_OBJ% -o yalu.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to build Release yalu.exe
    exit /b %ERRORLEVEL%
)
echo Successfully built yalu.exe

echo [2/3] Compiling Debug build (yalu_debug.exe)...
%COMPILER% -Wall -Wextra -std=c11 -g -O0 -DYALU_DEBUG %SRC% %RES_OBJ% -o yalu_debug.exe
if %ERRORLEVEL% neq 0 (
    echo [WARNING] Failed to build Debug yalu_debug.exe
) else (
    echo Successfully built yalu_debug.exe
)

echo [3/3] Running Automated Tests...
powershell -ExecutionPolicy Bypass -File tests\run_all.ps1

echo ========================================================
echo Build complete! You can run Yalu with:
echo   yalu.exe examples\hello.yl
echo ========================================================
