@echo off
rem RAG Build Script for Windows
rem Usage: build.bat [Debug|Release]

setlocal

set BUILD_TYPE=%~1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

set BUILD_DIR=build

echo === RAG Build Script ===
echo Build type: %BUILD_TYPE%
echo.

rem Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%~dp0%BUILD_DIR%"

rem Configure CMake
echo === Configuring CMake ===
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DRAG_BUILD_TESTS=ON
if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

rem Build
echo.
echo === Building ===
cmake --build . --parallel
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

rem Run tests
echo.
echo === Running Tests ===
ctest --output-on-failure
if errorlevel 1 (
    echo Tests failed!
    exit /b 1
)

echo.
echo === Build Complete ===
exit /b 0
