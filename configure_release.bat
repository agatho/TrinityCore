@echo off
REM TrinityCore CMake Configuration - Release Build
REM This script configures the build directory for Release compilation

echo ========================================
echo TrinityCore Release Configuration
echo ========================================
echo.

REM Set timeout to 30 minutes (1800 seconds)
set TIMEOUT=1800

REM Set library paths. Override any of these by setting the matching env var
REM before running, or edit the defaults below to match your install locations.
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\vcpkg"
if not defined BOOST_ROOT set "BOOST_ROOT=C:\local\boost"
set BOOST_INCLUDEDIR=%BOOST_ROOT%\boost
set BOOST_LIBRARYDIR=%BOOST_ROOT%\lib64-msvc-14.3

REM Qt6 kit for the world_editor target (no Qt6_DIR in machine/user env).
if not defined QT_PREFIX set "QT_PREFIX=C:\Qt\6.11.1\msvc2022_64"

echo VCPKG_ROOT: %VCPKG_ROOT%
echo BOOST_ROOT: %BOOST_ROOT%
echo.

REM ---- Always build with the MOST RECENT installed Visual Studio ----
REM Use vswhere to find the newest VS, then drive configure with THAT VS's
REM bundled cmake (default generator = its own VS version + newest toolset).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
set "CMAKE_EXE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" (
    echo WARNING: newest-VS cmake not found -- falling back to PATH cmake.
    set "CMAKE_EXE=cmake"
)
echo Using VS install: %VSPATH%
echo Using CMake:      %CMAKE_EXE%
echo.

REM Create or clean build directory (generator change needs a full wipe).
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
echo Running CMake configuration for Release...
echo.

"%CMAKE_EXE%" .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_PLAYERBOT=1 ^
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
echo Release Configuration Complete!
echo ========================================
echo.
echo Next step: Run build_release.bat to compile
echo.
pause
