# Roadmap

Die nächsten möglichen Schritte für die Physik, nach der Untersuchung vom 17. Oktober 2026 auf `physik-19` und
`physik-20` und der Messung auf dem Ryzen mit `physik-21`. Begründungen und Messwerte stehen in
[`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge. Nach jeder Runde wird der Stand hier
nachgetragen.

## Fazit

- **Ein eigenes Box3D lohnt nicht.** Hüllentest, Löser und CCD kosten in jeder Engine dasselbe, und der Fork lässt sich
  an jeder Stelle ändern, siehe „Eigene Engine statt Box3D, dritte Prüfung“.
- **Mit 8 Threads bestimmt der serielle Teil den Frame.** Auf dem Ryzen beim Abriss der Stadt in der Demo gemessen:
  Mit 8 statt 4 Threads kostet ein Schritt bei ×4 1,74 statt 2,10 ms und bei ×2 3,46 statt 4,42 ms, und hochgerechnet
  bleiben mit 8 Threads 72 und 68 % auf einem Thread, siehe „Skalierung auf dem Ryzen“.
- **Was dort bremst, ist der Löser.** Kontakt-Schleife, Paarsuche und Abschluss der Körper werden mit 8 Threads 3,5-
  bis 6-mal so schnell, die Bedingungen des Lösers nur 2-mal, und von 4 auf 8 Threads gar nicht mehr. Mit 8 Threads
  sind sie 43 bis 48 % des Frames. Update, Einschläge, Anlegen und Zustandswechsel der Kontakte sind zusammen 22 bis
  24 %.
- **Der Löser hängt am Überlauf des Graphen.** Kontakte, für die keine der 20 Farben frei ist, löst Box3D auf dem
  aufrufenden Thread allein und einzeln. Beim Abriss sind das die Kontakte an Knoten: große abgebrochene Teile aus
  Dutzenden bis Hunderten Bruchstücken, an denen Hunderte kleine Trümmer liegen. Mit einem Thread sind das bei ×4 20 %
  der Kontakte und 42 % der Zeit der Bedingungen, bei ×2 10 und 26 %, siehe „Der Überlauf beim Abriss“. Ein Knoten muss
  seine Kontakte nacheinander lösen, bitgleich lässt sich das kaum verteilen.
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
| 10 | **Knoten im Überlauf mit geteilter Masse**: bis zu acht Kontakte eines Knotens in den acht Spuren einer breiten Bedingung, jede gegen eine Kopie mit einem Achtel seiner Masse, ändert die Ergebnisse | 15 bis 20 %, mit einem Thread 5 bis 11 % | 2c | als Nächstes, auf `physik-22` |
| 11 | **Überlauf in unabhängigen Gruppen** auf mehreren Threads, bitgleich | ×4 rund 10 %, ×2 rund 2 %, nach 10 weniger | 2c | offen, nach 10 neu messen |
| 2b | **Schwelle zwischen Paketen und Farben nach Threads**: Box3D nimmt Farben erst über der Hälfte, bitgleich | nach 10 neu zu schätzen | 10 | offen |
| 4 | **Größte Insel im Team**, bitgleich | ×1 6 %, ×2 3 %, beim Abriss nach 10 neu zu schätzen | 10 | offen |
| 3 | **Prüfungen des Updates auf den Workern, zweiter Versuch**, bitgleich | 4 bis 6 % | ✅ Wecken in 8 bis 10 µs | offen |
| 5 | **Schwellen fürs Wecken** der Bruchrechnung, bitgleich | 1 bis 3 % | ✅ Wecken | offen |
| 6 | **Typwechsel gesammelt**, bitgleich | rund 1 % | – | offen |
| 7 | **Rand der Boxen mit 8 Threads** nachmessen, ändert die Ergebnisse | klein | 2 | offen, niedrig |
| 8 | **Kompakte Paare** statt Kontakte für nicht berührende Paare, ändert die Ergebnisse | 2 bis 4 % | 2 | offen, niedrig |
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

## Kleinere Punkte

Mit niedriger Priorität vorgemerkt, sie sparen vor allem Arbeit, die schon parallel läuft: „Box gegen gemerkte Fläche
im ersten Durchgang“, „Kollision zweimal übersetzen“, „Gyroskopische Drehung nach upstream #191“, „Löser-Umbau von
upstream #189“ und, nur als Versuch, „Physik mit 30 Hz und 8 Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU, kleine Trümmer als Partikel, mehr
Farben im Graphen und den Überlauf nachträglich einfärben.
