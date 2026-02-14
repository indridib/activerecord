# Compiler Support Strategy

## Officially Supported Compilers

ActiveRecord C++ focuses on **two primary compilers** for optimal quality and maintainability:

### Windows: MSVC (Microsoft Visual C++)

**Compiler:** Visual Studio 2022 17.7+ (MSVC 19.37+)

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

### Linux/macOS: GCC (GNU Compiler Collection)

**Compiler:** GCC 14+ (minimum GCC 13)

**Why GCC on Linux/macOS?**
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

## Why Not Other Compilers?

### Clang
**Status:** Not officially supported

**Reason:** While Clang has excellent C++23 support, focusing on MSVC (Windows) + GCC (Linux/macOS) provides:
- Simpler maintenance
- Better static linking on Windows
- Standard toolchain coverage
- More predictable behavior across platforms

**Will it work?** Probably, but not tested or supported.

### MinGW
**Status:** Deprecated

**Reason:**
- ❌ Cannot link with MSVC-compiled vcpkg libraries (incompatible ABIs)
- ❌ Doesn't support static linking with vcpkg's default packages
- ❌ DLL path issues on Windows
- ❌ Additional complexity without benefits

**Migration:** Use MSVC on Windows instead. It works better and is simpler.

## Compiler Feature Matrix

| Feature | MSVC 19.39+ | GCC 14+ | GCC 13 |
|---------|-------------|---------|--------|
| `std::expected` | ✅ | ✅ | ⚠️ Partial |
| `std::print` | ✅ | ✅ | ❌ |
| `std::format` | ✅ | ✅ | ✅ |
| Concepts | ✅ | ✅ | ✅ |
| Ranges | ✅ | ✅ | ✅ |
| Deducing this | ✅ | ✅ | ⚠️ Limited |
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
   # Should show: Microsoft (R) C/C++ Optimizing Compiler Version 19.39+ ...
   ```

**Option 2: Build Tools Only**
1. Download [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. Install "C++ build tools" workload
3. Same verification as above

### Linux (GCC)

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential gcc-14 g++-14 cmake git

# Set as default (optional)
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100

# Verify
g++ --version  # Should show 14.x
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc gcc-c++ cmake git

# Verify
g++ --version  # Should show 13.x or 14.x
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake git

# Verify
g++ --version  # Should show 14.x (Arch usually has latest)
```

### macOS (GCC via Homebrew)

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install GCC
brew install gcc@14 cmake git

# Verify
g++-14 --version  # Should show 14.x

# Optional: Create symlink
sudo ln -sf /opt/homebrew/bin/g++-14 /usr/local/bin/g++
```

## CI/CD Configuration

**GitHub Actions Example:**
```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        include:
          - name: Windows MSVC
            os: windows-latest
            compiler: msvc

          - name: Linux GCC 14
            os: ubuntu-latest
            compiler: gcc-14

          - name: macOS GCC 14
            os: macos-latest
            compiler: gcc-14

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup (Windows)
        if: runner.os == 'Windows'
        run: .\setup.bat

      - name: Setup (Linux/macOS)
        if: runner.os != 'Windows'
        run: chmod +x setup.sh && ./setup.sh

      - name: Build (Windows)
        if: runner.os == 'Windows'
        run: .\build.bat

      - name: Build (Linux/macOS)
        if: runner.os != 'Windows'
        run: chmod +x build.sh && ./build.sh
```

## Troubleshooting

### Windows: "cl is not recognized"

**Solution:** Run from **Developer Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**

Or initialize the environment:
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### Linux: "GCC 14 not found"

**Solution:** Install from testing/backports or build from source:
```bash
# Ubuntu (if 14 not in repos)
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-14 g++-14
```

### macOS: "command not found: g++-14"

**Solution:** Homebrew installation path issue:
```bash
# Check installation
brew --prefix gcc@14

# Add to PATH
export PATH="/opt/homebrew/bin:$PATH"  # Apple Silicon
# OR
export PATH="/usr/local/bin:$PATH"     # Intel
```

## Summary

| Aspect | Windows | Linux/macOS |
|--------|---------|-------------|
| **Compiler** | MSVC 19.39+ | GCC 14+ |
| **Distribution** | Visual Studio 2022 | System package manager |
| **Linking** | Static (no DLLs) | Dynamic (standard) |
| **Build Tool** | `build.bat` | `build.sh` |
| **Package Manager** | vcpkg (bundled) | vcpkg (bundled) |
| **Deployment** | Single .exe | Executable + shared libs |

**The goal:** Simplicity, reliability, and optimal performance on each platform.
