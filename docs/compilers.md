# Compiler Support Strategy

## Supported Platforms and Compilers

ActiveRecord C++ is built and tested on Windows and Linux. Every compiler below runs the full [hardening checks](hardening.md) before a release:

| Platform | Compiler | Standard library |
|----------|----------|------------------|
| Windows | MSVC (Visual Studio 2022) | MSVC STL |
| Linux | GCC 14+ | libstdc++ |
| Linux | Clang 18+ | libstdc++ |

Other platforms and compilers are not tested.

### Windows: MSVC (Microsoft Visual C++)

**Compiler:** Visual Studio 2022, kept up to date

**Why MSVC on Windows?**
- ✅ **Static Linking** - No DLL dependencies, single executable deployment
- ✅ **Full C++23 Support** - Complete implementation including std::expected, std::print, std::format
- ✅ **Native vcpkg Integration** - Seamless package management
- ✅ **Optimal Windows Performance** - Native toolchain, best optimization
- ✅ **Zero DLL Headaches** - Eliminates PATH issues, deployment complexity

**Build Command:**
```cmd
build.bat           # Automatic static linking
build.bat Debug     # Debug build
```

**Executable Sizes (with SQLite statically linked):**
- Example: ~1.1 MB (includes SQLite + your code)
- Tests: ~500 KB - 1.6 MB
- **No external DLL dependencies required**

### Linux: GCC (GNU Compiler Collection)

**Compiler:** GCC 14+

**Why GCC on Linux?**
- ✅ **Standard Toolchain** - Default on most Linux distributions
- ✅ **Excellent C++23 Support** - Mature, well-tested implementation
- ✅ **Broad Ecosystem** - Compatible with system package managers
- ✅ **Performance** - Highly optimized code generation
- ✅ **Debuggability** - Excellent GDB integration

**Build Command:**
```bash
./build.sh          # Automatic configuration
./build.sh Debug    # Debug build
```

### Linux: Clang

**Compiler:** Clang 18+, with the system libstdc++

Clang also powers the project's static analysis (clang-tidy) and sanitizer builds.

**Build Command:**
```bash
CC=clang CXX=clang++ ./build.sh
```

**Sanitizer build:**
```bash
cmake -B build-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DAR_BUILD_TESTS=ON \
  -DAR_SANITIZE_ADDRESS=ON \
  -DAR_SANITIZE_UNDEFINED=ON

cmake --build build-clang
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
ctest --test-dir build-clang --output-on-failure
```

## Not Supported

### MinGW
**Reason:**
- ❌ Cannot link with MSVC-compiled vcpkg libraries (incompatible ABIs)
- ❌ Doesn't support static linking with vcpkg's default packages
- ❌ DLL path issues on Windows
- ❌ Additional complexity without benefits

Use MSVC on Windows instead. It works better and is simpler.

## Compiler Feature Matrix

| Feature | MSVC (VS 2022) | GCC 14+ | Clang 18+ |
|---------|----------------|---------|-----------|
| `std::expected` | ✅ | ✅ | ✅ |
| `std::print` | ✅ | ✅ | ✅ |
| `std::format` | ✅ | ✅ | ✅ |
| Concepts | ✅ | ✅ | ✅ |
| Ranges | ✅ | ✅ | ✅ |
| Deducing this | ✅ | ✅ | ✅ |
| Static linking | ✅ Perfect | ✅ Standard | ✅ Standard |

## Installation Instructions

### Windows (MSVC)

**Option 1: Visual Studio Installer (Recommended)**
1. Download [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)
2. During installation, select:
   - "Desktop development with C++"
   - Individual components: "MSVC v143 - VS 2022 C++ x64/x86 build tools"
   - Individual components: "C++ CMake tools for Windows"
3. Verify:
   ```cmd
   cl
   # Should show: Microsoft (R) C/C++ Optimizing Compiler Version 19.4x ...
   ```

**Option 2: Build Tools Only**
1. Download [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. Install "C++ build tools" workload
3. Same verification as above

### Linux (Ubuntu 24.04)

Older Ubuntu releases ship compilers too old for C++23.

```bash
sudo apt update
sudo apt install build-essential g++-14 clang cmake git

# Set GCC 14 as default (optional)
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100

# Verify
g++ --version      # Should show 14.x
clang++ --version  # Should show 18.x or later
```

## Troubleshooting

### Windows: "cl is not recognized"

**Solution:** Run from **Developer Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**

Or initialize the environment:
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### Linux: "GCC 14 not found"

**Solution:** Use Ubuntu 24.04 or later, where `g++-14` is in the standard repositories.

## Summary

| Aspect | Windows | Linux |
|--------|---------|-------|
| **Compiler** | MSVC (VS 2022) | GCC 14+ or Clang 18+ |
| **Distribution** | Visual Studio 2022 | System package manager |
| **Linking** | Static (no DLLs) | Dynamic (standard) |
| **Build Tool** | `build.bat` | `build.sh` |
| **Package Manager** | vcpkg (bundled) | vcpkg (bundled) |
| **Deployment** | Single .exe | Executable + shared libs |

**The goal:** Simplicity, reliability, and optimal performance on each platform.
