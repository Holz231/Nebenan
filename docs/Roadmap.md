# Roadmap

Die nächsten möglichen Schritte für die Physik, nach der Untersuchung vom 17. Oktober 2026 auf `physik-19` und
`physik-20`. Begründungen und Messwerte stehen in [`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die
Reihenfolge. Nach jeder Runde wird der Stand hier nachgetragen.

## Fazit

- **Ein eigenes Box3D lohnt nicht.** Hüllentest, Löser und CCD kosten in jeder Engine dasselbe, und der Fork lässt sich
  an jeder Stelle ändern, siehe „Eigene Engine statt Box3D, dritte Prüfung“.
- **Mit 8 Threads bestimmt der serielle Teil den Frame.** Hochgerechnet von der VM laufen bei ×1 56 bis 66 % und bei ×4
  60 bis 78 % eines Frames nur auf einem Thread: `nbWorld_Update`, die Einschläge, das Anlegen und Abbauen von Kontakten
  in Box3D und die größte Insel, die bei ×1 28 % der Arbeit des Lösers trägt. 8 statt 4 Threads sparen nur 15 bis 28 %,
  siehe „Was mit 8 Threads seriell bleibt“.
- **Der Cache ist nicht das Problem.** Der wache Arbeitssatz ist bei ×1 gut 10 MB groß und passt schon in 32 MB. Der
  Ryzen wartet kürzer, weil sein L3 schneller antwortet, nicht weil er größer ist, siehe „Arbeitssatz im Cache“.
- **Was noch mehr Kerne bringen, entscheidet der Ryzen.** Die VM hat 4 vCPUs, Zahlen für 8 Threads sind hochgerechnet.
  Deshalb kommt zuerst ein Werkzeug, das die Skalierung dort misst.

## Schritte

Gewinn jeweils geschätzt für einen Frame mit 8 Threads. „Bitgleich“ heißt: die Ergebnisse ändern sich nicht.

| Nr. | Schritt | Gewinn | Voraussetzung | Stand |
| ---: | --- | --- | --- | --- |
| 1 | **Skalierung messen in der Bibliothek**, mit Knopf in der Demo und Ausgabe im Benchmark | Werkzeug | – | ✅ auf `physik-21` |
| 2 | **Auf dem Ryzen messen**: in der Demo „Skalierung messen“, die Tabelle aus der Zwischenablage in den Chat | entscheidet 3 bis 9 | 1 | offen |
| 2a | **Große Insel beim Abriss in der Demo**: zählen, wie viel der 73 % zusammenhängt, dann veraltete Inseln teilen | Demo bis rund 30 %, wenn die Insel zerfällt | – | offen |
| 3 | **Prüfungen des Updates auf den Workern, zweiter Versuch**, bitgleich | 4 bis 13 % | 2: Wecken deutlich unter 67 µs | offen |
| 4 | **Größte Insel im Team**, bitgleich | ×1 6 %, ×2 3 % | 2: die größte Insel bremst die Pakete | offen |
| 5 | **Schwellen fürs Wecken** der Bruchrechnung, bitgleich | ×1 und ×4 je 3 % | 2: Weckzeit | offen |
| 6 | **Typwechsel gesammelt**, bitgleich | 2 bis 3 % | – | offen |
| 7 | **Rand der Boxen mit 8 Threads** nachmessen, ändert die Ergebnisse | offen | 2 | offen |
| 8 | **Kompakte Paare** statt Kontakte für nicht berührende Paare, ändert die Ergebnisse | 2 bis 4 % | 2 | offen |
| 9 | **Buchhaltung von Box3D pro Schritt neu bauen**, großer Umbau über Monate, ändert die Ergebnisse | ×1 bis 15 %, ×4 bis 10 % | 2: Anlegen und Abbauen von Kontakten sind der Boden | nur nach 2 |

## Was die Messung auf dem Ryzen entscheidet

Alle Zahlen dafür stehen in der Tabelle von „Skalierung messen“: die Weckzeit in „Workers started“, die größte Insel in
„Largest island“, Anlegen und Zustandswechsel der Kontakte in den Zeilen `createContacts` und `contactStates`, das
Update in `update`, und was mit 8 Threads seriell bleibt, in der Spalte `8 w` und der Zeile darunter.

- **Weckzeit eines Workers** unter rund 20 µs: Schritt 3 und 5 zuerst. Auf der VM sind es 67 bis 100 µs.
- **Löser mit 8 Threads** deutlich langsamer als ein Achtel der Zeit mit einem Thread, und die größte Insel über einem
  Achtel der Arbeit: Schritt 4.
- **Anlegen und Zustandswechsel der Kontakte** zusammen über rund 15 % des Frames: Schritt 9 prüfen, vorher 7 und 8.
- **Update** über einem Viertel des Frames: Schritt 3 hat Vorrang vor allem im Box3D-Schritt.

## Kleinere Punkte

Mit niedriger Priorität vorgemerkt, sie sparen vor allem Arbeit, die schon parallel läuft: „Box gegen gemerkte Fläche
im ersten Durchgang“, „Kollision zweimal übersetzen“, „Gyroskopische Drehung nach upstream #191“, „Löser-Umbau von
upstream #189“ und, nur als Versuch, „Physik mit 30 Hz und 8 Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU und kleine Trümmer als Partikel.
