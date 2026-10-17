# Hardening Runbook

Recurring checks that keep the codebase warning-free and free of memory and undefined-behavior errors. Run them all before a release and after any change to build flags, compilers, or low-level code (adapters, `attribute`, `datetime`).

This implements the project rule "We don't like warnings in our compiled code" ([AGENTS.md](../AGENTS.md#core-principle-simplicity-first)). Compiler warnings are errors in every build (`/WX`, `-Werror` in [cmake/Warnings.cmake](../cmake/Warnings.cmake)), and so are clang-tidy findings (`WarningsAsErrors: '*'` in [.clang-tidy](../.clang-tidy)). A new warning fails the build instead of accumulating.

## Checks

| # | Check | Platform | Clean means |
|---|-------|----------|-------------|
| 1 | MSVC Release + Debug | Windows | Builds (warnings are errors), `ctest` 100% |
| 2 | GCC 14 Release | WSL / Linux | Builds (warnings are errors), `ctest` 100% |
| 3 | Clang Release | WSL / Linux | Builds (warnings are errors), `ctest` 100% |
| 4 | clang-tidy | WSL / Linux | Builds (findings are errors); log contains no `warning:` lines |
| 5 | ASan + UBSan | WSL / Linux | `ctest` 100%, no sanitizer report in the output |
| 6 | MySQL + PostgreSQL adapter runs | WSL / Linux | Adapter tests ran against the real servers; only documented waivers skipped |
| 7 | MS-SQL (MSVC Release + Debug) | Windows | Builds (warnings are errors), `ctest` 100%, MS-SQL tests ran; only documented waivers skipped |
| 8 | Installed package | WSL / Linux and Windows | `find_package(activerecord)` works from an install: the in-repo consumer configures, builds (warnings are errors) and runs; the config requires only the enabled adapters' packages |

Checks 2-5 build with the MySQL and PostgreSQL adapters enabled, so they need the WSL database servers below. SQLite is always on. Do not run the Windows and WSL checks at the same time: both use the repo's `vcpkg/`, which allows one install at a time (the second waits on `vcpkg/.vcpkg-root`).

## Prerequisites

**Windows:** Visual Studio 2022 17.9+ with the C++ workload, and Python 3 for the check-7 script. `build.bat` bootstraps vcpkg on first run.

**WSL / Linux:** Ubuntu 24.04 (older releases ship compilers too old for C++23):

```bash
# libtirpc-dev ... libtool: vcpkg needs them to build libmysql / libpq; unixodbc-dev: ODBC headers for check 4
sudo apt update && sudo apt install -y build-essential g++-14 clang clang-tidy libclang-rt-18-dev \
  cmake ninja-build pkg-config zip unzip curl git python3 \
  libtirpc-dev bison flex autoconf autoconf-archive automake libtool unixodbc-dev
cd vcpkg && ./bootstrap-vcpkg.sh -disableMetrics && cd ..   # from the repo root, once; coexists with vcpkg.exe
```

Keep WSL build directories on the Linux filesystem (`~/ar-build/...` below; any directory there works). Building from `/mnt/c` works but is several times slower. For the same reason, and because renaming directories on `/mnt/c` can fail with `Permission denied`, vcpkg's buildtrees and packages go under `~/ar-build` (see `VCPKG_OPTS` below). The first adapter-enabled configure builds `libmysql` and `libpq` with their dependencies from source (95 ports, including Boost); later configures reuse the cache.

**WSL database servers** (once). Enable systemd so the servers start with WSL: put `[boot]` / `systemd=true` in `/etc/wsl.conf`, run `wsl --shutdown` from Windows, reopen Ubuntu. Then:

```bash
sudo apt install -y mysql-server postgresql
sudo mysql -e "CREATE DATABASE test_db;
  CREATE USER 'test'@'localhost' IDENTIFIED BY 'test'; CREATE USER 'test'@'127.0.0.1' IDENTIFIED BY 'test';
  GRANT ALL ON test_db.* TO 'test'@'localhost'; GRANT ALL ON test_db.* TO 'test'@'127.0.0.1';"
sudo -u postgres psql -c "CREATE ROLE test LOGIN PASSWORD 'test';" -c "CREATE DATABASE test_db OWNER test;"
systemctl is-enabled mysql postgresql            # enabled, enabled
systemctl is-active mysql postgresql@16-main     # active, active
```

Check PostgreSQL through the cluster unit `postgresql@16-main`: the `postgresql` umbrella unit reports active even when the cluster is down. Both servers listen on `127.0.0.1` only, so the `test`/`test` login is not reachable from other machines. The MySQL tests connect to `127.0.0.1` (TCP) rather than `localhost`, which would use a Unix socket whose default path depends on how `libmysql` was built.

**Windows SQL Server** (once, elevated PowerShell). SQL Server 2025 Developer: the 2022 winget package points at a retired installer. `--interactive` is needed on systems whose Windows language SQL Server does not support: setup asks "continue in English?" and exits with 1009 when run silently.

```powershell
winget install Microsoft.SQLServer.2025.Developer --interactive   # Basic install, default instance MSSQLSERVER
winget install Microsoft.msodbcsql.17                             # the adapter hardcodes "ODBC Driver 17 for SQL Server"
winget install Microsoft.Sqlcmd
# Basic install allows Windows logins only and has TCP off; enable SQL logins and TCP (port 1433 is preset):
$base = 'HKLM:\SOFTWARE\Microsoft\Microsoft SQL Server\MSSQL17.MSSQLSERVER\MSSQLServer'
Set-ItemProperty $base -Name LoginMode -Value 2
Set-ItemProperty "$base\SuperSocketNetLib\Tcp" -Name Enabled -Value 1
Restart-Service MSSQLSERVER -Force
# Open a new shell first, or call "C:\Program Files\SqlCmd\sqlcmd.exe". -C accepts the self-signed certificate.
sqlcmd -S localhost -E -C -Q "CREATE LOGIN test WITH PASSWORD = 'test', CHECK_POLICY = OFF; CREATE DATABASE test_db; ALTER AUTHORIZATION ON DATABASE::test_db TO test;"
sqlcmd -S localhost,1433 -U test -P test -d test_db -C -Q "SELECT 1"
```

`CHECK_POLICY = OFF` is required because Windows password policy rejects `test`. The server listens on all interfaces; Windows Firewall blocks port 1433 from other machines unless a rule allows it.

## Commands

### 1. MSVC (Windows, from the repo root)

```bat
build.bat Release
build.bat Debug
```

Debug matters: MSVC reports some warnings (e.g. C4702 unreachable code) only in Debug.

### Shared WSL setup (checks 2-6)

```bash
R=$(git rev-parse --show-toplevel)                         # run inside the clone (a Windows clone under /mnt/c works)
TC=$R/vcpkg/scripts/buildsystems/vcpkg.cmake
VCPKG_OPTS="-DVCPKG_INSTALL_OPTIONS=--x-buildtrees-root=$HOME/ar-build/vcpkg-buildtrees;--x-packages-root=$HOME/ar-build/vcpkg-packages"
COMMON="-G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC -DAR_BUILD_TESTS=ON -DAR_BUILD_EXAMPLES=ON -DAR_SQLITE=ON -DAR_MYSQL=ON -DAR_POSTGRES=ON"
set -o pipefail                                            # a failed build piped into tee must still fail
```

Every `cmake -S` below also passes `"$VCPKG_OPTS"`, quoted because it contains a `;`.

### 2. GCC 14 Release

```bash
cmake -S $R -B ~/ar-build/gcc $COMMON "$VCPKG_OPTS" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build ~/ar-build/gcc && ctest --test-dir ~/ar-build/gcc --output-on-failure
```

### 3. Clang Release

```bash
cmake -S $R -B ~/ar-build/clang $COMMON "$VCPKG_OPTS" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build ~/ar-build/clang && ctest --test-dir ~/ar-build/clang --output-on-failure
```

### 4. clang-tidy

```bash
cmake -S $R -B ~/ar-build/tidy $COMMON "$VCPKG_OPTS" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAR_CLANG_TIDY=ON -DAR_MSSQL=ON
cmake --build ~/ar-build/tidy --clean-first 2>&1 | tee ~/ar-build/tidy.log
! grep -n "warning:" ~/ar-build/tidy.log    # must print nothing; exit status 0 means clean
```

clang-tidy runs inline during compilation, and any finding fails the build. `set -o pipefail` (in the shared setup) keeps that failure visible through `tee`. The `grep` is a backstop for findings from checks that are not promoted to errors: it lists any it finds, and its exit status is 0 only when there are none. `--clean-first` makes sure every file is re-analyzed.

Only this check adds `-DAR_MSSQL=ON`: it compiles the MS-SQL adapter, its example and the `AR_MSSQL` test code against unixODBC so clang-tidy analyzes them, and it never runs tests. Checks 2, 3 and 5 run `ctest` and leave MS-SQL out, because no SQL Server is reachable from WSL; MS-SQL behavior is tested on Windows (check 7).

Two limitations:
- It only works with Ninja or Makefile generators. CMake's Visual Studio generator silently ignores `CMAKE_CXX_CLANG_TIDY`, so there is no Windows equivalent of this check.
- It sees the Linux preprocessor view, so code under `#ifdef _WIN32` / `_MSC_VER` is not analyzed.

### 5. ASan + UBSan

```bash
cmake -S $R -B ~/ar-build/san $COMMON "$VCPKG_OPTS" -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DAR_SANITIZE_ADDRESS=ON -DAR_SANITIZE_UNDEFINED=ON
cmake --build ~/ar-build/san
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
ctest --test-dir ~/ar-build/san --output-on-failure
```

The options are required. By default UBSan prints a report and keeps running, so `ctest` would pass despite undefined behavior. `halt_on_error=1` makes any report fail the test. The library, tests and examples are all instrumented; the vcpkg-built `libmysql` and `libpq` are not (ASan still intercepts their allocations). A report from inside them is still a failure: first find out whether project code caused it.

### 6. MySQL + PostgreSQL adapter runs (after check 3)

`ctest` passing is not enough, because it would also pass if the adapter tests were skipped. An adapter compiled into the build that cannot connect fails its tests; this check confirms the tests actually ran:

```bash
T=~/ar-build/clang/tests/integration_tests; failed=0
for A in MySQL PostgreSQL; do
  "$T" --gtest_filter="AllAdapters/*/$A" --gtest_output=xml:$HOME/ar-build/$A.xml &&
  python3 $R/tools/check_adapter_tests.py ~/ar-build/$A.xml $R/docs/hardening.md || { echo "FAILED: $A"; failed=1; }
done
test $failed -eq 0
```

Each adapter must print `OK: <n> test case(s) completed, no unwaived skips`; the block exits 0 only if both did.

**Outage proof** (occasionally, and after changing connection or test setup code; needs `sudo`). It stops a server, requires the tests to fail because of the connection, and always restarts the server:

```bash
( T=~/ar-build/clang/tests/integration_tests; test -x "$T" || exit 1
  trap 'sudo systemctl start mysql' EXIT
  sudo systemctl stop mysql
  "$T" --gtest_filter="AllAdapters/*/MySQL" > ~/ar-build/outage.log 2>&1; rc=$?
  test $rc -ne 0 && grep -q "Cannot connect to MySQL" ~/ar-build/outage.log && grep -q "\[  PASSED  \] 0 tests" ~/ar-build/outage.log
) && echo "outage OK" || echo "outage check FAILED"
```

For PostgreSQL use the unit `postgresql@16-main` and `PostgreSQL` in the filter and message.

### 7. MS-SQL (Windows PowerShell, from the repo root)

`build.bat` stays SQLite-only. The MS-SQL build uses its own directory outside the repo:

```powershell
$AR = "C:\ar-build"   # any build directory outside the repo
cmake -S . -B "$AR\mssql" -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$PWD\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DAR_BUILD_TESTS=ON -DAR_SQLITE=ON -DAR_MSSQL=ON
foreach ($cfg in "Release", "Debug") {
  cmake --build "$AR\mssql" --config $cfg; if ($LASTEXITCODE) { throw "build $cfg failed" }
  ctest --test-dir "$AR\mssql" -C $cfg --output-on-failure; if ($LASTEXITCODE) { throw "ctest $cfg failed" }
  & "$AR\mssql\tests\$cfg\integration_tests.exe" --gtest_filter="AllAdapters/*/MSSQL" "--gtest_output=xml:$AR\MSSQL-$cfg.xml"
  if ($LASTEXITCODE) { throw "MSSQL tests $cfg failed" }
  python tools\check_adapter_tests.py "$AR\MSSQL-$cfg.xml" docs\hardening.md; if ($LASTEXITCODE) { throw "MSSQL check $cfg failed" }
}
```

**Outage proof** (elevated PowerShell, from the repo root):

```powershell
$AR = "C:\ar-build"   # as in check 7
$T = "$AR\mssql\tests\Release\integration_tests.exe"; if (-not (Test-Path $T)) { throw "build check 7 first" }
try {
  Stop-Service MSSQLSERVER -Force
  $out = & $T --gtest_filter="AllAdapters/*/MSSQL" 2>&1 | Out-String; $rc = $LASTEXITCODE
  if ($rc -ne 0 -and $out -match "Cannot connect to MSSQL" -and $out -match "\[  PASSED  \] 0 tests") { "outage OK" } else { "outage check FAILED" }
} finally { Start-Service MSSQLSERVER }
```

### 8. Installed package (`find_package`)

Installs the library, then builds [tests/package_consumer](../tests/package_consumer) against the install: a separate CMake project that calls `find_package(activerecord CONFIG REQUIRED)`, saves and reloads one model through SQLite, and prints `package consumer OK`. The consumer uses the vcpkg toolchain in classic mode, pointed at the library build's `vcpkg_installed` tree, so it finds the same dependency packages (and vcpkg's package wrappers) a downstream vcpkg user would. Each variant also checks that the generated config does not ask for packages of adapters that were off.

**WSL** (after the shared setup; it does not use `COMMON`):

```bash
for variant in sqlite db; do
  B=~/ar-build/pkg-$variant; P=~/ar-build/pkg-$variant-prefix; C=~/ar-build/pkg-$variant-consumer
  rm -rf "$B" "$P" "$C"
  if [ $variant = db ]; then extra="-DAR_MYSQL=ON -DAR_POSTGRES=ON"; absent="ODBC"
  else extra=""; absent="libmysql|MySQL|PostgreSQL|ODBC"; fi
  cmake -S $R -B $B -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC "$VCPKG_OPTS" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 -DAR_BUILD_EXAMPLES=OFF -DAR_SQLITE=ON $extra
  cmake --build $B && cmake --install $B --prefix $P
  ! grep -En "find_dependency\((unofficial-)?($absent)" $P/lib/cmake/activerecord/activerecord-config.cmake
  cmake -S $R/tests/package_consumer -B $C -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_TOOLCHAIN_FILE=$TC \
    -DVCPKG_MANIFEST_MODE=OFF "-DVCPKG_INSTALLED_DIR=$B/vcpkg_installed" "-DCMAKE_PREFIX_PATH=$P"
  cmake --build $C && $C/package_consumer
done
```

**Windows** (PowerShell, from the repo root; not at the same time as WSL checks):

```powershell
$AR = "C:\ar-build"   # any build directory outside the repo
$TC = "$PWD\vcpkg\scripts\buildsystems\vcpkg.cmake"
foreach ($variant in "sqlite", "mssql") {
  $B = "$AR\pkg-$variant"; $P = "$B-prefix"; $C = "$B-consumer"
  foreach ($d in $B, $P, $C) { if (Test-Path $d) { cmd /c "rd /s /q `"$d`"" } }
  $extra = @(); if ($variant -eq "mssql") { $extra += "-DAR_MSSQL=ON" }   # array, so @extra splats whole arguments
  $absent = if ($variant -eq "mssql") { "libmysql|MySQL|PostgreSQL" } else { "libmysql|MySQL|PostgreSQL|ODBC" }
  cmake -S . -B $B -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$TC" -DAR_BUILD_EXAMPLES=OFF -DAR_SQLITE=ON @extra
  if ($LASTEXITCODE) { throw "configure $variant failed" }
  foreach ($cfg in "Release", "Debug") {
    cmake --build $B --config $cfg; if ($LASTEXITCODE) { throw "build $variant $cfg failed" }
    cmake --install $B --config $cfg --prefix $P; if ($LASTEXITCODE) { throw "install $variant $cfg failed" }
  }
  if (Select-String -Path "$P\lib\cmake\activerecord\activerecord-config.cmake" -Pattern "find_dependency\((unofficial-)?($absent)") { throw "config $variant requires a disabled adapter" }
  cmake -S tests\package_consumer -B $C -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$TC" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_MANIFEST_MODE=OFF "-DVCPKG_INSTALLED_DIR=$B\vcpkg_installed" "-DCMAKE_PREFIX_PATH=$P"
  if ($LASTEXITCODE) { throw "consumer configure $variant failed" }
  foreach ($cfg in "Release", "Debug") {
    cmake --build $C --config $cfg; if ($LASTEXITCODE) { throw "consumer build $variant $cfg failed" }
    & "$C\$cfg\package_consumer.exe"; if ($LASTEXITCODE) { throw "consumer run $variant $cfg failed" }
  }
}
```

The library is static and links the database client libraries privately, so the installed `activerecord-config.cmake` calls `find_dependency()` for exactly the packages the build used (recorded in `src/CMakeLists.txt` with `ar_package_dependency()`, including the `FindSQLite3` and caller-supplied `FindMySQL` fallbacks). On Windows a consumer must use the same static runtime (`CMAKE_MSVC_RUNTIME_LIBRARY` `MultiThreaded$<$<CONFIG:Debug>:Debug>`) and the `x64-windows-static` triplet. Debug builds are named `activerecordd` (`DEBUG_POSTFIX`), so Release and Debug installs can share one prefix.

## Waived findings

Every clang-tidy suppression in the codebase, with its reason. Add new entries here when adding a `NOLINT` or relaxing a check.

### Checks disabled in [.clang-tidy](../.clang-tidy)

| Check | Reason |
|-------|--------|
| `modernize-use-trailing-return-type` | Style-only; the codebase uses leading return types throughout |
| `modernize-use-nodiscard` | Project convention: no `[[nodiscard]]` ([AGENTS.md](../AGENTS.md#code-style-preferences)) |

`HeaderFilterRegex` is `'/include/activerecord/'`. clang-tidy matches it against absolute paths, so a regex anchored with `^` silently hides every finding in the headers.

### Checks disabled in [tests/.clang-tidy](../tests/.clang-tidy)

| Check | Reason |
|-------|--------|
| `bugprone-unchecked-optional-access` | Does not treat gtest `ASSERT_TRUE(opt)` as a guard, so every guarded `*opt` in tests is a false positive |

### Inline suppressions

| Location | Check | Reason |
|----------|-------|--------|
| `include/activerecord/model.h` (`model` ctor) | `bugprone-easily-swappable-parameters` | Public API (`table_name, primary_key, con_name`); changing it breaks callers |
| `src/datetime.cpp` (`datetime(y, m, d, h, min, s)`) | `bugprone-easily-swappable-parameters` | Public API; conventional field order |
| `include/activerecord/attribute.h` (`attribute_type`) | `performance-enum-size` | Public unscoped enum; a `uint8_t` base would print as a character through ostream/gtest |
| `tests/unit/typed_model_test.cpp` (`task_state`) | `performance-enum-size` | `int32_t` base is deliberate: the test covers enum <-> INTEGER mapping |
| `include/activerecord/transaction.h` (destructor) | `bugprone-empty-catch` | A destructor must not throw; rollback errors are swallowed on purpose |
| `tests/integration/adapter_tests.cpp` (2x) | `bugprone-empty-catch` | Best-effort `DROP TABLE` in setup/teardown |
| `tests/unit/transaction_test.cpp` | `bugprone-empty-catch` | The exception is the point of the test |
| `src/adapters/mssql_adapter.cpp` (`check_odbc_error`) | `bugprone-easily-swappable-parameters` | `SQLRETURN` and `SQLSMALLINT` are both `short`; internal helper whose order follows ODBC's (return code, handle type, handle) |

### Waived adapter tests

Adapter tests skipped for one adapter because of a known adapter bug. Each is skipped in the test body with `GTEST_SKIP() << "WAIVED: <reason>"` and listed here by full gtest name (`AllAdapters/DatabaseAdapterTest.<Test>/<Adapter>`); `tools/check_adapter_tests.py` rejects any other skip.

_None._

## Known gaps

- Sanitizers run only on Linux/Clang. MSVC builds are compiled but not sanitized.
- MySQL and PostgreSQL are built and tested only on Linux, MS-SQL only on Windows (MSVC). clang-tidy analyzes the MS-SQL adapter only in its Linux (unixODBC) preprocessor view (check 4); it is not analyzed as MSVC compiles it, and it does not run under the sanitizers.
- The integration suites (`adapter_tests.cpp`, `database_operations_test.cpp`, `datetime_integration_test.cpp`) run on every enabled adapter. Still SQLite-only: `sqlite_adapter_test.cpp` (SQLite-specific behavior) and `QueryBuilderSqlTest` (asserts generated SQL text, which differs per adapter).
- `ar::datetime` is always written as a Unix timestamp (INTEGER/BIGINT columns). Reading native DATETIME/TIMESTAMP columns is tested on every adapter; on MySQL, PostgreSQL and MS-SQL, writing to them is not supported or tested (backlog). SQLite has no native date/time type, so a DATETIME-declared column simply stores the integer.
- `ModelSizeTest.BaseModelSize` pins `sizeof(model)` exactly per standard library (MSVC Release/Debug, libstdc++). Any growth fails on purpose; if it is intended, update the budget in [tests/unit/model_size_test.cpp](../tests/unit/model_size_test.cpp).
- Clang 18 (Ubuntu 24.04 default) cannot use libstdc++ 14's `<expected>`. If the project adopts `std::expected`, install `clang-20` and use it for checks 3-5.
