#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"

: "${ANDROID_HOME:?Set ANDROID_HOME to the Android SDK directory}"
: "${VCPKG_ROOT:?Set VCPKG_ROOT to the pinned vcpkg checkout}"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$ANDROID_HOME}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/28.2.13676358}"
export VCPKG_INSTALLED_DIR="${VCPKG_INSTALLED_DIR:-$repo_root/.vcpkg-installed}"
export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$repo_root/android/.gradle-user-home}"

"$repo_root/android/gradlew" -p "$repo_root/android" assembleDebug
