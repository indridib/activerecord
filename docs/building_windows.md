# Building ActiveRecord on Windows

## Prerequisites
- **Visual Studio 2022** with the "Desktop development with C++" workload
- CMake 3.20+
- Git

MSVC is the only supported Windows compiler; MinGW is not supported (see [Compilers](compilers.md)).

## Quick Start

The project includes vcpkg as a git submodule:

```cmd
git clone --recurse-submodules https://github.com/indridib/activerecord.git
cd activerecord
setup.bat
build.bat
```

- `setup.bat` initializes the vcpkg submodule if needed and bootstraps vcpkg.
- `build.bat [Release|Debug]` (default Release) configures with Visual Studio 2022 for x64, builds the library, tests and examples with the SQLite adapter, and runs the tests.

Then run the example:

```cmd
.\build\examples\Release\sqlite_example.exe
```

## Building With CMake Directly

`build.bat` is a thin wrapper. The same build by hand:

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64 -DAR_BUILD_TESTS=ON -DAR_SQLITE=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

No toolchain argument is needed: when `CMAKE_TOOLCHAIN_FILE` is not set, `CMakeLists.txt` uses the bundled `vcpkg/scripts/buildsystems/vcpkg.cmake`, and on Windows it selects the `x64-windows-static` triplet, so every dependency is linked statically. vcpkg installs the dependencies listed in `vcpkg.json` during the first configure.

With `AR_BUILD_TESTS=ON`, CMake downloads GoogleTest itself (FetchContent), so the first configure with tests needs network access. The optional `tests` feature in `vcpkg.json` is not used by the build.

## Other Database Adapters

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64 -DAR_BUILD_TESTS=ON -DAR_SQLITE=ON -DAR_MSSQL=ON
```

- **MS-SQL** (`AR_MSSQL=ON`) is tested on Windows. It builds against the Windows ODBC headers. The server/database/user/password constructor connects with Microsoft's "ODBC Driver 17 for SQL Server", which must be installed (`winget install Microsoft.msodbcsql.17`); the connection-string and DSN constructors can use any installed driver. See [Database Setup](database_setup.md).
- **MySQL** (`AR_MYSQL=ON`) and **PostgreSQL** (`AR_POSTGRES=ON`) are tested on Linux only.

See [Database Setup](database_setup.md) for setting up each database server.

## Troubleshooting

- **`vcpkg not found or not bootstrapped`, or the `vcpkg` folder is empty:** the submodule is missing. Run `git submodule update --init --recursive`, then `setup.bat`.
- **Configure picks another generator or compiler:** pass `-G "Visual Studio 17 2022" -A x64` as shown above, and delete `build\CMakeCache.txt` before configuring again.
- **Configure fails while downloading GoogleTest:** check the network connection, or configure with `-DAR_BUILD_TESTS=OFF`.
