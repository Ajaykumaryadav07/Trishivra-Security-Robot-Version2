@echo off
cd /d "%~dp0"
if not exist "build" mkdir "build"
cd /d "%~dp0build"
set "VCPKG_ROOT=%~dp0..\..\vcpkg"
set "VCPKG_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN_FILE%"
if errorlevel 1 exit /b 1
cmake --build . --config Release
exit /b %errorlevel%
