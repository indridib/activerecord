# Hardening Runbook

Recurring checks that keep the codebase warning-free and free of memory and undefined-behavior errors. Run them all before a release and after any change to build flags, compilers, or low-level code (adapters, `attribute`, `datetime`).

This implements the project rule "We don't like warnings in our compiled code" ([AGENTS.md](AGENTS.md#core-principle-simplicity-first)). Compiler warnings are errors in every build (`/WX`, `-Werror` in [cmake/Warnings.cmake](cmake/Warnings.cmake)), and so are clang-tidy findings (`WarningsAsErrors: '*'` in [.clang-tidy](.clang-tidy)). A new warning fails the build instead of accumulating.

## Checks

| # | Check | Platform | Clean means |
|---|-------|----------|-------------|
| 1 | MSVC Release + Debug | Windows | Builds (warnings are errors), `ctest` 100% |
| 2 | GCC 14 Release | WSL / Linux | Builds (warnings are errors), `ctest` 100% |
| 3 | Clang Release | WSL / Linux | Builds (warnings are errors), `ctest` 100% |
| 4 | clang-tidy | WSL / Linux | Builds (findings are errors); log contains no `warning:` lines |
| 5 | ASan + UBSan | WSL / Linux | `ctest` 100%, no sanitizer report in the output |

## Prerequisites

**Windows:** Visual Studio 2022 17.9+ with the C++ workload. `build.bat` bootstraps vcpkg on first run.

**WSL / Linux:** Ubuntu 24.04 (older releases ship compilers too old for C++23):

```bash
sudo apt update && sudo apt install -y build-essential g++-14 clang clang-tidy libclang-rt-18-dev \
  cmake ninja-build pkg-config zip unzip curl git
cd /mnt/c/dev/github/activerecord/vcpkg && ./bootstrap-vcpkg.sh -disableMetrics   # once; coexists with vcpkg.exe
```

Keep WSL build directories on the Linux filesystem (`~/ar-build/...`). Building from `/mnt/c` works but is several times slower.

## Commands

### 1. MSVC (Windows, from the repo root)

```bat
build.bat Release
build.bat Debug
```

Debug matters: MSVC reports some warnings (e.g. C4702 unreachable code) only in Debug.

### Shared WSL setup (checks 2-5)

```bash
R=/mnt/c/dev/github/activerecord                           # repo as seen from WSL
TC=$R/vcpkg/scripts/buildsystems/vcpkg.cmake
COMMON="-G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC -DAR_BUILD_TESTS=ON -DAR_BUILD_EXAMPLES=ON -DAR_SQLITE=ON"
set -o pipefail                                            # a failed build piped into tee must still fail
```

### 2. GCC 14 Release

```bash
cmake -S $R -B ~/ar-build/gcc $COMMON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14
cmake --build ~/ar-build/gcc && ctest --test-dir ~/ar-build/gcc --output-on-failure
```

### 3. Clang Release

```bash
cmake -S $R -B ~/ar-build/clang $COMMON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build ~/ar-build/clang && ctest --test-dir ~/ar-build/clang --output-on-failure
```

### 4. clang-tidy

```bash
cmake -S $R -B ~/ar-build/tidy $COMMON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DAR_CLANG_TIDY=ON
cmake --build ~/ar-build/tidy --clean-first 2>&1 | tee ~/ar-build/tidy.log
! grep -n "warning:" ~/ar-build/tidy.log    # must print nothing; exit status 0 means clean
```

clang-tidy runs inline during compilation, and any finding fails the build. `set -o pipefail` (in the shared setup) keeps that failure visible through `tee`. The `grep` is a backstop for findings from checks that are not promoted to errors: it lists any it finds, and its exit status is 0 only when there are none. `--clean-first` makes sure every file is re-analyzed.

Two limitations:
- It only works with Ninja or Makefile generators. CMake's Visual Studio generator silently ignores `CMAKE_CXX_CLANG_TIDY`, so there is no Windows equivalent of this check.
- It sees the Linux preprocessor view, so code under `#ifdef _WIN32` / `_MSC_VER` is not analyzed.

### 5. ASan + UBSan

```bash
cmake -S $R -B ~/ar-build/san $COMMON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DAR_SANITIZE_ADDRESS=ON -DAR_SANITIZE_UNDEFINED=ON
cmake --build ~/ar-build/san
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
ctest --test-dir ~/ar-build/san --output-on-failure
```

The options are required. By default UBSan prints a report and keeps running, so `ctest` would pass despite undefined behavior. `halt_on_error=1` makes any report fail the test. The library, tests and examples are all instrumented.

## Waived findings

Every clang-tidy suppression in the codebase, with its reason. Add new entries here when adding a `NOLINT` or relaxing a check.

### Checks disabled in [.clang-tidy](.clang-tidy)

| Check | Reason |
|-------|--------|
| `modernize-use-trailing-return-type` | Style-only; the codebase uses leading return types throughout |
| `modernize-use-nodiscard` | Project convention: no `[[nodiscard]]` ([AGENTS.md](AGENTS.md#code-style-preferences)) |

`HeaderFilterRegex` is `'/include/activerecord/'`. clang-tidy matches it against absolute paths, so a regex anchored with `^` silently hides every finding in the headers.

### Checks disabled in [tests/.clang-tidy](tests/.clang-tidy)

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

## Known gaps

- Sanitizers run only on Linux/Clang. MSVC builds are compiled but not sanitized.
- Only the SQLite adapter is built and tested. The MySQL, PostgreSQL and MS-SQL adapters are not covered by any check here.
- `ModelSizeTest.BaseModelSize` pins `sizeof(model)` exactly per standard library (MSVC Release/Debug, libstdc++). Any growth fails on purpose; if it is intended, update the budget in [tests/unit/model_size_test.cpp](tests/unit/model_size_test.cpp).
- Clang 18 (Ubuntu 24.04 default) cannot use libstdc++ 14's `<expected>`. If the project adopts `std::expected`, install `clang-20` and use it for checks 3-5.
