# Building Dune Legacy

This guide covers building Dune Legacy on all platforms using modern dependency management.

## Quick Start

### Prerequisites

**All Platforms:**
- CMake 3.15+
- C++17 compatible compiler
- Git

**Platform-Specific:**
- **Windows**: Visual Studio 2019+ or MinGW-w64
- **macOS**: Xcode 11+ or Command Line Tools (`xcode-select --install`)
- **Linux**: Build essentials (`sudo apt install build-essential cmake`)

---

## Recommended: Build with vcpkg

vcpkg provides consistent, reproducible builds across all platforms. This is what our CI/CD uses.

### One-Time vcpkg Setup

```bash
# Clone vcpkg (pick a location like ~/vcpkg or C:\vcpkg)
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Bootstrap
./bootstrap-vcpkg.sh      # macOS/Linux
.\bootstrap-vcpkg.bat     # Windows
```

### Build Dune Legacy

```bash
cd /path/to/dunelegacy

# Configure (replace /path/to/vcpkg with your vcpkg location)
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

**First build takes ~5 minutes** as vcpkg downloads and compiles SDL2 libraries.

### Run the Game

- **macOS**: `open build/bin/dunelegacy.app`
- **Windows**: `build\bin\Release\dunelegacy.exe`
- **Linux**: `build/bin/dunelegacy`

### What vcpkg Does

✅ Automatically downloads SDL2, SDL2_mixer, SDL2_ttf  
✅ Builds optimized binaries for your platform  
✅ Handles all linking (static or dynamic)  
✅ No manual dependency installation needed  

---

## Alternative: System Package Managers

If you prefer using system-installed libraries:

### macOS (Homebrew)

```bash
# Install dependencies
brew install sdl2 sdl2_mixer sdl2_ttf cmake

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt install libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev cmake

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (Fedora/RHEL)

```bash
# Install dependencies
sudo dnf install SDL2-devel SDL2_mixer-devel SDL2_ttf-devel cmake

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Platform-Specific Builds

### macOS: Xcode Project (Legacy)

The native Xcode project is still maintained for local macOS development:

```bash
# Open project
open IDE/xCode/Dune\ Legacy.xcodeproj

# Build in Xcode (⌘B)
```

**Note**: Requires SDL2 frameworks in `external/frameworks/`. Download from [SDL Releases](https://github.com/libsdl-org/SDL/releases).

### Windows: Visual Studio

Generate a Visual Studio solution:

```bash
cmake -B build -G "Visual Studio 17 2022" \
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Open build/DuneLegacy.sln in Visual Studio
```

### macOS: Performance Build

For optimized dynamic libraries (better FPS):

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-dynamic \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

---

## Distribution Builds

For creating distributable binaries:

### Windows

vcpkg automatically bundles DLLs:
```bash
# DLLs are in build/bin/Release/ alongside .exe
```

### macOS

Creates proper .app bundle:
```bash
# App is at: build/bin/dunelegacy.app
# Can be distributed as-is or packaged in DMG
```

### Linux Packages

```bash
cd build

# Debian/Ubuntu
cpack -G DEB -D CPACK_DEBIAN_PACKAGE_MAINTAINER="Your Name <your@email.com>"

# RedHat/Fedora
cpack -G RPM

# Tarball
cpack -G TGZ
```

---

## Troubleshooting

### "SDL2 not found"

**With vcpkg:**
- Ensure `-DCMAKE_TOOLCHAIN_FILE` points to correct vcpkg location
- Delete `build/` and reconfigure
- Check `vcpkg.json` exists in project root

**Without vcpkg:**
- Install SDL2 via your package manager
- macOS: `brew install sdl2 sdl2_mixer sdl2_ttf`
- Linux: `sudo apt install libsdl2-dev libsdl2-mixer-dev libsdl2-ttf-dev`

### "vcpkg not found"

```bash
# Use absolute path to vcpkg toolchain file
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/full/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### macOS Code Signing Errors

For local development builds:
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="" \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO
```

### Linux Runtime Library Issues

If using system packages and libraries aren't found:
```bash
sudo ldconfig
```

Or use static linking with vcpkg:
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux-static
```

---

## Advanced Configuration

### vcpkg Triplets

Control how dependencies are built:

- `arm64-osx` / `x64-osx` - macOS static libs
- `arm64-osx-dynamic` / `x64-osx-dynamic` - macOS dynamic libs (better performance)
- `x64-windows` - Windows DLLs
- `x64-windows-static` - Windows static linking
- `x64-linux` / `x64-linux-static` - Linux variants

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-dynamic
```

### Build Types

- `Release` - Optimized, no debug symbols (recommended)
- `Debug` - Debug symbols, no optimization
- `RelWithDebInfo` - Optimized with debug symbols

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

---

## Continuous Integration

GitHub Actions automatically builds for all platforms on every commit:

- **Windows** (x64) - MSVC + vcpkg
- **macOS** (arm64) - Clang + vcpkg  
- **Linux** (x64) - GCC + vcpkg

See `.github/workflows/build.yml` for the configuration.

Artifacts are uploaded for each successful build.

---

## For Contributors

- **Use vcpkg** for consistent dependency versions
- `vcpkg.json` defines all dependencies
- Run `cmake --build build` to build after making changes
- The project auto-formats code - follow existing style

### Project Structure

```
dunelegacy/
├── src/              - Game source code
├── include/          - Headers
├── data/             - Game assets (.PAK files)
├── config/           - Configuration templates
├── IDE/xCode/        - Xcode project (macOS)
├── vcpkg.json        - vcpkg dependencies
└── CMakeLists.txt    - Build configuration
```

---

## Getting Original Dune II Data Files

Dune Legacy requires the original Dune II data files (.PAK files). These should be placed in:

- **Development**: `dunelegacy/data/` directory
- **Installed game**: Game will prompt for location on first run

Sources:
- [Dune II - The Building of a Dynasty](https://www.myabandonware.com/game/dune-ii-the-building-of-a-dynasty-1d4)
- Original game CDs/floppies

---

## Need Help?

- **Issues**: https://github.com/dunelegacy/dunelegacy/issues
- **Website**: https://dunelegacy.sourceforge.net
- **Discord**: [Community link]

---

## Version Information

This build system uses:
- **SDL2**: 2.32.10+
- **SDL2_mixer**: 2.8.1+
- **SDL2_ttf**: 2.24.0+

Versions automatically managed by vcpkg baseline in `vcpkg.json`.
