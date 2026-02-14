@echo off
REM Setup script for ActiveRecord C++ project (Windows)
REM This script initializes vcpkg and bootstraps it if needed

echo === ActiveRecord Setup ===
echo.

set PROJECT_ROOT=%~dp0
set VCPKG_PATH=%PROJECT_ROOT%vcpkg

REM Check if vcpkg submodule exists
if not exist "%VCPKG_PATH%\.git" (
    echo Initializing vcpkg submodule...
    git submodule update --init --recursive
    if errorlevel 1 (
        echo ERROR: Failed to initialize vcpkg submodule
        echo Make sure you cloned with: git clone --recurse-submodules
        echo Or run: git submodule update --init --recursive
        exit /b 1
    )
)

REM Check if vcpkg is bootstrapped
if not exist "%VCPKG_PATH%\vcpkg.exe" (
    echo Bootstrapping vcpkg...
    cd "%VCPKG_PATH%"
    call bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo ERROR: vcpkg bootstrap failed
        cd "%PROJECT_ROOT%"
        exit /b 1
    )
    cd "%PROJECT_ROOT%"
    echo vcpkg bootstrapped successfully!
) else (
    echo vcpkg already bootstrapped
)

echo.
echo === Setup Complete! ===
echo.
echo Next steps:
echo   1. Configure: cmake -B build
echo   2. Build:     cmake --build build --config Release
echo   3. Test:      ctest --test-dir build -C Release
echo.
echo Or use the automated build script:
echo   build.bat
echo.
