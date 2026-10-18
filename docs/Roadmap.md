# Roadmap

Die nächsten möglichen Schritte für die Physik, nach der Untersuchung vom 17. Oktober 2026 auf `physik-19` und
`physik-20`, der Messung auf dem Ryzen mit `physik-21`, den Knoten auf `physik-22`, dem Team für die größte Insel auf
`physik-24`, den Prüfungen des Updates auf den Workern auf `physik-25` und den Zeiten des Teams und den Schwellen fürs
Wecken auf `physik-26`, alle vier auf dem Ryzen nachgemessen, den Prototypen und Zerlegungen auf `physik-27`, den Lagen
zum Zeichnen zwischen zwei Schritten auf `physik-28`, den Kontakt-Kernen von upstream #189 auf `physik-29` und der
Trennachsen-Suche von upstream #173 und #177 auf `physik-30`. Begründungen und Messwerte stehen in
[`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge. Nach jeder Runde wird der Stand hier
nachgetragen.

## Fazit

- **Ein eigenes Box3D lohnt nicht.** Hüllentest, Löser und CCD kosten in jeder Engine dasselbe, und der Fork lässt sich
  an jeder Stelle ändern, siehe „Eigene Engine statt Box3D, dritte Prüfung“.
- **Mit 8 Threads bestimmt der serielle Teil den Frame.** Auf dem Ryzen beim Abriss der Stadt in der Demo gemessen: Mit
  8 Threads kostet ein Schritt mit `physik-25` bei ×4 1,24 und bei ×2 2,61 ms, mit `physik-24` 1,23 und 2,58 ms, mit
  `physik-22` 1,29 und 2,81 ms, davor 1,74 und 3,46 ms. Mit `physik-25` lagen dabei die Stufen des Box3D-Schritts, an
  denen sich nichts geändert hat, mit 8 Threads 1 bis 11 % höher. Hochgerechnet bleiben 60 und 56 % auf einem Thread,
  mit `physik-24` 62 und 59 %, davor 72 und 68 %, siehe „Skalierung auf dem Ryzen mit `physik-25`“. Mit `physik-26`
  kostet ein Schritt bei ×4 1,25 ms wie mit `physik-25`, siehe „Skalierung auf dem Ryzen mit `physik-26`“.
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
  sind 6 bis 9 %, aber nur mit einem großen Umbau von Box3D zu holen, siehe Schritt 9.
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
| 14 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-25` bei ×4 und ×2 | entschied 15 und 5 | 3 | ✅ am 18. Oktober: die Ruhe-Prüfung mit 8 Threads 43 und 42 % billiger, der Löser ist der größte Posten |
| 15 | **Wartezeiten im Löser zählen**: wie lange die Threads an den Stufen des Teams warten, was der Überlauf auf dem aufrufenden Thread kostet und wie ungleich die Stufen verteilt sind | Werkzeug, entscheidet, wie es im Löser weitergeht | 14 | ✅ auf `physik-26`: auf der VM mit 4 Threads rechnet der aufrufende Thread ein Fünftel bis ein Sechstel der Zeit des Teams allein am Überlauf, und jeder Worker wartet ein Drittel davon |
| 5 | **Schwellen fürs Wecken** der Bruchrechnung, bitgleich | 1 bis 3 % | ✅ Wecken | ✅ auf `physik-26`: Die Worker helfen, sobald der aufrufende Thread allein dreimal so lange bräuchte, wie einer zum Anlaufen braucht. Auf der VM bleiben sie nahe 32 und 512 |
| 16 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `physik-26` bei ×4 und ×2 | entschied 17 | 15 und 5 | ✅ am 18. Oktober: Im Team rechnet der aufrufende Thread bei ×4 40 % der Blöcke der Stufen selbst, jeder Worker wartet 47 % |
| 17 | **Übrige Pakete im Team anders verteilen**: Ein Worker nimmt ein Paket erst nach einer Weile ohne neue Stufe, nur im Überlauf, erst nach der letzten Stufe oder nur, solange wenige andere eines rechnen, bitgleich | – | 16 | ⛔ im Prototyp auf `physik-27`: Der aufrufende Thread rechnet dann weniger Blöcke selbst, aber die Pakete fehlen an anderer Stelle, das Team wird nicht kürzer |
| 20 | **Lagen zum Zeichnen zwischen zwei Schritten**: Die Bibliothek gibt zum Zeichnen eine Lage zwischen den letzten zwei Ständen der Physik, die Demo ruft das nur auf | flüssige Bewegung auf Monitoren über 60 Hz, keine Rechenzeit, das Bild liegt einen Schritt zurück | – | ✅ auf `physik-28`: `nbWorld_GetDrawPoses` und `nbWorld_GetStepFraction`, in der Demo mit Schalter und Zeitlupe. Ein Aufruf kostet auf der VM bei ×4 0,16 ms pro Bild |
| 18 | **Kontakt-Kerne von upstream #189 messen**: ob upstreams neue breite Kontakt-Kerne im Haufen aus Hüllen schneller rechnen als die des Forks | prüfen, bei Erfolg 1 bis 3 %, ändert die Ergebnisse | – | ✅ auf `physik-29`: Upstream rechnet die Bedingungen mit #189 2 bis 8 % billiger. Übernommen kosten sie im Benchmark auf der VM 1 bis 8,5 % weniger, meist 3 bis 4 %, beim Abriss in der Demo bei ×4 1,9 bis 4,9 % |
| 21 | **Trennachsen-Suche von upstream #173 und #177**: der Test zweier Hüllen wie upstream, ändert die Ergebnisse | rund 1 % | – | ✅ auf `physik-30`: Die Suche braucht auf denselben Hüllenpaaren 5,1 % weniger Takte, mit einem Thread 0,5 bis 0,7 % eines Schritts, und erkennt getrennte Hüllen, die die alte übersah. Die zweite Suche, die upstream gestrichen hat, bleibt, ebenso das Format der Hüllen |
| 22 | **Upstream sichten**: was upstream Box3D nach #191 geändert hat, im Haufen aus Hüllen messen und übernehmen, was sich lohnt, wie bei 18 und 21 | unbekannt | – | offen |
| 6 | **Typwechsel gesammelt**, bitgleich | unter 1 % | – | zurückgestellt nach 16: Ein Typwechsel ist vor allem Warten auf Speicher, der Ryzen braucht 2,6 bis 3,2 µs dafür |
| 19 | **Physik mit 30 Hz und 8 Teilschritten**, als Versuch auf einer eigenen Branch, ändert das Verhalten überall | pro simulierter Sekunde ein Viertel bis ein Drittel der Physik | 20 | zurückgestellt am 18. Oktober: bringt nur Rechenzeit, und die fehlt auf dem Ryzen nicht. Den Takt wählt ein Spiel selbst, mit 20 ohne Ruckeln |
| 7 | **Rand der Boxen mit 8 Threads** nachmessen, ändert die Ergebnisse | klein | 2 | offen, niedrig |
| 8 | **Kompakte Paare** statt Kontakte für nicht berührende Paare, ändert die Ergebnisse | 2 bis 4 % | 2 | offen, niedrig |
| 11 | **Überlauf in unabhängigen Gruppen** auf mehreren Threads, bitgleich | höchstens 1 bis 2 %: der Überlauf ist mit 8 Threads noch 4 und 6 % des Frames | 2c | zurückgestellt nach 12, nach 16 weiter: Der Überlauf allein ist mit 8 Threads 28 und 29 % der Zeit des Teams, unter einem Drittel |
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

Bei der Rechenzeit ist nach `physik-30` kein kleiner Schritt mehr übrig, der mit 8 Threads mehr als 1 bis 2 % des Frames
verspricht, siehe die Schritte 6, 7 und 11 und die kleineren Punkte unten. Was mehr bringen könnte, ist größer oder
ändert das Verhalten:

- **Schritt 8**, kompakte Paare für nicht berührende Kontakte: Wochen Umbau in Box3D für geschätzt 2 bis 4 % mit 8
  Threads. Ändert die Ergebnisse.
- **Schritt 19**, Physik mit 30 Hz und 8 Teilschritten: der einzige große Hebel, pro simulierter Sekunde ein Viertel bis
  ein Drittel der Physik, aber Kontakte entstehen später, Treffer kommen später, und mit den Lagen aus Schritt 20 läge
  das Bild 33 statt 17 ms hinter der Physik. Gewonnen wäre nur Rechenzeit, und die fehlt auf dem Ryzen nicht: Ein
  Schritt kostet dort mit 8 Threads 8 bis 17 % eines Frames mit 60 Hz. Zurückgestellt, bis die Physik irgendwo den Frame
  begrenzt, siehe „Physik mit 30 Hz und 8 Teilschritten“.

Daneben bleibt **Schritt 22**, upstream sichten: Die Runden 18 und 21 haben übernommen, was upstream bis #191 an Löser
und Test zweier Hüllen geändert hat, und beide Male war es messbar billiger, wenn auch nur um wenige Prozent. Was
upstream danach geändert hat, ist noch nicht gesichtet.

## Kleinere Punkte

Mit niedriger Priorität vorgemerkt, sie sparen vor allem Arbeit, die schon parallel läuft. Der Gewinn gilt mit einem
Thread, mit 8 Threads ist er kleiner:

- „Kollision zweimal übersetzen“: die Dateien der Kollision ein zweites Mal mit AVX2 übersetzen, gewählt zur Laufzeit
  wie beim Löser. Ganz mit AVX2 übersetzt kostete die Kollision 5,2 bis 5,6 % weniger, geschätzt 1 bis 2 % eines Frames.
  Ob das bitgleich bleibt, ist zu prüfen.
- „Box gegen gemerkte Fläche im ersten Durchgang“: ein billiger Vortest für Kontakte, die nicht berühren, bitgleich, bei
  ×1 2 bis 3 % der Instruktionen eines Frames, bei ×4 rund 1 %. Dafür müsste sich der Kontakt Ebene und Box merken.
- „Zweite Suche nur bei Überlappung“: die volle zweite Suche des Tests zweier Hüllen nur, wenn keine Fläche die Hüllen
  trennt. Bei ×1 bis 0,5 % eines Schritts, ändert die Ergebnisse, und 12 fragwürdige Kontakte im Benchmark fielen weg.
- „Gyroskopische Drehung nach upstream #191“: das Drehmoment ganz abstellen spart höchstens 2 bis 3 %, aber drehende
  Trümmer verlören ihr Taumeln. Nur, falls ein Spiel das nicht braucht.
- Nur als Versuch, beide ändern das Verhalten: „Weniger Substeps für langsame Inseln“ und „Physik mit 30 Hz und 8
  Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU, kleine Trümmer als Partikel, mehr
Farben im Graphen, den Überlauf nachträglich einfärben und die Kosinusse der Kanten in der Hülle wie upstream.
