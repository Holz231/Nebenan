# Roadmap

Die nächsten möglichen Schritte für die Physik, nach der Untersuchung vom 17. Oktober 2026 auf `physik-19` und
`physik-20`, der Messung auf dem Ryzen mit `physik-21`, den Knoten auf `physik-22` und dem Team für die größte Insel auf
`physik-24`, beide auf dem Ryzen nachgemessen, und den Prüfungen des Updates auf den Workern auf `physik-25`.
Begründungen und Messwerte stehen in [`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge.
Nach jeder Runde wird der Stand hier nachgetragen.

## Fazit

- **Ein eigenes Box3D lohnt nicht.** Hüllentest, Löser und CCD kosten in jeder Engine dasselbe, und der Fork lässt sich
  an jeder Stelle ändern, siehe „Eigene Engine statt Box3D, dritte Prüfung“.
- **Mit 8 Threads bestimmt der serielle Teil den Frame.** Auf dem Ryzen beim Abriss der Stadt in der Demo gemessen: Mit
  8 Threads kostet ein Schritt seit `physik-24` bei ×4 1,23 und bei ×2 2,58 ms, mit `physik-22` 1,29 und 2,81 ms, davor
  1,74 und 3,46 ms. Bei ×2 liegt der Unterschied zu `physik-22` im Rauschen. Hochgerechnet bleiben 62 und 59 % auf einem
  Thread, mit `physik-22` 63 %, davor 72 und 68 %, siehe „Skalierung auf dem Ryzen mit `physik-24`“.
- **Was dort bremst, sind der Löser und das Update.** Die Bedingungen sind mit 8 Threads 27 und 26 % des Frames, mit
  `physik-22` 31 und 26 %, davor 48 und 43 %. Seit `physik-24` rechnet ein Team aller Threads das Paket der größten
  Insel, das vorher ein Thread allein rechnete, mit mindestens 34 % der Arbeit: Auf dem Ryzen kosten die Bedingungen mit
  8 Threads bei ×4 damit 16 % weniger und werden von 4 auf 8 Threads wieder 15 statt 8 % schneller, bei ×2 bleiben sie
  gleich, siehe „Größte Insel im Team“. Sie brauchen aber noch 31 % der Zeit eines Threads. Update und Einschläge sind
  zusammen 24 und 17 % des Frames, die Ruhe-Prüfung allein 10 und 8 %, Anlegen und Zustandswechsel der Kontakte 5 und 8
  %. Seit `physik-25` prüfen die Worker die Trümmer im Update, auf der VM mit 4 Threads kostet die Ruhe-Prüfung beim
  Abriss damit 21 und 25 % weniger, siehe „Prüfungen des Updates auf den Workern, zweiter Versuch“.
- **Der Löser hing am Überlauf des Graphen.** Kontakte, für die keine der 20 Farben frei ist, löst Box3D auf dem
  aufrufenden Thread allein. Beim Abriss sind das die Kontakte an Knoten: große abgebrochene Teile aus Dutzenden bis
  Hunderten Bruchstücken, an denen Hunderte kleine Trümmer liegen, siehe „Der Überlauf beim Abriss“. Seit `physik-22`
  löst Box3D sie in Bündeln gegen Kopien des Knotens: Auf dem Ryzen kosten die Bedingungen mit 8 Threads 53 und 51 %
  weniger, ein Schritt 26 und 19 %, und der Überlauf ist nur noch 4 und 6 % des Frames, siehe „Knoten im Überlauf mit
  geteilter Masse“.
- **Der Cache ist nicht das Problem.** Der wache Arbeitssatz ist bei ×1 gut 10 MB groß und passt schon in 32 MB. Der
  Ryzen wartet kürzer, weil sein L3 schneller antwortet, nicht weil er größer ist, siehe „Arbeitssatz im Cache“.

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
| 14 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-25` bei ×4 und ×2 | entscheidet 5, 6 und den Löser | 3 | als Nächstes |
| 5 | **Schwellen fürs Wecken** der Bruchrechnung, bitgleich | 1 bis 3 % | ✅ Wecken | offen |
| 6 | **Typwechsel gesammelt**, bitgleich | rund 1 % | – | offen |
| 7 | **Rand der Boxen mit 8 Threads** nachmessen, ändert die Ergebnisse | klein | 2 | offen, niedrig |
| 8 | **Kompakte Paare** statt Kontakte für nicht berührende Paare, ändert die Ergebnisse | 2 bis 4 % | 2 | offen, niedrig |
| 11 | **Überlauf in unabhängigen Gruppen** auf mehreren Threads, bitgleich | höchstens 1 bis 2 %: der Überlauf ist mit 8 Threads noch 4 und 6 % des Frames | 2c | zurückgestellt nach 12 |
| 2b | **Schwelle zwischen Paketen und Farben nach Threads**: Box3D nimmt Farben erst über der Hälfte, bitgleich | allein nichts: Farben und Pakete stehen mit 8 Threads beide bei einem Drittel der Zeit eines Threads | 12 | ✅ in 4 aufgegangen: Ein Team nimmt das größte Paket ab fünf Vierteln des Anteils eines Workers, auch über der Hälfte |
| 9 | **Buchhaltung von Box3D pro Schritt neu bauen**, großer Umbau über Monate, ändert die Ergebnisse | höchstens 4 bis 6 % | 2 | zurückgestellt: Anlegen und Zustandswechsel sind nur 4 bis 6 % des Frames |

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

## Was die nächste Messung auf dem Ryzen entscheidet

Dieselbe Messung mit `physik-25`, bei ×4 und ×2. Neu im Bericht ist eine Zeile, in wie vielen Schritten die Prüfungen
des Updates an die Worker gingen, wie viel der bewegten Trümmer sie dort übernahmen und wie schnell sie anliefen.
Verglichen wird mit „Skalierung auf dem Ryzen mit `physik-24`“.

- **Ruhe-Prüfung mit 8 Threads** (`settle`) bei ×4 und ×2 rund halb so teuer wie die 0,125 und 0,212 ms von `physik-24`,
  und die Worker übernehmen drei Viertel der bewegten Stücke und mehr: Schritt 3 trägt. In der Ruhe-Prüfung bleibt dann
  vor allem das Erstarren, das ist Schritt 6.
- **Bleibt sie fast gleich**, zeigt die neue Zeile warum. Laufen die Worker später als rund 30 µs an oder übernehmen sie
  weniger als die Hälfte, kostet das Wecken mehr, als die Prüfungen bringen. Dann die Schwelle `NB_PARALLEL_SETTLE`
  heben oder die Worker nach einer Aufgabe kurz wach halten.
- **Was danach am meisten kostet**, kommt als Nächstes: die Einschläge mit den Zellen auf dem Hauptthread, bei ×4
  zuletzt 0,126 und 0,048 ms (Schritt 5), das Erstarren (Schritt 6) oder der Löser, dessen Bedingungen mit 8 Threads
  noch 31 % der Zeit eines Threads brauchen. Für den Löser wäre zuerst zu zählen, wie lange die Threads an den Sperren
  des Teams warten und was der Überlauf auf dem aufrufenden Thread kostet.

## Kleinere Punkte

Mit niedriger Priorität vorgemerkt, sie sparen vor allem Arbeit, die schon parallel läuft: „Box gegen gemerkte Fläche
im ersten Durchgang“, „Kollision zweimal übersetzen“, „Gyroskopische Drehung nach upstream #191“, „Löser-Umbau von
upstream #189“ und, nur als Versuch, „Physik mit 30 Hz und 8 Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU, kleine Trümmer als Partikel, mehr
Farben im Graphen und den Überlauf nachträglich einfärben.
