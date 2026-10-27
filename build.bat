@echo off
rem Build Nebenan with Visual Studio (needs CMake and Visual Studio 2022 or newer)
rem With the argument tracy it builds into build-tracy with the zones for the Tracy profiler, see the README
set builddir=build
set options=
if /i "%~1"=="tracy" (
	set builddir=build-tracy
	set options=-DNEBENAN_PROFILE=ON
)
cmake -S . -B %builddir% %options%
if errorlevel 1 exit /b 1
cmake --build %builddir% --config Release --parallel
if errorlevel 1 exit /b 1
echo Fertig: %builddir%\bin\Release\nebenan_demo.exe
