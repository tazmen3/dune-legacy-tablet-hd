# Android ARM64 build

This branch provides the first reproducible Android build foundation for **Dune Legacy Tablet HD**. It does not add tablet controls, HD rendering, gameplay changes, or multiplayer changes.

## Pinned toolchain

- Java: Eclipse Temurin/OpenJDK 17 (validated with `17.0.20.1+1`)
- Android compile SDK: 35, platform revision 2
- Android target SDK: 35
- Android minimum/native API: 28
- Android Build Tools: 35.0.0
- Android NDK: `28.2.13676358` (r28c, Clang 19.0.1)
- CMake: Android SDK CMake `3.22.1-g37088a8-dirty`
- Ninja: Android SDK Ninja 1.10.2
- Gradle: 8.9 (wrapper SHA-256 pinned)
- Android Gradle Plugin: 8.7.3
- SDL: 2.32.10, pinned as the `external/SDL2` Git submodule
- vcpkg baseline: `0804e3b55b7e435c593707071052363fa876f170`
- Android ABI: `arm64-v8a` only

API 28 is deliberate: the official vcpkg `arm64-android` triplet builds its libraries with `VCPKG_CMAKE_SYSTEM_VERSION=28`. Linking an API 23 game target against those API 28 archives produces unresolved Bionic symbols and is not a valid minimum-SDK configuration.

## Ubuntu prerequisites

Install the host tools and JDK:

```bash
sudo apt update
sudo apt install -y autoconf automake build-essential curl git libtool ninja-build pkg-config unzip zip openjdk-17-jdk
```

Install the Android command-line tools, accept the licenses, then install the pinned components:

```bash
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

yes | sdkmanager --licenses
sdkmanager \
  "platforms;android-35" \
  "build-tools;35.0.0" \
  "platform-tools" \
  "ndk;28.2.13676358" \
  "cmake;3.22.1"
```

Clone and pin vcpkg separately:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg-dunelegacy"
git -C "$HOME/vcpkg-dunelegacy" checkout 0804e3b55b7e435c593707071052363fa876f170
"$HOME/vcpkg-dunelegacy/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg-dunelegacy"
```

## Clone and build

The SDL source is a required submodule:

```bash
git clone --branch android --recurse-submodules https://github.com/tazmen3/dune-legacy-tablet-hd.git
cd dune-legacy-tablet-hd
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME="$HOME/Android/Sdk"
export VCPKG_ROOT="$HOME/vcpkg-dunelegacy"
./scripts/build-android.sh
```

On Windows PowerShell, after setting `JAVA_HOME`, use:

```powershell
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
$env:VCPKG_ROOT = "$env:LOCALAPPDATA\vcpkg-dunelegacy"
.\scripts\build-android.ps1
```

The Debug APK is written to:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

The repository does not distribute the proprietary Dune II data files. Producing an APK validates the Android packaging and native engine build; a complete play-through still requires legally obtained compatible game data and a real-device test.

On Android, the APK's assets are not treated as a desktop/POSIX directory. At startup, the Java host extracts only the redistributable engine resources to the app-private `files/engine-data` directory before `SDL_main()` starts. Logs, settings, saves, and other writable user files use the app-private `files` directory. Proprietary Dune II data supplied later by the user belongs in the separate `files/data` search directory; the Gradle build fails if a known proprietary PAK filename is found in the packaged source directories.

## Native-only cross-compilation

For a direct CMake test, configure with vcpkg as the primary toolchain and the NDK as its chainloaded toolchain:

```bash
cmake -S . -B out/android-engine -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ANDROID_HOME/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-android \
  -DVCPKG_INSTALLED_DIR="$PWD/.vcpkg-installed" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDUNELEGACY_ENABLE_PCH=OFF \
  -DDUNELEGACY_ENABLE_DISCORD_RPC=OFF
cmake --build out/android-engine --parallel
```

This produces `out/android-engine/lib/libdunelegacy.so`.

`miniupnpc` remains enabled because it builds and links successfully for ARM64 Android. Discord Rich Presence is optional and defaults to disabled on Android; it remains enabled by default on desktop platforms.
