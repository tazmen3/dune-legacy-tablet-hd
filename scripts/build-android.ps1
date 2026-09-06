param(
    [string]$AndroidSdk = $env:ANDROID_HOME,
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$VcpkgInstalledDir = $env:VCPKG_INSTALLED_DIR
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($AndroidSdk)) {
    throw "Set ANDROID_HOME or pass -AndroidSdk."
}
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw "Set VCPKG_ROOT or pass -VcpkgRoot."
}
if ([string]::IsNullOrWhiteSpace($VcpkgInstalledDir)) {
    $VcpkgInstalledDir = Join-Path $repoRoot ".vcpkg-installed"
}

$env:ANDROID_HOME = $AndroidSdk
$env:ANDROID_SDK_ROOT = $AndroidSdk
$env:ANDROID_NDK_HOME = Join-Path $AndroidSdk "ndk\28.2.13676358"
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_INSTALLED_DIR = $VcpkgInstalledDir
if ([string]::IsNullOrWhiteSpace($env:ANDROID_USER_HOME)) {
    $env:ANDROID_USER_HOME = Join-Path $repoRoot "android\.gradle-user-home\android-user-home"
}
if ([string]::IsNullOrWhiteSpace($env:GRADLE_USER_HOME)) {
    $env:GRADLE_USER_HOME = Join-Path $repoRoot "android\.gradle-user-home"
}

& (Join-Path $repoRoot "android\gradlew.bat") -p (Join-Path $repoRoot "android") assembleDebug
exit $LASTEXITCODE
