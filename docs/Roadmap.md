# Roadmap

Die nächsten möglichen Schritte für die Physik, nach der Untersuchung vom 17. Oktober 2026 auf `physik-19` und
`physik-20`, der Messung auf dem Ryzen mit `physik-21`, den Knoten auf `physik-22`, dem Team für die größte Insel auf
`physik-24`, den Prüfungen des Updates auf den Workern auf `physik-25` und den Zeiten des Teams und den Schwellen fürs
Wecken auf `physik-26`, alle vier auf dem Ryzen nachgemessen, den Prototypen und Zerlegungen auf `physik-27`, den Lagen
zum Zeichnen zwischen zwei Schritten auf `physik-28`, den Kontakt-Kernen von upstream #189 auf `physik-29`, der
Trennachsen-Suche von upstream #173 und #177 auf `physik-30`, der Sichtung von upstream auf `physik-31`, der Kollision
mit AVX2 auf `physik-32`, der schärferen Schranke für getrennte Hüllen auf `physik-33`, den ganz vorausgeholten Hüllen
auf `physik-34`, den seriellen Schleifen der Kontakte auf `physik-35`, dem Abbau der Kontakte und der Suche nach Schutt
auf `physik-36`, die letzten vier auf dem Ryzen nachgemessen, der Paarsuche in einem Durchgang auf `physik-38`, der
zweiten Suche nur bei Überlappung auf `physik-39`, den übrigen kleinen Schritten auf `physik-40` und dem Vorausladen zum
Schreiben samt der ersten Stufe von Schritt 9 auf `physik-41`, die letzten vier wieder auf dem Ryzen nachgemessen, dem
Radix-Sort für die neuen Paare auf `physik-43` und dem wählbaren Takt der Physik auf `physik-44`, die letzten zwei auf
dem Ryzen nachgemessen, und dem Budget und den Manifolds auf den Workern auf `kollaps-5`, mit „Parallel gegen
nacheinander“ auf dem Ryzen nachgemessen, dem Vergleich von 7 gegen 8 Threads auf `kollaps-6`, auf dem Ryzen
nachgemessen, und dem Überlauf an Knoten auf mehreren Threads auf `kollaps-7`. Begründungen und Messwerte stehen in
[`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge. Nach jeder Runde wird der Stand hier
nachgetragen.

`kollaps-1` ist keine Runde der Physik, sie baut den Einsturz nach Kapitel 17 der Roadmap des Spiels, siehe
[`Referenz`](Referenz/README.md). Sie ändert aber, was der Benchmark unter Trommelfeuer bei ×4 misst: Eine Etage gibt
dort schon nach, wenn 40 % ihrer Wände zerstört sind, und es stürzen 15 statt 12 Etagen ein. Ein Schritt kostet damit
mit 1 und 2 Threads 3,2 und 6,6 % mehr, siehe „Einsturz nach Kapitel 17 der Roadmap des Spiels“. Wer eine Runde danach
gegen eine davor misst, vergleicht dort verschiedene Szenen, bei ×1 und ×2 dieselben. `kollaps-2` lässt das Dach über
der obersten Etage über den Anker kippen wie die Stockwerke über den Etagen darunter. Im Benchmark enden damit alle
Städte bitgleich wie mit `kollaps-1`. Seit `kollaps-3` stellt ein Spiel ein, wie viel von einer Etage stehen bleibt
(`storeyResidue`), mit dem Standard enden die Städte weiter bitgleich. `kollaps-4` gibt jeder Etage eigene Werte aus
Bereichen, der obersten unter dem Dach eigene, und lässt den Rest in seine Zellen zerbrechen. Mit den ersten
Standardwerten, 19 bis 20 % und 5 bis 30 %, änderte das wieder, was der Benchmark misst: Unter dem Beschuss der Stadt
stürzten bei ×1 7 und bei ×2 4 Etagen ein statt keiner, unter dem Trommelfeuer bei ×4 68 statt 15, und ein Frame kostete
dort gut doppelt so viel, siehe „Eigene Werte je Etage, Rest in Zellen“. Seitdem stehen sie auf den Werten, die der
Nutzer in der Demo gewählt hat, 17 bis 31 % und dem ganzen Rest, die oberste Etage auf 18 bis 25 % und dem ganzen Rest,
siehe „Standard nach dem Augenschein“ und „Oberste Etage nach dem Augenschein“. Damit stürzen bei ×1 6, bei ×2 3 und bei
×4 114 Etagen ein, und ein Frame kostet bei ×4 52 bis 58 % mehr als mit `kollaps-3`. Kapitel 17 der Roadmap des Spiels
ist auf diesen Stand korrigiert. Wer eine Runde danach gegen eine davor misst, vergleicht verschiedene Szenen. Bei ×4
wachsen vor allem das Erstarren des Schutts über dem Budget und die Zustandswechsel in Box3D, ein Kandidat für eine
Runde der Physik. Danach bleibt nichts mehr in der Luft hängen, siehe „Nichts bleibt in der Luft hängen“: Über dem
Budget erstarren keine Trümmer im Flug mehr, und ein Frame kostet bei ×4 4 bis 13 % mehr. Dass das Budget weiter
rechnet, solange nichts mehr erstarren kann, gehört in dieselbe Runde. Auch kein Hausteil hängt mehr auf Schutt, der nur
durch es hält, siehe „Kein Hausteil auf seinem eigenen Schutt“, das Update kostet damit nicht mehr. Offen bleiben zwei
kleine Lücken, siehe „Nichts erstarrt schneller als 1 m/s“: Das Budget friert in einem Schub selten ein schnelles Stück
in der Luft ein, in der Stadt bei ×1 eins von 2 Litern, und dort stehen 94 Stücke 1 bis 7 cm über ihrer Auflage,
wahrscheinlich Schutt auf bewegten Stücken, die sich danach setzten. Die naheliegende Regel kostet bei ×1 10 bis 17 %
und macht das Zweite schlimmer, beides gehört in eine eigene Runde. Gibt nach einer Etage auch die darunter nach, kommt
jetzt alles darüber mit herunter, auch was beim ersten Einsturz herunterkam und erstarrte, und Teile von Gebäuden
erstarren erst, wenn sie mit den Zellen unter ihnen ruhen, siehe „Schutt über einer Etage, die nachgibt, wacht auf“ und
„Hausteile ruhen erst, wenn ihre Insel schläft“. Direkt gemessen kostet ein Frame damit bei ×1 −5 bis +5 % und bei ×4
5 bis 8 % mehr, ganz zerstört mit ×2 und Budget 5 000 7 %, siehe „Was die Einstürze die Stadt kosten“. Dort rechnet das
Budget 3,7 statt 1,9 ms pro Schritt, ein Kandidat für eine Runde der Physik. Ohne Einstürze kostet `kollaps-4` so viel
wie `physik-45`, mit ihnen bei ×4 84 bis 91 % mehr: Es stürzen 116 statt 12 Etagen ein, und eine kostet 47 statt 3 ms,
weil ihr Rest als lose Zellen unter dem Haus nachgibt, siehe „Einstürze gegen `physik-45` und ohne Einstürze“. Davon
wächst das Update am meisten, 3,2 statt 1,1 ms pro Schritt. `kollaps-5` holt davon einen Teil zurück, ohne dass sich ein
Bit ändert: Die Worker prüfen für das Budget vorab, was erstarren darf, und in der Kontakt-Schleife von Box3D holen sie
ihre Manifolds ohne Sperre, siehe „Das Budget schaut auf den Workern voraus“ und „Manifolds der Worker in der
Kontakt-Schleife“. Unter dem Trommelfeuer bei ×4 kostet das Update mit 4 Threads damit 14 % weniger und ein Frame 7 %,
ganz zerstört mit ×2 und Budget 5 000 das Update 29 % und ein Frame 11 %. Was dort bleibt, sind vor allem die
Zustandswechsel in Box3D, rund 1,2 ms pro Schritt, siehe „Zustandswechsel in Box3D billiger“.
Offen bleibt, dass das Budget auch sinkende Teile einfriert, wenn der übrige Schutt nicht reicht: unter dem Trommelfeuer
bei ×4, und mit dem Standard von 1 500 auch an einem einzelnen Haus, ringsum beschossen in der Hälfte der Läufe. In
einem von 20 hielt das ein Teil auf erstarrtem Schutt. `kollaps-7` ändert die Ergebnisse wieder, siehe Schritt 29: Beim
Abriss in der Demo fallen in 4 000 Bildern 6 statt 4 Häuser, und wer eine Runde danach gegen eine davor misst,
vergleicht dort verschiedene Szenen. Im Benchmark enden die Städte fast gleich, unter dem Trommelfeuer bei ×4 stürzen
114 statt 116 Etagen ein.

## Fazit

- **Ein eigenes Box3D lohnt nicht.** Hüllentest, Löser und CCD kosten in jeder Engine dasselbe, und der Fork lässt sich
  an jeder Stelle ändern, siehe „Eigene Engine statt Box3D, dritte Prüfung“.
- **Mit 8 Threads bestimmt der serielle Teil den Frame.** Auf dem Ryzen beim Abriss der Stadt in der Demo gemessen: Mit
  8 Threads kostet ein Schritt mit `physik-25` bei ×4 1,24 und bei ×2 2,61 ms, mit `physik-24` 1,23 und 2,58 ms, mit
  `physik-22` 1,29 und 2,81 ms, davor 1,74 und 3,46 ms. Mit `physik-25` lagen dabei die Stufen des Box3D-Schritts, an
  denen sich nichts geändert hat, mit 8 Threads 1 bis 11 % höher. Hochgerechnet bleiben 60 und 56 % auf einem Thread,
  mit `physik-24` 62 und 59 %, davor 72 und 68 %, siehe „Skalierung auf dem Ryzen mit `physik-25`“. Mit `physik-26`
  kostet ein Schritt bei ×4 1,25 ms wie mit `physik-25`, siehe „Skalierung auf dem Ryzen mit `physik-26`“. Mit
  `physik-41` kostet er bei ×4 1,12 und bei ×2 2,24 ms, in einem Lauf, in dem auch die Stufen ohne Änderung 5 bis 13 %
  schneller waren, hochgerechnet bleiben 58 und 55 % auf einem Thread, siehe „Skalierung auf dem Ryzen mit `physik-41`“.
  Mit `physik-44` kostet er bei ×2 2,25 ms, hochgerechnet bleiben 53 % auf einem Thread, siehe „Skalierung auf dem Ryzen
  mit `physik-44`“. Mit den Einstürzen kostet er auf `kollaps-6` bei ×2 2,61 ms, hochgerechnet bleiben 60 % auf einem
  Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-6`“.
- **Was dort bremst, ist vor allem der Löser.** Die Bedingungen sind mit 8 Threads 28 und 26 % des Frames, mit
  `physik-22` 31 und 26 %, davor 48 und 43 %, und brauchen 34 und 33 % der Zeit eines Threads statt eines Achtels. Seit
  `physik-24` rechnet ein Team aller Threads das Paket der größten Insel, das vorher ein Thread allein rechnete: Auf dem
  Ryzen kosten die Bedingungen mit 8 Threads bei ×4 damit 16 % weniger, bei ×2 bleiben sie gleich, siehe „Größte Insel
  im Team“. Was sie über einem Achtel der Zeit eines Threads brauchen, 17 und 16 % des Frames, ist der größte Posten
  dieser Art, darin der Überlauf auf dem aufrufenden Thread mit 7 %. Seit `physik-25` prüfen die Worker die Trümmer im
  Update: Auf dem Ryzen kostet die Ruhe-Prüfung mit 8 Threads 43 und 42 % weniger, das Update ist 9 % des Frames, siehe
  „Prüfungen des Updates auf den Workern, zweiter Versuch“. Die Einschläge sind 10 und 5 % des Frames, Anlegen und
  Zustandswechsel der Kontakte 6 und 8 %. Wohin die Zeit der Bedingungen in den Schritten mit Team geht, misst seit
  `physik-26` der Löser selbst, siehe „Wartezeiten im Löser zählen“. Auf dem Ryzen rechnet der aufrufende Thread dort
  mit 8 Threads 28 und 29 % der Zeit des Teams allein am Überlauf und bei ×4 40 % der Blöcke der Stufen selbst, während
  jeder Worker 47 % der Zeit auf eine neue Stufe wartet, siehe „Skalierung auf dem Ryzen mit `physik-26`“. Der Grund
  sind die übrigen Pakete, die die Worker zwischen zwei Stufen nehmen. Anders verteilt wird das Team aber nicht kürzer,
  siehe „Übrige Pakete im Team anders verteilen“.
- **Was danach bleibt, sind kleine Posten.** Mit 8 Threads auf dem Ryzen kosten die Typwechsel 3 % des Frames, der
  Aufbau des Lösers 3 %, und von den Einschlägen ist jeder Teil, Box3D-Körper, Einbau, Suche und Teilen der Akteure, 1
  bis 2 %, siehe „Wohin ein Typwechsel und ein Einschlag ihre Zeit geben“. Anlegen und Zustandswechsel der Kontakte
  sind 6 bis 9 %, aber nur mit einem großen Umbau von Box3D zu holen, siehe Schritt 9. Dessen erste Stufe, die neuen
  Kontakte parallel anzulegen, lohnte auf `physik-41` nicht, gebaut ist stattdessen „Vorausladen zum Schreiben“.
- **Der Löser hing am Überlauf des Graphen.** Kontakte, für die keine der 20 Farben frei ist, löst Box3D auf dem
  aufrufenden Thread allein. Beim Abriss sind das die Kontakte an Knoten: große abgebrochene Teile aus Dutzenden bis
  Hunderten Bruchstücken, an denen Hunderte kleine Trümmer liegen, siehe „Der Überlauf beim Abriss“. Seit `physik-22`
  löst Box3D sie in Bündeln gegen Kopien des Knotens: Auf dem Ryzen kosten die Bedingungen mit 8 Threads 53 und 51 %
  weniger, ein Schritt 26 und 19 %, und der Überlauf ist nur noch 4 und 6 % des Frames, siehe „Knoten im Überlauf mit
  geteilter Masse“. Mit den Einstürzen ist er wieder der größte Posten auf dem aufrufenden Thread: Große Reste von
  Etagen liegen auf Hunderten Zellen, und bei ×2 kostet der Überlauf mit 8 Threads 0,43 ms pro Schritt, 15 % des Bilds,
  siehe Schritt 29.
- **Der Cache ist nicht das Problem.** Der wache Arbeitssatz ist bei ×1 gut 10 MB groß und passt schon in 32 MB. Der
  Ryzen wartet kürzer, weil sein L3 schneller antwortet, nicht weil er größer ist, siehe „Arbeitssatz im Cache“.
- **Parallel zum Zeichnen lohnt nur, wenn der Hauptthread genug zu tun hat.** Auf dem Ryzen mit der RTX 4070 SUPER
  zeichnet die Demo ein Bild in 0,2 ms, und mit `kollaps-5` ist parallel beim Abriss bei ×2 je nach Sitzung 2,4 bis
  7,6 % langsamer als nacheinander, die Simulation auf ihrem Thread 11 bis 18 %. Auf der VM verliert der Schritt, weil
  einem Worker der Kern fehlt, solange der Hauptthread rechnet, und eine Stufe auf ihren langsamsten Worker wartet. Dort
  ist parallel mit 0,25 ms Arbeit auf dem Hauptthread gleichauf und mit 1 ms 20 % schneller, siehe „Parallel gegen
  nacheinander mit schneller Grafikkarte“. Auf dem Ryzen nimmt ein freier Kern den Verlust nicht: Mit 7 Threads sind
  parallel und nacheinander langsamer als mit 8, siehe Schritt 27 und 28. Woran es dort liegt, ist offen.

## Schritte

Gewinn jeweils geschätzt für einen Frame mit 8 Threads beim Abriss in der Demo, wenn nicht anders gesagt. „Bitgleich“
heißt: die Ergebnisse ändern sich nicht. Die Zeilen stehen in der Reihenfolge, in der sie dran sind, die Nummern
bleiben.

| Nr. | Schritt | Gewinn | Voraussetzung | Stand |
| ---: | --- | --- | --- | --- |
| 1 | **Skalierung messen in der Bibliothek**, mit Knopf in der Demo und Ausgabe im Benchmark | Werkzeug | – | ✅ auf `physik-21` |
| 2 | **Auf dem Ryzen messen**: in der Demo „Skalierung messen“, die Tabelle aus der Zwischenablage in den Chat | entschied 3 bis 9 | 1 | ✅ am 17. Oktober: Die Bedingungen skalieren nicht über 4 Threads |
| 2a | **Große Insel beim Abriss in der Demo**: zählen, wie viel der 73 % zusammenhängt | – | – | ✅ gezählt: 57 % hängen zusammen, Inseln teilen lohnt nicht |
| 2c | **Warum die Farben nicht skalieren**: Überlauf, Größe der Farben, Sperren | – | 2 | ✅ gezählt: der Überlauf an Knoten |
| 10 | **Knoten im Überlauf mit geteilter Masse**: bis zu acht Kontakte eines Knotens in den acht Spuren einer breiten Bedingung, jede gegen eine Kopie mit einem Achtel seiner Masse, ändert die Ergebnisse | 15 bis 20 %, mit einem Thread 5 bis 11 % | 2c | ✅ auf `physik-22`: auf dem Ryzen mit 8 Threads die Bedingungen 53 und 51 % und ein Schritt 26 und 19 % billiger |
| 12 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-22` bei ×4 und ×2 | entschied 11, 2b und 4 | 10 | ✅ am 17. Oktober: bei ×4 bremst jetzt die größte Insel |
| 4 | **Größte Insel im Team**: Alle Threads rechnen sie Farbe für Farbe, die übrigen Inseln füllen als Pakete die Zeit, in der ein Thread sonst an einer Sperre oder auf den Überlauf wartet, bitgleich | ×4 3 bis 8 %, ×2 2 bis 4 % | ✅ 12 | ✅ auf `physik-24`: auf der VM mit 4 Threads die Bedingungen bei ×4 15 % und bei ×2 4 % billiger |
| 13 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-24` bei ×4 und ×2 | entschied 3 | 4 | ✅ am 18. Oktober: Das Team trägt, bei ×4 mit 8 Threads kosten die Bedingungen 16 % weniger |
| 3 | **Prüfungen des Updates auf den Workern, zweiter Versuch**, bitgleich | 4 bis 6 % | ✅ Wecken in 7 bis 10 µs | ✅ auf `physik-25`: auf der VM mit 4 Threads die Ruhe-Prüfung bei ×4 21 % und bei ×2 25 % billiger |
| 14 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-25` bei ×4 und ×2 | entschied 15 und 5 | 3 | ✅ am 18. Oktober: die Ruhe-Prüfung mit 8 Threads 43 und 42 % billiger, der Löser ist der größte Posten |
| 15 | **Wartezeiten im Löser zählen**: wie lange die Threads an den Stufen des Teams warten, was der Überlauf auf dem aufrufenden Thread kostet und wie ungleich die Stufen verteilt sind | Werkzeug, entscheidet, wie es im Löser weitergeht | 14 | ✅ auf `physik-26`: auf der VM mit 4 Threads rechnet der aufrufende Thread ein Fünftel bis ein Sechstel der Zeit des Teams allein am Überlauf, und jeder Worker wartet ein Drittel davon |
| 5 | **Schwellen fürs Wecken** der Bruchrechnung, bitgleich | 1 bis 3 % | ✅ Wecken | ✅ auf `physik-26`: Die Worker helfen, sobald der aufrufende Thread allein dreimal so lange bräuchte, wie einer zum Anlaufen braucht. Auf der VM bleiben sie nahe 32 und 512 |
| 16 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-26` bei ×4 und ×2 | entschied 17 | 15 und 5 | ✅ am 18. Oktober: Im Team rechnet der aufrufende Thread bei ×4 40 % der Blöcke der Stufen selbst, jeder Worker wartet 47 % |
| 17 | **Übrige Pakete im Team anders verteilen**: Ein Worker nimmt ein Paket erst nach einer Weile ohne neue Stufe, nur im Überlauf, erst nach der letzten Stufe oder nur, solange wenige andere eines rechnen, bitgleich | – | 16 | ⛔ im Prototyp auf `physik-27`: Der aufrufende Thread rechnet dann weniger Blöcke selbst, aber die Pakete fehlen an anderer Stelle, das Team wird nicht kürzer |
| 20 | **Lagen zum Zeichnen zwischen zwei Schritten**: Die Bibliothek gibt zum Zeichnen eine Lage zwischen den letzten zwei Ständen der Physik, die Demo ruft das nur auf | flüssige Bewegung auf Monitoren über 60 Hz, keine Rechenzeit, das Bild liegt einen Schritt zurück | – | ✅ auf `physik-28`: `nbWorld_GetDrawPoses` und `nbWorld_GetStepFraction`, in der Demo mit Schalter und Zeitlupe. Ein Aufruf kostet auf der VM bei ×4 0,16 ms pro Bild |
| 18 | **Kontakt-Kerne von upstream #189 messen**: ob upstreams neue breite Kontakt-Kerne im Haufen aus Hüllen schneller rechnen als die des Forks | prüfen, bei Erfolg 1 bis 3 %, ändert die Ergebnisse | – | ✅ auf `physik-29`: Upstream rechnet die Bedingungen mit #189 2 bis 8 % billiger. Übernommen kosten sie im Benchmark auf der VM 1 bis 8,5 % weniger, meist 3 bis 4 %, beim Abriss in der Demo bei ×4 1,9 bis 4,9 % |
| 21 | **Trennachsen-Suche von upstream #173 und #177**: der Test zweier Hüllen wie upstream, ändert die Ergebnisse | rund 1 % | – | ✅ auf `physik-30`: Die Suche braucht auf denselben Hüllenpaaren 5,1 % weniger Takte, mit einem Thread 0,5 bis 0,7 % eines Schritts, und erkennt getrennte Hüllen, die die alte übersah. Die zweite Suche, die upstream gestrichen hat, bleibt, ebenso das Format der Hüllen |
| 22 | **Upstream sichten**: was upstream Box3D nach #191 geändert hat, im Haufen aus Hüllen messen und übernehmen, was sich lohnt, wie bei 18 und 21 | unbekannt | – | ✅ auf `physik-31`: Nach #191 kam nichts mehr. Von den PRs seit dem gepinnten Stand betreffen Nebenan nur die drei Fehlerbehebungen aus #187, übernommen, für Nebenans Trümmer bitgleich |
| 6 | **Typwechsel gesammelt**, bitgleich | unter 1 % | – | ⛔ verworfen am 20. Oktober: Die Hälfte eines Erstarrens ist das Löschen der Kontakte, das seit `physik-36` vorausholt, der Rest Umzug, Baum und Masse. Gesammelt ließen sich nur Wartezeiten überlappen, auf dem Ryzen unter 1 % |
| 19 | **Physik mit 30 Hz und 8 Teilschritten**, als Versuch auf einer eigenen Branch, ändert das Verhalten überall | pro simulierter Sekunde ein Viertel bis ein Drittel der Physik | 20 | ✅ wählbar auf `physik-44`: Ein Spiel stellt 30, 60 oder 120 Hz ein, jeweils mit 240 Teilschritten pro Sekunde. Auf der VM kostet eine simulierte Sekunde mit 30 Hz 14 bis 29 % weniger, mit 120 Hz 28 bis 44 % mehr. Die Vorgabe bleibt 60 Hz, dort ist alles bitgleich. Auf dem Ryzen kostet eine simulierte Sekunde beim Abriss bei ×2 mit 8 Threads 24 % weniger, aber ungebremst kommt der teurere Schritt in jedem zweiten Bild, das 1 % Low fällt von 215 auf 186 FPS |
| 7 | **Rand der Boxen mit 8 Threads** nachmessen, ändert die Ergebnisse | klein | 2 | ⛔ nachgemessen am 20. Oktober: Ein größerer Rand spart die seriellen Teile nicht. Mit 10 mm kostet ein Schritt bei ×1 hochgerechnet auf 8 Threads 9 % mehr, bei ×2 und ×4 hängt es vom Verlauf ab. Der Rand bleibt bei 5 mm |
| 8 | **Kompakte Paare** statt Kontakte für nicht berührende Paare, ändert die Ergebnisse | 2 bis 4 % | 2 | ⛔ verworfen am 20. Oktober: Die vollen Tests bleiben auch mit kompakten Paaren, auf dem Ryzen wären mit 8 Threads 1 bis 2 % zu holen, und Box3D bekäme für jedes Paar zwei Wege |
| 11 | **Überlauf in unabhängigen Gruppen** auf mehreren Threads, bitgleich | höchstens 1 bis 2 %: der Überlauf ist mit 8 Threads noch 4 und 6 % des Frames | 2c | ⛔ gezählt am 20. Oktober: Beim Abriss trägt die größte Gruppe bei ×2 88 bis 90 % des Überlaufs, den der aufrufende Thread allein rechnet, bei ×4 78 bis 79 %. Er würde höchstens 10 und 22 % kürzer, auf dem Ryzen rund 0,02 ms pro Schritt, für zwölf Sperren mehr |
| 2b | **Schwelle zwischen Paketen und Farben nach Threads**: Box3D nimmt Farben erst über der Hälfte, bitgleich | allein nichts: Farben und Pakete stehen mit 8 Threads beide bei einem Drittel der Zeit eines Threads | 12 | ✅ in 4 aufgegangen: Ein Team nimmt das größte Paket ab fünf Vierteln des Anteils eines Workers, auch über der Hälfte |
| 9 | **Buchhaltung von Box3D pro Schritt neu bauen**, großer Umbau über Monate, ändert die Ergebnisse | höchstens 4 bis 6 % | 2 | ⛔ nach der ersten Stufe verworfen am 20. Oktober: Neue Kontakte parallel anzulegen kostete mit 4 Threads 3 bis 10 % mehr als die serielle Schleife, die seit `physik-41` zum Schreiben vorauslädt. Hochgerechnet auf 8 Threads blieben höchstens 0,03 ms pro Schritt |
| 23 | **Budget auf den Workern**: Die Worker prüfen vorab, was über dem Budget erstarren darf, der aufrufende Thread nimmt eine Prüfung, solange nichts erstarrt ist, was sie gelesen hat, bitgleich | ×4 5 bis 7 % auf der VM | – | ✅ auf `kollaps-5`: auf der VM mit 4 Threads bei ×4 das Budget 24 % und das Update 14 % billiger |
| 24 | **Manifolds der Worker**: In der Kontakt-Schleife holt jeder Worker Manifolds aus einem eigenen Vorrat statt einzeln unter der Sperre, bitgleich | ×4 1 bis 2 % auf der VM | – | ✅ auf `kollaps-5`: auf der VM mit 4 Threads bei ×4 die Kontakt-Schleife 7 bis 9 % billiger |
| 25 | **Auf dem Ryzen nachmessen**: „Parallel gegen nacheinander messen“ und „Skalierung messen“ mit `kollaps-5` beim Abriss | entscheidet 26, ergab 27 | 23 und 24 | ✅ „Parallel gegen nacheinander“ am 23. Oktober: parallel 7,6 % langsamer, die Grafik kostet auf der RTX nur noch 0,24 ms, und der Schritt verliert 18 %, solange der Hauptthread zeichnet. „Skalierung messen“ am 23. Oktober mit `kollaps-6`: Die Worker laufen nach 6 bis 7 µs an, mit 8 Threads bleiben 60 % auf einem Thread, und der Überlauf an Knoten kostet 0,43 ms pro Schritt |
| 27 | **Mit eigenem Schritt einen ganzen Kern frei lassen**: `nbGetRecommendedWorkerCount` lässt mit `asyncStep` auf Prozessoren mit Hyper-Threading oder Effizienzkernen einen Performance-Kern frei statt eines Hardware-Threads, auf dem 7800X3D 7 statt 8 Threads, bitgleich | parallel auf dem Ryzen rund 6 % pro Bild, mit mehr Arbeit auf dem Hauptthread mehr | 25 | ⛔ zurückgenommen auf `kollaps-6` nach 28: In einer Sitzung ist parallel mit 8 Threads 2,5 % schneller als mit 7, und die Wahl von 7 parallel gegen 8 nacheinander ist parallel 5,1 % langsamer. Vorab mit `--threads 7` über zwei Sitzungen hatte es umgekehrt ausgesehen, die erste war insgesamt langsamer |
| 28 | **Auf dem Ryzen nachmessen**: „7 gegen 8 Threads messen“ mit `kollaps-6` beim Abriss | bestätigt 27 oder nimmt es zurück | 27 | ✅ am 23. Oktober: Mit 8 Threads ist ein Bild parallel 2,5 % und nacheinander 1,7 % schneller als mit 7, parallel mit 8 nur 2,4 % langsamer als nacheinander. Schritt 27 zurückgenommen |
| 29 | **Überlauf an Knoten auf mehreren Threads**: die Bündel eines Knotens in einer festen Zahl von Gruppen zugleich auf mehreren Threads rechnen, jede gegen eigene Kopien des Knotens, danach der Mittelwert, wie „Knoten im Überlauf mit geteilter Masse“ über die Spuren hinaus, ändert die Ergebnisse | mit den Einstürzen bei ×2 bis zu 0,3 ms pro Schritt, rund 10 % eines Bilds mit 8 Threads | 25 | ✅ auf `kollaps-7`: Liegen an den Knoten großer Inseln mindestens 256 Kontakte, rechnen die Threads ihre Bündel in acht Gruppen gegen eigene Kopien. Beim Abriss auf der VM bei ×2 kostet der Überlauf auf dem aufrufenden Thread mit 2 und 4 Threads 19 und 42 % weniger, die Bedingungen 9 und 11 %, mit einem Thread pro Kontakt 17 % mehr. Der Abriss verläuft anders, in 4 000 Bildern fallen 6 statt 4 Häuser. Schwere Teile liegen wie vorher und kommen öfter zur Ruhe |
| 30 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-7` beim Abriss bei ×2 | zeigt, was 29 mit 8 Threads bringt | 29 | 🔜 |
| 26 | **Worker wach halten statt schlafen legen**: kurz aktiv warten, bevor ein Worker schläft, bitgleich | auf der VM rund ein Zehntel der Ruhe-Prüfung, kostet Rechenzeit beim Zeichnen | 25 | ⛔ nach 25: Auf dem Ryzen laufen die Worker nach 6 bis 7 µs an, aktiv zu warten spart unter 0,5 % eines Schritts |

## Was die Messung auf dem Ryzen gezeigt hat

Gemessen am 17. Oktober 2026 mit „Skalierung messen“ beim Abriss der Stadt in der Demo, ×4 und ×2, mit 1, 2, 4 und 8
Threads, siehe „Skalierung auf dem Ryzen“. Die Regeln, nach denen die Messung entscheiden sollte, und was daraus wurde:

- **Weckzeit eines Workers** unter rund 20 µs: Schritt 3 und 5 zuerst. Gemessen 8 und 10 µs im Mittel, höchstens
  40 µs. Beide gehen jetzt, sind beim Abriss aber klein: Das Update ist mit 8 Threads 10 bis 11 % des Frames, die Zellen
  auf dem Hauptthread 1 bis 3 %.
- **Löser mit 8 Threads** deutlich langsamer als ein Achtel der Zeit mit einem Thread, und die größte Insel über einem
  Achtel der Arbeit: Schritt 4, bei vielen Schritten mit Farben zuerst 2b. Gemessen: Die Bedingungen brauchen mit 8
  Threads 51 und 54 % der Zeit mit einem, die größte Insel trägt 47 und 73 %, und 42 und 84 % der Schritte lösen mit
  Farben. Gezählt, warum die Farben nicht skalieren (2c): wegen des Überlaufs, nicht wegen der Schwelle und nicht wegen
  der Sperren. Deshalb zuerst 10, danach 2b und 4 neu schätzen.
- **Anlegen und Zustandswechsel der Kontakte** zusammen über rund 15 % des Frames: Schritt 9 prüfen. Gemessen 4 bis
  6 %, Schritt 9 ist zurückgestellt.
- **Update** über einem Viertel des Frames: Schritt 3 hat Vorrang vor allem im Box3D-Schritt. Gemessen 10 bis 11 %, kein
  Vorrang.

## Was die Messung mit `physik-22` gezeigt hat

Dieselbe Messung wie bei Schritt 2, am 17. Oktober 2026 mit `physik-22` in einem selbst gebauten Release-Build, siehe
„Skalierung auf dem Ryzen mit `physik-22`“. Neu im Bericht waren die Zeile `overflow` unter den Bedingungen, die Zeit,
die der aufrufende Thread allein am Überlauf rechnet, und eine Zeile mit den Kontakten im Überlauf und wie viele davon
in Bündeln von Knoten stehen. Die Regeln, nach denen die Messung entscheiden sollte, und was daraus wurde:

- **Bedingungen mit 8 Threads** deutlich schneller als mit 4: Der Überlauf war der Boden, als Nächstes das Update
  (Schritt 3). Bleiben sie nah bei 4 Threads, bremsen die Sperren und die größte Insel: zuerst Schritt 4, dann 2b.
  Gemessen bei ×2 26 % schneller, bei ×4 nur 8 %. Bei ×4 lösen 59 % der Schritte in Paketen, und dort nimmt die größte
  Insel mit 8 Threads mindestens 34 % der Arbeit. Deshalb zuerst Schritt 4. 2b brächte allein nichts, denn Farben und
  Pakete stehen beide bei einem Drittel der Zeit eines Threads. Die Schwelle geht in Schritt 4 auf.
- **`overflow`** mit 8 Threads über rund 10 % des Frames: Schritt 11 prüfen, sonst bleibt er liegen. Gemessen 4 und
  6 %, Schritt 11 bleibt liegen.
- **Update und Einschläge** wie vorher bei rund einem Viertel des Frames: Schritt 3 und 5. Gemessen 23 und 17 %, Schritt
  3 kommt nach 4. Schritt 5 bleibt klein, die Zellen auf dem Hauptthread kosten 4 und 1 %.

Die Bildzeiten zeigen noch etwas anderes: Bei ×2 mit 8 Threads dauert ein Bild 6,31 ms bei 2,67 ms Simulation, länger
als mit 4 Threads, und das 1 % Low fällt auf 29 FPS. Das liegt außerhalb der Physik, beim Zeichnen auf der Grafik im
Prozessor, auf der die Demo dort lief. Für Bildzeiten der Demo auf dem Ryzen kommt es deshalb darauf an, auf welcher
Grafik sie zeichnet. Seit `physik-23` nimmt die Demo unter Windows selbst die Grafik mit der höchsten Leistung, siehe
„Stärkste Grafikkarte in der Demo“.

## Was die Messung mit `physik-24` gezeigt hat

Dieselbe Messung mit `physik-24`, am 18. Oktober 2026 bei ×4 und ×2, zum ersten Mal auf der Grafikkarte, siehe
„Skalierung auf dem Ryzen mit `physik-24`“. Neu im Bericht waren die Grafikkarte und eine Zeile, in wie vielen Schritten
ein Team das größte Paket rechnet und wie viel Arbeit es dort hält. Verglichen mit „Skalierung auf dem Ryzen mit
`physik-22`“, an der Physik hat `physik-23` nichts geändert, und die Welt endet wie dort. Die Regeln, nach denen die
Messung entscheiden sollte, und was daraus wurde:

- **Bedingungen mit 8 Threads** bei ×4 deutlich unter den 0,393 ms von `physik-22`, und von 4 auf 8 Threads wieder
  schneller: Das Team trägt, als Nächstes das Update (Schritt 3). Bleiben sie nah bei 0,39 ms, bremsen die Sperren des
  Teams. Dann zuerst zählen, wie lange die Threads dort warten, und was ein Team mit weniger Stufen bringen würde.
  Gemessen 0,329 ms, 16 % weniger, an der Kontakt-Schleife gemessen 17 %, und von 4 auf 8 Threads 15 statt 8 %
  schneller. Das Team trägt, Schritt 3 ist dran. Mit 8 Threads brauchen die Bedingungen aber noch 31 % der Zeit eines
  Threads statt eines Achtels. Was davon die Sperren des Teams kosten und was der Überlauf, den der aufrufende Thread
  jetzt mit 0,087 ms rechnet, ist nach Schritt 3 zu zählen.
- **Bei ×2** rechnete schon vorher ein Löser mit Farben über alle Threads, dort ist wenig zu erwarten. Werden die
  Bedingungen dort langsamer als 0,744 ms, kostet das Team mehr, als die Pakete nebenher sparen, dann die Schwelle
  `B3_TEAM_SHARE_NUM` / `B3_TEAM_SHARE_DEN` prüfen. Gemessen 0,678 ms, an der Kontakt-Schleife gemessen so viel wie mit
  `physik-22`, bei Läufen, die dort um bis zu 16 % schwankten. Die Schwelle bleibt.
- **Update und Einschläge** wie zuletzt bei rund einem Viertel des Frames: Schritt 3. Gemessen 24 und 17 %, die
  Ruhe-Prüfung allein 0,125 und 0,212 ms, 10 und 8 %. Schritt 3 kommt als Nächstes, auf `physik-25`.

Mit der Grafikkarte sagen jetzt auch die Bildzeiten etwas: Bei ×2 mit 8 Threads dauert ein Bild 2,99 statt 6,31 ms,
schneller als mit 4 Threads, und das 1 % Low steigt von 29 auf 189 FPS. Neben der Simulation kostet ein Bild mit 8
Threads rund 0,5 ms.

## Was die Messung mit `physik-25` gezeigt hat

Dieselbe Messung mit `physik-25`, am 18. Oktober 2026 bei ×4 und ×2, siehe „Skalierung auf dem Ryzen mit `physik-25`“.
Neu im Bericht war eine Zeile, in wie vielen Schritten die Prüfungen des Updates an die Worker gingen, wie viel der
bewegten Trümmer sie dort übernahmen und wie schnell sie anliefen. Verglichen mit „Skalierung auf dem Ryzen mit
`physik-24`“. Die Welt endet wie dort. Die Regeln, nach denen die Messung entscheiden sollte, und was daraus wurde:

- **Ruhe-Prüfung mit 8 Threads** (`settle`) bei ×4 und ×2 rund halb so teuer wie die 0,125 und 0,212 ms von `physik-24`,
  und die Worker übernehmen drei Viertel der bewegten Stücke und mehr: Schritt 3 trägt. In der Ruhe-Prüfung bleibt dann
  vor allem das Erstarren, das ist Schritt 6. Gemessen 0,071 und 0,123 ms, 43 und 42 % weniger. Die Worker übernehmen 70
  und 71 %, gemittelt über die Läufe mit 2, 4 und 8 Threads, in denen ihr gleicher Anteil 50, 75 und 87,5 % wäre.
  Schritt 3 trägt.
- **Bleibt sie fast gleich**, zeigt die neue Zeile warum. Laufen die Worker später als rund 30 µs an oder übernehmen sie
  weniger als die Hälfte, kostet das Wecken mehr, als die Prüfungen bringen. Dann die Schwelle `NB_PARALLEL_SETTLE`
  heben oder die Worker nach einer Aufgabe kurz wach halten. Nicht eingetreten, die Worker laufen nach 7 und 8 µs an.
- **Was danach am meisten kostet**, kommt als Nächstes: die Einschläge mit den Zellen auf dem Hauptthread, bei ×4
  zuletzt 0,126 und 0,048 ms (Schritt 5), das Erstarren (Schritt 6) oder der Löser, dessen Bedingungen mit 8 Threads
  noch 31 % der Zeit eines Threads brauchen. Für den Löser wäre zuerst zu zählen, wie lange die Threads an den Sperren
  des Teams warten und was der Überlauf auf dem aufrufenden Thread kostet. Gemessen über einem Achtel der Zeit eines
  Threads: die Bedingungen mit 0,216 und 0,429 ms, 17 und 16 % des Frames, die Kontakt-Schleife mit 9 und 8 %, die
  Einschläge mit 9 und 4 %, darin die Zellen auf dem Hauptthread mit 4 und 1 %, das Update mit 7 % und das Erstarren mit
  3 %. Als Nächstes deshalb Schritt 15, zählen, wo die Zeit der Bedingungen hingeht, und mit ihm Schritt 5, denn ein
  Worker läuft auf dem Ryzen nach 7 µs an, die Schwellen der Bruchrechnung sind für die 67 µs der VM gewählt.

Ein Schritt samt Update und Einschlägen kostete mit 8 Threads trotzdem gleich viel wie mit `physik-24`, 1,242 statt
1,234 und 2,607 statt 2,579 ms, denn die Stufen des Box3D-Schritts, an denen sich nichts geändert hat, lagen in dieser
Messung mit 8 Threads 1 bis 11 % höher, mit 1, 2 und 4 Threads bei ×4 bis 5 % daneben. Mit 4 Threads kostet ein Schritt
bei ×4 4 % weniger. Mit einem Thread kostet die Ruhe-Prüfung 6 und 9 % mehr, 0,007 und 0,018 ms, die Verwaltung der
Blöcke.

## Was die Messung mit `physik-26` gezeigt hat

Dieselbe Messung mit `physik-26`, am 18. Oktober 2026 bei ×4 und ×2, siehe „Skalierung auf dem Ryzen mit `physik-26`“.
Neu im Bericht waren eine Tabelle, wohin die Zeit des Teams geht, gemessen in jedem achten Schritt mit Team, eine Zeile,
wie lange der aufrufende Thread auf die letzten Pakete wartet, und eine Zeile, ab wie vielen Zellen und Stücken die
Worker bei einem Bruch helfen und ab wie vielen bewegten Stücken bei den Prüfungen des Updates. Verglichen mit
„Skalierung auf dem Ryzen mit `physik-25`“. Die Welt endet wie dort. Der Lauf bei ×2 war insgesamt langsamer, auch die
Kontakt-Schleife, an der sich nichts geändert hat, lag mit 1, 2, 4 und 8 Threads 7, 19, 22 und 7 % über `physik-25`.
Seine Zeiten taugen nicht für den Vergleich mit `physik-25`, die Anteile in der Tabelle des Teams schon. Auf der VM
rechnete der aufrufende Thread mit 4 Threads bei ×4 und ×2 ein Fünftel und ein Sechstel der Zeit des Teams allein am
Überlauf, und jeder Worker wartete ein Drittel davon auf eine neue Stufe, siehe „Zeiten des Teams beim Abriss“. Die
Regeln, nach denen die Messung entscheiden sollte, und was daraus wurde:

- **Überlauf allein** mit 8 Threads über einem Drittel der Zeit des Teams: Der Überlauf bestimmt das Team, die Worker
  warten auf ihn oder rechnen die übrigen Pakete. Dann als Nächstes den Überlauf des Teams auf mehrere Threads
  verteilen, Schritt 11 mit den Zahlen der Tabelle neu schätzen. Gemessen 28 und 29 %, 0,106 und 0,216 ms pro
  gemessenem Schritt. Knapp darunter, Schritt 11 bleibt zurückgestellt.
- **Warten pro Worker** über der Hälfte der Zeit des Teams, deutlich länger als der Überlauf allein, und mehr als ein
  Yield pro Stufe: Die Worker warten zwischen den Stufen, geben dabei ihren Kern ab und kommen spät in die nächste. Dann
  im Team länger drehen, bevor ein Worker seinen Kern abgibt, bitgleich, und auch unter Last messen, siehe „Beim Warten
  den Kern abgeben“. Gemessen 47 % bei ×4 und ×2, 0,177 und 0,358 ms, 1,7-mal so lange wie der Überlauf allein, und 4,5
  und 4,3 Yields pro Stufe. Knapp unter der Hälfte, nicht eingetreten. So hätte die Regel auch nicht stehen dürfen:
  Länger zu drehen ist auf der VM schon zweimal gemessen und verworfen, siehe „Im Team länger drehen“. Dazu zeigt die
  Tabelle, wer die Stufen rechnet: Bei ×4 bleiben dem aufrufenden Thread neben Überlauf und Warten 0,256 ms, einem
  Worker neben Warten und Paketen 0,054 ms. Der aufrufende Thread rechnet also rund 40 % der Blöcke der Stufen selbst,
  jeder der sieben Worker 8,6 %, bei ×2 23 und 11 %. Eine Stufe dauert bei ×4 auf dem aufrufenden Thread im Mittel
  2,5 µs. Warum, zeigte ein Prototyp auf der VM: Die Worker nehmen zwischen zwei Stufen die übrigen Pakete, rund 0,2 ms
  lang, und kommen so im Mittel 2,6 statt 0,8 µs nach dem Öffnen in eine Stufe. Ohne diese Pakete zwischen den Stufen
  rechnet der aufrufende Thread mit 4 Threads 29 statt 52 % der Blöcke, aber die Pakete fehlen dann an anderer Stelle,
  und keine Regel dazwischen machte das Team kürzer, siehe „Übrige Pakete im Team anders verteilen“. Schritt 17
  verworfen.
- **Warten am Ende der Stufen** über einem Viertel der Zeit des Teams: Die rund 110 Stufen bei ×4 und 210 bei ×2 pro
  Schritt sind für 8 Threads zu kurz. Dann kleine Stufen den aufrufenden Thread allein rechnen lassen, ohne Sperre,
  bitgleich. Gemessen 4 und 7 %, nicht eingetreten: Der aufrufende Thread wartet kaum, weil er die meisten Blöcke selbst
  rechnet. Auch das wäre eine Wiederholung gewesen, kleine Farben allein auf dem aufrufenden Thread änderten auf der VM
  nichts, siehe „Kleine Farben auf dem Hauptthread“ und „Kleine Farben auf dem aufrufenden Thread“.
- **Nichts davon groß**: Das Team verteilt seine Arbeit, und die Bedingungen hängen an ihrer Menge. Dann ist am Team
  wenig zu holen, und als Nächstes kommt, was danach am meisten kostet: die Kontakt-Schleife, die Einschläge oder das
  Erstarren (Schritt 6). Nach den Grenzen eingetreten, nach der Verteilung der Blöcke nicht: Was die Bedingungen mit 8
  Threads über einem Achtel der Zeit eines Threads brauchen, 0,224 und 0,468 ms, 18 und 17 % des Frames, ist weiter der
  größte Posten, vor der Kontakt-Schleife mit 9 und 8 % und den Einschlägen mit 9 und 4 %. Die Kontakt-Schleife verteilt
  ihre Blöcke schon von selbst. Typwechsel und Einschläge zerfallen in Posten von je 1 bis 3 % des Frames, siehe „Wohin
  ein Typwechsel und ein Einschlag ihre Zeit geben“, Schritt 6 ist deshalb zurückgestellt.
- **Schwellen**: Laufen die Worker wie zuletzt nach 7 bis 8 µs an, helfen sie mit 8 Threads bei einem Bruch ab rund 10
  bis 16 Zellen und Stücken, denn eine Zelle kostet auf der VM 6,5 bis 10 µs und auf dem Ryzen rund ein Viertel davon.
  Beim Abriss bei ×4 haben 93 % der Brüche 8 bis 31 Zellen und Stücke, rund zwei Drittel gingen dann an die Worker. Die
  Zellen auf dem Hauptthread (`cellsAlone`) fallen damit bei ×4 deutlich unter die 0,049 ms von `physik-25`, und die
  Einschläge werden billiger. Werden sie teurer, ist der Faktor `NB_WORKER_WAKE_FACTOR` zu klein. Die Schwelle der
  Prüfungen des Updates hängt davon ab, wie teuer die bewegten Stücke gerade sind, auf der VM 0,2 bis 0,65 µs: Auf dem
  Ryzen liegt sie zwischen rund 150 und 512. Gemessen: Die Worker laufen nach 6 und 7 µs an und helfen bei einem Bruch
  ab 8 Zellen und Stücken, der Untergrenze, bei den Prüfungen ab 512 und 434 bewegten Stücken. Die Zellen auf dem
  Hauptthread kosten bei ×4 mit 8 Threads 0,001 statt 0,049 ms, alle Zellen 31 % weniger, mit 4 Threads 37 %. Die
  Einschläge kosten mit 8 Threads aber gleich viel, 0,129 statt 0,130 ms, mit 2 und 4 Threads 8 und 4 % weniger, in
  Läufen, in denen sie mit einem Thread ohne jede Änderung 12 % mehr kosteten. Im Frame ist das nicht zu sehen. Der
  Faktor bleibt.

Ein Schritt samt Update und Einschlägen kostet bei ×4 mit 8 Threads 1,247 statt 1,242 ms. Die Bedingungen liegen mit 8
Threads 3 % höher, 0,354 statt 0,343 ms, mit einem Thread, wo kein Team rechnet, ebenso 3 %. Mit 1, 2 und 4 Threads
lagen bei ×4 auch die Stufen, an denen sich nichts geändert hat, 2 bis 6 % höher.

## Was die Messung mit `kollaps-5` gezeigt hat

Gemessen am 23. Oktober 2026 mit „Parallel gegen nacheinander messen“ beim Abriss Haus für Haus bei ×2 mit Budget 5 000,
zum ersten Mal auf der RTX 4070 SUPER statt der Grafik im Prozessor, siehe „Parallel gegen nacheinander mit schneller
Grafikkarte“. „Skalierung messen“ mit `kollaps-5` steht noch aus, und damit, was Schritt 26 entscheiden sollte: wie
schnell die Worker dort anlaufen. Nachgemessen mit `kollaps-6`, dessen Physik der von `kollaps-5` gleicht, siehe
„Skalierung auf dem Ryzen mit `kollaps-6`“.

- **Parallel gegen nacheinander**: Parallel ist 7,6 % langsamer, 3,33 gegen 3,09 ms pro Bild. Das Zeichnen kostet den
  Hauptthread nur noch 0,24 ms, die Simulation braucht auf ihrem Thread aber 3,13 statt 2,66 ms. Eine Probe auf der VM
  zeigt, warum: Allein rechnet der Schritt auf seinem Thread so schnell wie auf dem aufrufenden. Rechnet der aufrufende
  Thread in der Zeit, verlieren die parallelen Stufen, weil eine Stufe auf ihren langsamsten Worker wartet und einem
  Worker der Kern fehlt. Auf dem Ryzen teilt sich der Hauptthread einen Kern mit einem der 8 Threads. Daraus Schritt 27.
  Vorab mit `--threads 7` gemessen, beide Arten mit 7 Threads: Parallel ist nur noch 2,9 % langsamer, 3,13 gegen
  3,04 ms, und der Schritt verliert 11 statt 18 %. In einer Sitzung mit „7 gegen 8 Threads messen“ waren beide Arten mit
  8 Threads aber schneller als mit 7, der Vergleich über zwei Sitzungen täuschte, siehe Schritt 28.
- **Für Schritt 26** heißt das: Worker, die aktiv warten statt zu schlafen, hielten die Kerne auch im Update besetzt,
  bisher tun das nur die Worker von Box3D im Schritt. Ob sich das lohnt, muss die Messung mit einem Programm zeigen, das
  in der Zeit rechnet, nicht nur mit „Skalierung messen“, wo die Physik nacheinander läuft. Gemessen: Die Worker laufen
  nach 6 bis 7 µs an, aktiv zu warten spart unter 0,5 % eines Schritts. Schritt 26 verworfen.
- **Der Überlauf an Knoten** ist mit den Einstürzen wieder der größte Posten auf dem aufrufenden Thread: bei ×2 mit 8
  Threads 0,43 ms pro Schritt, 15 % des Bilds, in den Schritten mit Team 40 % von dessen Zeit, und jeder Worker wartet
  61 % auf eine neue Stufe. Mit `physik-26` lag er bei 28 und 29 %, unter dem Drittel, ab dem er vorgezogen werden
  sollte. Daraus Schritt 29. Auf `kollaps-7` gebaut: Auf der VM kostet der Überlauf beim Abriss mit 4 Threads 42 %
  weniger, auf dem Ryzen nachzumessen, siehe Schritt 30.

## Was als Nächstes in Frage kommt

Schritt 20, die Lagen zum Zeichnen zwischen zwei Schritten, ist auf `physik-28` umgesetzt. Er spart keine Rechenzeit,
macht aber die Bewegung auf Monitoren über 60 Hz flüssig, und mit ihm kann ein Spiel einen langsameren Takt der Physik
wählen, ohne dass es ruckelt, siehe „Lagen zum Zeichnen zwischen zwei Schritten“. Auf dem Ryzen ist nichts zu messen,
nur anzusehen: in der Demo „Zeitlupe“ an und „Zwischen Schritten zeichnen“ an und aus, am besten mit der Kanone. Ohne
die Lagen dazwischen springen die Trümmer in der Zeitlupe sichtbar von Schritt zu Schritt.

Schritt 18, die Kontakt-Kerne von upstream #189, ist auf `physik-29` übernommen: Pro Punkt eines Kontakts rechnen sie
weniger, und die Bedingungen kosten im Benchmark auf der VM 1 bis 8,5 % weniger, meist 3 bis 4 %, beim Abriss in der
Demo bei ×4 1,9 bis 4,9 %. Auf dem Ryzen wäre das mit 8 Threads rund 1 % des Frames, siehe „Löser-Umbau von upstream
#189“. Nachmessen lässt es sich dort mit „Skalierung messen“ bei ×4 und ×2, die Zeile `constraints` sollte um 3 bis 4 %
sinken. Das liegt aber innerhalb dessen, was die Läufe dort streuen, und entscheidet nichts.

Schritt 21, die Trennachsen-Suche von upstream #173 und #177, ist auf `physik-30` übernommen. Gemessen mit beiden Suchen
im selben Programm auf denselben Hüllenpaaren braucht die neue 5,1 % weniger Takte, mit einem Thread 0,5 bis 0,7 % eines
Schritts, im Benchmark ist das nicht zu sehen. Sie ist auch genauer: In 0,17 % der Suchen erkennt sie Hüllen als
getrennt, die die alte übersah, und GJK bestätigt das. Die volle zweite Suche, die upstream gestrichen hat, bleibt,
sonst bekämen Hüllen, die sich überlappen, in seltenen Fällen keinen Kontakt. Den Kosinus der Kanten rechnet der Fork
aus den Normalen, statt ihn wie upstream in der Hülle zu speichern, damit Hüllen und Pakete ihr Format behalten, siehe
„SAT-Verbesserungen von upstream“. Auf dem Ryzen ist nichts zu entscheiden, der Gewinn liegt unter dem, was die Läufe
dort streuen.

Schritt 22, upstream sichten, ist auf `physik-31` erledigt: Nach #191 vom 15. Oktober hat upstream nichts mehr geändert.
Von den PRs seit dem gepinnten Stand betreffen die meisten, was Nebenan nicht nutzt, Netze, Compounds, Recording und
Mover. Übernommen sind die drei Fehlerbehebungen aus #187: vorausschauende Kontakte zwischen Kugeln und Kugeln oder
Kapseln, kein Wiederverwenden ohne vorausschauende Punkte und eine verfallene Lage, wenn PreSolve einen Kontakt
abschaltet, siehe „Upstream bis #191 gesichtet“. Für Nebenans Trümmer ändert sich nichts. Wieder zu sichten, wenn
upstream am Löser oder am Test zweier Hüllen etwas ändert.

Der kleine Punkt „Kollision zweimal übersetzen“ ist auf `physik-32` umgesetzt: Eine Welt mit acht Spuren kollidiert
konvexe Formen mit einer zweiten Fassung von `convex_manifold.c`, übersetzt für AVX2. Gemessen mit beiden Fassungen auf
denselben Hüllenpaaren braucht der Test zweier Hüllen damit 3,4 bis 3,9 % weniger Takte, gut 1 % eines Schritts mit
einem Thread, bitgleich. Im Benchmark ist das nicht zu sehen, und auf dem Ryzen läge es unter dem, was die Läufe
streuen, siehe „Kollision zweimal übersetzen“.

Der kleine Punkt „Box gegen gemerkte Fläche im ersten Durchgang“ ist auf `physik-33` umgesetzt, mit einer schärferen
Schranke dazu: Für zwei Hüllen, die eine Fläche auseinanderhielt, merkt sich der Kontakt die Ebene der Fläche und die
Grenzen der anderen Hülle. Die Kollision überspringt den Test, solange die Grenzen weit genug vor der Ebene liegen oder
sich der gemerkte Abstand seitdem nicht genug verkleinert haben kann, wobei nur die Bewegung entlang der Normalen zählt.
In den drei Städten des Benchmarks kommen damit 31 % weniger Kontakte in den Test, die zweite Schleife der Kollision
braucht 6,7 % weniger Takte, bitgleich. Ein Kontakt ist dafür 248 statt 232 Byte groß. Gepaart gegen `physik-32` kostet
die Kollision bei ×1 rund 5 % weniger, bei ×4 mit einem und zwei Threads 3 bis 5 %, bei ×2 ist nichts zu sehen, siehe
„Box gegen gemerkte Fläche im ersten Durchgang“.

Der kleine Punkt „Neue Kontakte zwischen Hüllen“ ist auf `physik-34` untersucht, und er führte woanders hin: Die volle
Suche nach der Trennachse ist schon beschnitten und rechnet im Mittel nur 3,2 Stützpunkte, aber der Test wartete auf die
Arrays der Hüllen, und zwar bei allen Kontakten. Jetzt holt die Kollision die Hüllen jedes Kontakts ganz voraus,
bitgleich. Gepaart gegen `physik-33` kostet sie bei ×1 8 bis 13 % weniger, bei ×4 14 bis 17 %, ein Frame bei ×1 3 bis 6
% und bei ×4 7 bis 10 %, siehe „Hüllen ganz vorausladen“. Auf dem Ryzen antwortet der L3 schneller, dort bringt es
weniger, siehe unten.

Auf `physik-35` ist nachgesehen, ob auch die Suche in den Bäumen auf Speicher wartet. Die Abfragen kommen vor allem von
der CCD, und dort kostet nicht der Baum, sondern die Zeit bis zum Treffer, die in 79 % der Fälle keinen findet. Die
Kinder vorauszuholen bringt nichts, ein schärferer Vortest der CCD netto 0,6 %, beides nicht gebaut, siehe „Kinder im
Baum bei Abfragen vorausladen“ und „Schärferer CCD-Vortest“. Mit 8 Threads zählen aber die Stufen, die allein auf dem
aufrufenden Thread laufen. Zwei davon in Box3D holen jetzt voraus, was sie lesen: Das Anlegen neuer Kontakte kostet bei
×1 28 bis 36 % weniger, das Abarbeiten der Kontakte mit neuem Zustand 17 bis 20 %, bitgleich, siehe „Serielle Schleifen
vorausladen“. Gepaart gegen `physik-34` fallen bei ×1 0,20 bis 0,23 ms pro Schritt auf dem aufrufenden Thread weg. Auf
dem Ryzen nachgemessen, siehe unten.

Auf `physik-36` ist gemessen, worauf die übrigen seriellen Stufen warten, siehe „Worauf die übrigen seriellen Stufen
warten“. Gerechnet wird dort wenig, gewartet auf Box3Ds Buchführung: verkettete Listen, Lücken in Arrays und die Bäume.
Zwei Dinge ließen sich bitgleich holen: Box3D holt beim Löschen von Kontakten voraus, was es anfasst, und Nebenan sucht
den Schutt, den ein wegrückendes Stück freigibt, nur noch im Baum der statischen Körper. Gepaart gegen `physik-35`
kostet das Lösen von Schutt bei ×1 30 bis 32 % weniger, die Zustandswechsel der Kontakte bis 10 % und die Typwechsel 3
bis 7 %, zusammen 0,06 bis 0,12 ms pro Schritt auf dem aufrufenden Thread, siehe „Abbau der Kontakte vorausladen“ und
„Schutt nur im statischen Baum suchen“. Der größte Posten des Updates auf der VM, der Kollisionsschaden, wartet dort auf
Zellen, die auf dem Ryzen schon die Worker rechnen.

Auf dem Ryzen nachgemessen am 19. Oktober, `physik-32` gegen `physik-36` beim Abriss in der Demo, siehe „Skalierung auf
dem Ryzen mit `physik-36`“: Die Kollision kostet dort 4 bis 7 % weniger, auf der VM waren es 8 bis 17 %, die
Zustandswechsel der Kontakte 7 bis 18 %, das Lösen von Schutt 23 bis 33 %, die Typwechsel gleich viel. Ein Schritt
kostet mit 1 bis 4 Threads 2 bis 4 % weniger, mit 8 Threads bei ×4 gleich viel und bei ×2 2 %. Mit 8 Threads ist ein
Schritt dort 7 und 15 % eines Bildes bei 60 Hz, und was bleibt, ist vor allem der Löser mit dem Überlauf auf dem
aufrufenden Thread. Was feinere Bruchstücke, ein größeres Budget der bewegten Trümmer und eine längere Ruhezeit kosten,
steht in „Was mehr Realismus kostet“. Ein Spiel stellt sie selbst ein, die Vorgaben bleiben.

Auf `physik-38` sucht Box3D die neuen Paare in einer parallelen Schleife statt in zwei, siehe „Paarsuche in einem
Durchgang“. Auf dem Ryzen kostet die Paarsuche mit 8 Threads bei ×2 0,255 ms pro Schritt, 10 %, und sie wird von 4 auf 8
Threads nur 1,5-mal schneller: Wer mit seinem Teil der ersten Schleife fertig war, wartete an ihrem Ende, und die zweite
weckte die Worker noch einmal. Gerechnet aus Uhren je Startpaar brauchten die zwei Schleifen mit 8 Threads 1,37- bis
1,73-mal so lange wie ein gleichmäßig geteilter Durchgang, in einer sind es 1,19- bis 1,29-mal. Gepaart gegen
`physik-36` kostet die Paarsuche auf der VM mit 4 Threads bei ×2 7 bis 10 % weniger, bitgleich. Auf dem Ryzen
nachzumessen, mit „Skalierung messen“ bei ×2 und ×4, die Zeile `pairs` sollte mit 8 Threads sinken. Auf dem Ryzen
nachgemessen mit `physik-41`, siehe „Skalierung auf dem Ryzen mit `physik-41`“: Ohne das Anlegen kostet die Paarsuche
mit 8 Threads bei ×2 0,171 statt 0,255 ms und bei ×4 0,090 statt 0,122 ms, an den Stufen ohne Änderung gemessen 24 und
19 % weniger, und von 4 auf 8 Threads wird sie 1,6- und 1,5-mal schneller.

Auf `physik-39` ist der Überlauf in unabhängigen Gruppen gezählt und verworfen, siehe Schritt 11: Beim Abriss trägt die
größte Gruppe 78 bis 90 % des Überlaufs, den der aufrufende Thread allein rechnet. Umgesetzt ist dort „Zweite Suche nur
bei Überlappung“: Trennt eine Fläche zwei Hüllen, ergab die volle zweite Suche nur falsche Kontakte, die die Stücke
wegstießen, als steckten sie 3 cm bis 1,2 m ineinander.

Auf `physik-40` sind die übrigen kleinen Schritte gemessen und verworfen: Typwechsel gesammelt (Schritt 6), weil die
Hälfte eines Erstarrens das Löschen der Kontakte ist, das schon vorausholt, der Rand der Boxen (Schritt 7), weil ein
größerer Rand die seriellen Teile nicht spart, und weniger Teilschritte für langsame Inseln, weil diese nur 1 bis 13 %
der Arbeit des Lösers tragen. Von den kleinen Schritten ist damit keiner mehr offen. Was bleibt, ist Schritt 9, der
Umbau der Buchhaltung von Box3D, in Stufen auf eigenen Branches.

Auf `physik-41` ist die erste Stufe von Schritt 9 gebaut, gemessen und verworfen, siehe „Neue Kontakte parallel
anlegen“: Die Worker lasen Formen und Körper, füllten die Kontakte und hängten sie in die Listen, bitgleich. Gegen die
alte Schleife kostete das Anlegen mit 4 Threads 13 bis 17 % weniger, mit 2 Threads aber mehr, weil der einzige Worker
dann die Bäume neu baut. Beim Zählen der Durchgänge zeigte sich, dass die Schleifen auf Zeilen warten, die vorher andere
Kerne gelesen hatten. Dagegen ist jetzt „Vorausladen zum Schreiben“ gebaut: Was die seriellen Schleifen der Kontakte
schreiben, holen sie zum Schreiben voraus. Gepaart gegen `physik-40` kostet das Anlegen mit 4 Threads 13 bis 15 %
weniger, mit 2 Threads 7 bis 9 %, die Zustandswechsel mit 4 Threads 4 bis 9 %, bitgleich. Danach war der parallele Weg
mit 4 Threads 3 bis 10 % teurer als die serielle Schleife, und hochgerechnet auf 8 Threads blieben höchstens 0,03 ms pro
Schritt. Wie verabredet bleibt Schritt 9 damit stehen. Auf dem Ryzen nachzumessen, mit „Skalierung messen“ bei ×2 und
×4: Die Zeilen `createContacts` und `contactStates` sollten mit 8 Threads sinken. Auf dem Ryzen nachgemessen am 20.
Oktober: An den Stufen ohne Änderung gemessen kostet das Anlegen bei ×2 mit 1 bis 8 Threads 12 bis 17 % weniger, bei ×4
mit 1 bis 4 Threads 12 bis 18 % und mit 8 Threads 2 %, die Zustandswechsel meist 5 bis 12 %. Ein Schritt mit 8 Threads
kostet damit samt der Paarsuche aus `physik-38` 1,2 und 1,4 % weniger.

Auf `physik-43` sortiert Box3D die neuen Paare ab 64 mit einem Radix-Sort statt mit `QSORT`, siehe „Neue Paare mit einem
Radix-Sort sortieren“: ein stabiler Durchgang für jedes Byte, in dem sich die Schlüssel unterscheiden, mit derselben
Reihenfolge. Gepaart gegen `physik-42` kostet das Anlegen der Kontakte bei ×1 15 % weniger, 0,03 ms pro Schritt auf der
VM, bei ×2 8 bis 12 % und bei ×4 10 bis 14 %, bitgleich. Auf dem Ryzen sind 0,01 bis 0,02 ms pro Schritt zu erwarten,
die Zeile `createContacts` sollte sinken. Damit ist auch der letzte der kleinen Punkte mit messbarem Gewinn
abgearbeitet. Auf dem Ryzen nachgemessen mit `physik-44` bei 60 Hz, siehe „Skalierung auf dem Ryzen mit `physik-44`“:
Beim Abriss bei ×2 kostet das Anlegen mit 1 bis 8 Threads 23 bis 25 % weniger, 0,062 bis 0,068 statt 0,082 bis 0,087 ms
pro Schritt.

Auf `physik-44` ist der Takt der Physik eine Einstellung der Welt, siehe Schritt 19 und „Physik mit 30 Hz und 8
Teilschritten“: 30, 60 oder 120 Schritte pro Sekunde, jeder mit so vielen Teilschritten, dass es 240 pro Sekunde sind.
Die Grenzen pro Update und die Planung der Schritte rechnen jetzt in Zeit statt in Schritten, so nimmt die Welt bei
jedem Takt gleich viel pro Sekunde, und ein Messlauf mit 1/60 s pro Bild bleibt wiederholbar. Gemessen pro simulierter
Sekunde auf der VM: 30 Hz kostet bei ×1, ×2 und ×4 14, 23 und 29 % weniger, 120 Hz 35, 44 und 28 % mehr. Die Zerstörung
verläuft je nach Takt etwas anders, mit 30 Hz bleiben bei ×1 6 % mehr Bruchstücke liegen. Die Vorgabe bleibt 60 Hz, dort
ist alles bitgleich. Auf dem Ryzen mit „Physiktakt“ im Menü und „Skalierung messen“ nachzumessen, wenn ein Spiel den
Takt braucht. Auf dem Ryzen nachgemessen, siehe „Skalierung auf dem Ryzen mit `physik-44`“: Beim Abriss bei ×2 spart
30 Hz pro simulierter Sekunde mit 1 und 2 Threads 29 %, mit 4 Threads 26 % und mit 8 Threads 24 %. Paarsuche und
Kollision sparen ein Drittel, der Löser nur ein Sechstel, und was pro Ereignis kostet, bleibt gleich. Ein Schritt mit
30 Hz kostet das 1,4- bis 1,5-Fache und kommt nur in jedem zweiten Bild: Ungebremst fällt das 1 % Low um 13 bis 18 %,
mit VSync fällt das nicht auf, solange der Schritt in ein Bild passt. Die Vorgabe bleibt 60 Hz.

Bei der Rechenzeit ist nach `physik-36` kein gemessener kleiner Schritt mehr übrig, der mit 8 Threads mehr als 1 bis 2 %
des Frames verspricht, siehe die Schritte 6, 7 und 11 und die kleineren Punkte unten, auch nicht in den seriellen
Stufen. Was mehr bringen könnte, ist größer oder ändert das Verhalten:

- **Schritt 8**, kompakte Paare für nicht berührende Kontakte: am 20. Oktober gemessen und verworfen. Die nicht
  berührenden Kontakte kosten bei ×1 mit einem Thread rund 3,5 ms pro Schritt, 2,1 ms davon für die vollen Tests, die
  auch ein kompaktes Paar braucht. Zu holen wären auf dem Ryzen mit 8 Threads 1 bis 2 %, und Box3D bekäme für dasselbe
  Paar zwei Arten mit einem zweiten Weg durch Paarsuche, Schlafen, Erstarren, Löschen und Speichern, siehe „Was nicht
  berührende Kontakte kosten“.
- **Schritt 19**, Physik mit 30 Hz und 8 Teilschritten: der einzige große Hebel, pro simulierter Sekunde ein Viertel bis
  ein Drittel der Physik, aber Kontakte entstehen später, Treffer kommen später, und mit den Lagen aus Schritt 20 läge
  das Bild 33 statt 17 ms hinter der Physik. Gewonnen wäre nur Rechenzeit, und die fehlt auf dem Ryzen nicht: Ein
  Schritt kostet dort mit 8 Threads 8 bis 17 % eines Frames mit 60 Hz. Zurückgestellt, bis die Physik irgendwo den Frame
  begrenzt, siehe „Physik mit 30 Hz und 8 Teilschritten“. Auf `physik-44` als Einstellung gebaut: `nbWorld_SetStepRate`
  mit 30, 60 oder 120 Hz, `nbWorld_Advance` für die Schritte eines Bildes. 30 Hz spart auf der VM pro simulierter
  Sekunde 14 bis 29 %, weniger als geschätzt, denn der Löser rechnet gleich viel, und es bleiben mehr Stücke liegen. Auf
  dem Ryzen spart 30 Hz beim Abriss bei ×2 mit 8 Threads 24 %, aber ungebremst werden die Bilder ungleichmäßiger, siehe
  „Skalierung auf dem Ryzen mit `physik-44`“.

## Kleinere Punkte

Mit niedriger Priorität vorgemerkt, sie sparen vor allem Arbeit, die schon parallel läuft. Der Gewinn gilt mit einem
Thread, mit 8 Threads ist er kleiner:

- „Paarsuche mit mehr Ladevorgängen gleichzeitig“: auf `physik-38` gemessen und verworfen. Eine kleine Box folgt im Baum
  meist einem einzigen Pfad, da gibt es nichts, das schon laden könnte, und ein Prototyp kostete 3 bis 22 % mehr.
  Gebaut ist stattdessen „Paarsuche in einem Durchgang“, siehe oben.
- „Zweite Suche nur bei Überlappung“: auf `physik-39` umgesetzt. Trennt eine Fläche die Hüllen, ergab die volle zweite
  Suche nur falsche Kontakte, die 3 cm bis 1,2 m Überlappung behaupteten, wo GJK die Hüllen auseinander fand. Bei ×1
  0,65 % eines Schritts mit einem Thread, die Ergebnisse ändern sich nur nach einem solchen Kontakt.
- „Gyroskopische Drehung nach upstream #191“: das Drehmoment ganz abstellen spart höchstens 2 bis 3 %, aber drehende
  Trümmer verlören ihr Taumeln. Nur, falls ein Spiel das nicht braucht.
- „Weniger Substeps für langsame Inseln“: auf `physik-40` gezählt und verworfen. Inseln, deren Körper sich alle
  langsamer als 0,25 m/s bewegen, tragen beim Abriss bei ×4 nur 4,8 % der Arbeit des Lösers, langsame Inseln schlafen
  bald oder erstarren. Halbe Teilschritte für sie sparten unter 1 % eines Schritts und träfen gerade die Haufen, die zur
  Ruhe kommen.
- Die neuen Paare schneller sortieren: Seit `physik-41` ist das Sortieren ein Viertel bis ein Drittel des Anlegens, bei
  ×1 mit 4 Threads auf der VM 103 000 bis 150 000 Takte pro Schritt für 1 415 Paare. Ein Radix-Sort über die Bits, die
  sich ändern, oder vorsortierte Listen je Worker, die der aufrufende Thread nur noch mischt, bräuchten ein Drittel
  davon, bitgleich. Auf dem Ryzen rund 0,01 bis 0,02 ms pro Schritt. Auf `physik-43` umgesetzt, als Radix-Sort, siehe
  „Neue Paare mit einem Radix-Sort sortieren“: Der Sort braucht bei ×1 41 000 bis 52 000 statt 111 000 bis 126 000
  Takte, gepaart kostet das Anlegen bei ×1 15 % weniger, bei ×2 und ×4 8 bis 14 %, bitgleich. Auf dem Ryzen kostet das
  Anlegen bei ×2 23 bis 25 % weniger, rund 0,02 ms pro Schritt.
- Nur als Versuch, ändert das Verhalten überall: „Physik mit 30 Hz und 8 Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU, kleine Trümmer als Partikel, mehr
Farben im Graphen, den Überlauf nachträglich einfärben, die Kosinusse der Kanten in der Hülle wie upstream, kompakte
Paare für nicht berührende Kontakte, einen Vorfilter über die Boxen vor dem Hüllentest, mehr Ladevorgänge gleichzeitig
in der Paarsuche, den Überlauf in unabhängigen Gruppen, gesammelte Typwechsel, weniger Teilschritte für langsame Inseln,
einen anderen Rand der Boxen und die neuen Kontakte parallel anzulegen, die erste Stufe des Umbaus der Buchhaltung von
Box3D.
