# Optimierungen: vorgemerkt, umgesetzt, ausgesiebt

Review einer externen Liste von 93 Optimierungsideen, geprüft gegen den Code und gegen Messungen vom 3. Oktober
2026. Die Nummern entsprechen der Liste. Nichts ist gelöscht: Ausgesiebtes bleibt mit Grund stehen, falls man doch
noch einmal nachsehen will.

| Zeichen | Bedeutung |
| --- | --- |
| 🔜 | Vorgemerkt, mit Priorität: **hoch**, **mittel**, **niedrig** oder **Stadt** (erst für eine große Stadt) |
| ✅ | Schon umgesetzt, in Nebenan, der Demo oder Box3D |
| ⛔ | Ausgesiebt, mit Grund |

Feste Regeln, an denen jede Idee gemessen wurde: Kein Trümmerteil wird gelöscht, keine Kollision wird abgeschaltet,
nichts bleibt in der Luft hängen, und das Ergebnis bleibt deterministisch.

## Reihenfolge der vorgemerkten Arbeit

Ziel seit dem 3. Oktober 2026: Bruchstückgröße ×4 und Szenen so groß wie möglich, erst die Physik, dann die Grafik.

1. ✅ **Schutt aus den Schleifen pro Frame nehmen** (Nr. 1). `nbWorld_Update` braucht in der Stadt 26 % weniger Zeit,
   mit doppelter Bruchstückgröße 22 %, mit vierfacher kaum etwas.
2. ✅ **Große Arrays wachsen an Ort und Stelle** (eigene Idee, unten). Keine Ruckler mehr, wenn in großen Szenen ein
   Array voll wird, und kein Speicher auf Vorrat. Ersetzt das Reservieren von Platz vorab.
3. ✅ **Hüllen ohne Box3Ds Hüllen-Tabelle** (eigene Idee, unten). Keine Tabelle mehr, die mitten im Spiel voll werden
   und alles aufhalten kann. Laden 6 % schneller, 17 MB weniger bei 1 024 Häusern.
4. ✅ **Laden und Speicher großer Szenen, verlustfrei** (eigene Idee, unten). Alle stehenden Bruchstücke eines Hauses
   teilen sich einen statischen Box3D-Körper: 16 % weniger Speicher, Laden bei 1 024 Häusern fast doppelt so schnell,
   Einschläge dort 40 % billiger, Ergebnisse bitgleich. Weniger Bruchstücke gibt es dafür nicht: Häuser erst beim
   ersten Treffer zu zerlegen machte genau diesen Treffer langsamer, und größere Zellen ließen größere Stücke abbrechen.
   Beides steht unten mit den Messwerten.
5. ✅ **Neue Bruchstücke gesammelt in den Box3D-Baum einfügen** (eigene Idee, unten). Laden ab 64 Häusern 15 bis
   24 % schneller, und ein Einschlag kostet in der Stadt aus 1 024 Häusern so viel wie in der aus 16, vorher rund 60 %
   mehr.
6. ✅ **Geometrie nur einmal speichern** (eigene Idee, unten). Box3D benutzt die Hülle, die Nebenan in die Form jedes
   Bruchstücks baut, statt eine eigene Kopie zu halten: 12 % weniger Speicher pro Bruchstück, 14 % weniger
   Prozess-Speicher, 389 statt 450 MB bei 1 024 Häusern, und schneller geladen. Einschläge kosten gleich viel oder
   weniger, die Ergebnisse bleiben bitgleich.
7. ✅ **Schneller laden, kleine Einschläge ohne Worker** (eigene Idee, unten). Die Worker messen beim Laden die
   Kontaktflächen zwischen den Bruchstücken, und eine Granate bei ×4 läuft auf dem aufrufenden Thread: Laden 12 bis
   15 % schneller, eine Granate auf ein Haus bis zu 14 % billiger, und kein Einschlag wird langsamer. Die Ergebnisse
   bleiben bitgleich.
8. ✅ **Biegemomente nur bei Bedarf** (eigene Idee, unten). Die zweiten Momente der Verbindungen, die nur die
   Lastprüfung liest, rechnet und speichert Nebenan nur noch für Bauwerke, die sie prüft: 4 % weniger
   Prozess-Speicher, 5 % weniger Instruktionen pro Einschlag und 3 % beim Laden. Die Ergebnisse bleiben bitgleich.
9. ✅ **Häuser gesammelt laden** (eigene Idee, unten). `nbCreateDestructibles` legt viele Zerstörbare auf einmal an:
   Während der aufrufende Thread ein Haus in die Welt einbaut, berechnen die Worker schon die Zellen des nächsten.
   Eine Stadt lädt so ein Drittel schneller, 1 024 Häuser in 0,65 statt 0,99 s, mit denselben Bruchstücken,
   Verbindungen und Ids wie einzeln angelegt.
10. Grafik, sobald die Physik fertig ist: **Räumliche Render-Seiten und Frustum-Culling** (Nr. 49 bis 51) und **nur
    bewegte Transformationen hochladen** (Nr. 74). Beides wächst mit der Szene, denn die Demo zeichnet jedes Bild alles
    und lädt bei jeder Bewegung alle Transformationen hoch.
11. Für eine große Stadt: Regionen, HLOD, Verdeckung, Streaming, Physik nur in aktiven Regionen (Nr. 52 bis 60, 65,
    69, 75, 88 bis 92).

Was nur kleinen Bruchstücken hilft, bringt bei ×4 kaum etwas und ist zurückgestellt: Trümmer früher zur Ruhe bringen,
ein billigeres Trümmerbudget. Grobe Kollisionsformen für Schutt (Nr. 2) sind inzwischen gemessen und verworfen, siehe
„Zerstörte Stadt unter Dauerfeuer“.

Zuerst immer messen, wo die Zeit hingeht. Das Menü der Demo trennt Simulation, Box3D-Schritt und Grafik und sagt, wer
die FPS begrenzt. „Messwerte kopieren“ legt alles in die Zwischenablage.

Mit vierfacher Bruchstückgröße rechnet die Simulation wenig: In der Benchmark-Stadt kostet ein Frame auf der VM im
Mittel 0,65 ms, davon 0,07 ms `nbWorld_Update`. Läuft die Demo dort mit rund 290 FPS, also 3,4 ms pro Bild, geht der
größere Teil wahrscheinlich an die Grafik.

## Große Szenen bei ×4

Skalierungstest auf der VM, zuletzt am 4. Oktober 2026: Städte aus 16 bis 1 024 Häusern wie im Benchmark,
Bruchstückgröße ×4, 4 Threads. Nach dem Aufbau 120 Frames Ruhe, dann 20 s lang alle fünf Frames eine Granate auf ein
zufälliges Haus.

| Häuser | Bruchstücke | Aufbau | Speicher | Frame unter Beschuss |
| ---: | ---: | ---: | ---: | ---: |
| 16 | 3 848 | 0,01 s | 16 MB | 0,4 ms |
| 64 | 15 425 | 0,04 s | 35 MB | 0,3 ms |
| 256 | 61 538 | 0,16 s | 104 MB | 0,4 ms |
| 1 024 | 246 141 | 0,65 s | 375 MB | 0,3 ms |

