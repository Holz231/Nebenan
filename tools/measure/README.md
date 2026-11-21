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
| `tolerance122.py`, `check122.sh`, `eval122.py` | Runde 122: die Toleranzen auf `active-ragdoll-121`, auch für die Summe dessen, was Schienbeine über 37 mm hinaus springen, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE122.txt) |
| `rhythm123.py`, `pelvis123.py`, `tolerance123.py`, `check123.sh`, `eval123.py` | Runde 123: `rhythm123.py` zählt die Wechsel, nach denen die Füße zusammen statt im Wechsel abheben, `pelvis123.py` zeigt die Updates um den härtesten Sprung des Beckens, dazu die Toleranzen auf `active-ragdoll-122`, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE123.txt) |
| `land124.py`, `tolerance124.py`, `check124.sh`, `eval124.py` | Runde 124: `land124.py` zeigt für jede Landung, wie weit die Hüfte entlang der Zehen vor dem Knöchel liegt, wie der Fuß gerollt landet und wie weit die Wurzel danach sinkt, dazu die Toleranzen auf `active-ragdoll-123`, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE124.txt) |
| `tolerance126.py`, `check126.sh`, `eval126.py`, `falls126.py` | Runde 126: die Toleranzen auf `active-ragdoll-124`, die unabhängige Prüfung mit vier Treffertabellen und ihre Bedingungen, die Balance als eigener Schutz, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE126.txt). `falls126.py` teilt die Stürze zweier Stände nach Tabelle, nach der Richtung, in die der Treffer schiebt, und nach leichten Treffern auf |
| `turn128.py`, `tolerance128.py`, `check128.sh`, `eval128.py` | Runde 128: `turn128.py` fährt die Kehrtwenden im Rennen mit Blick in Laufrichtung nach und sagt für jede Stelle, was das Bein tut, wo sein Schienbein am härtesten springt: es steht, hebt ab, schwingt oder ist gerade gelandet. Dazu die Toleranzen auf `active-ragdoll-127`, die unabhängige Prüfung und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE128.txt) |
| `where129.py`, `tolerance129.py`, `check129.sh`, `eval129.py` | Runde 129: `where129.py` fährt einen Wechsel mit festem Blick nach, oder mit `run` die acht Wechsel im Rennen, und sagt für jede Stelle, was das Bein tut, wo sein Schienbein am härtesten springt: es hebt ab, schwingt, ist gerade gelandet oder steht. Dazu die Toleranzen auf `active-ragdoll-128`, die unabhängige Prüfung mit 23 stetigen Läufen, die ab 5 s Byte für Byte gleich bleiben müssen, und ihre Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE129.txt) |
| `steady130.py`, `tolerance130.py`, `check130.sh`, `eval130.py`, `check130b.sh`, `eval130b.py` | Runde 130: `steady130.py` sucht über 231 stetige Läufe von 1,00 bis 4,80 m/s, vorwärts, seitwärts und rückwärts, oder mit `FINE=1` über 96 Läufe des schnellen Rennens in Schritten von 0,01 m/s, welche sich ab 5 s gegen einen alten Stand ändern: Damit wird die Stärke einer Grenze gesucht, die den stetigen Gang nicht anfassen soll. Dazu die Toleranzen auf `active-ragdoll-129` und die zwei unabhängigen Prüfungen der Runde mit ihren Bedingungen, die Regeln liegen in [docs/Regeln](../../docs/Regeln/RULE130b.txt) |
| `hops131.py`, `tolerance131.py`, `check131.sh`, `eval131.py` | Runde 131: `hops131.py` zählt in den Wechseln im Rennen mit festem Blick die Füße, die sofort wieder abheben, höchstens zwei Updates am Boden zwischen zwei Schwüngen, während der andere Fuß am Boden ist, und wie hart das Schienbein dabei springt: eine Zielgröße für einen Eingriff, der nur wenige harte Stellen ändert und gegen die Streuung der Summen nicht zu prüfen wäre. Dazu die Toleranzen auf `active-ragdoll-130` und die unabhängige Prüfung der Runde mit ihren Bedingungen, die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE131.txt) |
| `tolerance133.py`, `check133.sh`, `eval133.py` | Runde 133: die Toleranzen auf `active-ragdoll-132` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgrößen sind die Summe und die Zahl der Stellen, an denen das härteste Schienbein in den acht Wechseln im Rennen über 37 mm springt, aus `where129.py`, und die Schienbeine mit Blick in Laufrichtung; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE133.txt) |
| `lifts134.py`, `tolerance134.py`, `check134.sh`, `eval134.py` | Runde 134: `lifts134.py` misst in den Wechseln im Rennen mit festem Blick jedes Abheben, bei dem der Gang um mehr als 0,7 m/s schneller ist als die Figur und der Fuß nicht über die Zehen gerollt ist: den härtesten Sprung des Schienbeins im Update des Abhebens und in den zwei danach, getrennt danach, ob die Beine noch rückwärts gehen. Dazu die Toleranzen auf `active-ragdoll-133` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist die Summe dessen, was das härteste Schienbein in den acht Wechseln im Rennen über 37 mm hinaus springt, aus `where129.py`, auf drei Satzpaaren; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE134.txt) |
| `flips135.py`, `tolerance135.py`, `check135.sh`, `eval135.py` | Runde 135: `flips135.py` misst in den Wechseln im Rennen mit festem Blick jeden Fuß, der am Boden steht, während die Beine zwischen vorwärts und rückwärts wechseln: den härtesten Sprung seines Schienbeins im Update danach und in den zwei folgenden, und wie weit sein Rollen in diesem Update zurückging. Dazu die Toleranzen auf `active-ragdoll-134` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist das Ereignis selbst als Eigenschaft: von seitwärts auf rückwärts auf drei Satzpaaren kein Fuß mit dem Schienbein über 37 mm, und alt mindestens einer; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE135.txt) |
| `pitch136.py`, `tolerance136.py`, `check136.sh`, `eval136.py` | Runde 136: `pitch136.py` misst in den Wechseln im Rennen mit festem Blick jeden Schwung, in dem der Rennanteil des Gangs um mehr als 0,1 unter den beim Abheben fällt: den härtesten Sprung seines Schienbeins und wie weit sich sein Fuß in einem Update höchstens zur Ferse und zu den Zehen dreht. `lab101` schreibt dafür den Rennanteil, `gaitRun`, als letzte Spalte. Dazu die Toleranzen auf `active-ragdoll-135` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist das Ereignis selbst: kein solcher Schwung, der seinen Fuß in einem Update um mehr als 0,17 rad zur Ferse dreht, und alt mindestens sechs; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE136.txt) |
| `lifts137.py`, `tolerance137.py`, `check137.sh`, `eval137.py` | Runde 137: `lifts137.py` misst in den Wechseln im Rennen mit festem Blick jeden Abheber ab dem Wechsel: die Rennanteile von Gang und Feder beim Abheben, wie weit die Figur hinter ihrem Gang zurückbleibt, wie hoch der Fuß kommt und wie weit er in einem Update steigt, wie weit er sich zur Ferse dreht und den härtesten Sprung seines Schienbeins im Update nach dem Abheben und den drei folgenden. Gezählt sind die Abheber, bei denen die Feder mehr als 0,5 über dem Gang rennt und die Figur mehr als 0,8 m/s langsamer ist als ihr Gang. Dazu die Toleranzen auf `active-ragdoll-136` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist das Ereignis selbst: kein solcher Abheber mit dem Schienbein über 37 mm, und alt mindestens zwei; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE137.txt) |
| `lifts138.py`, `tolerance138.py`, `check138.sh`, `eval138.py` | Runde 138: `lifts138.py` misst in den Wechseln im Rennen mit festem Blick jeden Abheber ab dem Wechsel: ob die Beine rückwärts gehen, wie weit die Figur gegen die Richtung gesteuert wird, in die sie sich bewegt, wie schnell das Bein beim Abheben länger wurde und wie abrupt dieses Wachsen danach endete, und den härtesten Sprung seines Schienbeins im Update nach dem Abheben und den drei folgenden. Gezählt sind die Abheber mit rückwärts gehenden Beinen, während die Figur mehr als halb gegen ihre Richtung gesteuert wird. Dazu die Toleranzen auf `active-ragdoll-137` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist die Richtung an der Summe von rückwärts auf vorwärts und an diesen Abhebern; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE138.txt) |
| `land139.py` | Runde 139: misst von seitwärts auf rückwärts im Rennen mit festem Blick jede Stelle, an der das härteste Schienbein 0,6 s oder später nach dem Wechsel an einem landenden Bein über 37 mm springt: wie lange sein Schwung dauerte, wie weit sein Fuß ging, die Geschwindigkeit des Gangs beim Abheben, die der Figur beim Abheben und beim Landen, die Höhe der Wurzel beim Landen, wie weit der Fuß im Update der Landung bremst und wie schnell das Knie dann läuft. Über 0,88 m Höhe der Wurzel streckt sich das Bein, während der Gang ins Rennen wechselt, darunter hob der Schwung gehend ab, siehe Runde 139 in [docs/Roadmap.md](../../docs/Roadmap.md) |
| `hops140.py`, `tolerance140.py`, `check140.sh`, `eval140.py` | Runde 140: `hops140.py` zählt von seitwärts auf rückwärts im Rennen mit festem Blick die Füße, die sofort wieder abheben, nachdem sie gelandet sind, wie `hops131.py`, aber an 96 statt 24 Punkten je Satz: Ein solcher Fuß kommt nur in einem schmalen Fenster des Zyklus und einem schmalen Band der Geschwindigkeit um 4,4 m/s. Dazu die Toleranzen auf der Bibliothek von `active-ragdoll-139` und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist das Ereignis selbst: auf vier frischen Sätzen nahe dem Band kein solcher Fuß mit dem Schienbein über 37 mm, und alt mindestens einer; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE140.txt) |
| `tolerance141.py`, `check141.sh`, `eval141.py` | Runde 141: die Toleranzen auf `active-ragdoll-140`, Byte für Byte dieselben wie in Runde 140, und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße ist die Antwort der Kapsel aus der Tabelle mit festem Blick (`resp` von `sweeph111.py`): rennend von einer Seite zur anderen im Mittel kleiner und spätestens nicht später, über alle Wechsel im Mittel kleiner; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE141.txt) |
| `tolerance143.py`, `check143.sh`, `eval143.py` | Runde 143: die Toleranzen auf `active-ragdoll-141`, dessen Bibliothek die von `active-ragdoll-142` ist, und die unabhängige Prüfung der Runde mit ihren Bedingungen. Zielgröße sind die Abheber von `lifts138.py` auf den drei Paaren: mit dem Schienbein über 37 mm seltener und mit kleinerer Summe darüber; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE143.txt) |
| `walk144.py`, `tolerance144.py`, `check144.sh`, `eval144.py` | Runde 144: `walk144.py` misst gehend von seitwärts auf rückwärts mit festem Blick an 24 Stellen je Satz den härtesten Sprung von Zehen oder Fersen und von einem Schienbein, schnell genug für zwölf Sätze. Dazu die Toleranzen auf `active-ragdoll-143` und die unabhängige Prüfung der Runde mit ihren Bedingungen: Zielgröße sind Zehen und Fersen gehend von seitwärts auf rückwärts auf zwölf frischen Sätzen, die Schienbeine dort dürfen nicht mehr springen; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE144.txt) |
| `tolerance145.py`, `check145.sh`, `eval145.py` | Runde 145: die Toleranzen auf `active-ragdoll-144`, jetzt auch für die Antwort im Seitenwechsel gehend und rennend, im Mittel und die späteste, und die unabhängige Prüfung der Runde mit ihren Bedingungen: Zielgröße ist die Antwort im Seitenwechsel im Rennen auf drei frischen Paaren von Sätzen, im Mittel und die späteste; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE145.txt) |
| `track147.py`, `tolerance147.py`, `check147.sh`, `eval147.py` | Runde 147: `track147.py` lässt stetige Läufe von 9 s laufen, vorwärts mit Blick in Laufrichtung oder mit festem Blick, seitwärts und rückwärts, und misst ab 5 s die Spur, wie weit aufeinanderfolgende linke und rechte Landungen quer zum Weg auseinanderliegen, mit dem, was `gait100.py` über den Lauf sagt. Dazu die Toleranzen auf `active-ragdoll-146` und die unabhängige Prüfung der Runde: Zielgröße ist die Spur im stetigen Lauf vorwärts auf zehn frischen Geschwindigkeiten; die Regel liegt in [docs/Regeln](../../docs/Regeln/RULE147.txt) |
| `facing119.py`, `jumps119.py` | Vergleich zweier Stände mit Blick in Laufrichtung, und die härtesten Sprünge eines Laufs mit dem, was die Figur dort tut |
| `table.sh`, `tests.sh`, `sanitize.sh` | Treffertabelle, Tests mit Hashes, Sanitizer |
| `cost_pairs.sh`, `cost.py`, `bench_pairs.sh`, `bench.py` | Kosten und Benchmark gepaart, alt gegen neu |
| `cost_count.sh` | Runde 120: die Befehle, die das Update der Figuren auf den beiden Ständen von `cost_pairs.sh` ausführt, gezählt mit callgrind, in jedem Lauf dieselbe Zahl |
| `speeds.py` | Welche Geschwindigkeiten schon gelaufen sind, und für neue die Dauer eines Zyklus aus der Formel der Kadenz, ohne einen Lauf: So wird ein frischer Satz für eine unabhängige Prüfung festgelegt |

Die Zahlen in den Namen sind die Runden, in denen ein Werkzeug entstand. Eine neue Runde bekommt ihre eigene Prüfung und
Auswertung daneben, mit frischen Geschwindigkeiten.
