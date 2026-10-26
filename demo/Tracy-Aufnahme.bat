@echo off
rem Records the demolition for the Tracy profiler 0.13.1: 900 steps of 1/60 s at x2 with 8 threads, the physics not
rem beside drawing, as in "Skalierung messen". Every machine takes the same steps. With tracy-capture.exe of Tracy
rem 0.13.1 in this folder it writes abriss.tracy, packed with Zstd if tracy-update.exe is here as well. Without, the
rem Tracy profiler records after Connect. Needs the demo built with NEBENAN_PROFILE, see the README.
cd /d "%~dp0"
echo Die Demo reisst die Stadt bei x2 mit 8 Threads in 900 Schritten ab und beendet sich dann.
start "" nebenan_demo.exe --demolition --threads 8 --fragment-scale 2 --serial-physics --frames 900
if not exist tracy-capture.exe (
	echo Ohne tracy-capture.exe in diesem Ordner nimmt nur der Tracy Profiler auf, mit Connect auf 127.0.0.1.
	pause
	exit /b
)
tracy-capture.exe -o abriss-lz4.tracy -a 127.0.0.1 -f
if exist tracy-update.exe (
	tracy-update.exe -z 10 abriss-lz4.tracy abriss.tracy
	del abriss-lz4.tracy
) else (
	move /y abriss-lz4.tracy abriss.tracy > nul
)
echo Fertig: abriss.tracy
pause