Gemessen mit allen Änderungen für große Szenen: große Arrays, die an Ort und Stelle wachsen, Hüllen ohne Box3Ds
Hüllen-Tabelle, statische Formen, die gesammelt in Box3Ds Suchbaum gehen, ein statischer Box3D-Körper pro Haus,
Geometrie, die jedes Bruchstück nur einmal speichert, Kontaktflächen, die beim Laden die Worker messen, Verbindungen
ohne Biegemomente und Häuser, die gesammelt laden. Der Aufbau ist der Median aus sechs Läufen bei 16 und 64 Häusern
und aus zwölf bei 256 und 1 024, alle Häuser auf einmal mit `nbCreateDestructibles`. Einzeln angelegt brauchten sie
0,017, 0,063, 0,26 und 0,99 s. Wie lange der Aufbau großer Städte dauert, hängt stark von der Tagesform der VM ab: Sie
gibt frischen Speicher oft sehr langsam heraus, und die zwölf Läufe mit 1 024 Häusern brauchten zwischen 0,96 und
1,69 s. Bei der ersten Messung, noch ohne diese Änderungen und auf einer langsamen VM, brauchten 1 024 Häuser 3 bis
8 s und 533 MB, und ein Frame unter Beschuss kostete 0,5 bis 0,7 ms.

- **Die Physik pro Frame hängt nicht an der Größe der Szene.** Box3D rechnet nur, was sich bewegt, und Nebenan geht im
  Update nie über die ganze Welt, nur beim Verstellen von Reglern und beim Löschen der Welt. In Ruhe kostet die Welt bei
  jeder Größe so gut wie nichts. Seit die statischen Formen gesammelt in Box3Ds Suchbaum gehen und jedes Haus einen
  statischen Körper hat, gilt das auch für Einschläge: im Mittel 0,03 bis 0,04 ms pro Frame von 16 bis 1 024 Häusern.
  Vorher wuchsen sie mit der Stadt, bei 1 024 Häusern auf 0,05 bis 0,065 ms.
- **Ruckler, wenn ein großes Array voll wurde** (behoben, siehe „Große Arrays wachsen an Ort und Stelle“). Bei
  65 536 Bruchstücken verdoppelten sich das Formen-Array von Box3D und das Bruchstück-Array von Nebenan. Das Update,
  das darauf stieß, dauerte je nach Tagesform der VM 16 bis 137 ms, bei 16 384 Bruchstücken 6 bis 9 ms. Der erste
  Einschlag in der Stadt aus 1 024 Häusern kostete 8 bis 15 ms: Der erste Trümmerkörper bekommt eine Box3D-Nummer
  hinter allen 246 000 statischen Körpern, und Nebenan musste seine Zuordnung von Körpern zu Akteuren bis dorthin
  füllen. Die meiste Zeit gibt das System frischen Speicher heraus, auf einem PC geht das schneller als auf der VM.
  Seit jedes Haus nur einen statischen Körper hat, beginnen die Nummern der Trümmerkörper bei rund 1 000.
- **Laden** braucht auf der VM rund 0,65 ms pro Haus, wenn alle Häuser gesammelt laden, einzeln rund 1 ms, in großen
  Städten nicht mehr als in kleinen. Bei der ersten Messung, auf der langsamen VM und vor den Änderungen für große
  Szenen, waren es 3 ms in kleinen Städten und 3 bis 8 ms bei 1 024 Häusern. Der Hauptthread rechnet die Hälfte der
  Zeit mit den Workern an den Voronoi-Zellen der Vorzerlegung und rund ein Sechstel an den Kontaktflächen zwischen den
  Bruchstücken, die er früher allein maß. Box3D-Formen samt Suchbaum kosten ihn ein Zehntel bis ein Sechstel, die
  Worker zu wecken 4 bis 6 %. Box3D-Körper legt Nebenan nur noch einen pro Haus an, vorher einen pro Bruchstück. Das
  Einsortieren in Box3Ds Suchbaum, vorher knapp ein Fünftel des Aufbaus (`b3InsertLeaf`), kostet gesammelt noch wenige
  Prozent. Weggefallen sind das Umkopieren von Arrays, vorher 18 %, Box3Ds Hüllen-Tabelle, vorher rund 7 %, und Box3Ds
  Kopien der Hüllen. Einen großen Teil kostet der Kernel, der frische Speicherseiten nullt, vor allem für die Formen
  der Bruchstücke. Wie viel, schwankt auf der VM stark, auf dem Hauptthread zwischen einem Zwanzigstel und einem
  Drittel der Samples. Gemessen mit `perf` in der Stadt aus 1 024 Häusern, Haus für Haus angelegt. Dabei ist der
  Hauptthread zu 93 % beschäftigt und sind es die Worker zu 55 %. Gesammelt laden sind es 96 und 83 %.
- **Speicher**: rund 0,37 MB pro Haus aus 240 Bruchstücken, ein Fünftel davon in Box3D. Vorher waren es 0,38 MB mit
  den Biegemomenten in jeder Verbindung, davor 0,44 MB, gut die Hälfte davon in Box3D, und davor 0,52 MB, knapp zwei
  Drittel davon in Box3D.

## Zerstörte Stadt unter Dauerfeuer

Gemessen am 4. Oktober 2026 auf der VM: die Stadt der Demo, 20 Häuser mit zwei oder drei Etagen, Bruchstückgröße ×4,
4 Threads. Zwölf Granaten pro Sekunde, erst nach dem Skript der Demo, dann auf zufällige Reste, auch in die
Schutthaufen, zehn Minuten lang. Danach liegen 24 000 Bruchstücke herum, mehr werden es nicht, alles ist klein. Gezählt
sind in jedem Frame die berührenden Kontakte der wachen Trümmer, nach dem, was sie berühren, gemittelt über je 30 s:

| Stand | Box3D-Schritt | Wache Körper | Berührende Kontakte | Trümmer | Schutt | Häuser | Boden |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Häuser stehen noch | 0,3 bis 1,2 ms | 50 bis 270 | 140 bis 540 | 26 bis 36 % | 14 bis 27 % | 24 bis 52 % | 8 bis 13 % |
| Alles flach, Feuer in den Schutt | 3,9 bis 4,3 ms | 760 bis 860 | 2 400 bis 2 750 | 44 bis 47 % | 44 bis 46 % | 0 | 9 bis 10 % |
| Nach dem Feuer | 0,09 ms | 15 | 47 | | | | |

- **Die Kosten sind die Zerstörung selbst.** Jede Granate in den Schutt taut im Mittel 63 Stücke auf und zerteilt kaum
  noch etwas. 81 % davon fliegen mit 1 m/s und mehr los, die meisten mit über 2 m/s. Nur die 15 %, die weiter als 2 m
  vom Einschlag liegen, bekommen von der Explosion weniger als 1 m/s. Ein aufgetautes Stück ist im Mittel eine Sekunde
  wach, dann friert es wieder ein. Bei zwölf Granaten pro Sekunde sind so ständig rund 760 Körper wach.
- **Backen lohnt nicht** (Nr. 2). Der Schutt, den bewegte Trümmer berühren, ist in 56 % der Fälle keine Sekunde
  eingefroren, in 64 % keine drei Sekunden, nur in 24 % länger als zehn. Die Kontakte entstehen am Rand des Geschehens,
  nicht an ruhigen Haufen.
- **Box3D führt dabei 12 600 bis 14 600 wache Kontakte**, von denen knapp ein Fünftel berührt. Der Rest sind sich
  überlappende Hüllquader im dichten Schutt, die in der Kollision mitkosten. Das ließe sich nur in Box3D ändern, über
  den Rand der Hüllquader, und der schützt vor Durchschlägen.
- **Hört das Feuer auf**, ist nach rund 30 s wieder alles eingefroren und kostet nichts mehr.
- Einzelne Box3D-Schritte von 5 bis 46 ms in einzelnen Läufen kamen von der VM. In sechs Läufen derselben Simulation
  lag der langsamste Schritt zwischen 1,1 und 4,8 ms und nie im selben Frame.

## Was die Liste übersieht

Seit der Liste ist dazugekommen oder gemessen worden:

