@echo off
REM TrinityCore CMake Configuration - RelWithDebInfo Build
REM This script configures the build directory for RelWithDebInfo compilation
REM (Optimized release build with debug symbols)

echo ========================================
echo TrinityCore RelWithDebInfo Configuration
echo ========================================
echo.

REM Set timeout to 30 minutes (1800 seconds)
set TIMEOUT=1800

REM Set library paths. Override any of these by setting the matching env var
REM before running, or edit the defaults below to match your install locations.
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"
if not defined BOOST_ROOT set "BOOST_ROOT=C:\local\boost"
set BOOST_INCLUDEDIR=%BOOST_ROOT%\boost
set BOOST_LIBRARYDIR=%BOOST_ROOT%\stage\lib

REM Qt6 kit for the world_editor target.  The Qt prefix must be passed
REM explicitly (no Qt6_DIR in the machine/user env).  msvc2022_64 (vc143 ABI)
REM links cleanly into the newer 14.5x toolset.  Bump to the newest installed
REM C:\Qt\<ver>\msvc2022_64 when Qt is updated.
if not defined QT_PREFIX set "QT_PREFIX=C:\Qt\6.11.1\msvc2022_64"

echo VCPKG_ROOT: %VCPKG_ROOT%
echo BOOST_ROOT: %BOOST_ROOT%
echo.

REM ---- Always build with the MOST RECENT installed Visual Studio ----
REM vswhere finds the newest VS install (incl. previews); we then use THAT
REM VS's bundled cmake.exe, whose DEFAULT generator is its own VS version
REM (e.g. "Visual Studio 18 2026").  By NOT passing -G we always inherit the
REM newest generator + toolset automatically -- no per-VS-version edits, and
REM no risk of MSBuild's default toolset silently floating to an old one.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
set "CMAKE_EXE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" (
    echo WARNING: newest-VS cmake not found at "%CMAKE_EXE%" -- falling back to PATH cmake.
    set "CMAKE_EXE=cmake"
)
echo Using VS install: %VSPATH%
echo Using CMake:      %CMAKE_EXE%
echo.

REM Create or clean build directory.  A generator change requires wiping the
REM whole CMake generator state (CMakeCache.txt + CMakeFiles), not just the cache.
if exist build (
    echo Build directory exists. Cleaning CMake cache + generator state...
    del /q build\CMakeCache.txt 2>nul
    rmdir /s /q build\CMakeFiles 2>nul
) else (
    echo Creating build directory...
    mkdir build
)

cd build

echo.
echo Running CMake configuration for RelWithDebInfo...
echo.

"%CMAKE_EXE%" .. ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DBUILD_PLAYERBOT_V2=1 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DBOOST_ROOT="%BOOST_ROOT%" ^
    -DBOOST_INCLUDEDIR="%BOOST_INCLUDEDIR%" ^
    -DBOOST_LIBRARYDIR="%BOOST_LIBRARYDIR%" ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ========================================
    echo ERROR: CMake configuration failed!
    echo ========================================
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ========================================
echo RelWithDebInfo Configuration Complete!
echo ========================================
echo.
echo Next step: Run build_relwithdebinfo.bat to compile
echo.
pause
