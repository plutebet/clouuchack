@echo off
rem Builds the overlay with the newest NDK installed under %LOCALAPPDATA%\Android\Sdk\ndk.
rem Put this file in the project root (next to jni\, src\, includes\, assets\) and run it.
setlocal
cd /d "%~dp0"

echo [*] project: %CD%
echo.

if not exist "assets\maps\Arena.pkm" (
    echo [!] assets\maps\Arena.pkm not found.
    echo     Unpack unity6-bypass-maps.zip into the assets\maps folder here.
    goto fail
)
if not exist "includes" (
    echo [!] includes\ not found.
    echo     Copy the includes folder of your full project here (Android_draw, ImGui, backends).
    goto fail
)

rem for /d enumerates in ascending order, so the last match is the newest NDK.
set "NDK_BUILD="
for /d %%i in ("%LOCALAPPDATA%\Android\Sdk\ndk\*") do set "NDK_BUILD=%%i\ndk-build.cmd"
if not defined NDK_BUILD (
    for %%i in (ndk-build.cmd) do if not "%%~$PATH:i"=="" set "NDK_BUILD=%%~$PATH:i"
)
if not defined NDK_BUILD (
    echo [!] no NDK found under "%LOCALAPPDATA%\Android\Sdk\ndk".
    echo     Install one with:  sdkmanager "ndk;27.2.12479018"
    goto fail
)

echo [*] NDK: %NDK_BUILD%
echo [*] building...
echo.

rem NDK_PROJECT_PATH must be the project root: .incbin in src\game\maps_embedded.S
rem resolves "assets/maps/<Name>.pkm" against this working directory.
call "%NDK_BUILD%" NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk -j8
if errorlevel 1 (
    echo.
    echo [!] ndk-build failed - the first error is above.
    goto fail
)
if not exist "libs\arm64-v8a\eeeww" (
    echo [!] ndk-build finished but libs\arm64-v8a\eeeww is missing.
    goto fail
)

echo.
for %%f in ("libs\arm64-v8a\eeeww") do echo [+] built: %%~ff  (%%~zf bytes)
echo.
echo Next:
echo     adb push libs\arm64-v8a\eeeww /data/local/tmp/sf_render
echo     adb shell chmod 755 /data/local/tmp/sf_render
echo     adb shell su -c "/data/local/tmp/sf_render"
echo.
pause
exit /b 0

:fail
echo.
pause
exit /b 1