- **Mehr vom Einschlag läuft parallel** (zu Nr. 36, 37). Bruchpunkte, Schätzung des getroffenen Volumens ab 32
  Stücken, Voronoi-Zellen und Hüllen laufen auf den Workern, und das Aufwecken des Schutts läuft gleichzeitig mit den
  Zellen. Einen Einschlag mit weniger als 32 Zellen und Stücken rechnet der aufrufende Thread allein, siehe „Kleine
  Einschläge ohne Worker“. Seriell bleiben die Änderungen an der Box3D-Welt, der Einbau der Stücke und die Inselsuche.
  Eine große Explosion braucht mit 4 Threads 4,8 ms, davon rund 3 ms Box3D-Aufrufe.
- **CCD nur für Trümmer, die durch eine Wand könnten** (zu Nr. 27). Umgesetzt mit `debrisSweepDistance`: Der
  Box3D-Schritt der Stadt braucht 6 bis 9 % weniger Zeit. Würfel von 6 bis 50 cm, mit 6 bis 50 m/s auf 12 oder 20 cm
  dicke Wände geschossen, prallen alle ab.
- **Einfachere Kollisionsformen für Trümmer sind gemessen und verworfen** (zu Nr. 23). Quader statt Zellen: 53 %
  langsamer, weil sie sich dort überlappen, wo die Zellen lückenlos in der Wand liegen. Hüllen mit höchstens 8 oder 12
  Ecken: 4 bis 8 %, im Rauschen. Selbst alle Hüllen vereinfacht, auch Gebäude und Schutt, kostete die Kollision nur
  4 % weniger. Box3D zahlt pro Kontakt, nicht pro Ecke.
- **Der Box3D-Schritt** ist 74 bis 87 % eines Frames der Stadt. Nach Instruktionen sind davon 54 % Kollision (40 %
  Hülle gegen Hülle), 42 % Löser samt CCD, 4 % Broadphase. Ihn bestimmt die Zahl der wachen Kontakte und damit
  Bruchstückgröße und Trümmerbudget.
- **Die Thread-Zahl** stellt die Demo selbst ein, einen pro Performance-Kern, höchstens 8. Mehr Threads als Kerne
  bremsen.

## Physik, Zerstörung und Schutt

| Nr. | Idee | Status | Bewertung |
| ---: | --- | --- | --- |
| 1 | Bewegte Trümmer und Schutt getrennt führen | ✅ | Umgesetzt. Vorher liefen vier Schleifen pro Frame über `world->debris`, Schutt eingeschlossen, eine schrieb sogar `age` und `budgetAge` in jeden Schutt. Jetzt trägt `movingDebris` ein Bit pro Eintrag, gesetzt für alles, was kein Schutt ist, und die Schleifen springen per Bit-Scan über den Schutt, 64 Einträge auf einmal, in derselben Reihenfolge wie vorher. Eine eigene Liste der bewegten Trümmer hätte die Reihenfolge und damit die Simulation geändert. Alter und Budget-Alter sind Zeitstempel gegen eine Weltuhr, Schutt altert ohne Schleife. Bei 60 Hz überschreitet die Uhr die Schonfrist von 0,25 s im selben Schritt wie das alte Hochzählen in `float`, bei anderen Schrittweiten wie 240 Hz kann es einen Schritt abweichen. Die Simulation bleibt bitgleich: derselbe Determinismus-Hash und dieselben Endzustände in der Stadt (×1, ×2, ×4), bei 64 großen Explosionen auf 16 Häusern und im Einschlag-Benchmark, mit 1 und 4 Threads. `nbWorld_Update` in der Stadt mit 4 Threads, Median aus sechs Läufen im Wechsel: ×1 2,65 statt 3,59 ms (−26 %), ×2 0,69 statt 0,88 ms (−22 %), ×4 0,070 statt 0,075 ms, dort liegen am Ende nur 1200 Schutt-Körper statt 34 000. Die vier Schleifen kosten bei ×1 jetzt 0 + 0,21 + 0,38 + 0,85 ms statt 0,39 + 0,45 + 0,61 + 0,90 ms. Übrig bleibt vor allem das Budget: Es rankt und friert ein, das Überspringen war dort nie der große Teil. |
| 2 | Ruhenden Schutt zu groben Kollisionsformen zusammenbacken | ⛔ | Möglich, aber groß: Auflagen, Halter, Aufwecken und Einschläge arbeiten mit den einzelnen Schutt-Stücken. Lohnt nur, wenn viele der wachen Kontakte zwischen bewegten Trümmern und Schutt liegen, denn es zählt die Zahl der Kontakte, nicht die Form (siehe oben). Gemessen und verworfen, siehe „Zerstörte Stadt unter Dauerfeuer“: Der Anteil ist groß, unter Feuer in einer flachen Stadt rund 45 % der berührenden Kontakte. Aber der Schutt, den die Trümmer berühren, ist in 56 % der Fälle keine Sekunde eingefroren, in 64 % keine drei, nur in 24 % länger als zehn. Backen ließen sich nur ruhige Haufen, erreichbar wäre etwa ein Zehntel der Kontakte und damit ein paar Prozent des Box3D-Schritts. Dafür müsste jeder Treffer einen gebackenen Haufen erst wieder zerlegen, und Speicher spart es nicht, die genauen Stücke bleiben. |
| 3 | Genaue Stücke nach dem Backen behalten | ✅ | Schutt wird nie gelöscht, `nbShape` und Hülle bleiben. |
| 4 | Nur Schutt nahe der Explosion aufwecken | ✅ | `nbThawRubbleInBoxes` mit AABB-Abfrage, und nur, was der Einschlag bewegen kann. |
| 5 | Nur den getroffenen Teil einer gebackenen Form neu bauen | ⛔ | Gehört zu Nr. 2, regional bauen. Mit Nr. 2 verworfen. |
| 6 | Gebackene Formen ohne Treffer-Events | ⛔ | Gehört zu Nr. 2. Mit Nr. 2 verworfen. |
| 7 | Gleichzeitig bewegte Trümmer begrenzen | ✅ | `maxDebrisBodies` (1500), Regler „Bewegte Trümmer“ in der Demo. 1000 statt 1500 sparten in einem Einzellauf 24 % des Box3D-Schritts (×1). Das ist eine Einstellung, keine Arbeit. |
| 8 | Physik schlafen legen | ✅ | Box3D schläfert ein, Nebenan macht ruhigen, getragenen Schutt sogar statisch. |
| 9 | Ruhe statt fester Zeit erkennen | ✅ | Ruhe, Auflage, Wackeln am Ort, wegrutschende Auflage. |
| 10 | Aufwecken von Schutt beschränken | ✅ | Fallende Trümmer wecken Schutt nicht, schwerer Schutt bleibt bei Explosionen liegen. |
| 11 | Nur begrenzt viel Schutt auf einmal aufwecken | ✅ | `NB_MAX_THAW` 512, außer beim Einsturz einer Etage. |
| 12 | Kontakte filtern, kleine Trümmer untereinander ignorieren | ⛔ | Widerspricht der Regel „keine Kollision abschalten“. Trümmer sollen untereinander kollidieren. `b3Filter` bleibt für das Spiel verfügbar. |
| 13 | Winzige Trümmer nur als Bild | ⛔ | Widerspricht „nichts löschen, keine Kollision abschalten“, und die Demo verzichtet bewusst auf Partikel. `minFragmentVolume` verhindert winzige Splitter schon beim Bruch. |
| 14 | Bewegte Trümmer zusammenfassen | ✅ | Verbundene Stücke sind ein Körper aus mehreren Hüllen. Fremde Stücke zusammenzukleben lohnt nicht. |
| 15 | Physik-LOD nach Kameraentfernung | ⛔ | Fern einfrieren ließe Teile in der Luft hängen. Ersetzt durch Nr. 91 für eine große Stadt. |
| 16 | Fernes seltener simulieren | ⛔ | Die Zerstörung läuft schon nur bei Änderungen, und Box3D rechnet alle wachen Körper in einem Schritt. Seltenere Schritte für einen Teil der Welt gingen nur mit einem Eingriff in Box3D. |
| 17 | Fokussierter, hierarchischer Bruch | ✅ | Nur getroffene Stücke werden verfeinert. |
| 18 | Höchste Bruchtiefe | ✅ | `maxDepth` je Material. |
| 19 | Splitterbudget je Einschlag | ✅ | `maxFragmentsPerImpact`, Verteilung nach beschädigtem Volumen. |
| 20 | Bruch vorberechnen | ✅ | Vorzerlegung beim Laden, über Stöße und um Öffnungen, plus fokussierter Bruch zur Laufzeit. |
| 21 | Statik vorberechnen | ✅ | Verbindungen, Etagen und lokale Suche statt globaler Spannungsrechnung. |
| 22 | Hüllen zwischenspeichern | ⛔ | Hüllen entstehen direkt aus der Topologie, und Bruchhüllen sind fast immer einmalig. Deshalb gehen sie auch nicht durch Box3Ds Hüllen-Datenbank, siehe „Hüllen ohne Box3Ds Hüllen-Tabelle“. |
| 23 | Einfachere Kollisionshüllen für bewegte Trümmer | ⛔ | Gemessen und verworfen, siehe oben. |
| 24 | Nur die Oberfläche eines Schutthaufens kollidiert | ⛔ | Teil von Nr. 2, nicht als eigenes System. Mit Nr. 2 verworfen. Dazu hätten die inneren Stücke keine Kollision mehr, das widerspricht „keine Kollision abschalten“, und würde die Oberfläche weggeschossen, hinge kurz etwas in der Luft. |
| 25 | Grobe Kollision für ferne Viertel | 🔜 Stadt | Erst mit Regionen (Nr. 88). |
| 26 | Kontaktbudget | ⛔ | Bräuchte einen Eingriff in Box3Ds Löser. Das Trümmerbudget begrenzt die Kontakte schon indirekt. |
| 27 | CCD nur wo nötig | ✅ | Neu: `debrisSweepDistance`, siehe oben. |
| 28 | Löser-Iterationen je Körper | ⛔ | Tief in Box3D, viel Aufwand, unklarer Gewinn. Die Substeps stellt die Anwendung ein. |
| 29 | Substeps je Körper | ⛔ | Wie Nr. 28. |

