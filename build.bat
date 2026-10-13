@echo off
REM Build with MSVC for static linking support
REM Usage: build.bat [Release|Debug]

setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo === ActiveRecord Build Script (MSVC) ===
echo.

set PROJECT_ROOT=%~dp0
set VCPKG_PATH=%PROJECT_ROOT%vcpkg

REM Check if vcpkg exists and is bootstrapped
if not exist "%VCPKG_PATH%\vcpkg.exe" (
    echo vcpkg not found or not bootstrapped. Running setup...
    echo.
    call "%PROJECT_ROOT%setup.bat"
    if errorlevel 1 (
        echo ERROR: Setup failed
        exit /b 1
    )
    echo.
)

echo Using project vcpkg at: %VCPKG_PATH%
echo.
echo Note: Dependencies will be statically linked (no DLL issues!)
echo.

REM Delete CMake cache to ensure clean configuration
if exist "build\CMakeCache.txt" (
    echo Cleaning CMake cache for fresh configuration...
    del /Q "build\CMakeCache.txt"
)

REM Configure with MSVC and static libraries
set VCPKG_TOOLCHAIN=%PROJECT_ROOT%vcpkg\scripts\buildsystems\vcpkg.cmake
echo Configuring CMake with MSVC and static linking...
cmake -B build ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN% ^
  -DAR_BUILD_TESTS=ON ^
  -DAR_SQLITE=ON

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build
echo.
echo Building (%BUILD_TYPE%) with MSVC...
cmake --build build --config %BUILD_TYPE%

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    exit /b 1
)

REM Test
echo.
echo Running tests...
ctest --test-dir build -C %BUILD_TYPE% --output-on-failure

if errorlevel 1 (
    echo.
    echo WARNING: Some tests failed!
) else (
    echo.
    echo All tests passed!
)

REM Success!
echo.
echo === Build Complete! ===
echo.
echo Run the SQLite example:
echo   .\build\examples\%BUILD_TYPE%\sqlite_example.exe
echo.
echo Run tests manually:
echo   ctest --test-dir build -C %BUILD_TYPE% -V
echo.
echo Benefits of static linking:
echo   - No DLL dependencies
echo   - Single executable
echo   - More portable
echo.

endlocal
