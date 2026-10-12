# Building ActiveRecord on Windows

> The supported way to build is the [Quick Start](#quick-start-easiest) below (`setup.bat` + `build.bat`, see also the [README](../README.md)).
> The manual options predate the vcpkg submodule and are kept as alternatives; they are not part of the tested workflow.

## Prerequisites
- **Visual Studio 2022** (required for C++23 support)
- CMake 3.20+
- Git

**Why MSVC?** The project uses MSVC on Windows for:
- ✅ Full C++23 support
- ✅ Static linking (no DLL dependencies)
- ✅ Best vcpkg integration
- ✅ Native Windows toolchain

## Quick Start (Easiest!)

The project includes vcpkg as a submodule and automated build scripts:

```cmd
REM Clone with submodules
git clone --recurse-submodules https://github.com/indridib/activerecord.git
cd activerecord

REM Bootstrap vcpkg
setup.bat

REM Build and test
build.bat
```

That's it! The scripts handle everything automatically.

## Manual Build Options

### Option 1: Using vcpkg (Recommended)

### Step 1: Install vcpkg (one-time setup)

```powershell
# In a directory of your choice
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Add vcpkg to PATH (optional, makes it easier to use)
# Or remember the full path: <vcpkg-root>\vcpkg.exe
```

### Step 2: Install SQLite3 via vcpkg

```powershell
# For 64-bit Windows
<vcpkg-root>\vcpkg install sqlite3:x64-windows

# For 32-bit Windows
<vcpkg-root>\vcpkg install sqlite3:x86-windows

# (Optional) For testing
<vcpkg-root>\vcpkg install gtest:x64-windows
```

### Step 3: Build ActiveRecord with vcpkg

```cmd
cd path\to\activerecord

REM Configure with vcpkg toolchain
cmake -B build ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake ^
  -DAR_BUILD_TESTS=ON ^
  -DAR_SQLITE=ON

REM Build
cmake --build build --config Release

REM Run tests
ctest --test-dir build -C Release --output-on-failure

REM Run the SQLite example
.\build\examples\Release\sqlite_example.exe
```

### Option 2: Using vcpkg Manifest Mode (Even Easier!)

With manifest mode, vcpkg automatically installs dependencies when you configure CMake.

### Step 1: Install vcpkg (same as Option 1)

```cmd
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
```

### Step 2: Configure and Build (dependencies install automatically!)

```cmd
cd path\to\activerecord

REM The vcpkg.json in the project tells vcpkg what to install
cmake -B build ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake ^
  -DAR_BUILD_TESTS=ON

REM Build (vcpkg installed dependencies during configure!)
cmake --build build --config Release

REM Run tests
ctest --test-dir build -C Release --output-on-failure
```

### Option 3: Manual SQLite3 Installation (Without vcpkg)

If you prefer not to use vcpkg:

### Step 1: Download SQLite

1. Go to https://www.sqlite.org/download.html
2. Download "sqlite-amalgamation" (source) and "sqlite-dll" (precompiled binaries) for Windows
3. Extract to `<sqlite-dir>`

### Step 2: Create CMake Config

Create `<sqlite-dir>\sqlite3-config.cmake`:

```cmake
set(SQLite3_FOUND TRUE)
set(SQLite3_INCLUDE_DIR "<sqlite-dir>")
set(SQLite3_LIBRARY "<sqlite-dir>/sqlite3.lib")

if(NOT TARGET SQLite::SQLite3)
  add_library(SQLite::SQLite3 UNKNOWN IMPORTED)
  set_target_properties(SQLite::SQLite3 PROPERTIES
    IMPORTED_LOCATION "${SQLite3_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}"
  )
endif()
```

### Step 3: Build with Manual SQLite

```cmd
cd path\to\activerecord

cmake -B build ^
  -DSQLite3_DIR=<sqlite-dir> ^
  -DAR_BUILD_TESTS=ON ^
  -DAR_SQLITE=ON

cmake --build build --config Release
```

## Troubleshooting

### Error: "Could NOT find SQLite3"

**Solution:** Make sure you specified the correct vcpkg toolchain file path:
```powershell
# Check vcpkg location
dir <vcpkg-root>\scripts\buildsystems\vcpkg.cmake

# Use that path in CMAKE_TOOLCHAIN_FILE
```

### Error: "Could NOT find unofficial-sqlite3"

**Solution:** This is normal - it falls back to FindSQLite3. Make sure SQLite3 is installed:
```powershell
<vcpkg-root>\vcpkg list | findstr sqlite
```

If not installed:
```powershell
<vcpkg-root>\vcpkg install sqlite3:x64-windows
```

### GoogleTest Not Found

**Solution:** Install via vcpkg:
```cmd
<vcpkg-root>\vcpkg install gtest:x64-windows
```

Or disable tests:
```cmd
cmake -B build -DAR_BUILD_TESTS=OFF
```

### Build Configuration Issues

If CMake picks the wrong generator, specify explicitly:

```cmd
REM For Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake

REM For MinGW
cmake -B build -G "MinGW Makefiles" ^
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

## Environment Variables (Optional)

To avoid typing the vcpkg path every time, set an environment variable:

**Command Prompt (current session):**
```cmd
set VCPKG_ROOT=<vcpkg-root>

REM Then use:
cmake -B build ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

**Permanent (via System Properties):**
1. Press `Win + R`, type `sysdm.cpl`, press Enter
2. Go to Advanced → Environment Variables
3. Add new User or System variable:
   - Variable name: `VCPKG_ROOT`
   - Variable value: `<vcpkg-root>`

Then in CMakeLists.txt you can use:
```cmake
if(DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
```

## Additional Database Adapters

To build with multiple database adapters:

```cmd
REM Enable MySQL
cmake -B build -DAR_MYSQL=ON
cmake --build build --config Release

REM Enable PostgreSQL
cmake -B build -DAR_POSTGRES=ON
cmake --build build --config Release

REM Enable MS-SQL
cmake -B build -DAR_MSSQL=ON
cmake --build build --config Release

REM Enable multiple at once
cmake -B build -DAR_MYSQL=ON -DAR_POSTGRES=ON -DAR_MSSQL=ON
cmake --build build --config Release
```

See [database_setup.md](database_setup.md) for detailed setup instructions for each database.