## CPU, Daten und Threads

| Nr. | Idee | Status | Bewertung |
| ---: | --- | --- | --- |
| 30 | Pools mit Freilisten | ✅ | Bruchstücke, Verbindungen, Akteure, Destructibles mit Generations-IDs. |
| 31 | Arena für temporäre Daten | ✅ | Haupt-Arena und eine je Worker, pro Operation zurückgesetzt. |
| 32 | Datenorientierte Ablage | ✅ | Zusammenhängende Arrays. Auf SoA umbauen nur, wenn eine Schleife es im Profil zeigt. |
| 33 | SIMD | ⛔ | Box3D hat es. Nebenans Geometrie läuft parallel und ist mit 4 Threads ein kleiner Teil der Frame-Zeit. |
| 34 | Voronoi-Zellen parallel | ✅ | |
| 35 | Hüllen parallel | ✅ | |
| 36 | Statik parallel | ⛔ | Die Prüfungen sind lokal und billig. Im Update kosteten die Schleifen aus Nr. 1 mehr. |
| 37 | Einbau und Box3D-Formen parallel | ⛔ | Box3D erlaubt Änderungen an der Welt nur von einem Thread. Was ging, läuft jetzt parallel (siehe oben). Die Restidee steht unten als „gesammelt einfügen“. |
| 38 | Wenige große Aufgaben statt vieler kleiner | ✅ | Eine Aufgabe je Worker, Arbeit über einen atomaren Zähler. |
| 39 | Statik nur bei Änderungen | ✅ | `supportDirty`, `supportChecks`, `splitSeeds`. |
| 40 | Nur betroffene Bereiche bearbeiten | ✅ | |
| 41 | Dirty-Flags | ✅ | `massDirty`, berührte Stücke und Akteure, Events. |
| 42 | Arbeit über Frames verteilen | ✅ | Grenzen beim Aufwecken, Freigeben, Zerquetschen, Seiten verdichten. |
| 43 | Gemeinsame Welt-Regionen | 🔜 Stadt | Siehe Nr. 88. |
| 44 | Hierarchische Broadphase | ✅ | Box3D. Keine zweite bauen. |
| 45 | Räumliche Sortierung im Speicher (Morton) | ⛔ | Kein Profil zeigt Cache-Probleme in diesen Daten. Die Ausnahme waren die Schleifen aus Nr. 1, und die sind gelöst. |

## Darstellung und Sichtbarkeit (Demo)

