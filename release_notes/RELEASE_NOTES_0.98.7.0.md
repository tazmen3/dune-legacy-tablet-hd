# Dune Legacy 0.98.7.0 Release Notes

**Release Date:** November 16, 2024

This release brings major improvements to multiplayer infrastructure, build system modernization, and project hosting migration.

---

## 🎮 Multiplayer & Infrastructure

### New Features
- **New Metaserver**: Moved to https://dunelegacy.com with full HTTPS support
- **Secure Connections**: Integrated libcurl for reliable HTTPS communication
- **Game Statistics**: Track recent games and popular maps on the metaserver status page
- **Automatic Migration**: Existing installations automatically update to the new metaserver URL

### Improvements
- **Better Deduplication**: Games now use unique identifiers to prevent duplicate listings
- **Enhanced Privacy**: Player IP addresses are no longer displayed publicly on the metaserver
- **Reliable Announcements**: Fixed issues with game server announcements not appearing

---

## 🔧 Build System & Development

### New Features
- **vcpkg Integration**: Consistent, reproducible builds across all platforms (Windows, macOS, Linux)
- **Automated Installers**: 
  - macOS: DMG creation integrated into build process
  - Windows: NSIS installer support
  - Linux: DEB, RPM, and TGZ package generation
- **IDE Support**: VS Code/Cursor build tasks with clear warnings about proper build targets
- **Build Helper Script**: `scripts/build.sh` for simplified building on macOS/Linux

### Improvements
- **Comprehensive Documentation**: Single `BUILD.md` with platform-specific guides
- **macOS App Bundle**: Properly packages all dependencies (SDL2, libcurl, etc.)
- **CMake Modernization**: Cleaner packaging workflow using CPack
- **vcpkg Custom Triplet**: Optimized builds with `-O3 -ffast-math` for better performance

---

## 🌐 Website & Project

### New Infrastructure
- **GitHub Repository**: Project moved from SourceForge to GitHub for better collaboration
- **New Website**: Modern, responsive design at https://dunelegacy.com
- **GitHub Actions**: Automated CI/CD builds for all platforms
- **DigitalOcean Hosting**: Reliable hosting for metaserver and website

### Features
- **Live Metaserver Status**: View active games and statistics at https://dunelegacy.com/metaserver/
- **Game History**: Track recent games (last 30 days) and popular maps
- **Automated Deployments**: Push to main branch automatically deploys updates

---

## 🐛 Bug Fixes

- Fixed multiplayer game listings not appearing on metaserver
- Fixed duplicate game entries when hosting behind load balancers/NAT
- Fixed HTTP redirect issues with new HTTPS metaserver
- Resolved macOS DMG packaging issues with unnecessary folders
- Fixed build system not updating installer artifacts

---

## 📦 Technical Details

### Dependencies
- **libcurl**: Added for HTTPS support with automatic redirect handling
- **SDL2**: Updated to 2.32.10
- **SDL2_mixer**: Updated to 2.8.1
- **SDL2_ttf**: Updated to 2.24.0

### Build Requirements
- CMake 3.15+
- C++17 compatible compiler
- vcpkg (recommended) or system packages

### Supported Platforms
- **macOS**: 14.0+ (Apple Silicon optimized)
- **Windows**: Visual Studio 2019+ or MinGW-w64
- **Linux**: Ubuntu 20.04+, Fedora 35+, other modern distributions

---

## 🎯 Getting Started

### Download
- Visit https://dunelegacy.com to download the latest build
- Or build from source: https://github.com/svan058/dunelegacy

### Building
```bash
# macOS/Linux with vcpkg
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --target dmg --config Release  # macOS
cmake --build build --target package --config Release  # Linux

# Windows with vcpkg
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --target installer --config Release
```

### Multiplayer
- Existing installations automatically migrate to the new metaserver
- No manual configuration needed!
- View active games at https://dunelegacy.com/metaserver/

---

## 👏 Acknowledgments

Special thanks to:
- The SDL2 development team for the excellent multimedia library
- Microsoft vcpkg team for the cross-platform package manager
- DigitalOcean for reliable hosting infrastructure
- The Dune Legacy community for continued support and feedback

---

## 🔗 Links

- **Website**: https://dunelegacy.com
- **Source Code**: https://github.com/svan058/dunelegacy
- **Discord**: https://discord.gg/6sAcZr6y3B
- **Metaserver**: https://dunelegacy.com/metaserver/

---

**Full Changelog**: https://github.com/svan058/dunelegacy/compare/v0.98.6.6...v0.98.7.0

