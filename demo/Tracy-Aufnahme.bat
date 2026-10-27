@echo off
rem Records the demolition for the Tracy profiler 0.13.1: 900 steps of 1/60 s at x2 with 8 threads, the physics not
rem beside drawing, as in "Skalierung messen". Every machine takes the same steps. tracy-capture.exe writes abriss.tracy
rem and tracy-update.exe packs it with Zstd. Without them in this folder, the script fetches them first from the release
rem of Tracy 0.13.1 on GitHub and checks its SHA-256. Needs the demo built with NEBENAN_PROFILE, see the README.
setlocal
cd /d "%~dp0"
rem With the argument fetch it only fetches Tracy, the CI tests that
if "%~1"=="fetch" goto :fetch
if not exist tracy-capture.exe call :fetch
if not exist tracy-capture.exe (
	echo Ohne tracy-capture.exe nimmt nur der Tracy Profiler auf, vorher mit Connect auf 127.0.0.1.
	pause
	exit /b
)
echo Die Demo reisst die Stadt bei x2 mit 8 Threads in 900 Schritten ab und beendet sich dann.
start "" nebenan_demo.exe --demolition --threads 8 --fragment-scale 2 --serial-physics --frames 900
echo Bleibt es bei "Connecting...", ist die Demo ohne Tracy gebaut: Fenster schliessen und die Demo aus
echo nebenan-demo-windows-tracy oder von build.bat tracy nehmen.
tracy-capture.exe -o abriss-lz4.tracy -a 127.0.0.1 -f
if exist tracy-update.exe (
	tracy-update.exe -z 10 abriss-lz4.tracy abriss.tracy
	del abriss-lz4.tracy
) else (
	move /y abriss-lz4.tracy abriss.tracy > nul
)
echo Fertig: abriss.tracy
pause
exit /b

:fetch
set "zip=tracy-0.13.1.zip"
set "url=https://github.com/wolfpld/tracy/releases/download/v0.13.1/windows-0.13.1.zip"
set "sha=ee6db1a7e71a12deb5973a8dbfdf9f36d3635bec0e0b31b1cc74f28de7dac4c9"
echo Lade Tracy 0.13.1 von GitHub, 12 MB.
"%SystemRoot%\System32\curl.exe" -sSLf -o %zip% %url%
if errorlevel 1 exit /b
certutil -hashfile %zip% SHA256 | findstr /i /c:%sha% > nul
if errorlevel 1 (
	echo Die Pruefsumme von Tracy stimmt nicht, nichts entpackt.
	del %zip%
	exit /b
)
"%SystemRoot%\System32\tar.exe" -xf %zip% tracy-capture.exe tracy-update.exe tracy-profiler.exe
del %zip%
exit /b