| Nr. | Idee | Status | Bewertung |
| ---: | --- | --- | --- |
| 46 | Backface-Culling | ✅ | `SG_CULLMODE_BACK`. |
| 47 | Verdeckte Flächen zwischen verbundenen Stücken weglassen | ✅ | `nbChunk_GetVisibleFaces`. |
| 48 | Flächen zwischen aufeinanderliegendem Schutt weglassen | 🔜 niedrig | Schwer robust zu erkennen und beim Aufwecken rückgängig zu machen. Erst nach Nr. 50. |
| 49 | Frustum-Culling | 🔜 mittel | Bestätigt: Jede Seite wird gezeichnet, auch hinter der Kamera. Braucht räumliche Seiten (Nr. 50). |
| 50 | Räumliche Render-Seiten | 🔜 mittel | Seiten füllen sich heute in der Reihenfolge der Entstehung. Seiten je Raumzelle mit Grenzen machen Nr. 49, 51, 52 und HLOD möglich. Hoch, sobald die Grafik der Engpass ist (Menü). |
| 51 | Schutt je Region statt je Stück verwerfen | 🔜 mit Nr. 50 | |
| 52 | Winzige Stücke in der Ferne nicht zeichnen | 🔜 Stadt | Nur Darstellung, in der Simulation bleibt alles. Bei großer Sichtweite, nach projizierter Größe statt Entfernung. |
| 53 | Mesh-LOD | 🔜 Stadt | |
| 54 | Cluster- oder Meshlet-LOD | ⛔ | Erst Regionen und Frustum. Bei 200 000 Dreiecken kein Bedarf. |
| 55 | HLOD | 🔜 Stadt | Ein fernes zerstörtes Haus als ein grobes Modell. |
| 56 | Impostoren | ⛔ | Erst HLOD. |
| 57 | Verdeckungs-Culling | 🔜 Stadt | In dichten Straßen viel wert. |
| 58 | Hierarchische Verdeckung | 🔜 Stadt | Mit Nr. 57. |
| 59 | Hi-Z | 🔜 Stadt | Mit Nr. 57. |
| 60 | Sichtbarkeit vom letzten Bild weiterverwenden | 🔜 Stadt | Mit Nr. 59. |
| 61 | Portale und Sektoren | ⛔ | Zerstörbare Wände machen Portale unzuverlässig. Erst Hi-Z. |
| 62 | Vorberechnete Sichtbarkeit (PVS) | ⛔ | Aus demselben Grund. |
| 63 | Meshlets | ⛔ | Erst Regionen, Frustum, Verdeckung. |
| 64 | Meshlets nach Normalenkegel verwerfen | ⛔ | Wie Nr. 63. |
| 65 | Ruhenden Schutt zu Regions-Meshes backen | 🔜 Stadt | Nicht wegen Draw Calls (es sind 12), sondern für Grenzen, ohne Transformation, als Grundlage für HLOD. |
| 66 | Instancing | ✅ | Gelöst durch gepackte Seiten mit Transformations-Slot je Ecke. |
| 67 | Batching | ✅ | Eine Seite, ein Draw Call. |
| 68 | Nach Material zusammenfassen | ✅ | Material steckt in der Ecke. |
| 69 | GPU-getriebenes Zeichnen (indirekt) | 🔜 Stadt | Erst sinnvoll als Ausführung für GPU-Culling. |
| 70 | Feste GPU-Puffer | ✅ | Volle Seiten sind unveränderlich, nur die offene ist dynamisch. |
| 71 | Seiten nach und nach verdichten | ✅ | Ab einem Drittel totem Inhalt, mit Budget je Frame. |
| 72 | Ecken weiter komprimieren | 🔜 niedrig | Normale und Material sind schon gepackt (20 Byte je Ecke). 16-Bit-Positionen je Region erst mit Nr. 50. |
| 73 | Index-Reihenfolge für den Vertex-Cache | ⛔ | Flache Flächen haben eigene Ecken je Fläche, kaum Wiederverwendung. |
| 74 | Transformationen nur hochladen, was sich bewegt | 🔜 mittel | Bestätigt: Bewegt sich irgendein Körper, lädt `Renderer::Upload` alle benutzten Slots hoch, 32 Byte je Slot, und die Demo gibt jedem Bruchstück einen eigenen Slot. Am Ende der Stadt mit 78 000 Bruchstücken sind das 2,5 MB pro Bild, mit der Demo-Bruchstückgröße 0,8 MB. Achtung: `sg_update_buffer` schreibt immer ab dem Anfang. Nötig ist also ein eigener kleiner Puffer für bewegte Körper oder eine Ordnung der Slots. |
| 75 | Ruhender Schutt ganz ohne Transformation | 🔜 Stadt | Mit Nr. 65. |
| 76 | Von vorn nach hinten zeichnen | ⛔ | Der Pixel-Shader ist trivial. Erst mit teurem Shading. |
| 77 | Textur-Atlanten | ⛔ | Die Demo hat keine Texturen. |
| 78 | Bindless | ⛔ | Kein Binden pro Material. |
| 79 | Materialien in der Ferne vereinfachen | ⛔ | Licht wird schon in den Ecken gerechnet. |
| 80 bis 83 | Schatten-Optimierungen | ⛔ | Die Demo hat bewusst keine Schatten. |
| 84 | Variable Rate Shading | ⛔ | Kaum Pixelarbeit. |
| 85 | Clustered/Forward+ | ⛔ | Eine Sonne, keine Punktlichter. |
| 86 | Async Compute | ⛔ | Nichts läuft auf der GPU außer dem Zeichnen. |
| 87 | GPU-Partikel | ⛔ | Bewusst kein Staub. Wäre ein neues Bild-Feature, keine Optimierung. |

## Große Welt (für eine große Stadt)

| Nr. | Idee | Status | Bewertung |
| ---: | --- | --- | --- |
| 88 | Gemeinsames Regionssystem | 🔜 Stadt | Eine Raumzelle trägt Render-Seiten, Grenzen, LOD, gebackenen Schutt, Streaming-Zustand. Grundlage für Nr. 25, 50, 55, 57, 65, 89, 91. |
| 89 | Regionen streamen | 🔜 Stadt | |
| 90 | Nur Aktives bearbeiten | ✅ | In der Zerstörung schon der Kern. Fehlt nur auf Weltebene (Nr. 91). |
| 91 | Physik nur in aktiven Regionen | 🔜 Stadt | Nach Aktivität (Spieler, Geschosse, Explosionen), nicht nach Kamera. Ruhige Regionen frieren nur ein, was schon ruht, damit nichts in der Luft hängt. |
| 92 | HLOD für ruhende zerstörte Regionen | 🔜 Stadt | Mit Nr. 55. |
| 93 | Dynamische Portale | ⛔ | Siehe Nr. 61. |

## Eigene Ideen aus den Messungen

