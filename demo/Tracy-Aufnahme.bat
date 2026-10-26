@echo off
rem Records the demolition for the Tracy profiler 0.13.1: 900 steps of 1/60 s at x2 with 8 threads, the physics not
rem beside drawing, as in "Skalierung messen". Every machine takes the same steps. Tracy waits after Connect and
rem records from the first step until the demo quits. Needs the demo built with NEBENAN_PROFILE, see the README.
cd /d "%~dp0"
echo Die Demo reisst die Stadt bei x2 mit 8 Threads in 900 Schritten ab und beendet sich dann.
echo Tracy zeichnet auf, wenn es vorher mit Connect auf 127.0.0.1 wartet.
start "" /wait nebenan_demo.exe --demolition --threads 8 --fragment-scale 2 --serial-physics --frames 900
echo Fertig. Jetzt in Tracy die Aufnahme speichern.
pause
