# Messwerkzeuge

Die Werkzeuge, mit denen die Runden der gesteuerten Figur gemessen werden, so wie Runde 118 sie benutzt hat. Sie gehören
nicht zur Bibliothek: Sie bauen die Bibliothek mit Schaltern, lassen eine Figur einem Skript von Geschwindigkeiten
folgen und werten aus, was sie dabei tut. Bis Runde 118 lagen sie nur im Arbeitsordner der Sitzung.

Gebraucht werden gcc, cmake und Python 3 mit numpy, unter Linux oder unter Windows in WSL2. Alle Ergebnisse landen in
`out/` in diesem Ordner, den git ignoriert, oder im Ordner aus `NB_MEASURE_OUT`. Gebaut wird der Quelltext dieses
Repositories oder der aus `NB_SRC`: So lässt sich ein älterer Stand, als zweiter Arbeitsbaum ausgecheckt, unter eigenem
Namen daneben bauen und mit denselben Werkzeugen messen.

## Ablauf einer Runde

```
tools/measure/build.sh neu                               # Bibliothek und Werkzeuge nach out/vb/neu und out/res/neu
NB_SRC=/pfad/zum/alten/stand tools/measure/build.sh alt  # der Stand, gegen den gemessen wird
tools/measure/build.sh versuch "-DNB_X_SCHALTER=0.8f"    # eine Variante mit einem Schalter im Quelltext
tools/measure/sweeps.sh neu dev devf h1 h2 h5 h6         # Entwicklungssätze
python3 -I tools/measure/full118.py alt neu              # Vergleich auf den Entwicklungssätzen
python3 -I tools/measure/spread118.py alt neu            # Streuung zwischen ähnlichen Sätzen und Änderung dagegen
tools/measure/tests.sh neu                               # alle Tests, mit jedem geänderten Hash
tools/measure/check118.sh alt neu                        # unabhängige Prüfung von Runde 118
python3 -I tools/measure/eval118.py alt neu              # ihre Bedingungen
tools/measure/sanitize.sh                                # Sanitizer für Adressen und undefiniertes Verhalten
```

Kosten und Benchmark brauchen zwei Release-Builds mit cmake, alt und neu, und eine Maschine, auf der sonst nichts läuft:

```
tools/measure/cost_pairs.sh alt_quelle alt_build neu_quelle neu_build && python3 -I tools/measure/cost.py
tools/measure/bench_pairs.sh alt_build neu_build r && tools/measure/bench_pairs.sh alt_build neu_build s
TAGS="r1 r2 r3 r4 r5 r6 s1 s2 s3 s4 s5 s6" python3 -I tools/measure/bench.py
```

## Was gemessen wird

Eine Szene ist ein Wechsel, etwa aus dem Stand losrennen oder rennend von seitwärts auf rückwärts, und läuft an 24
Stellen, über einen Zyklus der Gangart verteilt. Ein Satz ist ein Paar von Geschwindigkeiten für Gehen und Rennen, 21
Szenen mit festem Blick sind 504 Stellen, 13 Szenen mit Blick in Laufrichtung 312. Je Stelle: der größte Sprung von
Füßen, Schienbeinen, Händen und Becken, die zweite Differenz des Ursprungs eines Knochens je Update, mit den Grenzen 37,
37, 15 und 10 mm, Mittel und oberes Zehntel dieser Sprünge, wie tief das Becken unter dem Stand steht, ob die Füße
kreuzen, der längste Flug, die Zeit, in der beide Füße schwingen, und die Zeit, bis die Kapsel 90 % des gewünschten
Tempos geht. Die Sätze stehen in `common.sh`. Geschwindigkeiten, die schon gelaufen sind, taugen nicht mehr für eine
unabhängige Prüfung, siehe die Runden in [Roadmap](../../docs/Roadmap.md).

## Dateien

| Datei | |
| --- | --- |
| `lab101.c` | eine gesteuerte Figur folgt einem Skript `t:vx:vz[:blick],...` und schreibt jedes Update als CSV |
| `bal103.c` | die Treffertabelle: 8 928 Treffer auf eine stehende Figur |
| `cost110.c` | die Kosten von 20 gesteuerten Figuren je Update in vier Modi |
| `build.sh`, `common.sh` | Bibliothek und Werkzeuge bauen, Ordner und Sätze |
| `sweeps.sh` | Sätze laufen lassen: `sweep101.py` und `sweepf102.py` für die Entwicklung, `sweeph111.py` und `toes112.py` mit festem Blick, `sweepf112.py` mit Blick in Laufrichtung |
| `gait100.py`, `extra118.py`, `bones.txt` | Auswertung eines Laufs: Sprünge, Becken, Füße, Flug |
| `cross118.py`, `full118.py`, `spread118.py` | Vergleich zweier Stände auf den Entwicklungssätzen, mit der Streuung zwischen ähnlichen Sätzen |
| `start118.py`, `first118.py` | ein einzelner Lauf in Zahlen und Update für Update |
| `check118.sh`, `eval118.py` | die unabhängige Prüfung von Runde 118 und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE118.txt) |
| `tolerance119.py`, `check119.sh`, `eval119.py` | Runde 119: die Toleranzen aus drei Paaren ähnlicher Sätze, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE119.txt) |
| `tolerance120.py`, `check120.sh`, `eval120.py` | Runde 120: die Toleranzen aus Paaren von Sätzen, deren Geschwindigkeiten höchstens 5 % auseinanderliegen, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE120.txt) |
| `tolerance121.py`, `check121.sh`, `eval121.py` | Runde 121: die Toleranzen auf `active-ragdoll-120`, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE121.txt) |
| `lift121.py`, `fixed121.py` | Jedes Abheben eines Fußes in einem Lauf mit der Streckung seines Beins und den Sprüngen danach, und der Vergleich zweier Stände auf Sätzen mit festem Blick |
| `facing119.py`, `jumps119.py` | Vergleich zweier Stände mit Blick in Laufrichtung, und die härtesten Sprünge eines Laufs mit dem, was die Figur dort tut |
| `table.sh`, `tests.sh`, `sanitize.sh` | Treffertabelle, Tests mit Hashes, Sanitizer |
| `cost_pairs.sh`, `cost.py`, `bench_pairs.sh`, `bench.py` | Kosten und Benchmark gepaart, alt gegen neu |
| `cost_count.sh` | Runde 120: die Befehle, die das Update der Figuren auf den beiden Ständen von `cost_pairs.sh` ausführt, gezählt mit callgrind, in jedem Lauf dieselbe Zahl |
| `speeds.py` | Welche Geschwindigkeiten schon gelaufen sind, und für neue die Dauer eines Zyklus aus der Formel der Kadenz, ohne einen Lauf: So wird ein frischer Satz für eine unabhängige Prüfung festgelegt |

Die Zahlen in den Namen sind die Runden, in denen ein Werkzeug entstand. Eine neue Runde bekommt ihre eigene Prüfung und
Auswertung daneben, mit frischen Geschwindigkeiten.
