@echo off
rem Build Nebenan with Visual Studio (needs CMake and Visual Studio 2022 or newer)
cmake -S . -B build
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel
if errorlevel 1 exit /b 1
echo Fertig: build\bin\Release\nebenan_demo.exe