| Idee | Status | Bewertung |
| --- | --- | --- |
| Große Arrays wachsen an Ort und Stelle | ✅ | Umgesetzt in Nebenan und Box3D, jeweils in `src/core.c` (siehe `extern/README.md`). Ein Block ab 1 MB reserviert beim Anlegen Adressraum, 16 GB in Nebenan, 4 GB in Box3D, und bekommt beim Wachsen dort Speicherseiten dazu, statt umzuziehen. Nichts wird kopiert, und alter und neuer Block liegen nie gleichzeitig im Speicher. Gemessen abwechselnd mit dem Stand ohne und dem mit Reserve, je drei Läufe: Der Ruckler bei 65 536 Bruchstücken, ohne Reserve 16 bis 18 ms, bleibt weg. Das langsamste Frame der Stadt aus 256 Häusern dauert 2,5 bis 3,8 ms, in einem von fünf Läufen 8 ms durch einen Ausreißer der VM. Laden geht bei 1 024 Häusern 8 % schneller als ohne Reserve, 3,2 statt 3,5 s, und 12 % schneller als mit, bei 256 Häusern 3 und 10 %. Der Speicher liegt fast wieder beim Stand ohne Reserve: höchstens 546 MB statt 572 MB mit und 539 MB ohne Reserve. Die 7 MB sind der Platz in der Hüllen-Tabelle. Die Ergebnisse bleiben bitgleich. Mit eigenen Speicherfunktionen (`nbSetAllocator`, `b3SetAllocator`), unter ASan und in 32-Bit-Programmen wachsen Arrays wie früher durch Umkopieren. |
| Platz vorab reservieren | ⛔ | Ersetzt durch „Große Arrays wachsen an Ort und Stelle“. `nbKeepRoom` verdoppelte nach jedem Aufbau die Kapazität der großen Arrays, sobald weniger als ein Viertel frei war, auch in Box3D über ein ergänztes `b3World_Reserve`. Das nahm die Ruckler aus dem Spiel, kostete aber 6,5 % Speicher, und wuchs eine Szene beim Spielen über den freien Platz hinaus, stockte ein Update doch. Geblieben ist, dass die Zuordnungen von Box3D-Nummern beim Aufbau alle Körper und Formen abdecken. So kostet der erste Einschlag in der Stadt aus 1 024 Häusern unter 1 ms statt 8 bis 15 ms. Platz in Box3Ds Hüllen-Tabelle hielt Nebenan noch bis „Hüllen ohne Box3Ds Hüllen-Tabelle“ frei. |
| Hüllen ohne Box3Ds Hüllen-Tabelle | ✅ | Umgesetzt mit der Ergänzung `b3ShapeDef::uniqueHull` in Box3D (siehe `extern/README.md`): Die Form eines Bruchstücks behält eine eigene Kopie ihrer Hülle. Vorher legte Box3D jede Hülle in einer Hash-Tabelle ab, damit gleiche Hüllen nur einmal gespeichert werden. Wurde die Tabelle voll, ordnete sie alle Hüllen in eine doppelt so große ein, auf der VM 30 ms bei 61 500 Hüllen und 0,4 s bei 246 000, und das konnte nach einem Viertel bis dem Vierfachen an Bruchstücken mehr mitten im Spiel passieren. Jetzt gibt es keine Tabelle mehr, die voll werden kann, und keinen Platz auf Vorrat, `b3World_ReserveHulls` ist wieder weg. Gemessen abwechselnd mit dem Stand davor, je drei Läufe: Laden 6 % schneller, 2,87 statt 3,05 s bei 1 024 Häusern und 0,70 statt 0,74 s bei 256, und höchstens 536 statt 553 MB bei 1 024 Häusern, 145,5 statt 150 MB bei 256. Ein Frame kostet gleich viel, die Ergebnisse bleiben bitgleich. Wer viele gleiche Objekte mit gleichem `seed` baut, bekommt gleiche Hüllen, die Box3D vorher nur einmal speicherte. Seit „Geometrie nur einmal speichern“ benutzt die Form die Hülle in Nebenans Form, statt sie zu kopieren. |
| Laden und Speicher großer Szenen | ✅ verlustfrei | Zuerst gemessen, wohin Speicher und Ladezeit pro Bruchstück gehen, bei 256 Häusern: Box3Ds Kopie der Hülle 731 B, Nebenans eigene Form 386 B, Box3D-Körper samt Simulationsdaten 352 B, Bindungen 223 B, Box3D-Form 208 B, Suchbaum rund 120 B. Von der Rechenzeit beim Laden, über alle Threads gezählt, geht gut die Hälfte an die Voronoi-Zellen der Vorzerlegung. Umgesetzt ist, was am Ergebnis nichts ändert und keinen Einschlag verlangsamt: ein statischer Box3D-Körper pro Haus und die Suche nach Bindungen per Sweep, siehe die beiden Zeilen darunter. Weniger Bruchstücke hätten viel mehr gebracht, sind aber verworfen, siehe „Häuser erst beim ersten Treffer zerlegen“ und „Vorzerlegung mit der Bruchstückgröße wachsen lassen“. Der nächste verlustfreie Hebel war „Geometrie nur einmal speichern“, inzwischen umgesetzt. |
| Ein statischer Box3D-Körper pro Haus | ✅ | Umgesetzt mit der Ergänzung „Kontakte pro Form“ in Box3D (`B3_HAS_SHAPE_CONTACT_LISTS`, siehe `extern/README.md`). Vorher hatte jedes stehende Bruchstück einen eigenen statischen Körper, weil Box3D beim Löschen einer Form alle Kontakte ihres Körpers durchging: An einem gemeinsamen Körper hätte jedes zerstörte Bruchstück die Kontakte des ganzen Hauses abgeklappert, und Einschläge auf Häuser mit viel Schutt darauf wären langsamer geworden. Jetzt führt jede Form ihre eigenen Kontakte, und alle stehenden Bruchstücke eines Zerstörbaren hängen an einem Körper. Gemessen abwechselnd mit dem Stand davor, ×4, 4 Threads, zwei Reihen zu je drei Läufen: Speicher nach dem Laden 439 statt 520 MB bei 1 024 Häusern und 111 statt 131 MB bei 256, 16 % weniger. Laden bei 1 024 Häusern 1,20 bis 1,27 statt 1,95 bis 2,55 s, bei 256 Häusern 0,30 bis 0,35 statt 0,32 bis 0,40 s, beides samt der Suche nach Bindungen per Sweep. Einschläge kosten bei 1 024 Häusern im Mittel 0,033 bis 0,036 statt 0,053 bis 0,065 ms pro Frame, p95 0,20 bis 0,21 statt 0,33 bis 0,39 ms, das teuerste Einschlag-Frame meist 0,5 statt 2,1 bis 3,1 ms, ein Frame unter Beschuss 0,29 bis 0,31 statt 0,32 bis 0,36 ms. Wahrscheinlich, weil Trümmerkörper jetzt Box3D-Nummern ab rund 1 000 bekommen statt ab 246 000, so bleiben die Tabellen klein, die Box3D und Nebenan nach Körpernummer führen. Bei 256 Häusern kosten Einschläge und Frames gleich viel. Am Tag dieser Messung kostete ein Einschlag bei 1 024 Häusern auch mit gesammeltem Einfügen 0,053 bis 0,065 ms, am Vortag hatte derselbe Stand 0,031 bis 0,035 ms gemessen: Die VM streut hier stark, verlässlich sind die abwechselnd gemessenen Vergleiche. Im Einschlag-Benchmark (Gewehr, Explosionen, Gebäude) mit 1 und 4 Threads liegen die Zeiten im Rauschen, Callgrind zählt in `nbApplyImpact` 0,5 % weniger Instruktionen, in `nbCommitPhysics` 5,4 % weniger. Die Ergebnisse bleiben bitgleich. Ohne die Ergänzung bekommt jedes stehende Bruchstück weiter einen eigenen Körper. |
| Bindungen per Sweep suchen | ✅ | Beim Anlegen eines Zerstörbaren prüfte Nebenan jedes Paar seiner Bruchstücke auf eine gemeinsame Fläche, bei einem Haus rund 29 000 Paare. Jetzt findet ein Sweep entlang x die Paare, deren Grenzen sich berühren, und prüft sie in derselben Reihenfolge wie vorher, die Bindungen bleiben bitgleich. Das bringt beim Laden nur wenige Prozent, weil die Paare, die sich wirklich berühren, die meiste Arbeit machen. Es wächst aber mit n log n statt n² und hilft so großen Gebäuden. In `nbShape_ContactArea` zuerst die billigere Bedingung zu prüfen, gleiche Ebene vor gegenüberliegenden Normalen, war gemessen langsamer und ist wieder raus. |
| Häuser erst beim ersten Treffer zerlegen | ⛔ | Ein unberührtes Haus bestünde nur aus seinen Teilen zwischen Fenstern und Türen, 39 statt 240 Bruchstücke. Gemessen bei 1 024 Häusern: 98 statt 550 MB und 0,33 statt 2,1 s Laden. Beim ersten Treffer müsste Nebenan das Haus aber erst vorzerlegen, geschätzt so teuer wie heute das Laden eines Hauses, auf der VM 1,5 bis 3 ms, und eine Explosion über mehrere frische Häuser zahlt das in einem Frame mehrfach. Verworfen: Ein erster Treffer auf ein Haus soll nicht langsamer laufen als bisher. Dasselbe gilt für die Variante, ein Haus als Einheit zu halten und erst an der Trefferstelle aufzuteilen, geschätzt 20 bis 80 % mehr für den ersten Treffer auf jede Stelle. |
| Vorzerlegung mit der Bruchstückgröße wachsen lassen | ⛔ | Die Bruchstückgröße wirkt nur auf Einschläge, die Zellen der Vorzerlegung bleiben 1,2 m groß. Wüchsen sie mit, bei ×4 auf 4,8 m, hätte ein Haus 75 statt 240 Bruchstücke, mit doppelt so großen Zellen 111. Gemessen bei 256 Häusern mit 4,8 m, je drei Läufe: Laden 0,14 statt 0,35 s, Speicher am Ende 55 statt 145 MB, Einschläge billiger, ein Frame unter Beschuss gut ein Drittel billiger. Verworfen, weil dann größere Stücke abbrechen und am Ende 766 statt 1 284 Trümmer herumliegen: Das Bild der Zerstörung soll so bleiben. |
| Geometrie nur einmal speichern | ✅ | Umgesetzt mit der Ergänzung `b3ShapeDef::externalHull` in Box3D (siehe `extern/README.md`). Vorher hielt jedes Bruchstück seine Form zweimal, als Nebenans Form (386 B) und als Box3D-Hülle (731 B). Jetzt baut Nebenan die Box3D-Hülle direkt in die Allokation seiner Form: Die Ecken und die Ebenen der Flächen sind die Punkte und Ebenen der Hülle, daneben stehen nur noch die Flächen als Schleifen über die Ecken mit ihren Materialien, und die Box3D-Form benutzt die Hülle an Ort und Stelle. Die Geometrie eines Bruchstücks braucht so 888 statt 1 117 B. Die frühere Schätzung, das spare rund ein Drittel des Speichers pro Bruchstück, war zu hoch: Die Hülle trägt mehr als die Geometrie, nämlich Halbkanten, Ecken und Normalen ein zweites Mal für SIMD und einen Kopf von 144 B, und die Flächen-Schleifen braucht Nebenan weiter, über die Halbkanten zu laufen wäre beim Schneiden langsamer. Gezählt sind es 235 B oder 12 % weniger pro Bruchstück, 408 statt 463 MB bei 1 024 Häusern, im Prozess 14 %, weil pro Bruchstück eine Allokation wegfällt. Gemessen abwechselnd mit dem Stand davor, ×4, 4 Threads, je sechs Läufe: Speicher nach dem Laden 379 statt 439 MB bei 1 024 Häusern und 96 statt 111 MB bei 256, am Ende 389 statt 450 MB und 108 statt 124 MB. Laden bei 1 024 Häusern 1,12 bis 1,32 statt 1,31 bis 1,64 s, in einer ersten Reihe 1,10 bis 1,18 statt 1,24 bis 1,29 s, bei 256 Häusern 0,28 bis 0,32 statt 0,30 bis 0,39 s. Frames und Einschläge in der Stadt kosten gleich viel. In der zweiten Reihe hatte der alte Stand bei 1 024 Häusern die langsame Laune der VM, die schon bei „Ein statischer Box3D-Körper pro Haus“ auffiel, Einschläge 0,049 bis 0,072 statt 0,032 bis 0,044 ms, in der ersten kosteten beide 0,028 bis 0,035 ms. Im Einschlag-Benchmark mit 4 Threads, je 24 Läufe: Gewehr 0,269 statt 0,271 ms, Explosionen 1,76 statt 1,79 ms, Gebäude 1,28 statt 1,34 ms. Der Teil auf den Workern kostet 3 bis 6 % mehr, weil die Hülle jetzt in frischen Speicher der Form geschrieben wird statt in die immer wieder benutzte Arena. Das Einfügen in Box3D spart mehr, weil die Kopie wegfällt. Callgrind zählt in `nbApplyImpact` 0,8 % weniger Instruktionen und in der Cache-Simulation 8 % weniger Lese- und 17 % weniger Schreibfehler im L1, im Box3D-Schritt gleich viele Instruktionen und 2 % weniger Lesefehler. Die Hülle beginnt wie eine Allokation von Box3D auf 16 Byte. Die Ergebnisse bleiben bitgleich. Die öffentliche `nbFace` hat dafür ihre Ebene verloren, die Ebenen stehen in `nbGeometry::planes`. Ohne die Ergänzung behält Box3D wie vorher eine eigene Kopie. |
| Neue Bruchstücke gesammelt in den Box3D-Baum einfügen | ✅ | Umgesetzt für statische Formen mit der Ergänzung `b3World_BeginStaticBatch` und `b3World_EndStaticBatch` in Box3D (siehe `extern/README.md`). Vorher suchte Box3D für jede Form von der Wurzel seines Suchbaums aus ihren Platz und drehte auf dem Rückweg Knoten, beim Laden von 1 024 Häusern knapp ein Fünftel der Ladezeit (`b3InsertLeaf`). Jetzt sammelt `nbCommitPhysics` die statischen Formen eines Aufbaus oder Einschlags, Box3D baut aus ihnen einen Teilbaum, geteilt wie bei einem Neuaufbau, und hängt ihn mit einer Suche ein. Das kostet noch rund 2 % des Aufbaus. Gemessen abwechselnd mit dem Stand davor, Median aus sechs Läufen bei 256 und 1 024 Häusern, sonst aus drei: Laden bei 1 024 Häusern 1,46 statt 1,92 s (−24 %), bei 256 Häusern 0,39 statt 0,45 s (−15 %), bei 64 Häusern 0,094 statt 0,119 s (−21 %). Ein Einschlag kostet im Mittel 0,032 bis 0,035 ms pro Frame, ob die Stadt aus 16 oder 1 024 Häusern besteht. Vorher wuchs das mit der Größe des Baums, bei 1 024 Häusern auf 0,054 ms, p95 0,33 statt jetzt 0,21 ms. Der Box3D-Schritt und der Speicher bleiben gleich. Bewegte Trümmer gehen weiter einzeln in Box3Ds Baum der bewegten Körper, ein Einschlag bei ×4 erzeugt nur wenige. Voraussetzung war, dass die Ergebnisse nicht an der Form des Baums hängen, und das taten sie an zwei Stellen: Schutt taute in der Reihenfolge auf, in der Box3D ihn fand, und Box3Ds CCD rechnete die Zeit bis zum Aufprall auf jede statische Form nur bis zum frühesten Aufprall davor, was die letzten Bits des Ergebnisses von der Reihenfolge abhängig machte. Jetzt taut Schutt in der Reihenfolge der Akteure auf, und die CCD rechnet über den ganzen Schritt, ohne messbare Kosten. Das änderte die Ergebnisse einmal: neuer Determinismus-Hash und andere Endstände im Benchmark, die Städte und großen Explosionen enden innerhalb von 2,3 % der Bruchstücke, des Schutts und der Trümmer von vorher. Das gesammelte Einfügen selbst ist bitgleich zum einzelnen. Ohne die Ergänzung (`B3_HAS_STATIC_BATCH` fehlt) fügt Nebenan einzeln ein. |
| Nebenan ohne Box3D | ⛔ | Box3D macht die eigentliche Arbeit (Kontakte, Löser, Broadphase, CCD, Threads, Determinismus) bereits sehr schnell. Eine eigene Engine wäre nur schneller, wenn sie weniger rechnet, und die meisten Vereinfachungen gehen auch mit Box3D. |
| Trümmer-CCD-Schwelle | ✅ | Umgesetzt (`debrisSweepDistance`). |
| Einschlag weiter parallelisieren | ✅ | Umgesetzt, siehe oben. |
| Kontaktflächen beim Laden auf den Workern | ✅ | Beim Anlegen eines Zerstörbaren mit Öffnungen oder mehreren Teilen misst Nebenan für jedes Paar sich berührender Bruchstücke die gemeinsame Fläche. Das lief auf dem aufrufenden Thread, beim Laden einer Stadt ein Viertel seiner Zeit, die Paarsuche davor knapp ein Zehntel, und die Worker warteten solange. Jetzt messen die Worker die Flächen in Blöcken zu 16 Paaren, ab 8 Blöcken. Die Verbindungen entstehen danach auf dem aufrufenden Thread in derselben Reihenfolge wie vorher. Dazu liest die Paarsuche die Grenzen aus ihrer eigenen sortierten Liste, statt für jeden Kandidaten Bruchstück und Form nachzuschlagen, und jede Voronoi-Zelle kopiert nur den benutzten Teil ihres Eltern-Polyeders statt immer 7 KB. Gemessen im Wechsel mit dem Stand davor, je zwölf Läufe mit Bruchstückgröße ×4 und 4 Threads: Die Stadt aus 256 Häusern lädt im Median in 0,26 statt 0,31 s, die aus 1 024 Häusern in 1,14 statt 1,30 s, 15 und 12 % schneller. Die Ergebnisse bleiben bitgleich. Der Worker-Test vergleicht dafür die Verbindungen einer Wand mit Öffnungen mit einem und mit vier Threads. |
| Kleine Einschläge ohne Worker | ✅ | Ein Einschlag weckte die Worker ab 16 Zellen und Stücken. Bei vierfacher Bruchstückgröße hat eine Granate auf ein Haus 15 bis 30, und mit einem Thread war sie schneller als mit vier: in der Stadt aus 256 Häusern 0,145 bis 0,157 statt 0,176 bis 0,185 ms. Die Worker zu wecken und ihre Zellen danach aus den Caches anderer Kerne zu holen kostete mehr, als sie abnahmen. Jetzt helfen sie ab 32. Dazu ist der Zufallsgenerator inline, das Ziehen der Bruchpunkte und die Schätzung der getroffenen Volumen brauchen Tausende Zahlen pro Einschlag. Gemessen im Wechsel mit dem Stand davor, 4 Threads, ×4: In der Stadt aus 256 Häusern kostet eine Granate im Median 0,159 statt 0,185 ms, je zwölf Läufe. Bei 1 024 Häusern gibt die VM in jedem zweiten Lauf frischen Speicher langsam heraus, Läufe im selben Zustand verglichen sind es 1 bis 4 % weniger. In der Stadt der Demo, 20 Häuser bis zum Schuttfeld beschossen, je vier Läufe, kostet der erste Treffer auf ein Haus im Mittel 0,14 statt 0,16 ms und eine Granate in den Schutt 2 % weniger. Einschläge mit 16 bis 31 Zellen und Stücken, die jetzt ohne Worker laufen, kosten dort 6 % weniger, und keine Größe wird langsamer. Im Benchmark mit Bruchstückgröße ×1, je sechs Läufe, liegen Gewehr, Explosionen und Gebäude zwischen 3 % schneller und 4 % langsamer, so weit schwankt auch der unveränderte Box3D-Schritt. Die Ergebnisse bleiben bitgleich. Auf einem Rechner, der Threads schneller weckt als die VM, kann die beste Schwelle niedriger liegen. |
| Rückprall 0 statt 0,05 | ⛔ | Box3D rechnet die Restitution nur für Kontakte mit Rückprall, und Nebenans Standardmaterial hat 0,05. Im Box3D-Schritt der Stadt sind das 6 % der Instruktionen. Gemessen mit 0 bei 256 Häusern, ×4, je acht Läufe im Wechsel: Der Löser rechnet pro Kontakt 5 % billiger, aber ohne Rückprall kommen die Trümmer anders zur Ruhe, und es sind 8 % mehr Kontakte wach. Ein Frame kostet gleich viel. |
| Nur auftauen, was die Explosion schiebt | ⛔ | Eine Granate taut allen leichten Schutt in einem Würfel um den Einschlag auf. Die rund 15 %, die weiter als 2 m vom Einschlag liegen, bekommen von der Explosion weniger als 1 m/s, siehe „Zerstörte Stadt unter Dauerfeuer“. Sie liegen zu lassen spart höchstens 15 % der wachen Körper im Schuttfeld, aber man sähe es: Brocken am Rand blieben starr liegen, während ihre Nachbarn wegfliegen, und ein Trümmerteil prallte von ihnen ab wie von einer Wand. |
| Verbindungen zwischen Splittern und alten Nachbarn auf den Workern | ⛔ | Die Flächen zwischen den neuen Splittern und den Nachbarn des getroffenen Stücks misst ein Einschlag nach den Zellen auf dem aufrufenden Thread, rund 9 % seiner Instruktionen. Auf den Workern wären sie nur bitgleich, solange keine zwei Nachbarn im selben Einschlag zerteilt werden, und bei Granaten ist das häufig. Für geschätzt 3 bis 6 % pro Einschlag nicht den Umbau wert. |
| Hüllen schneller bauen | ⛔ | Rund 3 600 Instruktionen pro Hülle, verteilt auf Masse und Trägheit, Prüfsumme, Kantenpaare und die SIMD-Kopien der Ecken und Normalen, ohne Stelle, an der sich viel holen ließe. |
| Box3D-Formen verkleinern | ⛔ | 216 Byte pro Form, mit Material, Filter und einer Union aller Formtypen. Das wäre ein Eingriff in Box3Ds Kern für wenige Prozent Speicher. |
| Biegemomente nur bei Bedarf | ✅ | Die zweiten Momente jeder Verbindung, 24 Byte, braucht nur die Lastprüfung. Die ist standardmäßig aus und prüft Gebäude mit Decken nie. Jetzt stehen die Momente nicht mehr in der Verbindung, sondern in einem eigenen Array, und nur Zerstörbare, die die Lastprüfung prüfen kann, führen sie: Sie ist an, und das Objekt hat keine Etagen. Für alle anderen rechnet Nebenan sie weder in den Voronoi-Zellen noch in den Kontaktflächen aus. Kommt die Lastprüfung erst zur Laufzeit dazu, misst Nebenan die Momente der Objekte ohne Etagen einmal an den Formen ihrer Bruchstücke nach. Bei Verbindungen zwischen Voronoi-Zellen weichen sie dann in den letzten Bits von denen der Schnittfläche ab, im Test um höchstens 4·10⁻⁵ relativ. Eine Verbindung hat 64 statt 88 Byte, genau eine Cache-Zeile. Gemessen: 3,7 % weniger Prozess-Speicher, am Ende 375 statt 389 MB bei 1 024 Häusern. Callgrind zählt für eine Granate in der Stadt bei ×4 5,1 % weniger Instruktionen, im Einschlag-Benchmark 5,2 % und beim Laden 3,1 %, im Box3D-Schritt gleich viele. Auf der VM geht das im Rauschen unter: In zwei Serien von je zwölf Läufen im Wechsel kostete eine Granate einmal 14 % weniger und einmal 3 % mehr. Die Ergebnisse bleiben bitgleich, auch mit eingeschalteter Lastprüfung: Drei Bauwerke ohne Decken unter Beschuss enden mit 1 und 4 Threads gleich, ob die Prüfung von Anfang an läuft oder erst später dazukommt. Geschätzt waren vorher 4 % weniger Speicher, 2 % schneller geladen und 4 % weniger Instruktionen pro Einschlag. |
| Häuser gesammelt laden | ✅ | Umgesetzt als `nbCreateDestructibles`. Das Anlegen eines Zerstörbaren hat jetzt vier Stufen: vorbereiten, also Materialien, Anker, Teile und Bruchpunkte, dann die Voronoi-Zellen, dann die Liste der neuen Bruchstücke mit den Paaren, die sich berühren, und ihren Kontaktflächen, zuletzt der Einbau in die Welt. Bis auf den Einbau hängt alles nur von der Definition ab. Beim gesammelten Laden starten die Zellen des nächsten Hauses auf den Workern, bevor der aufrufende Thread Paare, Kontaktflächen und Einbau des aktuellen erledigt. Danach hilft er bei den restlichen Zellen. Zwei Sätze Arenen wechseln sich ab, einer pro Haus im Flug. Der Einbau läuft in derselben Reihenfolge wie Haus für Haus, deshalb sind Ids, Bruchstücke, Verbindungen und Box3D-Körper bitgleich, auch mit einem Task-System der Anwendung, das jeden Task sofort ausführt. Gemessen bei ×4 mit 4 Threads, Median aus zwölf Läufen im Wechsel: 256 Häuser in 0,16 statt 0,26 s, 1 024 Häuser in 0,65 statt 0,99 s, 35 % schneller, bei gleichem Endzustand und 1 bis 2 MB mehr Prozess-Speicher für die zweite Arena. Ein erster Versuch, der die Zellen des nächsten Hauses erst nach den Kontaktflächen des aktuellen startete, brachte nur 15 %: Die Worker warteten, während der aufrufende Thread die Paare suchte und das nächste Haus vorbereitete. Benchmark und Demo laden ihre Städte jetzt so. Geschätzt war ein Drittel. |
