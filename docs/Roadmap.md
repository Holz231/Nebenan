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
nachgemessen, dem Überlauf an Knoten auf mehreren Threads auf `kollaps-7` und den Stufen des Teams auf `kollaps-8`,
beide auf dem Ryzen nachgemessen, den großen Paketen im Team auf `kollaps-9`, auf dem Ryzen nachgemessen, und dem Aufbau
des Lösers neben den Knoten auf `kollaps-10`, auf dem Ryzen nachgemessen, und der zerlegten Kontakt-Schleife auf
`kollaps-11`, auf dem Ryzen nachgemessen, und der zerlegten Ruhe-Prüfung auf `kollaps-12`, auf dem Ryzen nachgemessen,
und den Kontaktlisten der ruhigen Stücke nebeneinander auf `kollaps-13`, auf dem Ryzen nachgemessen, und dem billigeren
Blick auf große Stücke auf `kollaps-14`, auf dem Ryzen nachgemessen, und den Listen der berührenden Kontakte auf
`kollaps-15`, auf dem Ryzen nachgemessen, und den Schnitten der Blöcke der Prüfungen auf `kollaps-16`, simuliert und
verworfen, und der Arbeit des Lösers pro Einheit auf `kollaps-17`, auf dem Ryzen nachgemessen, und den Schnitten der
Blöcke des Teams auf `kollaps-18`, simuliert und verworfen, und dem Lesen der Ansprüche vor dem Vergleichstausch auf
`kollaps-19`, auf dem Ryzen nachgemessen, und dem, was der aufrufende Thread im Team allein rechnet, auf `kollaps-20`,
und den Durchgängen des Aufbaus der Knoten auf `kollaps-21` und den Zustandswechseln der Kontakte auf `kollaps-22` und
weniger Durchgängen beim Aufbau der Knoten auf `kollaps-23`, alle vier gemessen und verworfen, und der Zeitleiste des
Lösers mit Tracy auf `kollaps-24`, auf dem Ryzen aufgenommen, und den Gebieten der größten Insel auf `kollaps-25`,
gebaut und verworfen, und dem Erstarren des Schutts auf `kollaps-26` und dem Anlegen der Kontakte auf `kollaps-27`,
beide gemessen und verworfen, und mit `kollaps-27` auf dem Ryzen nachgemessen, und dem Preis eines Kontakts in der
Kontakt-Schleife auf `kollaps-28` und den Teilen eines Einschlags auf `kollaps-29` und den Prüfungen des Updates auf
`kollaps-30`, alle drei gemessen und verworfen, und den Prüfungen des Updates auf eigenen Cache-Zeilen auf `kollaps-31`,
auf Wunsch des Nutzers gebaut, auf dem Ryzen nachgemessen und auf `kollaps-33` zurückgenommen, und den dauerhaften
Gebieten auf `kollaps-32`, gebaut, auf dem Ryzen nachgemessen und übernommen, und dem Ausgleich der Gebiete auf
`kollaps-34`, gebaut, auf dem Ryzen nachgemessen und auf `kollaps-35` zurückgenommen, und den Mauern wie auf dem alten
Branch auf `kollaps-36`, auf Wunsch des Nutzers, und den Figuren nach Overgrowth auf `active-ragdoll-1`, ebenso auf
Wunsch des Nutzers, und den Treffern wie auf `ragdoll-11` auf `active-ragdoll-20`, ebenso auf Wunsch des Nutzers, und
dem Aufwachen im Stand auf `active-ragdoll-21`. Begründungen und Messwerte stehen in
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
114 statt 116 Etagen ein. Auch `kollaps-32` ändert die Ergebnisse, siehe Schritt 68: Beim Abriss in der Demo fallen bei
×2 in 4 000 Bildern 829 statt 711 Granaten und 7 statt 6 Häuser, und bei ×4 sind alle Häuser nach 1 161 statt 1 273
Bildern unten. `kollaps-36` ist wieder keine Runde der Physik: Mauern geben unter ihrer Last nach und zerbrechen beim
Aufprall, wie auf dem Branch `house-buckling-(incomplete)-+-wall-collapse`, ohne dass Häuser einknicken, siehe „Mauern
geben nach wie auf dem alten Branch“ in [Optimierungen.md](Optimierungen.md). Häuser enden bitgleich, der Abriss in der
Demo und der Benchmark messen dieselben Städte wie vorher. Anders enden die Szenen „Mauern“ und „Stresstest“ der Demo,
im Benchmark „rifle“ und „explosions“, die auf Mauern schießen, und in den Tests die Szene mit Mauer und Tor.
`active-ragdoll-1` ist auch keine Runde der Physik: Figuren nach Overgrowth, auf Wunsch des Nutzers, siehe „Figuren“
unten. Ohne Figuren rechnet alles bitgleich wie mit `kollaps-36`.

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
  Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-6`“. Auf `kollaps-11` kostet er 2,22 ms, hochgerechnet bleiben
  51 % auf einem Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-11`“. Auf `kollaps-12` kostet er 2,27 ms, im
  Rahmen der Schwankungen so viel wie davor, hochgerechnet bleiben 50 % auf einem Thread, siehe „Skalierung auf dem
  Ryzen mit `kollaps-12`“. Auf `kollaps-27` kostet er bei ×2 2,25 und bei ×4 1,64 ms, hochgerechnet bleiben bei ×2 49 %
  auf einem Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-27`“. Mit den dauerhaften Gebieten kostet er auf
  `kollaps-32` bei ×2 2,21 ms, hochgerechnet bleiben 45 % auf einem Thread, siehe „Skalierung auf dem Ryzen mit
  `kollaps-30` bis `kollaps-32`“.
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
  siehe „Übrige Pakete im Team anders verteilen“. Seit `kollaps-8` zählt der Löser die Stufen des Teams: Auf dem Ryzen
  wartet es mit 8 Threads ein Viertel seiner Zeit nach der letzten Stufe, auf das Paket einer zweiten großen Insel, das
  ein Thread allein rechnet, siehe Schritt 34 und 35. Seit `kollaps-9` rechnet das Team ein solches Paket mit, sobald es
  mehr als das Doppelte des Anteils eines Threads hält: Auf dem Ryzen kosten die Bedingungen mit 8 Threads damit 19 %
  weniger und ein Bild 6 %, siehe Schritt 36. Seit `kollaps-17` misst der Löser, was eine Einheit Arbeit kostet: Auf der
  VM braucht ein Thread mit 4 Threads in den Blöcken des Teams 1,39-mal und in den Paketen 1,18-mal so lange wie ein
  Thread allein, siehe Schritt 51. Auf dem Ryzen braucht ein Thread im Team mit 8 Threads das 1,34- bis 1,39-Fache, am
  Ende der Stufen warten die Threads 19 % der offenen Zeit, und der aufrufende Thread rechnet 13 % der Zeit des Teams
  allein, siehe Schritt 52. Auf `kollaps-18` zeigt eine Simulation der Stufen, dass ein Fünftel der Zeit in den Blöcken
  des Teams auf vergebliche Versuche geht, Blöcke zu nehmen, die andere schon haben, und dass feinere Blöcke deshalb
  verlieren, siehe Schritt 53. Seit `kollaps-19` liest ein Thread den Anspruch eines Blocks, bevor er ihn zu nehmen
  versucht, und ein vergeblicher Versuch kostet auf der VM 42 bis 58 % weniger, siehe Schritt 54. Auf dem Ryzen kostet
  das Team mit 8 Threads damit 0,022 ms pro Schritt weniger, siehe Schritt 55. Was der aufrufende Thread im Team allein
  rechnet, lässt sich kaum verteilen: 97 % der Kontakte des Überlaufs außerhalb der Knoten hängen an den Bündeln der
  Knoten, siehe Schritt 56. An den Sperren nach den rund 232 Stufen pro Schritt verlieren die Threads mit 8 Threads
  0,255 ms pro Schritt mit Team, siehe Schritt 61. Gebiete, die ein Thread ohne Sperren rechnet, sparen das auf der VM
  mit 4 Threads, kosten aber mit einem Thread 8 bis 11 % mehr, weil sie in jedem Schritt neu entstehen, siehe
  Schritt 62. Seit `kollaps-32` bleiben die Gebiete von Schritt zu Schritt: Beim Abriss kostet der Löser auf der VM mit
  einem Thread 0,2 % mehr und mit 4 Threads 12,2 % weniger, siehe Schritt 68. Auf dem Ryzen kostet er damit bei ×2 mit 8
  Threads 0,107 ms pro Schritt weniger, die Gebiete bleiben, siehe Schritt 71. Mit einem Thread kostet er dort 6 % mehr,
  und mit 8 Threads warten die Threads in den Durchgängen der Gebiete 31 % ihrer Zeit auf das langsamste, siehe
  Schritt 72. Das größte läuft nach einem Schnitt schnell davon, vor allem, weil Inseln, die dazukommen, ganz in ein
  Gebiet gehen. Nahmen ihre Körper auf `kollaps-34` das Gebiet, in dem sie liegen, warteten die Threads auf dem Ryzen
  dort 26 statt 31 % ihrer Zeit, aber `solve` kostete pro Einheit der Arbeit nur 1,5 % weniger, gefordert waren 3,5 %.
  Auf `kollaps-35` ist das wieder heraus, siehe Schritt 73.
- **Was danach bleibt, sind kleine Posten.** Mit 8 Threads auf dem Ryzen kosten die Typwechsel 3 % des Frames, der
  Aufbau des Lösers 3 %, und von den Einschlägen ist jeder Teil, Box3D-Körper, Einbau, Suche und Teilen der Akteure, 1
  bis 2 %, siehe „Wohin ein Typwechsel und ein Einschlag ihre Zeit geben“. Am meisten kostet an einem Einschlag
  inzwischen der Stoß der Explosion in Box3D, ein gutes Viertel, aber auf die Worker verteilt brächte er auf dem Ryzen
  nur rund 0,013 ms pro Schritt, siehe Schritt 66. Anlegen und Zustandswechsel der Kontakte sind 6 bis 9 %, aber nur mit
  einem großen Umbau von Box3D zu holen, siehe Schritt 9. Dessen erste Stufe, die neuen Kontakte parallel anzulegen,
  lohnte auf `physik-41` nicht, gebaut ist stattdessen „Vorausladen zum Schreiben“. Am 27. Oktober hat der Nutzer den
  Umbau ganz verworfen, siehe Schritt 63. Den Aufbau des Lösers teilt sich der aufrufende Thread seit `kollaps-10` mit
  zwei Workern, siehe Schritt 32. Was er daneben an den Knoten baut, lässt sich kaum verteilen: Nur knapp die Hälfte
  rechnet jeden Kontakt für sich, und auf die wartenden Worker verteilt spart das mit 8 Threads rund 0,013 ms, siehe
  Schritt 57. Die Zustandswechsel der Kontakte lassen sich ebenso wenig auf den Workern vorbereiten: Fast alles daran
  ändert, was der nächste Wechsel liest, siehe Schritt 58. Weniger Durchgänge beim Aufbau der Knoten sparen auf dem
  Ryzen nur rund 0,005 ms, siehe Schritt 59. Auch das Erstarren des Schutts lässt sich kaum vorbereiten: Zwei Drittel
  davon sind die Typwechsel in Box3D, die Kontakte, Sätze, Inseln und Bäume in fester Reihenfolge ändern, siehe
  Schritt 60. Ebenso das Anlegen der Kontakte: Die Hälfte davon wartet auf die Zeilen, die der aufrufende Thread selbst
  schreibt, siehe Schritt 63. Die Kontakt-Schleife misst Box3D seit `kollaps-11` selbst: Auf der VM warten die Threads
  mit 4 Threads 7 % ihrer Zeit, vor allem am Ende auf die teuersten Blöcke, und seit sie die Blöcke von beiden Enden
  nehmen, noch 4 %, siehe Schritt 38 und 39. Auf dem Ryzen warten sie mit 8 Threads 4,3 %, die Kontakt-Schleife
  skaliert, siehe Schritt 40. Ein Kontakt kostet dort bei ×4 mit 2 Threads aber 32 % mehr als mit einem, auf der VM nur
  2 %, und im Benchmark holen feste Blöcke für jeden Thread nichts Sicheres zurück, siehe Schritt 65. Die Ruhe-Prüfung
  misst Nebenan seit `kollaps-12` selbst: Auf der VM geht sie fast ganz in den Blick auf das, was die ruhigen Stücke
  trägt, und mit 4 Threads warteten die Threads 12 % ihrer Zeit auf große ruhige Stücke, seit die Blöcke mit großen
  Stücken zuerst drankommen, noch 4 %, siehe Schritt 41 und 42. Auf dem Ryzen warten sie mit 8 Threads 19 % ihrer Zeit
  nach ihrem letzten Block, und der Blick auf ein ruhiges Stück geht auf der VM zu 61 % in die Liste seiner Kontakte in
  Box3D, siehe Schritt 43. Seit `kollaps-13` läuft Box3D die Listen aller ruhigen Stücke eines Blocks nebeneinander ab:
  Beim Abriss auf der VM kostet der Blick damit halb so viel und die Prüfungen 40 bis 42 % weniger, siehe Schritt 44.
  Auf dem Ryzen kosten die Prüfungen damit mit einem Thread 27 % weniger und mit 8 Threads 7 %, und mit 8 Threads warten
  die Threads ein Viertel ihrer Zeit auf den Blick auf einzelne große Stücke, siehe Schritt 45. Seit `kollaps-14` ist
  der Blick auf große Stücke billiger: Beim Abriss auf der VM kosten die Prüfungen mit 4 Threads 14 % weniger, und am
  Ende warten die Threads 68 % kürzer, siehe Schritt 46. Auf dem Ryzen kosten die Prüfungen damit mit 8 Threads 18 %
  weniger, mit einem Thread 2 %. Am meisten kostet beim größten Stück jetzt das Ablaufen seiner Liste in Box3D, und bei
  allen Stücken berühren vier von fünf Einträgen darin nicht, siehe Schritt 47. Seit `kollaps-15` läuft Box3D für die
  Prüfungen nur noch die berührenden Kontakte ab: Beim Abriss auf der VM kostet der Blick damit 20 bis 24 % weniger und
  die Prüfungen 13 bis 20 %, die Pflege der Listen kostet in der seriellen Schleife der Zustandswechsel 0,014 bis
  0,024 ms pro Schritt, siehe Schritt 48. Auf dem Ryzen kosten die Prüfungen damit 8 bis 12 % weniger, aber mit 8
  Threads kostet die Pflege der Listen samt dem Anlegen der Kontakte so viel, wie das Update spart, und die Threads
  warten 17 % ihrer Zeit nach ihrem letzten Block, siehe Schritt 49. Kleinere Blöcke brächten dort nach einer Simulation
  höchstens 0,002 ms pro Update, denn sie kosten zusammen mehr, und in fast einem Viertel der Updates dauert der längste
  Block allein länger als ein Achtel der Arbeit, siehe Schritt 50. Dass eine Prüfung mit mehreren Threads mehr kostet
  als mit einem, machen die Prüfungen, die gleichzeitig laufen: Die Threads schreiben in Cache-Zeilen, die die anderen
  lesen, im Akteur und in den Arenen. Getrennt holt das auf der VM mit 4 Threads aber weniger als die Hälfte des
  Anstiegs zurück, siehe Schritt 67. Auf Wunsch des Nutzers war es auf `kollaps-31` trotzdem gebaut, mit einem festen
  Abschnitt der Blöcke für jeden Thread, in dem die großen Stücke zuerst drankamen: Beim Abriss auf der VM bei ×4
  kosteten die Prüfungen mit 4 Threads 6 % weniger und mit einem Thread 2 % mehr, im Benchmark unter Trommelfeuer 1 bis
  3 % mehr, siehe Schritt 69. Auf dem Ryzen kosteten sie mit 8 Threads 14 % mehr, weil die Threads am Ende länger
  aufeinander warteten, seit `kollaps-33` ist es wieder heraus, siehe Schritt 70.
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
| 9 | **Buchhaltung von Box3D pro Schritt neu bauen**, großer Umbau über Monate, ändert die Ergebnisse | höchstens 4 bis 6 % | 2 | ⛔ nach der ersten Stufe verworfen am 20. Oktober: Neue Kontakte parallel anzulegen kostete mit 4 Threads 3 bis 10 % mehr als die serielle Schleife, die seit `physik-41` zum Schreiben vorauslädt. Hochgerechnet auf 8 Threads blieben höchstens 0,03 ms pro Schritt. Am 27. Oktober vom Nutzer ganz verworfen: Zustandswechsel, Erstarren und Anlegen warten vor allem auf die Zeilen, die sie selbst schreiben, siehe 58, 60 und 63, zusammen auf dem Ryzen rund 0,16 ms pro Schritt |
| 23 | **Budget auf den Workern**: Die Worker prüfen vorab, was über dem Budget erstarren darf, der aufrufende Thread nimmt eine Prüfung, solange nichts erstarrt ist, was sie gelesen hat, bitgleich | ×4 5 bis 7 % auf der VM | – | ✅ auf `kollaps-5`: auf der VM mit 4 Threads bei ×4 das Budget 24 % und das Update 14 % billiger |
| 24 | **Manifolds der Worker**: In der Kontakt-Schleife holt jeder Worker Manifolds aus einem eigenen Vorrat statt einzeln unter der Sperre, bitgleich | ×4 1 bis 2 % auf der VM | – | ✅ auf `kollaps-5`: auf der VM mit 4 Threads bei ×4 die Kontakt-Schleife 7 bis 9 % billiger |
| 25 | **Auf dem Ryzen nachmessen**: „Parallel gegen nacheinander messen“ und „Skalierung messen“ mit `kollaps-5` beim Abriss | entscheidet 26, ergab 27 | 23 und 24 | ✅ „Parallel gegen nacheinander“ am 23. Oktober: parallel 7,6 % langsamer, die Grafik kostet auf der RTX nur noch 0,24 ms, und der Schritt verliert 18 %, solange der Hauptthread zeichnet. „Skalierung messen“ am 23. Oktober mit `kollaps-6`: Die Worker laufen nach 6 bis 7 µs an, mit 8 Threads bleiben 60 % auf einem Thread, und der Überlauf an Knoten kostet 0,43 ms pro Schritt |
| 27 | **Mit eigenem Schritt einen ganzen Kern frei lassen**: `nbGetRecommendedWorkerCount` lässt mit `asyncStep` auf Prozessoren mit Hyper-Threading oder Effizienzkernen einen Performance-Kern frei statt eines Hardware-Threads, auf dem 7800X3D 7 statt 8 Threads, bitgleich | parallel auf dem Ryzen rund 6 % pro Bild, mit mehr Arbeit auf dem Hauptthread mehr | 25 | ⛔ zurückgenommen auf `kollaps-6` nach 28: In einer Sitzung ist parallel mit 8 Threads 2,5 % schneller als mit 7, und die Wahl von 7 parallel gegen 8 nacheinander ist parallel 5,1 % langsamer. Vorab mit `--threads 7` über zwei Sitzungen hatte es umgekehrt ausgesehen, die erste war insgesamt langsamer |
| 28 | **Auf dem Ryzen nachmessen**: „7 gegen 8 Threads messen“ mit `kollaps-6` beim Abriss | bestätigt 27 oder nimmt es zurück | 27 | ✅ am 23. Oktober: Mit 8 Threads ist ein Bild parallel 2,5 % und nacheinander 1,7 % schneller als mit 7, parallel mit 8 nur 2,4 % langsamer als nacheinander. Schritt 27 zurückgenommen |
| 29 | **Überlauf an Knoten auf mehreren Threads**: die Bündel eines Knotens in einer festen Zahl von Gruppen zugleich auf mehreren Threads rechnen, jede gegen eigene Kopien des Knotens, danach der Mittelwert, wie „Knoten im Überlauf mit geteilter Masse“ über die Spuren hinaus, ändert die Ergebnisse | mit den Einstürzen bei ×2 bis zu 0,3 ms pro Schritt, rund 10 % eines Bilds mit 8 Threads | 25 | ✅ auf `kollaps-7`: Liegen an den Knoten großer Inseln mindestens 256 Kontakte, rechnen die Threads ihre Bündel in acht Gruppen gegen eigene Kopien. Beim Abriss auf der VM bei ×2 kostet der Überlauf auf dem aufrufenden Thread mit 2 und 4 Threads 19 und 42 % weniger, die Bedingungen 9 und 11 %, mit einem Thread pro Kontakt 17 % mehr. Der Abriss verläuft anders, in 4 000 Bildern fallen 6 statt 4 Häuser. Schwere Teile liegen wie vorher und kommen öfter zur Ruhe. Auf dem Ryzen mit 8 Threads am Überlauf 63 % weniger, siehe 30 |
| 30 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-7` beim Abriss bei ×2 | zeigt, was 29 mit 8 Threads bringt | 29 | ✅ am 23. Oktober: Mit 8 Threads rechnet der aufrufende Thread am Überlauf 0,16 statt 0,43 ms pro Schritt, die Bedingungen kosten 19 % weniger, ein Bild dauert 2,80 statt 2,96 ms, obwohl der Abriss weiter ist, und mit 8 Threads bleiben 54 statt 60 % auf einem Thread. Mit einem Thread kostet ein Kontakt an Knoten 30 % mehr |
| 31 | **Wartezeit des Teams auf eine Stufe zählen**: wie sich mit 8 Threads Arbeit und Warten auf die Stufen der Farben verteilen | Werkzeug, entscheidet, wie es im Löser weitergeht: Jeder Worker wartet im Team 47 % der Zeit, und die Bedingungen haben 0,55 ms seriell | 30 | ✅ auf `kollaps-8`: In jedem achten Schritt mit Team zählt Box3D pro Farbe und für Aufbau, Integration, Knoten und Speichern, wie lange die Stufen offen sind, wie die Threads die Zeit darin verbringen und was der aufrufende Thread zwischen ihnen allein rechnet, bitgleich. Beim Abriss auf der VM bei ×2 mit 4 Threads rechnen die Threads in den offenen Stufen 65 bis 66 % an Blöcken und 18 bis 19 % an den übrigen Paketen, 12 bis 13 % warten sie nach ihrem letzten Block und 3 % davor. Dort tritt keine der Regeln unter „Was als Nächstes in Frage kommt“ ein, auf dem Ryzen nachzumessen, siehe 34. Auf dem Ryzen tritt nur „Allein“ ein |
| 34 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-8` beim Abriss bei ×2 | zeigt, wo das Team mit 8 Threads wartet, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es im Löser weitergeht | 31 | ✅ am 24. Oktober: Mit 8 Threads tritt nur die Regel „Allein“ ein. Der aufrufende Thread rechnet 35 % der Zeit des Teams allein, 26 % davon nach der letzten Stufe. Dort wartet er, auf der VM gezählt, auf das Paket einer zweiten großen Insel, das ein Worker allein rechnet, siehe 35 |
| 35 | **Große Pakete im Team**: Das Team rechnet nicht nur das größte Paket in seinen Stufen, sondern jedes, das mehr als den Anteil eines Threads hält, bitgleich | geschätzt mit 8 Threads rund 15 % der Zeit des Teams, 3 bis 4 % eines Bilds, mit 4 Threads nichts | 34 | ✅ auf `kollaps-9`: Mit dem größten Paket rechnet das Team jedes weitere, das mehr als das Doppelte des Anteils eines Threads hält, bitgleich. Mit dem einfachen Anteil kostete das Team auf der VM mit 4 Threads in den Schritten mit zweitem Paket 5,6 % mehr, denn ein Thread im Team rechnet nur etwas mehr als halb so schnell wie allein. Mit 4 Threads hält eine zweite Insel nie das Doppelte, auf der VM ändert sich nichts. Mit 8 Threads nimmt das Team beim Abriss nach den Inseln auf der VM in rund 40 % seiner Schritte ein zweites Paket mit, auf dem Ryzen nachzumessen, siehe 36. Dort kosten die Bedingungen mit 8 Threads 19 % weniger und ein Bild 6 % |
| 36 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-9` beim Abriss bei ×2 | zeigt, was 35 mit 8 Threads bringt und ob die Schwelle passt | 35 | ✅ am 24. Oktober, `kollaps-8` und `kollaps-9` direkt nacheinander: Mit 8 Threads kostet das Team 0,724 statt 0,894 ms pro gemessenem Schritt, nach der letzten Stufe bleiben 0,001 statt 0,229 ms, die Bedingungen kosten 19 % weniger, ein Schritt 8 % und ein Bild 2,62 statt 2,78 ms, 6 % weniger. Mit 1, 2 und 4 Threads gleich. Von den Regeln tritt keine mehr ein, weiter mit 32 |
| 32 | **Aufbau des Lösers auf den Workern**: die Teile des Aufbaus, die nicht voneinander abhängen, parallel, bitgleich | höchstens 0,1 ms pro Schritt, rund 4 % eines Bilds mit 8 Threads | 30 | ✅ auf `kollaps-10`: Beim Abriss kosten die Knoten des Überlaufs gut die Hälfte des Aufbaus, die Pakete der Körper und Kontakte und das Ordnen der Kontakte fast den Rest. Zwei Aufgaben auf den Workern rechnen jetzt die Pakete und das Ordnen, während der aufrufende Thread die Knoten baut, ab 256 Kontakten im Überlauf, bitgleich. Auf der VM kostet der Aufbau beim Abriss mit 2 und 4 Threads rund ein Drittel weniger, im Benchmark in den Städten mit vielen Knoten 8 bis 13 %. Auf dem Ryzen nachzumessen, siehe 37. Dort kostet der Aufbau mit 8 Threads 23 % weniger |
| 37 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-10` beim Abriss bei ×2 | zeigt, was 32 mit 8 Threads bringt | 32 | ✅ am 24. Oktober, `kollaps-10` und `kollaps-9` direkt nacheinander: Der Aufbau des Lösers kostet mit 2, 4 und 8 Threads 28, 27 und 23 % weniger, mit 8 Threads 0,098 statt 0,127 ms, und der aufrufende Thread wartet nie auf die Worker. Ein Schritt kostet im Rahmen der Schwankungen gleich viel, die 0,03 ms gehen darin unter. Ein erster Lauf war am PC gestört |
| 38 | **Kontakt-Schleife zerlegen**: messen, was die Kontakt-Schleife mit 8 Threads bremst, lange Blöcke, die seriellen Teile davor und danach oder der Speicher | Werkzeug, entscheidet, wie es in der Kontakt-Schleife weitergeht: mit 8 Threads rund ein Drittel eines Schritts, und der Fit nennt 0,21 ms davon seriell | 37 | ✅ auf `kollaps-11`: Box3D misst in jedem Schritt das Sammeln vor der Schleife, die Schleife und das Zusammenführen danach und in jedem achten, wie die Threads die Schleife verbringen, bitgleich. Beim Abriss auf der VM bei ×2 mit 4 Threads sind Sammeln und Zusammenführen 2 % der Kontakt-Schleife, und die Threads rechnen 93 % ihrer Zeit in Blöcken, 2 % warten sie vor dem ersten und 5 % nach dem letzten. Ein Kontakt kostet mit 4 Threads so viel wie mit einem. Die teuersten Kontakte liegen an beiden Enden des Arrays, daraus 39 |
| 39 | **Blöcke der Kontakt-Schleife von beiden Enden**: Die Threads nehmen die Blöcke abwechselnd von vorne und von hinten, wo die teuersten Kontakte liegen, die billigen der Mitte zuletzt, bitgleich | 2 bis 3 % der Kontakt-Schleife | 38 | ✅ auf `kollaps-11`: Auf der VM mit 4 Threads warten die Threads nach ihrem letzten Block im Benchmark 55 bis 77 % und beim Abriss 56 % kürzer und rechnen 96 bis 97 statt 93 bis 95 % ihrer Zeit in Blöcken. Mehr oder weniger Blöcke, Blöcke, die zum Ende kleiner werden, rückwärts und gestreut brachten nichts oder kosteten mehr |
| 40 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-11` beim Abriss bei ×2 | zeigt, was die Kontakt-Schleife mit 8 Threads bremst, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es weitergeht | 38 und 39 | ✅ am 24. Oktober, `kollaps-10` und `kollaps-11` direkt nacheinander: Mit 8 Threads rechnen die Threads der Kontakt-Schleife 95,7 % ihrer Zeit in Blöcken, 2,6 % warten sie vor dem ersten und 1,5 % nach dem letzten, ein Kontakt kostet 9,7 % mehr als mit einem Thread, und Sammeln und Zusammenführen sind 1,7 % der Kontakt-Schleife. Keine Regel tritt ein, die Kontakt-Schleife skaliert. Mit 2, 4 und 8 Threads kostet sie 1,4 bis 2,4 % weniger als im Mittel der fünf Läufe davor, ein Schritt mit 8 Threads 2,222 ms, so wenig wie in keinem Lauf davor. Der Lauf mit `kollaps-10` war am PC gestört. Am weitesten über einem Achtel liegen danach die Bedingungen und die Ruhe-Prüfung, siehe 41 |
| 41 | **Ruhe-Prüfung zerlegen**: messen, was die Ruhe-Prüfung im Update mit 8 Threads bremst, die Prüfungen auf den Threads, die Verteilung ihrer Blöcke oder das Zusammensetzen und Erstarren danach auf dem aufrufenden Thread | Werkzeug, entscheidet, wie es in der Ruhe-Prüfung weitergeht: mit 8 Threads 0,14 ms über einem Achtel ihrer Zeit mit einem Thread, gut 6 % eines Schritts | 40 | ✅ auf `kollaps-12`: Nebenan misst in jedem Update das Sammeln, die Prüfungen auf den Threads, die Suche und das Erstarren und in jedem achten, wie die Threads die Prüfungen verbringen, bitgleich. Beim Abriss auf der VM bei ×2 sind 92 % der Ruhe-Prüfung die Prüfungen, 87 % davon der Blick auf das, was die ruhigen Stücke trägt, das Erstarren kostet seriell 0,17 ms. Mit 4 Threads warten die Threads 12,3 % ihrer Zeit nach ihrem letzten Block, vor allem auf große ruhige Stücke, daraus 42 |
| 42 | **Blöcke mit großen Stücken zuerst**: Die Welt führt ein Bit für die großen Trümmer, und mit Workern nehmen die Threads die Blöcke mit einem großen Stück zuerst, bitgleich | mit 8 Threads geschätzt 15 % der Prüfungen, rund 0,02 bis 0,03 ms pro Schritt | 41 | ✅ auf `kollaps-12`: Beim Abriss auf der VM mit 4 Threads warten die Threads nach ihrem letzten Block 65 % kürzer, und die Prüfungen kosten 4,7 % weniger, in einem Lauf, in dem die VM sonst 1,3 bis 2,6 % langsamer war. Im Benchmark gleich viel. Jedes große Stück in einem Block für sich kostete in der Stadt bei ×4 bis 15 % mehr |
| 43 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-12` beim Abriss bei ×2 | zeigt, was die Ruhe-Prüfung mit 8 Threads bremst, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es weitergeht | 41 und 42 | ✅ am 25. Oktober, `kollaps-11` und `kollaps-12` direkt nacheinander: Mit 8 Threads rechnen die Threads der Prüfungen 72,1 % ihrer Zeit in Blöcken, 8,3 % warten sie vor ihrem ersten Block und 19,4 % nach ihrem letzten, der Blick auf ein ruhiges Stück kostet 26,5 % mehr als mit einem Thread, und Sammeln, Suche und Erstarren sind 31,5 % der Ruhe-Prüfung. „Ende“ und „Speicher“ treten ein. Kleinere Teile brächten nach der Simulation höchstens 7 bis 10 %, ein billigerer Blick mehr. Die Ruhe-Prüfung kostet mit 4 und 8 Threads 2 bis 3 % weniger als im Mittel der Läufe davor, nicht die vorhergesagten 15 %, ein Schritt gleich viel. Der Lauf mit `kollaps-11` war am PC gestört. Auf der VM geht der Blick zu 61 % in die Liste der Kontakte in Box3D, daraus 44 |
| 44 | **Kontaktlisten der ruhigen Stücke nebeneinander**: Box3D läuft die Listen der Kontakte aller ruhigen Stücke eines Blocks zugleich ab, jede in ihrer Reihenfolge, sodass sich das Warten auf den Speicher überlappt, und Nebenan schlägt jeden berührenden Kontakt nur einmal nach statt in drei Durchgängen, bitgleich | im Nachbau auf der VM die Blicke 37,5 % billiger, nach der Simulation die Prüfungen mit 1 bis 8 Threads 29 bis 33 %, auf dem Ryzen mit 8 Threads rund 0,05 ms pro Schritt, 2 % eines Bilds, mit einem Thread 0,25 ms | 43 | ✅ auf `kollaps-13`: `b3Body_GetContactDataOfBodies` in Box3D, `nbTouch` in Nebenan. Beim Abriss auf der VM bei ×2 kostet der Blick auf ein ruhiges Stück mit 1, 2 und 4 Threads 48 bis 49 % weniger, die Prüfungen 40 bis 42 %, die Ruhe-Prüfung 33 bis 39 % und ein Schritt 4,2 bis 6,0 %, die Welt endet bitgleich. Im Benchmark in der Stadt bei ×4 die Prüfungen 13 bis 14 % weniger, bei ×1 und ×2 2 bis 5 %. Mit 4 Threads warten die Threads jetzt 11,4 % ihrer Zeit vor ihrem ersten Block und 7,2 % nach ihrem letzten, auf dem Ryzen nachzumessen, siehe 45 |
| 45 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-13` beim Abriss bei ×2 | zeigt, was 44 mit 8 Threads bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es weitergeht | 44 | ✅ am 25. Oktober, `kollaps-12` und `kollaps-13` direkt nacheinander, mit Workern beide 15 bis 18 % langsamer als `kollaps-12` am Morgen: Die Prüfungen kosten mit 1, 4 und 8 Threads 27, 7 und 7 % weniger, der Blick auf ein ruhiges Stück 16 bis 31 % statt wie auf der VM 48 bis 49 %, ein Schritt mit einem Thread 2 %, mit 8 Threads gleich viel. Mit 8 Threads rechnen die Threads 64,7 % ihrer Zeit in Blöcken und warten 25,2 % nach ihrem letzten, und der Blick kostet 56 % mehr als mit einem Thread. „Ende“ und „Speicher“ treten ein. Zwei Runden brächten nach der Simulation nur 1 bis 2 %, am Ende warten die Threads auf den Blick auf einzelne große Stücke, daraus 46 |
| 46 | **Blick auf große Stücke billiger**: die bewegten Stücke, die auf einem großen liegen, in einer Menge mit offener Adressierung statt sortiert und binär durchsucht, Bruchstück und Trümmerteil der anderen Seite jedes Kontakts einmal nachschlagen und vorausladen, wie „Speicher“ vorschlägt, und lange Kontaktlisten über die Listen ihrer Formen nebeneinander vorausladen, bevor Box3D sie in ihrer Reihenfolge abläuft, bitgleich | nach der Simulation mit 8 Threads 11 bis 21 % der Prüfungen, auf dem Ryzen rund 0,02 bis 0,035 ms pro Schritt, mit einem Thread 5 bis 8 % | 45 | ✅ auf `kollaps-14`: `nbRestingSet` und die Durchgänge in `nbDescribeTouches` in Nebenan, `b3PrefetchContactsOfShapes` in Box3D. Beim Abriss auf der VM bei ×2 kosten die Prüfungen mit 1, 2 und 4 Threads 7,5, 10,1 und 14,4 % weniger, der Blick 10 bis 13 %, der längste Block dauert 24 bis 32 % kürzer, und mit 4 Threads warten die Threads am Ende 68 % kürzer. Die Welt endet bitgleich. Den Ruhezustand hinter die Flags zu legen, brachte nichts. Auf dem Ryzen nachzumessen, siehe 47 |
| 47 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-14` beim Abriss bei ×2 | zeigt, was 46 mit 8 Threads bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es weitergeht | 46 | ✅ am 25. Oktober, `kollaps-13` und `kollaps-14` direkt nacheinander, ungestört: Die Prüfungen kosten mit 8 Threads 18 % weniger, 0,109 statt 0,133 ms, wie vorhergesagt, mit einem Thread 2 statt 7 %, der längste Block dauert 16 bis 24 % kürzer, und der Blick kostet mit 8 Threads nur noch 12 statt 25 % mehr als mit einem. Die Threads warten 20,4 % ihrer Zeit nach ihrem letzten Block. „Ende“ tritt ein, „Speicher“ nicht. Beim größten Stück kostet das Ablaufen seiner Liste mehr als der Blick, und bei allen Stücken berühren vier von fünf Einträgen nicht, daraus 48 |
| 48 | **Nur berührende Kontakte ablaufen**: Box3D führt für jeden Körper eine Liste seiner berührenden Kontakte, gepflegt, wo ein Kontakt zu berühren beginnt oder aufhört oder gelöscht wird, und `b3Body_GetContactData` und `b3Body_GetContactDataOfBodies` laufen nur sie ab, sortiert nach einer Nummer, die jeder Kontakt beim Anlegen bekommt, in die Reihenfolge der Liste des Körpers, bitgleich | nach der Simulation mit 8 Threads 14 % der Prüfungen, auf dem Ryzen rund 0,016 ms pro Schritt, mit einem Thread 18 %, rund 0,09 ms, abzüglich der Pflege von rund 440 Wechseln pro Schritt in der seriellen Schleife der Zustandswechsel | 47 | ✅ auf `kollaps-15`: Listen nur für Körper, die nicht statisch sind, mit den Formen des Kontakts in jedem Eintrag, ohne Vorausladen der Nachbarn. Beim Abriss auf der VM bei ×2 kostet der Blick auf ein ruhiges Stück mit 1, 2 und 4 Threads 20 bis 24 % weniger, die Prüfungen 20, 16 und 13 %, die Pflege 0,014 bis 0,024 ms in den Zustandswechseln und bis 0,011 ms in Typwechseln und Erstarren, und ein Schritt spart 0,27, 0,12 und 0,04 ms. In der Stadt bei ×4, wo 64 % der Einträge berühren, kostet die Pflege 0,02 bis 0,1 ms, und der Blick spart nichts. Bitgleich. Auf dem Ryzen nachzumessen, siehe 49 |
| 49 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-15` beim Abriss bei ×2, direkt nach `kollaps-14` | zeigt, was 48 mit 8 Threads nach der Pflege der Listen bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es weitergeht | 48 | ✅ am 26. Oktober, `kollaps-14` und `kollaps-15` direkt nacheinander, so ruhig wie am 25.: Die Prüfungen kosten mit 1, 2, 4 und 8 Threads 11, 9, 8 und 12 % weniger, mit 8 Threads 0,095 statt 0,108 ms, vorhergesagt waren 0,093 ms, mit einem Thread 11 statt 19 %, der Blick spart auf dem Ryzen weniger als auf der VM. Die Pflege kostet mit 8 Threads 0,009 ms in Zustandswechseln, Typwechseln und Erstarren und 0,004 ms beim Anlegen der Kontakte, so viel, wie das Update spart. Die Threads warten mit 8 Threads 16,7 % ihrer Zeit nach ihrem letzten Block, mit 4 Threads 1,4 %. „Ende“ tritt ein, „Pflege“ knapp nicht, daraus 50 |
| 50 | **Blöcke der Prüfungen schneiden**: die Zeit jedes Blocks auf der VM mit einem Thread schreiben und mit der Simulation, die den Ryzen mit 8 Threads trifft, prüfen, ob kleinere Blöcke am Ende der Reihenfolge, kleinere Blöcke um große Stücke oder eine Reihenfolge nach den Kontakten der Stücke das Warten am Ende verkürzen, und das Beste davon bauen, bitgleich | mit 8 Threads höchstens 0,016 ms pro Update, 17 % der Prüfungen, wenn am Ende kein Thread mehr wartet, mit 1 bis 4 Threads kaum etwas | 49 | ⛔ auf `kollaps-16` gemessen und simuliert: Kleinere Blöcke kosten zusammen 3,5 bis 7,6 % mehr, weil Box3D weniger Listen nebeneinander abläuft. Nach der Simulation, die `kollaps-15` auf dem Ryzen mit 8 Threads auf 1 und 3 % trifft, verkürzen sie die Prüfungen dort überall, nur am Ende oder um große Stücke um höchstens 0,002 ms pro Update. Ohne jedes Warten am Ende wären es 0,006 ms, nicht die 0,016 ms von oben: In 23 % der Updates dauert der längste Block allein länger als ein Achtel der Arbeit. Mit 4 Threads kosten Blöcke von 16 auf der VM gemessen 0,015 ms mehr. „Schneiden“ tritt nicht ein, daraus 51 |
| 51 | **Bedingungen des Lösers zerlegen**: messen, wohin die Bedingungen mit 8 Threads verlieren, was sie über einem Achtel ihrer Zeit mit einem Thread kosten, ob die Threads in den Paketen der Inseln langsamer rechnen als einer allein, ob sie auf ein großes Paket oder ein Team warten oder auf das, was der aufrufende Thread davor und danach allein rechnet, bitgleich | Werkzeug, entscheidet, wie es im Löser weitergeht: Auf dem Ryzen liegen die Bedingungen mit 8 Threads 0,377 ms über einem Achtel ihrer Zeit mit einem Thread, 18 % eines Schritts, der Überlauf davon 0,100 ms, und auf die letzten Pakete wartet der aufrufende Thread nur 0,036 ms | 50 | ✅ auf `kollaps-17`: Box3D misst in jedem achten Schritt die Arbeit der Inseln, auch mit einem Thread, und die Zeit der ganzen Pakete, der Bericht zeigt die Mikrosekunden pro 1 000 Einheiten Arbeit, bitgleich. Den größten Teil zerlegte schon der Bericht vom Ryzen mit `kollaps-15`: Mit 8 Threads rechnen die Threads 3,21 von 5,86 ms eines Schritts mit Team in seinen Blöcken, 1,02 ms warten sie am Ende der Stufen. Beim Abriss auf der VM bei ×2 kostet eine Einheit mit 2 und 4 Threads in den Paketen 1,07- und 1,18-mal und in den Blöcken des Teams 1,18- und 1,39-mal so viel wie mit einem Thread. Auf dem Ryzen nachzumessen, siehe 52 |
| 52 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-17` beim Abriss bei ×2 | zeigt, wie viel länger ein Thread mit 8 Threads im Team und in den Paketen für dieselbe Arbeit braucht, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es im Löser weitergeht | 51 | ✅ am 26. Oktober: Ein erster Lauf mit `kollaps-17` war überall gestört, dann `kollaps-16` und `kollaps-17` direkt nacheinander, mit 2 und 4 Threads beide gestört, `kollaps-17` auch mit 8. Die Stichprobe kostet nichts: Mit einem Thread kostet `kollaps-17` 1,5 % mehr, mit 4 Threads gleich viel. Eine Einheit Arbeit kostet in den Blöcken des Teams mit 8 Threads, mit der Zeit der Blöcke aus dem sauberen Lauf von `kollaps-16` gerechnet, das 1,34- bis 1,39-Fache eines Threads allein, gestört gelesen das 1,69-Fache. Am Ende der Stufen warten die Threads 19 % der offenen Zeit, 0,119 ms pro Schritt, und der aufrufende Thread rechnet 13 % der Zeit des Teams allein, 0,095 ms. „Langsamer“, „Ende“ und „Allein“ treten ein, „Ende“ hat knapp die meiste Zeit dahinter, daraus 53 |
| 53 | **Blöcke des Teams feiner schneiden**: die Blöcke jeder Stufe des Teams auf der VM schreiben und mit einer Simulation der Stufen, die den Ryzen mit 8 Threads trifft, prüfen, wie viel weniger Einträge pro Block oder mehr Blöcke pro Worker das Warten am Ende der Stufen verkürzen, und das Beste davon bauen, bitgleich | mit 8 Threads höchstens 0,12 ms pro Schritt, das Warten am Ende der Stufen, gut 5 % eines Schritts | 52 | ⛔ auf `kollaps-18`: Mit 8 Workern trifft die VM die Stufen des Ryzen genau, und die Simulation, an seine Tabelle angepasst, trifft seine offenen Zeiten auf wenige Prozent. Jeder Thread versucht in einer Stufe jeden Block einmal, mit 8 Threads sind das 17 600 vergebliche Versuche pro Schritt, 0,60 bis 0,69 ms über alle Threads. Feinere Blöcke vervielfachen sie und kosten 35 bis 352 µs pro Schritt mehr, gröbere brächten 9 bis 32 µs. Kein Schnitt kommt an 0,04 ms heran, nicht gebaut. Die vergeblichen Versuche sind der größte Teil von „Langsamer“, daraus 54 |
| 54 | **Vergebene Blöcke billiger überspringen**: auf der VM messen, wie viel billiger ein vergeblicher Versuch wird, wenn ein Thread den Anspruch eines Blocks erst liest und nur einen freien zu tauschen versucht, wenn die Ansprüche dichter oder auf eigenen Cache-Zeilen liegen oder wenn jeder Worker seine Blöcke über einen eigenen Zähler nimmt, und das Beste davon bauen, bitgleich | mit 8 Threads nach der Simulation 0,04 bis 0,05 ms pro Schritt, wenn ein Versuch so viel billiger wird wie auf der VM mit dem Lesen, ohne jeden Preis 0,09 bis 0,11 ms | 53 | ✅ auf `kollaps-19`: Liest ein Thread den Anspruch vor dem Vergleichstausch, kostet ein vergeblicher Versuch auf der VM mit 4 Threads 0,024 bis 0,025 statt 0,042 bis 0,057 µs, mit dichten Ansprüchen 0,036 und mit eigenen Cache-Zeilen 0,042 µs. Nach der Simulation bringt das Lesen auf dem Ryzen mit 8 Threads 0,039 bis 0,062 ms pro Schritt, dichte Ansprüche und eigene Zeilen weniger, das Verfahren von Box2D, das am ersten genommenen Block aufhört, 0,016 bis 0,041 ms. „Überspringen“ tritt ein, gebaut ist das Lesen, bitgleich, im Benchmark gleich viel im Rahmen der Schwankungen. Auf dem Ryzen nachzumessen, siehe 55 |
| 55 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-18` und `kollaps-19` direkt nacheinander beim Abriss bei ×2 | zeigt, wie viel das Lesen vor dem Vergleichstausch mit 8 Threads bringt, nach der Simulation 0,04 bis 0,05 ms pro Schritt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob es bleibt | 54 | ✅ am 26. Oktober: Beide Läufe ruhig. Das Team kostet mit 8 Threads 0,708 statt 0,730 ms pro gemessenem Schritt, 0,022 ms weniger, gegen vier ruhige Läufe davor 0,019 ms, die Bedingungen 3,2 % weniger. In den Blöcken des Teams braucht ein Thread pro Einheit das 1,24- statt das 1,33-Fache eines Threads allein, wie vorhergesagt, die Stufen schließen aber nur halb so viel früher wie vorhergesagt, weil die Threads am Ende länger warten. „Behalten“ tritt ein, daraus 56 |
| 56 | **Allein zerlegen**: messen, was der aufrufende Thread im Team allein rechnet, ohne die Uhr, die die Stichprobe in jeder Stufe liest: die Kontakte des Überlaufs außerhalb der Knoten in ihren Durchgängen, das Mitteln der Kopien, die Übergänge zwischen den Stufen und die Stufen mit einem Block, und was davon geht, auf die Worker verteilen | mit 8 Threads höchstens 0,096 ms pro Schritt, 13,6 % des Teams, davon gut 0,04 ms der Überlauf außerhalb der Knoten und 0,040 ms die Stufen mit einem Block | 55 | ⛔ auf `kollaps-20` gemessen: Von den rund 24 Kontakten des Überlaufs außerhalb der Knoten hängen 97 % an den Bündeln der Knoten und müssen nach ihnen kommen. Ohne Bedingung verteilbar sind nur ihr Vorbereiten und Speichern, auf dem Ryzen höchstens 0,014 ms pro Schritt, und die Stufen mit einem Block sind kleine Farben, die nacheinander kommen. „Verteilen“ tritt nicht ein, nicht gebaut, daraus 57 |
| 57 | **Aufbau der Knoten verteilen**: die Durchgänge von `b3BuildKnots` auf der VM einzeln messen und die, die jeden Kontakt für sich rechnen, vor allem den mit zufälligen Zugriffen auf die Kontakte, auf die Worker legen, die während des Aufbaus warten, bitgleich | mit 8 Threads ein Teil der 0,086 ms pro Schritt, die der aufrufende Thread die Knoten allein baut, während zwei Worker nach 0,032 ms fertig sind und die übrigen warten | 56 | ⛔ auf `kollaps-21` gemessen: Am teuersten sind das Lesen der Körper aus den verstreuten Kontakten mit 23 % des Aufbaus, das Bündeln mit 21 % und das Verteilen auf die Gruppen mit 18 %. Jeden Kontakt für sich rechnen 46 %, auf die wartenden Worker verteilt sparen sie auf dem Ryzen rund 0,013 ms pro Schritt, höchstens 0,029 ms. „Verteilen“ tritt nicht ein, nicht gebaut, daraus 58 |
| 58 | **Zustandswechsel zerlegen**: die Schleife der Zustandswechsel der Kontakte auf der VM nach der Art des Wechsels und nach ihren Teilen messen, getrennte Paare löschen, Berührungen beginnen und enden lassen, darin Inseln, Graph, Listen und Sätze, und was jeden Kontakt für sich rechnet und nichts ändert, was ein anderer Wechsel liest, in die Kontakt-Schleife auf den Workern vorziehen, bitgleich | mit 8 Threads ein Teil der 0,063 ms pro Schritt, die der aufrufende Thread die Zustandswechsel allein rechnet, während alle Worker warten | 57 | ⛔ auf `kollaps-22` gemessen: Beim Abriss bei ×2 hat ein Schritt im Mittel 881 Wechsel, 441 gelöschte Paare, 258 beginnende und 182 endende Berührungen. Jeden Kontakt für sich, ohne etwas zu ändern, was ein anderer Wechsel liest, rechnen nur das Sammeln der Bits und das Laden der Kontakte und ihrer Formen, 18 % der Schleife, auf dem Ryzen höchstens 0,011 ms pro Schritt. „Vorziehen“ tritt nicht ein, nicht gebaut, daraus 59 |
| 59 | **Ordnen, Gruppen und Bündeln billiger**: das Ordnen der Kontakte nach Knoten, das Verteilen auf die Gruppen und das Bündeln in `b3BuildKnots` bitgleich mit weniger Durchgängen über die Kontakte bauen und in einer Kopie, die in jedem Schritt beide Fassungen rechnet und vergleicht, gegen die alte messen | mit 1 bis 8 Threads ein Teil der rund 0,038 ms pro Schritt, die diese drei seriellen Durchgänge auf dem Ryzen kosten | 58 | ⛔ auf `kollaps-23` gemessen: Eine Fassung, die beim Verteilen auf die Gruppen vier von sechs Durchgängen spart und beim Ordnen und beim Zählen der großen Inseln je einen, liefert in allen Schritten dieselben Bündel und Gruppen, kostet auf der VM aber nur 6 bis 7 % weniger, 10 bis 12 µs pro Schritt, auf dem Ryzen rund 0,005 ms. Die Zeit liegt in den verstreuten Zugriffen auf Körper und Partner. „Billiger“ tritt nicht ein, nicht gebaut, daraus 60 |
| 61 | **Zeitleiste des Teams mit Tracy**: auf dem Ryzen mit `Tracy-Aufnahme.bat` aus `nebenan-demo-windows-tracy` die ersten 900 Schritte des Abrisses bei ×2 mit 8 Threads aufnehmen und mit `tools/tracy_team.py` auswerten, wie lange die Threads im Löser an den Enden der Stufen, in den Übergängen zwischen ihnen, beim Anlaufen und an Störungen verlieren | Werkzeug, entscheidet, ob die größte Insel in Gebiete ohne Sperren zerlegt wird: Mit 8 Threads warten die Threads am Ende der Stufen 21 % der offenen Zeit, und wie sich das Warten auf die rund 232 Stufen pro Schritt verteilt, zeigten bisher nur Summen | 59 | ✅ am 27. Oktober auf dem Ryzen aufgenommen, 852 der 900 Schritte: In den 848 Schritten mit Team dauert der Löser mit 8 Threads 0,755 ms. Die Threads verlieren 0,219 ms an den Enden der Stufen und 0,036 ms in den Übergängen, zusammen 0,255 ms, ein Drittel des Lösers. Ausreißer und Pausen kosten 0,008 ms, das Anlaufen 0,006 ms. „Zerlegen“ tritt ein, daraus 62 |
| 62 | **Größte Insel in Gebiete zerlegen**: auf einer eigenen Branch die größte Insel in eine feste Zahl von Gebieten teilen, die ein Thread jeweils ohne Sperren zwischen den Farben rechnet, die Körper an den Grenzen mit geteilter Masse wie bei den Knoten, nach jedem Durchgang des Lösers gemittelt, ändert die Ergebnisse | mit 8 Threads ein Teil der 0,255 ms pro Schritt mit Team, die die Threads an den Enden der Stufen und in den Übergängen verlieren, gut ein Zehntel eines Schritts | 61 | ⛔ auf `kollaps-25` gebaut und verworfen: Die Ergebnisse bleiben mit jeder Zahl von Threads und auf Windows, Linux und macOS gleich, die Tests grün, nichts hängt in der Luft. Mit einem Thread kostet der Löser auf der VM gepaart aber in zwei Messungen im Mittel 8,1 und 10,7 % mehr, erlaubt sind 5 %: Ein Thread baut die Gebiete in jedem Schritt neu, 0,67 ms, rund 100 ns pro Kontakt der großen Inseln. Auf dem Ryzen kostete das geschätzt 0,19 ms, selbst ohne jede Sperre blieben dort höchstens 0,065 statt der verlangten 0,10 ms. „Verwerfen“ tritt ein, der Prototyp liegt im Verlauf von `kollaps-25`, daraus 60 |
| 60 | **Erstarren zerlegen**: das Erstarren des Schutts auf der VM nach seinen Teilen messen, die Entscheidungen von Nebenan, die Typwechsel in Box3D mit dem Löschen der Kontakte, dem Umzug der Körper und dem Einbau in die Bäume, und das Lösen von Schutt unter großen Stücken, und was jedes Stück für sich rechnet und nichts ändert, was ein anderes liest, in die Prüfungen auf den Workern vorziehen, bitgleich | mit 8 Threads ein Teil der 0,047 ms pro Schritt, die der aufrufende Thread das Erstarren allein rechnet, während alle Worker warten | 59 und 62 | ⛔ auf `kollaps-26` gemessen: Beim Abriss bei ×2 erstarren pro Schritt 13,4 Stücke mit je 19 Kontakten. Ein Erstarren kostet auf der VM 16,7 µs, davon 55 % das Löschen der Kontakte und 25 % der Umzug in den statischen Satz. Jedes Stück für sich rechnen großzügig gerechnet 17 % des Erstarrens, auf dem Ryzen 0,008 ms pro Schritt. „Vorziehen“ tritt nicht ein, nicht gebaut, daraus 63 |
| 63 | **Anlegen zerlegen**: das Anlegen der Kontakte für neue Paare auf der VM nach seinen Teilen messen, die Schlüssel und Formen der Paare, die Kontakte selbst mit ihren Ids, die Menge der Paare und die Listen der Körper, und was jeden neuen Kontakt für sich rechnet und nichts ändert, was das Anlegen eines anderen liest, in die Paarsuche auf den Workern vorziehen, bitgleich | mit 8 Threads ein Teil der 0,049 ms pro Schritt, die der aufrufende Thread das Anlegen allein rechnet, während alle Worker warten | 60 | ⛔ auf `kollaps-27` gemessen: Beim Abriss bei ×2 entstehen pro Schritt 776 neue Kontakte, einer kostet auf der VM 461 ns. Die Hälfte davon wartet die Schleife auf die rund 15 Zeilen, die ein neuer Kontakt anfasst und die sie selbst schreibt. Jeden Kontakt für sich rechnen großzügig gerechnet 10 %, mit dem Sortieren auf dem Ryzen 0,007 ms pro Schritt. „Vorziehen“ tritt nicht ein, nicht gebaut, daraus 64 |
| 64 | **Auf dem Ryzen messen**: „Skalierung messen“ mit `kollaps-27` beim Abriss bei ×2 und ×4 | zeigt nach den Schritten 56 bis 63, wo mit 8 Threads noch Zeit über einem Achtel der Zeit eines Threads liegt, und entscheidet die nächsten Schritte | 63 | ✅ am 27. Oktober auf dem Ryzen: Mit 8 Threads kostet ein Schritt bei ×2 2,25 und bei ×4 1,64 ms. Über einem Achtel ihrer Zeit mit einem Thread liegen von den Posten, die 56 bis 63 noch nicht zerlegt haben, die Kontakt-Schleife mit 0,083 und 0,152 ms, die Einschläge mit 0,058 und 0,089 ms und bei ×4 die Prüfungen des Updates mit 0,051 ms. Bei ×4 kostet ein Kontakt mit 8 Threads 45 % mehr als mit einem. „Zerlegen“ tritt ein, daraus 65 |
| 65 | **Kontakt-Schleife bei ×4 zerlegen**: auf der VM beim Abriss bei ×4 messen, was ein Kontakt in den Blöcken der Kontakt-Schleife mit 1, 2 und 4 Threads kostet, nach Prüfungen und Narrowphase, woher der Anstieg mit dem zweiten Thread kommt, etwa weil jeder Block in jedem Schritt auf einem anderen Kern landet, und was ihn behebt, bitgleich | mit 8 Threads ein Teil der 0,152 ms pro Schritt bei ×4 und 0,083 ms bei ×2, die die Kontakt-Schleife über einem Achtel ihrer Zeit mit einem Thread liegt, wo dasselbe hilft auch in den Prüfungen des Updates und in den Paketen | 64 | ⛔ auf `kollaps-28` gemessen: Auf der VM kostet ein Kontakt bei ×4 mit 2 Threads in zwei Läufen 1,6 und 2,0 % mehr als mit einem, nicht 32 % wie auf dem Ryzen. Im Benchmark steigt er mit 2 Threads auch, wenn die Schleife allein auf dem aufrufenden Thread läuft, und feste Blöcke holen nichts Sicheres zurück, daraus 66 |
| 66 | **Einschläge zerlegen**: auf der VM beim Abriss bei ×4 messen, was ein Einschlag nach seinen Teilen kostet, die Zellen samt dem Warten auf die Worker, die Box3D-Körper und -Formen der neuen Teile, den Einbau der Kinder, die Suche, das Teilen der Akteure, die Überlappungen und die Bindungen, und was davon der aufrufende Thread allein rechnet und sich bitgleich auf die Worker legen oder sparen lässt | mit 8 Threads ein Teil der 0,089 ms pro Schritt bei ×4 und 0,058 ms bei ×2, die die Einschläge über einem Achtel ihrer Zeit mit einem Thread liegen | 65 | ⛔ auf `kollaps-29` gemessen: Ein Einschlag kostet auf der VM bei ×4 rund 310 µs, 73 % davon rechnet der aufrufende Thread allein. Am meisten kostet der Stoß der Explosion in Box3D mit 27 %, auf dem Ryzen 0,028 ms pro Schritt. Seine Schleife über die Formen auf die Worker zu legen brächte dort mit den 7 µs, die sie zum Anlaufen brauchen, 0,013 bis 0,014 ms, jeder andere Teil liegt unter 0,011 ms. „Verteilen“ tritt nicht ein, daraus 67 |
| 67 | **Prüfungen des Updates bei ×4 zerlegen**: auf der VM beim Abriss bei ×4 messen, was eine Prüfung auf Ruhe und der Blick auf die Auflage eines ruhigen Stücks mit 1, 2 und 4 Threads nach ihren Teilen kosten, woher ihr Anstieg mit dem zweiten Thread kommt, der anders als in der Kontakt-Schleife auch auf der VM auftritt, und was ihn behebt, bitgleich | mit 8 Threads ein Teil der 0,051 ms pro Schritt bei ×4, die die Prüfungen des Updates über einem Achtel ihrer Zeit mit einem Thread liegen | 66 | ⛔ auf `kollaps-30` gemessen: Der Preis pro Stück steigt beim Abriss auf der VM mit 2 und 4 Threads um 11 und 17 %, aber kaum, wenn der aufrufende Thread allein prüft. Die Threads schreiben in Zeilen, die die anderen lesen, im Akteur `checkedAsleep` und seine Ruhe neben `isFree`, `isStatic` und `isRubble`, und in den Arenen der Worker. Getrennt und mit festen Blöcken steigt er beim Abriss noch um 5 und 10 %, das holt mit 4 Threads 42 % zurück statt der Hälfte, daraus 68. Der Nutzer will die Trennung trotzdem, daraus 69 |
| 68 | **Dauerhafte Gebiete**: auf einer eigenen Branch die Gebiete der großen Inseln wie die Farben im Graphen über die Schritte führen, jeder Körper fest in einem, jeder Kontakt mit Gebiet und Farbe, sobald er zu berühren beginnt, neu geschnitten nur selten, die Körper an den Grenzen mit geteilter Masse wie im Prototyp von Schritt 62, ändert die Ergebnisse | mit 8 Threads ein Teil der 0,255 ms pro Schritt mit Team, die die Threads an den Sperren zwischen den Stufen verlieren, ohne den Aufbau in jedem Schritt, an dem der Prototyp scheiterte | 67 und die Entscheidung des Nutzers | ✅ auf `kollaps-32` gebaut: Die Ergebnisse ändern sich, bleiben aber mit jeder Zahl von Threads, in beiden Breiten und auf Windows, Linux und macOS gleich, die Tests grün, nichts hängt in der Luft. Beim Abriss bei ×2 kostet der Löser auf der VM gepaart mit einem Thread 0,2 % mehr, erlaubt sind 5 %, und mit 4 Threads 12,2 % weniger. Im Benchmark unter Trommelfeuer kostet er mit 1 bis 4 Workern bis zu 9 % mehr, mit so wenigen Threads lohnen Gebiete auf kleineren Inseln nicht. Auf dem Ryzen kostet der Löser bei ×2 mit 8 Threads 0,107 ms weniger, die Gebiete bleiben, siehe 71 |
| 69 | **Prüfungen des Updates auf eigenen Zeilen**: was die Prüfungen von jedem bewegten Stück schreiben, auf eigenen Cache-Zeilen am Ende des Akteurs, alle Allokationen auf 64 Bytes, die Arenen der Worker und die Aufgaben der Prüfungen auf eigenen Zeilen, und mit Workern für jeden Thread ein fester Abschnitt der Blöcke, den er von Update zu Update behält, darin die Blöcke mit großen Stücken zuerst, bitgleich, auf Wunsch des Nutzers trotz „Nichts davon“ in 67 | auf dem Ryzen hochgerechnet rund 0,011 ms pro Schritt bei ×4 mit 8 Threads | 67 und der Wunsch des Nutzers | ⛔ auf `kollaps-31` gebaut, auf `kollaps-33` zurückgenommen: Die Welt endet bitgleich, die Tests bleiben grün. Beim Abriss auf der VM bei ×4 kosten die Prüfungen mit 2 und 4 Threads 2,8 und 6,2 % weniger, ein Stück in den Blöcken mit 4 Threads 11,6 %, mit einem Thread 1,6 % mehr. Im Benchmark unter Trommelfeuer bei ×4 kosten sie 1,0 bis 3,3 % mehr. Ein Akteur ist 448 statt 384 Bytes groß, die Stadt braucht 1,5 % mehr Speicher. Auf dem Ryzen kosten die Prüfungen damit mit 8 Threads 14 % mehr, siehe 70 |
| 70 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-30` und `kollaps-31` direkt nacheinander beim Abriss bei ×4 und ×2 | zeigt, was 69 mit 8 Threads bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob es bleibt | 69 | ✅ am 28. Oktober: Bei ×4 kosten die Prüfungen mit `kollaps-31` mit 8 Threads 0,088 statt 0,077 ms pro Schritt, 14 % mehr, mit einem Thread 0,8 % mehr, bei ×2 mit 8 Threads 0,110 statt 0,105 ms. In den Blöcken kostet ein Stück etwas weniger, aber mit den festen Abschnitten warten die Threads nach ihrem letzten Block bei ×4 zusammen 0,161 statt 0,065 ms. „Zurücknehmen“ tritt ein, auf `kollaps-33` ist die Trennung wieder heraus, bitgleich |
| 71 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-31` und `kollaps-32` direkt nacheinander beim Abriss bei ×2 und ×4, gern mit 70 in einem Zug | zeigt, ob die dauerhaften Gebiete mit 8 Threads mindestens 0,10 ms pro Schritt bringen, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob sie bleiben | 68 | ✅ am 28. Oktober: Der erste Lauf von `kollaps-31` bei ×2 war gestört, auch die Kontakt-Schleife kostete dort 25 % mehr, ein zweiter lief sauber. Darin kostet `solve` mit 8 Threads mit `kollaps-32` 0,860 statt 0,967 ms, 0,107 ms weniger, über alle sauberen Läufe 0,102 ms. „Übernehmen“ tritt ein, knapp, die Gebiete bleiben. Bei ×4 bringen sie mit 8 Threads nichts, und mit einem Thread kostet der Löser auf dem Ryzen bei ×2 6 % mehr. In den Durchgängen der Gebiete warten die Threads 31 % ihrer Zeit auf das langsamste, daraus 72 |
| 72 | **Durchgänge der Gebiete zerlegen**: auf der VM beim Abriss bei ×2 messen, wie lange jedes Gebiet in jedem Durchgang des Lösers rechnet, mit seinen Kopien, Knoten, dem übrigen Überlauf und den breiten Bedingungen, mit einem Thread einzeln gestoppt, daraus für den Ryzen mit 8 Threads simulieren, wie lange die Threads am Ende der Durchgänge auf das langsamste Gebiet warten und woran das liegt, und was ein Schnitt nach der Arbeit statt nach den Kontakten oder ein früherer Neuschnitt davon holt, ändert die Ergebnisse, wenn gebaut | mit 8 Threads ein Teil der bis zu 0,13 ms pro Schritt bei ×2, die die Threads in den Durchgängen der Gebiete warten, nachdem ihr Gebiet fertig ist | 71 | ✅ auf `kollaps-34` gemessen und gebaut: Was ein Gebiet kostet, folgt seinen Kontakten, und nach einem Schnitt läuft das größte schnell davon, nach 16 bis 31 Schritten das 1,79-Fache des Mittels, vor allem, weil Inseln, die dazukommen, ganz in das Gebiet des Körpers gingen, an dem sie hingen. Ein Schnitt kostet 150 ns pro Kontakt. Häufiger schneiden allein brächte auf dem Ryzen netto rund 0,02 ms, die Körper dorthin legen, wo der letzte Schnitt sie hinlegen würde, rund 0,033 ms, beides zusammen rund 0,035 ms. Mit einem Thread kostet beides zusammen 5,1 % mehr, erlaubt sind 2 %, das Platzieren allein 0,9 % weniger. „Ausgleichen“ tritt für das Platzieren ein, gebaut, daraus 73. Auf `kollaps-35` zurückgenommen, siehe 73 |
| 73 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-33` und `kollaps-34` direkt nacheinander beim Abriss bei ×2 und ×4 | zeigt, was das Platzieren nach dem Schnitt mit 8 Threads bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob es bleibt | 72 | ✅ am 28. Oktober: Bei ×4 kostet `solve` mit 8 Threads pro Einheit der Arbeit 1,2 % mehr. Der erste Lauf von `kollaps-34` bei ×2 war mit 2 bis 8 Threads gestört, auch die Kontakt-Schleife kostete dort 31 % mehr, ein zweiter lief sauber. Darin kostet `solve` mit 8 Threads 0,790 statt 0,849 ms bei 5,6 % weniger Arbeit, pro Einheit der Arbeit 1,5 % weniger, gegen die sauberen Läufe von `kollaps-32` 2,7 und 3,1 %. In den Durchgängen der Gebiete warten die Threads 26 statt 31 % ihrer Zeit, die Durchgänge kosten pro Einheit der Arbeit 7 % weniger. „Zurücknehmen“ tritt ein, auf `kollaps-35` ist das Platzieren wieder heraus, bitgleich mit `kollaps-33`. Als Nächstes kommt die Grafik |
| 33 | **Weniger Kopien an kleinen Knoten**: eine Gruppe für je 64 statt 32 Kontakte, ändert die Ergebnisse | mit 1 und 2 Threads ein Teil der 30 und 7 % pro Kontakt | 30 | 🔜 niedrig |
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
  weniger, auf dem Ryzen nachzumessen, siehe Schritt 30. Nachgemessen: Mit 8 Threads kostet er dort 0,16 statt 0,43 ms
  pro Schritt, 6 statt 15 % eines Bilds, und jeder Worker wartet im Team 47 statt 61 % auf eine Stufe.

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

Nach `kollaps-7` ist der Überlauf an Knoten auf dem Ryzen mit 8 Threads 6 % eines Bilds, siehe Schritt 30. Was dort mit
8 Threads bleibt: der Löser mit 1,12 ms pro Schritt, darin die Bedingungen mit 0,88 ms, von denen der Fit 0,55 ms
seriell nennt, weil die Worker im Team fast die Hälfte der Zeit auf eine Stufe warten, die Kontakt-Schleife mit 0,74 ms,
die gut skaliert, das Update mit 0,29 ms und auf dem aufrufenden Thread allein rund 0,3 ms, davon 0,13 ms für den Aufbau
des Lösers. Daraus die Schritte 31 bis 33, zuerst das Zählen der Wartezeit im Team.

Auf `kollaps-8` ist Schritt 31 gebaut, siehe „Wartezeit des Teams auf eine Stufe“ in
[Optimierungen.md](Optimierungen.md): Der Bericht von „Skalierung messen“ zeigt für die meisten Threads eine Tabelle der
Stufen des Teams, eine Zeile pro Farbe und je eine für Aufbau, Integration, Knoten und Speichern. Beim Abriss auf der VM
bei ×2 mit 4 Threads dauert das Team 3,91 ms pro gemessenem Schritt: 85 % davon offene Stufen, 3 % Stufen mit einem
Block, die der aufrufende Thread allein rechnet, 3 % rechnet er zwischen den Stufen allein, fast nur am Überlauf
außerhalb der Gruppen, und 9 % bleiben nach der letzten Stufe für die letzten Pakete. In den offenen Stufen rechnen die
Threads 65 bis 66 % ihrer Zeit an Blöcken und 18 bis 19 % an den übrigen Paketen, 12 bis 13 % warten sie nach ihrem
letzten Block und 3 % davor. Nach diesen Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen, Schritt 34:

- **Große Farben**: Warten die Threads in den Farben mit mindestens 8 Blöcken pro Stufe zusammen mehr als ein Viertel
  ihrer Zeit, vor dem Hineinkommen und nach dem letzten Block, halten die letzten Blöcke die Stufen auf. Dann mehr und
  kleinere Blöcke für diese Farben mit 8 Threads messen, auf der VM mit 4 Threads hatte das nichts geändert, siehe
  „Kleinere Blöcke im Löser“. Auf der VM 13 bis 21 %.
- **Kleine Farben**: Kosten die Farben mit höchstens zwei Blöcken pro Stufe, offen und allein gerechnet, zusammen mehr
  als ein Zehntel der Zeit des Teams, sind es zu viele Sperren für zu wenig Arbeit. Dann ihre Bedingungen wie die Knoten
  in Gruppen gegen Kopien rechnen, in der Stufe der Knoten statt in eigenen Stufen, ändert die Ergebnisse. Auf der VM
  sind es die Farben 12 bis 18 mit 3 %.
- **Knoten**: Wartet die Stufe der Knoten mehr als ein Drittel nach dem letzten Block, sind ihre Gruppen zu ungleich.
  Dann die Gruppen nach ihren Bündeln ausgleichen, ändert die Ergebnisse. Auf der VM 14 bis 15 %.
- **Pakete**: Rechnen die Worker in den offenen Stufen mehr als ein Drittel der Zeit an den übrigen Paketen, während der
  aufrufende Thread mehr als das Doppelte seines Anteils an den Blöcken rechnet, mit 8 Threads mehr als ein Viertel,
  warten die Stufen auf ihn. Dann ein Paket an seinen eigenen Stufen unterbrechen, damit sein Worker in eine offene
  Stufe des Teams kommt und danach weiterrechnet, bitgleich, denn die Körper eines Pakets hängen von keinem anderen ab.
  Auf der VM 18 bis 19 % und 37 bis 38 % der Blöcke, das 1,5-Fache.
- **Allein**: Rechnet der aufrufende Thread zwischen den Stufen, in den Stufen mit einem Block und nach der letzten
  Stufe zusammen mehr als ein Viertel der Zeit des Teams allein, dann am größten dieser Teile weiter: den Überlauf
  außerhalb der Gruppen in die Gruppen geben oder die letzten Pakete früher verteilen. Auf der VM 15 %, davon 9 % nach
  der letzten Stufe.
- **Nichts davon**: Dann verteilt das Team seine Arbeit, und das Warten der Worker liegt nicht an den Stufen. Als
  Nächstes kommt Schritt 32, der Aufbau des Lösers.

Auf dem Ryzen nachgemessen am 24. Oktober, Schritt 34, siehe „Skalierung auf dem Ryzen mit `kollaps-8`“ in
[Optimierungen.md](Optimierungen.md): Mit 8 Threads dauert das Team 1,079 ms pro gemessenem Schritt. In den großen
Farben warten die Threads zusammen 19 %, die kleinen Farben kosten 3,9 % des Teams, die Stufe der Knoten wartet 20 %
nach dem letzten Block, die Worker rechnen 18 % an den übrigen Paketen, und der aufrufende Thread rechnet 20 % der
Blöcke. Allein rechnet er aber 35 % der Zeit des Teams, 26 % davon nach der letzten Stufe, also tritt nur „Allein“ ein.
Was dort geschieht, zeigt auf der VM eine Kopie mit einer Uhr an jedem Paket, siehe „Was nach der letzten Stufe bleibt“:
Der aufrufende Thread rechnet nach der letzten Stufe kein Paket mehr. Er wartet auf ein einziges, das Paket einer
zweiten großen Insel, das ein Worker gleich nach dem Start des Teams genommen hat. Die Pakete früher zu verteilen, hilft
also nicht, das Paket ist zu groß für einen Thread. Mit 4 Threads geschieht das in einem Drittel der Schritte mit Team,
in denen die zweite Insel 40 % der Arbeit trägt, und kostet 7,9 % der Zeit des Teams. Mit 8 Threads ist der Anteil eines
Threads ein Achtel, und mehr hält die zweite Insel in der Hälfte der Schritte mit Team. Daraus Schritt 35: Das Team
rechnet jedes Paket mit, das mehr als den Anteil eines Threads hält. Es bekommt dafür keine Stufe mehr, nur mehr Blöcke
in seinen Stufen. Mit 4 und 8 Threads kostete ein Schritt auf dem Ryzen diesmal 23 und 19 % mehr als mit `kollaps-7`,
auch in den Stufen, an denen sich nichts geändert hat. Wahrscheinlich lief auf dem PC etwas nebenher, für die Regeln
zählen die Anteile.

Auf `kollaps-9` ist Schritt 35 gebaut, siehe „Große Pakete im Team“ in [Optimierungen.md](Optimierungen.md): Mit dem
größten Paket rechnet das Team jedes weitere, das mehr als das Doppelte des Anteils eines Threads hält, in denselben
Stufen, bitgleich. Zuerst nahm es jedes über dem einfachen Anteil. Schritt für Schritt gepaart gegen `kollaps-8`
verschwand auf der VM mit 4 Threads der Rest nach der letzten Stufe, aber das Team kostete in den Schritten mit zweitem
Paket 5,6 % mehr, mit einer zweiten Insel von 25 bis 40 % der Arbeit 8 bis 15 %: Ein Thread im Team rechnet seinen Teil
nur etwas mehr als halb so schnell wie ein Thread allein ein Paket. Mit 4 Threads hält eine zweite Insel nie das
Doppelte, auf der VM ändert sich so nichts, gepaart gegen `kollaps-8` kostet ein Schritt beim Abriss mit 1, 2 und 4
Threads 1,3 und 0,7 % mehr und 3,8 % weniger, je zwei Läufe im Wechsel, gleichmäßig über alle Stufen, auch über die, an
denen sich nichts geändert hat, und die Welt endet gleich. Mit 8 Threads ist die Schwelle ein Viertel, beim Abriss in
rund 40 % der Schritte mit Team, und nach dem Rest von 26 % auf dem Ryzen kostet das Team dort geschätzt 15 % weniger, 3
bis 4 % eines Bilds. Auf dem Ryzen nachzumessen, Schritt 36: Bleibt nach der letzten Stufe deutlich weniger als ein
Viertel und dauert das Team kürzer, bleibt es. Dauert es länger, ist die Schwelle zu niedrig, dann eine höhere messen
oder Schritt 35 zurücknehmen. Am klarsten zeigt das ein Lauf mit `kollaps-8` und einer mit `kollaps-9` direkt
nacheinander.

Auf dem Ryzen nachgemessen am 24. Oktober, Schritt 36, `kollaps-8` und `kollaps-9` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-9`“ in [Optimierungen.md](Optimierungen.md): Mit 8 Threads kosten die Bedingungen
19 % weniger, ein Schritt 8 % und ein Bild 2,62 statt 2,78 ms, 6 % weniger, mehr als geschätzt. Das Team nimmt dort in
rund 41 % seiner Schritte weitere Pakete mit, nach der letzten Stufe bleibt so gut wie nichts, und jeder Worker wartet
34 statt 47 % der Zeit des Teams. Mit 1, 2 und 4 Threads ändert sich nichts. Der neue Lauf mit `kollaps-8` kostet mit 8
Threads so viel wie `kollaps-7`: Die 19 bis 23 % mehr im ersten Lauf lagen am PC. Nach den Regeln oben warten die
Threads in den großen Farben 22 %, die kleinen Farben kosten 6,6 % des Teams, die Stufe der Knoten wartet 25 %, die
Worker rechnen 11 % an den übrigen Paketen bei 17 % der Blöcke auf dem aufrufenden Thread, und allein rechnet er 13 %
der Zeit des Teams. Keine Regel tritt mehr ein, das Team verteilt seine Arbeit. Als Nächstes kommt deshalb Schritt 32,
der Aufbau des Lösers: mit 8 Threads 0,127 ms pro Schritt auf dem aufrufenden Thread allein, 6 % eines Schritts. Zuerst
ist zu messen, welche Teile das sind.

Auf `kollaps-10` ist Schritt 32 gebaut, siehe „Aufbau des Lösers beim Abriss“ in [Optimierungen.md](Optimierungen.md).
Zuerst misst Box3D die Teile des Aufbaus: Beim Abriss auf der VM mit 4 Threads kosten die Knoten 0,17 von 0,33 ms, die
Pakete der Körper und Kontakte 0,11 ms und das Ordnen der Kontakte 0,04 ms. Das Verteilen der Inseln auf die Pakete
kostet kaum etwas. Die Knoten brauchen die Pakete ihrer Kontakte erst am Ende, deshalb rechnen jetzt zwei Aufgaben auf
den Workern die Pakete und das Ordnen, während der aufrufende Thread die Knoten baut, sobald der Überlauf mindestens 256
Kontakte hat. Mit weniger sind die Knoten zu schnell gebaut, um die Aufgaben zu überdecken, und der aufrufende Thread
rechnet sie gleich selbst. Gepaart gegen die Runde davor kostet der Aufbau auf der VM beim Abriss mit 2 und 4 Threads
rund ein Drittel weniger, 0,20 statt 0,31 bis 0,33 ms, und im Benchmark in den Städten mit vielen Knoten 8 bis 13 %, in
der Stadt bei ×2 mit wenigen gleich viel. Die Ergebnisse bleiben bitgleich. Mit 8 Threads auf dem Ryzen dürfte der
Aufbau von 0,127 auf rund 0,08 ms fallen, knapp 2 % eines Bilds. Auf dem Ryzen nachzumessen, Schritt 37: Kostet der
Aufbau dort mit 8 Threads deutlich weniger und bleiben die übrigen Stufen gleich, bleibt es.

Auf dem Ryzen nachgemessen am 24. Oktober, Schritt 37, siehe „Skalierung auf dem Ryzen mit `kollaps-10`“ in
[Optimierungen.md](Optimierungen.md): Der Aufbau des Lösers kostet mit 8 Threads 0,098 statt 0,127 ms, 23 % weniger, mit
2 und 4 Threads 28 und 27 %. Die Aufgaben der Worker sind ganz verdeckt, der aufrufende Thread wartet nie auf sie, und
die Knoten bestimmen mit 0,087 ms allein, wie lange der Aufbau dauert. Ein Schritt kostet mit `kollaps-10` und
`kollaps-9` im Rahmen der Schwankungen gleich viel: Die 0,03 ms sind gut 1 % eines Schritts, und auch Stufen, an denen
sich nichts geändert hat, schwanken zwischen zwei Läufen um bis zu 4 %. Ein erster Lauf mit `kollaps-10` direkt nach
`kollaps-9` war mit 4 und 8 Threads in allen Stufen außer dem Aufbau 12 bis 28 % langsamer, auch in denen, an denen sich
nichts geändert hat. In umgekehrter Reihenfolge liegt `kollaps-10` bei `kollaps-9`, der erste Lauf lag am PC. Nach den
Regeln oben tritt in der Tabelle des Teams weiter keine ein. In der Liste der Schritte steht nur noch 33, mit 8 Threads
bleibt aber die Hälfte eines Schritts auf einem Thread, am meisten in der Kontakt-Schleife und im Löser.

Auf `kollaps-11` ist Schritt 38 gebaut, siehe „Kontakt-Schleife zerlegt“ in [Optimierungen.md](Optimierungen.md): Box3D
misst in jedem Schritt, was der aufrufende Thread vor und nach der parallelen Schleife allein rechnet, und in jedem
achten, wie die Threads die Schleife verbringen: in Blöcken, davon in der Entscheidung über jeden Kontakt und im Test
der übrigen, vor ihrem ersten Block, zwischen den Blöcken und nach ihrem letzten. Der Bericht von „Skalierung messen“
zeigt das als Tabelle für jede Zahl von Threads. Beim Abriss auf der VM bei ×2 mit 4 Threads kostet die Kontakt-Schleife
3,5 ms pro Schritt, davon Sammeln und Zusammenführen 0,07 ms und die Zustandswechsel 0,24 ms. In der Schleife rechnen
die Threads 93,3 % ihrer Zeit in Blöcken, 1,9 % warten sie vor ihrem ersten Block, 4,6 % nach ihrem letzten, und ein
Kontakt kostet mit 4 Threads 279 ns, mit einem 276 ns. Was bremst, ist also vor allem das Ende: Die teuersten Kontakte
liegen an beiden Enden des Arrays, vorne die Kontakte zwischen zwei bewegten Körpern, hinten die neuen Paare, und von
vorne nach hinten kamen zuletzt die teuren von hinten. Daraus Schritt 39: Die Threads nehmen die Blöcke abwechselnd von
vorne und von hinten. Beim Abriss warten sie nach ihrem letzten Block damit 56 % kürzer und rechnen 96,2 % ihrer Zeit in
Blöcken, die Ergebnisse bleiben bitgleich. Nach diesen Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen,
Schritt 40:

- **Anlauf**: Warten die Threads vor ihrem ersten Block mehr als 5 % ihrer Zeit, laufen Worker zu spät an. Dann ansehen,
  was sie aufhält, etwa die Aufgabe, die den Baum der Formen neu baut und einen Worker belegt, während die Schleife
  beginnt. Auf der VM 1,5 %.
- **Ende**: Warten sie nach ihrem letzten Block trotz Schritt 39 mehr als 5 %, sind die letzten Blöcke noch zu lang.
  Dann die Blöcke der Mitte teilen und die an den Enden dafür größer machen, damit es nicht mehr Anläufe werden. Auf der
  VM 2,2 %.
- **Speicher**: Kostet ein Kontakt in Blöcken mit 8 Threads mehr als 15 % mehr als mit einem, wartet die Schleife mit
  vielen Threads auf den Speicher. Dann den Anlauf jedes Blocks verkürzen, indem die Schleife die ersten Kontakte eines
  Blocks zusammen vorauslädt, und die Daten verkleinern, die die Entscheidung über jeden Kontakt liest. Auf der VM beim
  Abriss 5 %, im Benchmark mit kleinen Blöcken bis 31 %.
- **Seriell**: Kosten Sammeln und Zusammenführen mit 8 Threads mehr als 5 % der Kontakt-Schleife, dann die Bits der
  Worker durch Listen der Kontakte ersetzen, die ihren Zustand wechseln, denn heute leert und vereint der aufrufende
  Thread für jeden Worker Bits für alle Kontakte. Auf der VM 2,2 %.
- **Nichts davon**: Dann skaliert die Kontakt-Schleife, und was von ihr mit 8 Threads auf einem Thread bleibt, sind die
  Zustandswechsel. Als Nächstes kommt dann, was nach der Tabelle der Stufen mit 8 Threads am meisten seriell bleibt.

Auf dem Ryzen nachgemessen am 24. Oktober, Schritt 40, `kollaps-10` und `kollaps-11` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-11`“ in [Optimierungen.md](Optimierungen.md). Der Lauf mit `kollaps-10` war
gestört: Mit 2, 4 und 8 Threads kostete ein Schritt 18 bis 38 % mehr als mit demselben Stand zuvor, in fast allen
Stufen. Verglichen mit den fünf ungestörten Läufen seit `kollaps-8` kostet die Kontakt-Schleife mit 2, 4 und 8 Threads
1,4 bis 2,4 % weniger, und ein Schritt samt Update und Einschlägen kostet mit 8 Threads 2,222 ms, so wenig wie in keinem
Lauf davor. Mit 8 Threads rechnen die Threads in der Kontakt-Schleife 95,7 % ihrer Zeit in Blöcken. Vor ihrem ersten
Block warten sie 2,6 %, mit jeder Zahl von Threads zusammen rund 0,14 ms, wohl der Worker, der in dieser Zeit den Baum
der Formen neu baut, nach ihrem letzten 1,5 %. Ein Kontakt kostet mit 8 Threads 9,7 % mehr als mit einem, und Sammeln
und Zusammenführen sind 1,7 % der Kontakt-Schleife. Also tritt „Nichts davon“ ein: Die Kontakt-Schleife skaliert, mit 8
Threads liegt sie 0,085 ms über einem Achtel ihrer Zeit mit einem Thread, mit den Zustandswechseln 0,14 ms. Am weitesten
über einem Achtel liegen nach der Tabelle der Stufen die Bedingungen des Lösers mit 0,39 ms, der Fit nennt 0,48 ms von
ihnen seriell. In der Tabelle des Teams tritt aber weiter keine Regel ein: Ihr Rest verteilt sich auf das Warten am Ende
der Stufen, 20 % der Zeit der Threads in den offenen Stufen, auf das, was der aufrufende Thread allein rechnet, 13 % der
Zeit des Teams, und auf die Stufe der Knoten, die 26 % nach ihrem letzten Block wartet. Danach kommt die Ruhe-Prüfung im
Update mit 0,14 ms über einem Achtel, der Fit nennt 0,16 ms seriell. Von den großen Stufen skaliert sie am
schlechtesten: Mit 8 Threads braucht sie 31 % ihrer Zeit mit einem Thread, die Bedingungen 27 %, die Kontakt-Schleife
16 %. Und der aufrufende Thread prüft im Mittel der Schritte 30,5 % der bewegten Trümmer statt eines Achtels, wobei
kleine Schritte mit wenigen Blöcken so viel zählen wie große. Was davon die Prüfungen kosten und was das Zusammensetzen
und Erstarren danach auf dem aufrufenden Thread, zeigt der Bericht nicht. Daraus Schritt 41: die Ruhe-Prüfung zerlegen
wie die Kontakt-Schleife.

Auf `kollaps-12` ist Schritt 41 gebaut, siehe „Ruhe-Prüfung zerlegt“ in [Optimierungen.md](Optimierungen.md): Nebenan
misst in jedem Update, was der aufrufende Thread vor und nach den Prüfungen allein rechnet, das Sammeln der bewegten
Trümmer, die Suche nach dem, was die ruhigen Stücke trägt, und das Erstarren, und in jedem achten, wie die Threads die
Prüfungen verbringen: in Blöcken, davon in der Frage, ob ein Stück ruhig ist, und im Blick auf das, was ein ruhiges
Stück trägt, vor ihrem ersten Block, zwischen den Blöcken und nach ihrem letzten. Der Bericht von „Skalierung messen“
zeigt das als Tabelle für jede Zahl von Threads. Beim Abriss auf der VM bei ×2 kostet die Ruhe-Prüfung mit 4 Threads
1,29 ms pro Update, davon die Prüfungen 1,07 ms und das Erstarren 0,17 ms. In den Prüfungen geht die Zeit fast ganz in
die 556 ruhigen von 3 097 bewegten Stücken, mit einem Thread 4,3 µs je ruhiges Stück und 0,11 µs je bewegtes, obwohl nur
13 pro Schritt erstarren. Mit 4 Threads rechnen die Threads 80,7 % ihrer Zeit in Blöcken, 6,9 % warten sie vor ihrem
ersten Block, auf der VM laufen die Worker erst nach rund 70 µs an, und 12,3 % nach ihrem letzten. Was am Ende wartet,
sind vor allem große ruhige Stücke, im Mittel 98 µs, bis zu Millisekunden, siehe „Große Stücke in der Ruhe-Prüfung“.
Daraus Schritt 42: Mit Workern nehmen die Threads die Blöcke mit einem großen Stück zuerst. Beim Abriss warten sie nach
ihrem letzten Block damit 65 % kürzer, 4,4 % ihrer Zeit, die Prüfungen kosten 4,7 % weniger, und die Ergebnisse bleiben
bitgleich. Nach diesen Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen, Schritt 43:

- **Anlauf**: Warten die Threads vor ihrem ersten Block mehr als 10 % ihrer Zeit, laufen die Worker zu spät an. Dann die
  Aufgaben der Prüfungen schon vor dem Lösen des gehaltenen Schutts verteilen, sodass die Worker wach sind, wenn die
  Liste der bewegten Trümmer steht. Eine Aufgabe, die ein Task-System gleich selbst ausführt, darf dabei nicht auf die
  Liste warten. Auf der VM 7,5 %.
- **Ende**: Warten sie nach ihrem letzten Block trotz Schritt 42 mehr als 5 %, halten Haufen ruhiger Stücke die
  Prüfungen noch auf. Dann den Blick auf das, was die ruhigen Stücke trägt, in kleineren Teilen verteilen als die Frage,
  ob sie ruhig sind. Auf der VM 4,4 %.
- **Speicher**: Kostet der Blick auf das, was ein ruhiges Stück trägt, mit 8 Threads mehr als 20 % mehr als mit einem,
  wartet er auf den Speicher. Dann in diesem Blick die Kontakte des nächsten ruhigen Stücks vorausladen. Auf der VM mit
  4 Threads 33 %.
- **Seriell**: Kosten Sammeln, Suche und Erstarren mit 8 Threads zusammen mehr als ein Drittel der Ruhe-Prüfung, hängt
  sie am aufrufenden Thread. Dann das Erstarren billiger machen, das den größten Teil davon trägt. Auf der VM mit 4
  Threads 18 %.
- **Nichts davon**: Dann skaliert die Ruhe-Prüfung. Als Nächstes kommt, was nach der Tabelle der Stufen mit 8 Threads am
  meisten seriell bleibt.

Auf dem Ryzen nachgemessen am 25. Oktober, Schritt 43, `kollaps-11` und `kollaps-12` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-12`“ in [Optimierungen.md](Optimierungen.md). Der Lauf mit `kollaps-11` war
gestört: Schon mit einem Thread kostete ein Schritt 7,8 % mehr als mit demselben Stand am Tag davor, in fast allen
Stufen. Verglichen mit den sechs ungestörten Läufen seit `kollaps-8` kostet ein Schritt mit `kollaps-12` im Rahmen der
Schwankungen gleich viel, die Ruhe-Prüfung mit 4 und 8 Threads 2 bis 3 % weniger und mit 2 Threads 7 % mehr. Die
vorhergesagten 15 % mit 8 Threads bringt Schritt 42 nicht. Mit 8 Threads rechnen die Threads der Prüfungen 72,1 % ihrer
Zeit in Blöcken, 8,3 % warten sie vor ihrem ersten Block und 19,4 % nach ihrem letzten. Der Blick auf das, was ein
ruhiges Stück trägt, kostet 26,5 % mehr als mit einem Thread, und Sammeln, Suche und Erstarren sind 31,5 % der
Ruhe-Prüfung. Also treten „Ende“ und „Speicher“ ein, „Anlauf“ und „Seriell“ nicht. Mit 8 Threads liegen die Prüfungen
0,068 ms über einem Achtel ihrer Zeit mit einem Thread, davon 0,032 ms am Ende, 0,023 ms, weil ein Stück mehr kostet,
und 0,014 ms vor dem ersten Block.

Die Simulation aus Schritt 42 trifft den Ryzen mit 2 und 4 Threads fast genau und sagt mit 8 Threads 14,1 % nach dem
letzten Block voraus, gemessen sind 19,4 %. Was „Ende“ vorschlägt, den Blick in kleineren Teilen zu verteilen, brächte
danach wenig: In zwei Runden, erst alle Fragen, ob ein Stück ruhig ist, dann jeder Blick für sich, die teuersten zuerst,
dauerten die Prüfungen mit 8 Threads 6,9 % kürzer, und keine Verteilung kommt unter 13,8 % über ideal, vor allem, weil
die Worker später anlaufen. Ein billigerer Blick brächte mehr, und zwar mit jeder Zahl von Threads: Kleine Stücke um ein
Fünftel billiger sparten mit 8 Threads 10 % der Prüfungen und mit einem Thread 16 %, große halb so teuer mit 8 Threads
ebenfalls 10 %.

Wohin der Blick geht, zählt auf der VM eine Kopie, siehe „Wohin der Blick auf ein ruhiges Stück geht“ in
[Optimierungen.md](Optimierungen.md): Bei einem kleinen Stück gehen 63 % in `b3Body_GetContactData`, das die ganze Liste
der Kontakte des Körpers abläuft, im Mittel 28 Einträge für 6 berührende, jeder an einer anderen Stelle im Speicher, bei
einem großen 46 %, 439 Einträge für 90. Die Suche durch den Schutt unter einem großen Stück kostet nur 4 %. In einer
zweiten Kopie läuft Box3D die Listen aller ruhigen Stücke eines Blocks nebeneinander ab: Ein Eintrag kostet 61 statt 184
Takte, die Blicke zusammen 37,5 % weniger, bitgleich, und die Simulation sagt für die Prüfungen mit 1 bis 8 Threads 29
bis 33 % weniger voraus. Daraus Schritt 44: die Listen der Kontakte der ruhigen Stücke nebeneinander ablaufen, wie es
„Speicher“ vorschlug, nur für alle Stücke eines Blocks statt für das nächste, und jeden berührenden Kontakt nur einmal
nachschlagen.

Auf `kollaps-13` ist Schritt 44 gebaut, siehe „Kontaktlisten der ruhigen Stücke nebeneinander“ in
[Optimierungen.md](Optimierungen.md) und „Kontakte mehrerer Körper nebeneinander“ in
[`extern/README.md`](../extern/README.md). Beim Abriss auf der VM bei ×2 kostet der Blick auf ein ruhiges Stück mit 1, 2
und 4 Threads 48 bis 49 % weniger, mehr als im Nachbau, denn dazu schlägt Nebenan jeden berührenden Kontakt nur einmal
nach. Die Prüfungen kosten 40 bis 42 % weniger, die Ruhe-Prüfung 33 bis 39 %, ein Schritt 4,2 bis 6,0 %, und die Welt
endet bitgleich. Mit 4 Threads verschiebt sich, was bleibt: Die Threads rechnen 81,1 statt 88,9 % ihrer Zeit in Blöcken,
11,4 % warten sie vor ihrem ersten Block und 7,2 % nach ihrem letzten, ein Stück kostet im Blick 20 % mehr als mit einem
Thread, und Sammeln, Suche und Erstarren sind 27 statt 18 % der Ruhe-Prüfung. Auf den Ryzen übertragen sollten die
Prüfungen mit 8 Threads rund 0,10 statt 0,163 ms pro Update dauern und mit einem Thread rund 0,44 statt 0,76 ms. Nach
diesen Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen, Schritt 45. Treten mehrere ein, kommt zuerst die mit
der meisten Zeit pro Schritt dahinter:

- **Seriell**: Kosten Sammeln, Suche und Erstarren mit 8 Threads zusammen mehr als ein Drittel der Ruhe-Prüfung, hängt
  sie jetzt am aufrufenden Thread. Dann messen, wohin die Zeit des Erstarrens geht, und den größten Teil davon billiger
  machen oder auf die Worker legen. Auf der VM mit 4 Threads 27 %.
- **Anlauf**: Warten die Threads vor ihrem ersten Block mehr als 15 % ihrer Zeit, laufen die Worker zu spät an. Dann die
  Aufgaben der Prüfungen früher verteilen, sodass die Worker wach sind, wenn die Liste der bewegten Trümmer steht. Auf
  der VM mit 4 Threads 11,4 %, dort laufen die Worker erst nach rund 80 µs an.
- **Ende**: Warten sie nach ihrem letzten Block mehr als 10 %, dann in zwei Runden prüfen: erst alle Fragen, ob ein
  Stück ruhig ist, dann die Blicke einzeln, die Stücke mit den längsten Listen zuerst, nach der Simulation aus Schritt
  42 mit 8 Threads 7 % der Prüfungen. Auf der VM mit 4 Threads 7,2 %.
- **Speicher**: Kostet der Blick auf ein ruhiges Stück mit 8 Threads mehr als 25 % mehr als mit einem, wartet er weiter
  auf den Speicher. Dann beim Nachschlagen der Kontakte die Trümmerteile auf der anderen Seite vorausladen. Auf der VM
  mit 4 Threads 20 %.
- **Nichts davon**: Dann skaliert die Ruhe-Prüfung. Als Nächstes kommt, was nach der Tabelle der Stufen mit 8 Threads am
  meisten seriell bleibt.

Auf dem Ryzen nachgemessen am 25. Oktober, Schritt 45, `kollaps-12` und `kollaps-13` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-13`“ in [Optimierungen.md](Optimierungen.md). Mit Workern waren beide Läufe 15
bis 18 % langsamer als `kollaps-12` am Morgen, mit einem Thread gleich schnell, auch in Stufen, die seit `kollaps-11`
keine Runde geändert hat: Auf dem PC lief etwas mit. Verglichen wird deshalb nur innerhalb des Durchgangs. Die Prüfungen
kosten mit `kollaps-13` mit 1, 4 und 8 Threads 27, 7 und 7 % weniger, die Ruhe-Prüfung 25, 7 und 6 %, ein Schritt mit
einem Thread 2 %, mit 4 und 8 Threads gleich viel. Der Blick auf ein ruhiges Stück kostet auf dem Ryzen 16 bis 31 %
weniger, auf der VM 48 bis 49 %, und mit 8 Threads dauern die Prüfungen 0,168 statt der vorhergesagten 0,10 ms. Die
Threads rechnen 64,7 % ihrer Zeit in Blöcken, 9,9 % warten sie vor ihrem ersten Block und 25,2 % nach ihrem letzten, der
Blick kostet 56 % mehr als mit einem Thread, und Sammeln, Suche und Erstarren sind 29,2 % der Ruhe-Prüfung. Also treten
„Ende“ und „Speicher“ ein, „Anlauf“ und „Seriell“ nicht, und „Ende“ hat mit 0,042 ms pro Update mehr Zeit hinter sich
als „Speicher“ mit 0,031 ms.

Was „Ende“ vorschlägt, lohnt aber nicht, siehe „Was die Prüfungen am Ende aufhält“ in
[Optimierungen.md](Optimierungen.md). Der längste Block hält auf der VM 17 % der Arbeit eines Updates, mehr als das
Achtel eines Threads, und 69 % davon gehen in ein einziges großes Stück, das keine Verteilung teilen kann. Zwei Runden,
die längsten Listen zuerst, brächten nach der Simulation, die den Ryzen mit 1 und 4 Threads fast genau trifft, mit 8
Threads nur 1 bis 2 %. Am Ende warten die Threads auf den Blick auf große Stücke: Im Mittel liegen 3,4 pro Update im
Blick, und in 22 % der Updates eines mit Tausenden Einträgen in der Liste, auf dem fast tausend bewegte Stücke liegen,
fast immer dasselbe. Ein Blick darauf kostet auf der VM 353 µs und das Ablaufen der Listen seines Blocks 400 µs. Gut ein
Viertel des Blicks geht ins Sortieren und Durchsuchen der Stücke, die darauf liegen, 15 % ins Nachschlagen der anderen
Seite. Daraus Schritt 46: den Blick auf große Stücke billiger machen, mit dem Vorausladen der anderen Seite, das
„Speicher“ vorschlägt, und lange Listen über die Listen ihrer Formen vorausladen. Die Simulation sagt dafür mit 8
Threads 11 bis 21 % kürzere Prüfungen voraus, auf dem Ryzen rund 0,02 bis 0,035 ms pro Schritt, und mit einem Thread 5
bis 8 %.

Auf `kollaps-14` ist Schritt 46 gebaut, siehe „Blick auf große Stücke billiger“ in [Optimierungen.md](Optimierungen.md)
und „Lange Kontaktlisten über ihre Formen“ in [`extern/README.md`](../extern/README.md). Beim Abriss auf der VM bei ×2
kosten die Prüfungen mit 1, 2 und 4 Threads 7,5, 10,1 und 14,4 % weniger, der Blick auf ein ruhiges Stück 10 bis 13 %,
der längste Block dauert 24 bis 32 % kürzer, und mit 4 Threads warten die Threads nach ihrem letzten Block 68 % kürzer,
2,8 statt 7,4 % ihrer Zeit. Die Welt endet bitgleich. Dass die Fragen, ob ein Stück ruhig ist, Cache-Zeilen schreiben,
die die Blicke der anderen Threads lesen, erklärt nicht, was ein Blick mit mehr Threads mehr kostet: Den Ruhezustand
hinter die Flags zu legen, brachte mit 4 Threads nichts. Auf den Ryzen übertragen sollten die Prüfungen mit 8 Threads
rund 0,14 statt 0,168 ms pro Update dauern und mit einem Thread rund 0,51 statt 0,548 ms. Am 25. Oktober waren mit
Workern beide Läufe auf dem Ryzen 15 bis 18 % langsamer als am Morgen, gemessen wird deshalb mit möglichst wenig anderem
auf dem PC. Nach diesen Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen, Schritt 47. Treten mehrere ein,
kommt zuerst die mit der meisten Zeit pro Schritt dahinter:

- **Ende**: Warten die Threads nach ihrem letzten Block weiter mehr als 15 % ihrer Zeit, hält ein einzelnes großes Stück
  die Prüfungen noch auf. Dann messen, wohin der Blick auf die größten Stücke jetzt geht, und den größten Teil davon
  billiger machen. Auf der VM mit 4 Threads 2,8 %.
- **Speicher**: Kostet der Blick auf ein ruhiges Stück mit 8 Threads mehr als 40 % mehr als mit einem, dann messen,
  welche Lesezugriffe des Blicks mit mehr Threads teurer werden, die Kontakte und Körper in Box3D oder die Trümmerteile
  und Bruchstücke in Nebenan. Auf der VM mit 4 Threads 21 %.
- **Nichts davon**: Dann ist in der Ruhe-Prüfung wenig mehr zu holen. Als Nächstes kommt, was mit 8 Threads am weitesten
  über einem Achtel seiner Zeit mit einem Thread liegt, nach Schritt 45 die Bedingungen des Lösers mit 0,52 ms und die
  Kontakt-Schleife mit 0,26 ms.

Auf dem Ryzen nachgemessen am 25. Oktober, Schritt 47, `kollaps-13` und `kollaps-14` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-14`“ in [Optimierungen.md](Optimierungen.md). Der Durchgang war ungestört: Mit
`kollaps-13` kostete ein Schritt samt Update und Einschlägen mit 2, 4 und 8 Threads 29, 19 und 15 % weniger als im
gestörten Durchgang davor. Die Prüfungen kosten mit `kollaps-14` mit 1, 2, 4 und 8 Threads 2,3, 3,5, 3,7 und 18,0 %
weniger, die Ruhe-Prüfung 1,7 bis 2,2 und 12,2 %, und mit 8 Threads dauern sie 0,109 statt 0,133 ms. Vorhergesagt waren
mit 8 Threads 17 % und mit einem Thread 7 %. Mit einem Thread geht der ganze Gewinn in den längsten Block, die kleinen
Blicke werden auf dem Ryzen erst mit mehreren Threads billiger: Mit 8 Threads kostet ein Blick 12,5 % weniger und nur
noch 12 statt 25 % mehr als mit einem Thread. Die Threads rechnen 67,0 % ihrer Zeit in Blöcken, 12,2 % warten sie vor
ihrem ersten Block und 20,4 % nach ihrem letzten. Also tritt „Ende“ ein, mit 0,022 ms pro Update dahinter, „Speicher“
nicht.

Gemessen, wohin der Blick auf die größten Stücke jetzt geht, siehe „Wohin der Blick auf große Stücke jetzt geht“ in
[Optimierungen.md](Optimierungen.md): Ein Blick auf das Stück mit Tausenden Einträgen in der Liste kostet auf der VM 203
statt 353 µs, und kein Teil davon herrscht mehr vor, der größte, das Nachschlagen der anderen Seite, hält 23 %. Mehr als
der Blick kostet das Ablaufen seiner Liste in Box3D, 319 µs für seinen Block, und von 4 610 Einträgen berühren nur
1 094. Nach der Simulation, die den Ryzen mit 8 Threads trifft, brächte ein halb so teurer Blick auf diese Stücke 6 %
der Prüfungen, ein halb so teures Ablaufen ihrer Listen 8 %, beides 11 % und ohne sie ganz 14 %. Was am Ende danach noch
wartet, liegt an der Größe der Blöcke. Groß ist das Ablaufen der Listen aber bei allen Stücken: Es kostet 35 % der
Prüfungen, und 79 % der Einträge sind Kontakte, die nicht berühren. Liefe Box3D nur die berührenden ab, kostete das
Ablaufen nach einem Fit über die Blöcke noch 47 %, und die Prüfungen auf dem Ryzen nach der Simulation mit 8 Threads
14 % und mit einem Thread 18 % weniger, rund 0,016 und 0,09 ms pro Update. Dagegen steht die Pflege einer Liste der
berührenden Kontakte in der seriellen Schleife der Zustandswechsel, pro Schritt für rund 440 Kontakte, die zu berühren
beginnen oder aufhören. Daraus Schritt 48. Sammeln, Suche und Erstarren sind mit 8 Threads 38 % der Ruhe-Prüfung,
0,066 ms, davon 0,045 ms das Erstarren mit den Typwechseln der Stücke, die erstarren. Mit 8 Threads über einem Achtel
ihrer Zeit mit einem Thread liegen danach vor allem die Bedingungen des Lösers, mit 0,375 ms, die Kontakt-Schleife mit
0,085 ms und die Prüfungen nur noch mit 0,042 ms.

Auf `kollaps-15` ist Schritt 48 gebaut, siehe „Nur berührende Kontakte ablaufen“ in [Optimierungen.md](Optimierungen.md)
und „Listen der berührenden Kontakte“ in [`extern/README.md`](../extern/README.md). Beim Abriss auf der VM bei ×2 kostet
der Blick auf ein ruhiges Stück mit 1, 2 und 4 Threads 20 bis 24 % weniger, die Prüfungen 20, 16 und 13 %, und der
längste Block dauert 17 bis 23 % kürzer, so viel, wie die Simulation erwarten ließ. Die Pflege der Listen kostet in der
seriellen Schleife der Zustandswechsel 0,014 bis 0,024 ms pro Schritt, 7 bis 12 % von `contactStates`, und in den
Typwechseln und im Erstarren bis 0,011 ms. Ein Schritt spart damit 0,27, 0,12 und 0,04 ms. In der Stadt bei ×4 berühren
64 % der Einträge, dort spart der Blick nichts, und die Pflege kostet 0,02 bis 0,1 ms pro Schritt. Die Welt endet
bitgleich. Auf den Ryzen übertragen sollten die Prüfungen mit 8 Threads rund 0,093 statt 0,109 ms pro Update dauern und
mit einem Thread rund 0,42 statt 0,519 ms. `contactStates`, die Typwechsel und das Erstarren brauchen auf dem Ryzen ein
Viertel der Zeit der VM, die Pflege sollte dort mit 8 Threads 0,005 bis 0,01 ms kosten. Nach diesen Regeln entscheidet
die Tabelle mit 8 Threads auf dem Ryzen, Schritt 49. Treten mehrere ein, kommt zuerst die mit der meisten Zeit pro
Schritt dahinter:

- **Pflege**: Kosten `contactStates`, `typeChanges` und `freezeRubble` mit 8 Threads zusammen mehr als 0,01 ms mehr als
  mit `kollaps-14`, frisst die Pflege der Listen den größten Teil dessen, was die Prüfungen sparen. Dann messen, worauf
  sie wartet, und sie billiger machen. Auf der VM mit 4 Threads 0,035 ms.
- **Ende**: Warten die Threads nach ihrem letzten Block weiter mehr als 15 % ihrer Zeit, liegt es an der Größe der
  Blöcke. Dann die Blöcke so schneiden, dass am Ende kein großes Stück allein rechnet. Auf der VM mit 4 Threads 5,9 %.
- **Nichts davon**: Dann ist in der Ruhe-Prüfung wenig mehr zu holen. Als Nächstes kommen die Bedingungen des Lösers,
  die mit 8 Threads 0,375 ms über einem Achtel ihrer Zeit mit einem Thread liegen.

Auf dem Ryzen nachgemessen am 26. Oktober, Schritt 49, `kollaps-14` und `kollaps-15` direkt nacheinander, siehe
„Skalierung auf dem Ryzen mit `kollaps-15`“ in [Optimierungen.md](Optimierungen.md). Der Durchgang war so ruhig wie der
vom 25. Oktober. Die Prüfungen kosten mit `kollaps-15` mit 1, 2, 4 und 8 Threads 11,2, 8,6, 7,9 und 12,0 % weniger, mit
8 Threads 0,095 statt 0,108 ms, vorhergesagt waren 0,093 ms. Mit einem Thread waren 19 % vorhergesagt, der Blick auf ein
ruhiges Stück spart auf dem Ryzen aber nur 10 bis 15 %, auf der VM 20 bis 24 %. Die Pflege kostet mit 8 Threads in den
Zustandswechseln, den Typwechseln und im Erstarren 0,009 ms pro Schritt, wie vorhergesagt, und dazu, was die Regel nicht
nennt, beim Anlegen der Kontakte mit allen Threads 0,003 bis 0,004 ms: Box3D schreibt dort jetzt für jeden neuen Kontakt
seinen Eintrag. Update und Pflege zusammen sparen so mit 1, 2 und 4 Threads 0,059, 0,014 und 0,007 ms pro Schritt und
mit 8 Threads nichts. Mit 8 Threads rechnen die Threads 69,2 % ihrer Zeit in Blöcken, 13,8 % warten sie vor ihrem ersten
Block und 16,7 % nach ihrem letzten, mit 4 Threads nur 1,4 %. Also tritt „Ende“ ein, mit 0,016 ms pro Update dahinter,
„Pflege“ mit 0,009 ms knapp nicht. Mit dem Anlegen der Kontakte träte sie mit 0,013 ms ein, „Ende“ käme aber mit mehr
Zeit dahinter zuerst. Die Pflege ist unter „Kleinere Punkte“ vorgemerkt.

Daraus Schritt 50. Mit 8 Threads dauert der längste Block 0,062 ms, fast so lange wie ein Achtel aller Blöcke, 0,066 ms:
Wer ihn nimmt, rechnet daneben kaum etwas, und nimmt ihn ein Thread spät, warten die anderen auf ihn. Die Blöcke mit
großen Stücken kommen schon zuerst dran, aber jeder Block hält 64 bewegte Stücke, und was er kostet, hängt an seinen
ruhigen Stücken und ihren Kontakten. Die VM hat nur vier Kerne, dort warten die Threads am Ende kaum. Wie für die
Schritte 46 und 48 schreibt deshalb eine Kopie auf der VM mit einem Thread die Zeit jedes Blocks, und die Simulation,
die den Ryzen mit 8 Threads trifft, prüft die Schnitte. Was dabei herauskommt, entscheidet so:

- **Schneiden**: Verkürzt ein Schnitt nach der Simulation die Prüfungen mit 8 Threads um mindestens 0,008 ms pro Update,
  die Hälfte dessen, was am Ende wartet, wird der beste gebaut, bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst ist in der Ruhe-Prüfung wenig mehr zu holen. Als Nächstes kommen die Bedingungen des Lösers,
  die mit 8 Threads rund 0,38 ms über einem Achtel ihrer Zeit mit einem Thread liegen.

Auf `kollaps-16` ist Schritt 50 gemessen und simuliert, siehe „Die Blöcke der Prüfungen schneiden“ in
[Optimierungen.md](Optimierungen.md). Gehen die Zeiten der VM pro Stück auf den Ryzen über, sodass teure Stücke dort
verhältnismäßig weniger kosten, trifft die Simulation `kollaps-15` auf dem Ryzen mit 8 Threads: 0,094 ms für die
Prüfungen, gemessen 0,095 ms, und 0,124 ms nach dem letzten Block, gemessen 0,127 ms. Kleinere Blöcke kosten zusammen
3,5 bis 7,6 % mehr, denn Box3D läuft dann weniger Listen nebeneinander ab. Damit verkürzt kein Schnitt die Prüfungen mit
8 Threads um mehr als 0,002 ms pro Update, weder kleinere Blöcke überall noch nur am Ende noch nur um große Stücke.
Selbst ohne jedes Warten am Ende wären es nur 0,006 ms, nicht die 0,016 ms, die Schritt 50 nannte: In 23 % der Updates
dauert der längste Block allein länger als ein Achtel der Arbeit samt der Anläufe, und dort hilft keine Verteilung.
„Schneiden“ tritt nicht ein, also „Nichts davon“: In der Ruhe-Prüfung ist mit 8 Threads wenig mehr zu holen.

Daraus Schritt 51, die Bedingungen des Lösers. Sie liegen auf dem Ryzen mit 8 Threads 0,377 ms über einem Achtel ihrer
Zeit mit einem Thread, 18 % eines Schritts, der Überlauf davon 0,100 ms. Die Threads rechnen die Inseln in ganzen
Paketen, das größte zuerst, und ein Team aller Threads rechnet Farbe für Farbe das größte Paket, sobald es mehr als fünf
Viertel des Anteils eines Threads hält, und jedes weitere über dem Doppelten. Auf die letzten Pakete wartet der
aufrufende Thread mit 8 Threads nur 0,036 ms pro Schritt. Wohin der Rest geht, ob ein Thread in den Paketen mit 8
Threads langsamer rechnet als einer allein, ob die Threads auf ein großes Paket oder auf ein Team warten oder ob der
aufrufende Thread davor und danach allein rechnet, misst Schritt 51 zuerst. Danach stehen hier die Regeln, nach denen es
weitergeht.

Auf `kollaps-17` ist Schritt 51 gebaut, siehe „Was eine Einheit Arbeit im Löser kostet“ in
[Optimierungen.md](Optimierungen.md) und „Arbeit des Lösers pro Einheit“ in [`extern/README.md`](../extern/README.md).
Den größten Teil zerlegte schon der Bericht vom Ryzen mit `kollaps-15`: Mit 8 Threads verbringen die Threads einen
Schritt mit Team 5,86 ms lang, 3,21 ms davon in den Blöcken des Teams, 0,61 ms in den anderen Paketen, 1,02 ms nach
ihrem letzten Block am Ende einer Stufe, 0,26 ms vor dem Hineinkommen und 0,77 ms in dem, was der aufrufende Thread
allein rechnet. Neu ist, was eine Einheit Arbeit kostet. Auf der VM braucht ein Thread dafür mit 4 Threads in den
Paketen 1,18-mal und in den Blöcken des Teams 1,39-mal so lange wie ein Thread allein, mit 2 Threads 1,07- und 1,18-mal.
Mit 1,39 auf dem Ryzen steckten von den 3,21 ms in den Blöcken 0,9 ms im langsameren Rechnen, 0,11 ms pro Schritt, so
viel wie das Warten am Ende der Stufen, 0,13 ms, und das, was der aufrufende Thread allein rechnet, 0,10 ms. Nach diesen
Regeln entscheidet die Tabelle mit 8 Threads auf dem Ryzen, Schritt 52. Treten mehrere ein, kommt zuerst die mit der
meisten Zeit pro Schritt dahinter:

- **Langsamer**: Kostet eine Einheit in den Blöcken des Teams mit 8 Threads mehr als 1,3-mal so viel wie die Bedingungen
  mit einem Thread, rechnen sich die Threads im Team gegenseitig langsamer. Dann messen, in welchen Stufen ein Eintrag
  mit 8 Threads am meisten mehr kostet, und dort die Arbeit so verteilen, dass ein Thread von Stufe zu Stufe dieselben
  Körper behält. Dahinter steht, was ein Thread in den Blöcken länger rechnet als ein Thread allein. Auf der VM mit 4
  Threads 1,39.
- **Ende**: Warten die Threads im Team nach ihrem letzten Block mehr als 15 % der Zeit, in der eine Stufe offen ist,
  sind die Blöcke der Stufen zu grob. Dann die Blöcke des Teams feiner schneiden, zuerst in einer Simulation der Stufen.
  Dahinter steht dieses Warten pro Thread. Auf dem Ryzen mit `kollaps-15` 20 %, 0,127 ms pro Schritt.
- **Allein**: Rechnet der aufrufende Thread im Team zwischen den Stufen und in Stufen mit einem Block mehr als 10 % der
  Zeit des Teams allein, dann das auf die Worker verteilen. Dahinter steht diese Zeit. Auf dem Ryzen mit `kollaps-15`
  13 %, 0,094 ms pro Schritt.
- **Nichts davon**: Dann ist im Löser mit diesen Mitteln wenig zu holen. Als Nächstes kommen die Kontakt-Schleife und
  die Zustandswechsel der Kontakte, die mit 8 Threads 0,08 und 0,06 ms über einem Achtel ihrer Zeit mit einem Thread
  liegen.

Auf dem Ryzen nachgemessen am 26. Oktober, Schritt 52, siehe „Skalierung auf dem Ryzen mit `kollaps-17`“ in
[Optimierungen.md](Optimierungen.md). Drei Läufe waren nötig, und keiner war ganz ungestört: Mit 2 und 4 Threads liefen
alle langsamer als am Vortag, der erste Lauf mit `kollaps-17` überall und der zweite auch mit 8 Threads. Sauber sind der
Lauf von `kollaps-16` mit 1 und 8 Threads und der zweite von `kollaps-17` mit einem Thread. Die Stichprobe kostet
nichts. Eine Einheit Arbeit kostet in den Blöcken des Teams mit 8 Threads, mit der Zeit der Blöcke aus dem Lauf von
`kollaps-16` gerechnet, das 1,34- bis 1,39-Fache eines Threads allein, also tritt „Langsamer“ ein, mit 0,102 bis
0,114 ms pro Schritt dahinter. „Ende“ tritt mit 19 % und 0,119 ms ein und „Allein“ mit 13 % und 0,095 ms. Die drei
liegen nah beieinander, „Ende“ kommt knapp zuerst. Daraus Schritt 53. Am Ende der Stufen wartet ein Thread im Mittel
0,47 µs pro Stufe mit Sperre, gut ein Viertel davon in den Stufen der Knoten und zwei Drittel in denen der Farben. Je
weniger Einträge eine Farbe hat, desto mehr wartet er: In den Farben 9 bis 22, mit 3 bis 31 Einträgen pro Stufe, warten
die Threads 29 bis 62 % der offenen Zeit. Box3D schneidet eine Stufe in höchstens vier Blöcke pro Worker mit mindestens
vier breiten Bedingungen, Farbe 0 hat damit 25 Blöcke für 8 Threads, Farbe 12 zwei. Was die Simulation zeigt,
entscheidet so:

- **Schneiden**: Bringt ein Schnitt nach der Simulation mit 8 Threads mindestens 0,04 ms pro Schritt, ein Drittel des
  Wartens am Ende der Stufen, wird der beste gebaut, bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommt „Langsamer“: messen, in welchen Stufen ein Eintrag mit 8 Threads am meisten mehr kostet
  als mit einem, und dort die Arbeit so verteilen, dass ein Thread von Stufe zu Stufe dieselben Körper behält.

Auf `kollaps-18` ist Schritt 53 gemessen und simuliert, siehe „Die Blöcke des Teams schneiden“ in
[Optimierungen.md](Optimierungen.md). Mit 8 Workern trifft die VM die Stufen des Ryzen genau, und die Simulation, an
seine Tabelle angepasst, trifft seine offenen Zeiten auf wenige Prozent. Jeder Thread versucht in einer Stufe jeden
Block einmal mit einem atomaren Vergleichstausch, auch die, die andere längst genommen haben. Ein vergeblicher Versuch
kostet auf dem Ryzen 0,034 bis 0,039 µs, mit 8 Threads sind es 17 600 pro Schritt, 0,60 bis 0,69 ms über alle Threads,
ein Fünftel der Zeit in den Blöcken des Teams. Feinere Blöcke vervielfachen sie: Jeder feinere Schnitt kostet nach der
Simulation 35 bis 352 µs pro Schritt mehr, auf der VM mit 4 Threads kosteten die zwei gemessenen 1,5 und 3,5 % mehr, und
gröbere Blöcke brächten 9 bis 32 µs. „Schneiden“ tritt nicht ein, also „Nichts davon“, und nach der Regel käme
„Langsamer“, die Körper von Stufe zu Stufe auf demselben Thread halten. Die Simulation zeigt aber, dass die vergeblichen
Versuche den größten Teil davon sind: Ohne sie bräuchte ein Thread im Team pro Einheit das 1,05- bis 1,09-Fache eines
Threads allein statt des 1,34-Fachen, und nur um diesen Rest ginge es beim Halten der Körper. Daraus Schritt 54. Liest
ein Thread den Anspruch eines Blocks erst und versucht nur einen freien zu tauschen, kostet ein vergeblicher Versuch auf
der VM 0,025 statt 0,042 µs und das Team 4,2 % weniger. Wird ein Versuch auf dem Ryzen um denselben Anteil billiger,
bringt das nach der Simulation 0,039 bis 0,046 ms pro Schritt, kostete er nichts, 0,092 bis 0,106 ms. Eine eigene
Cache-Zeile für die Ansprüche jedes Workers ist schon einmal verworfen, aber nie sauber gemessen worden, siehe „Eigene
Cache-Zeile für die Ansprüche jedes Workers“ in [Optimierungen.md](Optimierungen.md). Was die Varianten auf der VM
bringen, entscheidet so:

- **Überspringen**: Senkt eine Variante den Preis eines vergeblichen Versuchs auf der VM mit 4 Threads, gemessen wie
  hier an den Enden der Ringe, um mindestens ein Drittel und bringt sie damit nach der Simulation mit 8 Threads
  mindestens 0,03 ms pro Schritt, ein Drittel dessen, was die vergeblichen Versuche kosten, wird die beste gebaut,
  bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommt „Allein“: Der aufrufende Thread rechnet im Team 13 % der Zeit allein, 0,095 ms pro
  Schritt, das auf die Worker verteilen.

Auf `kollaps-19` ist Schritt 54 gemessen und gebaut, siehe „Vergebene Blöcke billiger überspringen“ in
[Optimierungen.md](Optimierungen.md) und „Ansprüche vor dem Tausch lesen“ in [`extern/README.md`](../extern/README.md).
Ein Thread liest den Anspruch eines Blocks jetzt, bevor er ihn per Vergleichstausch zu nehmen versucht. Auf der VM mit 4
Threads kostet ein vergeblicher Versuch damit 0,024 bis 0,025 statt 0,042 bis 0,057 µs. Dichte Ansprüche und eigene
Cache-Zeilen waren teurer, und das Verfahren von Box2D, das am ersten genommenen Block aufhört, spart zwar die meisten
Versuche, lässt die Threads aber am Ende der Stufen länger warten. „Überspringen“ tritt ein. Nach der Simulation kostet
das Team auf dem Ryzen mit 8 Threads damit 39 bis 53 µs weniger pro gemessenem Schritt, und ein Thread braucht in den
Blöcken des Teams pro Einheit das 1,20- bis 1,23-Fache eines Threads allein statt des 1,34-Fachen. Was der Ryzen dazu
sagt, mit `kollaps-18` und `kollaps-19` direkt nacheinander gemessen, entscheidet so:

- **Behalten**: Kostet das Team auf dem aufrufenden Thread mit 8 Threads mit `kollaps-19` höchstens 0,02 ms pro Schritt
  mehr als mit `kollaps-18`, im Rahmen der Schwankungen, bleibt das Lesen. Spart es mindestens 0,02 ms pro Schritt,
  trifft die Simulation auch hier. Als Nächstes kommt „Allein“: Der aufrufende Thread rechnet 13 % der Zeit des Teams
  allein, 0,095 ms pro Schritt, das auf die Worker verteilen.
- **Zurück**: Kostet das Team mit `kollaps-19` mehr als 0,02 ms pro Schritt mehr, wird das Lesen zurückgenommen, und als
  Nächstes kommt ebenso „Allein“.

Auf dem Ryzen nachgemessen am 26. Oktober, Schritt 55, siehe „Skalierung auf dem Ryzen mit `kollaps-19`“ in
[Optimierungen.md](Optimierungen.md). Beide Läufe waren ruhig. Mit 8 Threads kostet das Team auf dem aufrufenden Thread
mit `kollaps-19` 0,022 ms pro gemessenem Schritt weniger als mit `kollaps-18`, also tritt „Behalten“ ein, und das Lesen
bleibt. In den Blöcken des Teams braucht ein Thread pro Einheit, wie die Simulation sagte, das 1,24- statt das
1,33-Fache eines Threads allein. Die Stufen schließen aber nur halb so viel früher wie vorhergesagt, weil die Threads,
die früher durch sind, länger auf den letzten warten. Daraus Schritt 56, „Allein“. Der aufrufende Thread rechnet 13,6 %
der Zeit des Teams allein, 0,096 ms pro gemessenem Schritt. 0,040 ms davon sind 91 Stufen mit einem Block, die Farben
mit bis zu vier Einträgen, und 0,055 ms liegen zwischen den Stufen. Nach den Zeiten des Überlaufs gehen davon gut
0,04 ms auf die rund 25 Kontakte des Überlaufs außerhalb der Knoten, die er in jedem Durchgang allein rechnet, 0,002 ms
auf das Mitteln der Kopien der Knoten, und ein Teil ist die Uhr, die die Stichprobe in jeder Stufe dreimal liest. Was
davon wirklich anfällt und was davon auf die Worker kann, misst Schritt 56 zuerst. Danach entscheidet so:

- **Verteilen**: Lässt sich ein Teil so auf die Worker legen, dass mit 8 Threads nach der Messung auf der VM und einer
  Hochrechnung wie in Schritt 53 mindestens 0,03 ms pro Schritt wegfallen, wird es gebaut, bitgleich, wo es geht, sonst
  deterministisch und begründet, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommen die Teile, die der aufrufende Thread mit 8 Threads außerhalb des Teams allein rechnet,
  0,36 ms pro Schritt, zuerst der Aufbau der Knoten mit 0,086 ms und die Zustandswechsel der Kontakte mit 0,063 ms.

Auf `kollaps-20` ist Schritt 56 gemessen, siehe „Was der aufrufende Thread im Team allein rechnet“ in
[Optimierungen.md](Optimierungen.md). Von den rund 24 Kontakten des Überlaufs außerhalb der Knoten, die der aufrufende
Thread im Team in jedem Durchgang allein rechnet, teilen 97 % einen Körper mit den Bündeln der Knoten oder mit einem
früheren Kontakt, der das tut, und müssen nach den Knoten kommen. Ohne Bedingung verteilbar sind nur ihr Vorbereiten und
Speichern, auf dem Ryzen höchstens 0,014 ms pro Schritt, und die Stufen mit einem Block sind kleine Farben, die
nacheinander kommen. „Verteilen“ tritt nicht ein, also „Nichts davon“. Daraus Schritt 57, der Aufbau der Knoten. Mit 8
Threads baut der aufrufende Thread die Knoten in 0,086 ms pro Schritt allein. Zwei Worker füllen in der Zeit in 0,032 ms
die Pakete und ordnen die Kontakte, die übrigen warten. Nach dem Profil von `kollaps-10` verteilt sich der Aufbau der
Knoten auf rund zehn Durchgänge über den Überlauf, und der teuerste greift zufällig auf die Kontakte zu. Was die Messung
der Durchgänge zeigt, entscheidet so:

- **Verteilen**: Rechnen Durchgänge jeden Kontakt für sich und sparen sie auf die wartenden Worker verteilt mit 8
  Threads nach der Messung auf der VM mindestens 0,03 ms pro Schritt, werden sie verteilt, bitgleich, und auf dem Ryzen
  nachgemessen.
- **Nichts davon**: Sonst kommen die Zustandswechsel der Kontakte, die der aufrufende Thread mit 8 Threads 0,063 ms pro
  Schritt allein rechnet.

Auf `kollaps-21` ist Schritt 57 gemessen, siehe „Die Durchgänge des Aufbaus der Knoten“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×2 mit 4 Threads baut der aufrufende Thread die Knoten auf der VM
in 198 µs pro Schritt. Am teuersten sind das Lesen der Körper aus den verstreuten Kontakten mit 23 %, das Bündeln mit
21 % und das Verteilen auf die Gruppen mit 18 %. Jeden Kontakt für sich rechnen nur das Lesen, das Abtrennen der
Kontakte zwischen zwei Knoten, das Schreiben der Bündel und die Wahl der Knoten bis auf ihre Rangfolge, zusammen 46 %.
Auf den Ryzen übertragen spart das Lesen auf die fünf wartenden Worker verteilt, die nach rund 6 µs anlaufen, 0,011 ms
pro Schritt, alle vier zusammen 0,013 ms und höchstens 0,029 ms. „Verteilen“ tritt nicht ein, also „Nichts davon“.
Daraus Schritt 58, die Zustandswechsel der Kontakte. Mit 8 Threads rechnet sie der aufrufende Thread in 0,063 ms pro
Schritt allein, mit einem Thread in 0,058 ms, und alle Worker warten. Beim Abriss bei ×2 beginnen oder enden pro Schritt
rund 440 Berührungen, dazu löscht Box3D die Paare, deren Boxen sich nicht mehr überlappen. Fast alles daran ändert
gemeinsame Strukturen in fester Reihenfolge: Inseln, die sich vereinen, die Farben des Graphen, die Listen der Körper
und Formen und die Arrays der Sätze, aus denen ein Kontakt verschwindet, indem der letzte seinen Platz nimmt. Seit
`physik-35` und `physik-36` holt die Schleife voraus, was sie anfasst. Was die Messung der Teile zeigt, entscheidet so:

- **Vorziehen**: Rechnen Teile der Wechsel jeden Kontakt für sich und ändern nichts, was ein anderer Wechsel liest, und
  sparen sie, in der Kontakt-Schleife auf den Workern vorbereitet, mit 8 Threads nach der Messung auf der VM mindestens
  0,02 ms pro Schritt, werden sie dorthin gelegt, bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommen die seriellen Durchgänge der Knoten, das Bündeln und das Verteilen auf die Gruppen, mit
  8 Threads rund 0,033 ms pro Schritt, und ob sie bitgleich billiger gehen.

Auf `kollaps-22` ist Schritt 58 gemessen, siehe „Die Zustandswechsel der Kontakte zerlegt“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×2 hat ein Schritt im Mittel 881 Wechsel, 441 gelöschte Paare, 258
Kontakte, die zu berühren beginnen, und 182, die aufhören, und mit 4 Threads kostet die Schleife auf der VM 356 µs pro
Schritt. Jeden Kontakt für sich, ohne etwas zu ändern, was ein anderer Wechsel liest, rechnen nur das Sammeln der Bits
und das Laden der Kontakte und ihrer Formen, 18 % der Schleife, auf dem Ryzen höchstens 0,011 ms pro Schritt, und die
Zeilen der Kontakte braucht die Schleife zum Schreiben ohnehin. Alles andere ändert Paarmenge, Listen, Inseln, Farben,
Sätze und Ids in der Reihenfolge der Bits. „Vorziehen“ tritt nicht ein, also „Nichts davon“. Daraus Schritt 59, die
seriellen Durchgänge der Knoten. Das Ordnen der Kontakte nach Knoten, das Verteilen auf die Gruppen und das Bündeln
kosten nach Schritt 57 45 % des Aufbaus der Knoten, auf dem Ryzen mit 8 Threads rund 0,038 ms pro Schritt und mit einem
Thread 0,036 ms. Das Ordnen geht zweimal über den Überlauf. Das Verteilen auf die Gruppen geht sechsmal über die
Kontakte an Knoten: um ihre Gruppen und die der Partner zu leeren, um sie zuzuteilen, und dann zum Zählen, Verteilen und
Zurückschreiben. Das Bündeln geht einmal über sie, mit mehreren Verzweigungen je Kontakt. Was eine Fassung mit weniger
Durchgängen zeigt, entscheidet so:

- **Billiger**: Kostet sie im selben Lauf auf der VM so viel weniger, dass es auf den Ryzen übertragen mindestens
  0,01 ms pro Schritt sind, und kommen in jedem Schritt dieselben Bündel und Gruppen heraus, wird sie gebaut, bitgleich,
  und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommt das Erstarren des Schutts, das der aufrufende Thread mit 8 Threads 0,047 ms pro Schritt
  allein rechnet, die Typwechsel darin eingeschlossen, zerlegt wie die Zustandswechsel.

Auf `kollaps-23` ist Schritt 59 gemessen, siehe „Weniger Durchgänge beim Aufbau der Knoten“ in
[Optimierungen.md](Optimierungen.md). Eine Fassung von `b3BuildKnots`, die beim Verteilen auf die Gruppen vier von sechs
Durchgängen spart und beim Ordnen und beim Zählen der großen Inseln je einen, liefert in allen Schritten dieselben
Bündel und Gruppen, kostet auf der VM aber nur 6 bis 7 % weniger, 10 bis 12 µs pro Schritt, auf dem Ryzen rund 0,005 ms.
Die gesparten Durchgänge liefen der Reihe nach durch Speicher im Cache, die Zeit liegt in den verstreuten Zugriffen auf
Körper und Partner. „Billiger“ tritt nicht ein, also „Nichts davon“. Daraus Schritt 60, das Erstarren des Schutts. Mit 8
Threads rechnet es der aufrufende Thread in 0,047 ms pro Schritt allein, samt den Typwechseln der Stücke, die erstarren
oder unter großen Stücken wieder losgelassen werden, und dem Einbau ihrer Formen in die Bäume. Die Typwechsel ändern
Box3Ds Sätze, Inseln, Kontakte und Bäume in fester Reihenfolge, siehe Schritt 6 und „Zustandswechsel in Box3D billiger“
in [Optimierungen.md](Optimierungen.md). Was Nebenan vorher für jedes Stück entscheidet, könnte aber schon in den
Prüfungen auf den Workern stehen. Was die Messung der Teile zeigt, entscheidet so:

- **Vorziehen**: Rechnen Teile des Erstarrens jedes Stück für sich und ändern nichts, was ein anderes Stück liest, und
  sparen sie, in den Prüfungen auf den Workern vorbereitet, mit 8 Threads nach der Messung auf der VM mindestens
  0,015 ms pro Schritt, werden sie dorthin gelegt, bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommt das Anlegen der Kontakte, das der aufrufende Thread mit 8 Threads 0,049 ms pro Schritt
  allein rechnet, zerlegt wie die Zustandswechsel.

Vor Schritt 60 kam die Frage des Nutzers, ob es einen anderen Weg gibt. Die Schritte 56 bis 59 haben gezeigt, dass in
dem, was der aufrufende Thread um das Team herum allein rechnet, mit 8 Threads nur noch Hundertstel Millisekunden
liegen. Der größte Posten bleibt das Team selbst: Auf dem Ryzen kostet es mit 8 Threads 0,708 ms pro gemessenem Schritt,
am Ende der Stufen warten die Threads 21 % der offenen Zeit, und in seinen Blöcken braucht ein Thread pro Einheit das
1,24-Fache eines Threads allein, siehe Schritt 55. Ein Schritt mit Team hat rund 232 Stufen mit Sperre, und an jeder
warten alle Threads auf den letzten. Ganz vermeiden ließe sich das nur mit einem großen Umbau: die größte Insel in eine
feste Zahl von Gebieten zerlegen, die jeder Thread ohne Sperren zwischen den Farben rechnet, die Körper an den Grenzen
mit geteilter Masse wie die Knoten. Ob sich das lohnt, soll zuerst eine Zeitleiste vom Ryzen zeigen, Block für Block und
Thread für Thread. Die Zeiten des Teams geben nur Summen, wann und worauf jeder Thread wartet, zeigen sie nicht. Daraus
Schritt 61, Schritt 60 wartet so lange.

Auf `kollaps-24` ist das Werkzeug gebaut, siehe „Die Zeitleiste des Lösers mit Tracy“ in
[Optimierungen.md](Optimierungen.md) und „Mit Tracy messen“ in der [README](../README.md). `Tracy-Aufnahme.bat` aus
`nebenan-demo-windows-tracy` nimmt die ersten 900 Schritte des Abrisses bei ×2 mit 8 Threads auf, mit der Physik
nacheinander wie „Skalierung messen“. `tools/tracy_team.py` teilt in jedem Schritt mit Team die Zeit jedes Threads im
Löser in Arbeit und Warten und das Warten danach, was die anderen Threads gerade tun: Ein Worker hat noch nicht
angefangen („Anlaufen“), ein anderer Thread rechnet noch einen Block der Stufe („Stufen“), dieser Block dauert über
viermal so lange wie der Median seiner Stufe und über 20 µs („Ausreißer“), ein anderer rechnet ein Paket, der aufrufende
Thread rechnet allein, oder niemand rechnet, bis zu 20 µs am Stück zwischen zwei Stufen („Übergänge“) oder länger
(„Pausen“). Ein Warten, geteilt durch die Zahl der Threads, sagt, um wie viel der Löser kürzer würde, wäre es Arbeit.
Auf der VM zeigt eine Aufnahme mit 4 Threads und der Physik nacheinander 0,76 ms an den Enden der Stufen und 0,20 ms in
Übergängen bei 4,52 ms Löser, mit der Physik neben dem Zeichnen fast die Hälfte des Lösers in Pausen. Die VM entscheidet
hier nichts. Was die Aufnahme vom Ryzen zeigt, im Mittel über die Schritte mit Team, entscheidet so:

- **Gestört**: Kosten Ausreißer und Pausen zusammen mindestens 0,05 ms pro Schritt, lief beim Aufnehmen etwas nebenher.
  Dann eine zweite Aufnahme ohne andere Programme, und die Regeln darunter gelten für die ruhigere der beiden.
- **Zerlegen**: Kosten die Enden der Stufen und die Übergänge zusammen mindestens 0,10 ms pro Schritt, kommt als
  Nächstes auf einer eigenen Branch ein Prototyp, der die größte Insel in eine feste Zahl von Gebieten zerlegt und jedes
  Gebiet ohne Sperren zwischen den Farben rechnet, die Körper an den Grenzen mit geteilter Masse. Die Zahl der Gebiete
  hängt nicht an der Zahl der Threads, damit die Ergebnisse mit jeder Zahl von Threads gleich bleiben. Gegenüber heute
  ändert er sie. Gemessen wird er auf der VM und nachgemessen auf dem Ryzen, mit eigenen Regeln.
- **Anlaufen**: Kostet das Anlaufen der Worker mindestens 0,03 ms pro Schritt, kommt als Nächstes, dass die Worker
  früher im Löser sind, bitgleich. Treten „Zerlegen“ und „Anlaufen“ beide ein, kommt zuerst, was mehr Zeit dahinter hat.
- **Nichts davon**: Sonst kommt Schritt 60, das Erstarren des Schutts.

Was der aufrufende Thread allein rechnet und das Warten auf Pakete zeigt die Auswertung mit, sie entscheiden nichts: Das
eine hat Schritt 56 zerlegt, und auf die Pakete warten die Threads seit Schritt 35 kaum noch.

Auf `kollaps-24` ist Schritt 61 aufgenommen und ausgewertet, siehe „Zeitleiste des Teams auf dem Ryzen“ in
[Optimierungen.md](Optimierungen.md). Der Nutzer hat am 27. Oktober mit `Tracy-Aufnahme.bat` 852 der 900 Schritte
aufgenommen, die ersten liefen, bevor `tracy-capture` verbunden war. In allen 848 Schritten mit Löser rechnet ein Team,
und mit 8 Threads dauert der Löser im Mittel 0,755 ms pro Schritt. Gleichmäßig auf die Threads verteilt bräuchte seine
Arbeit 0,446 ms. Von den 0,309 ms dazwischen verlieren die Threads 0,219 ms an den Enden der Stufen, 0,036 ms in den
Übergängen, 0,035 ms, während der aufrufende Thread allein rechnet, und jeweils höchstens 0,006 ms beim Anlaufen, an
Ausreißern, auf Pakete und in Pausen. Ausreißer und Pausen kosten zusammen 0,008 ms, „Gestört“ tritt nicht ein. Die
Enden der Stufen und die Übergänge kosten zusammen 0,255 ms, „Zerlegen“ tritt ein, „Anlaufen“ mit 0,006 ms nicht. Die
Enden der Stufen kosten in jedem Abschnitt von 100 Schritten zwischen 0,188 und 0,245 ms. Das sind nicht einzelne lange
Schritte, sondern die Form der Stufen: Jeder Worker wartet pro Schritt 0,24 ms darauf, dass der letzte Block einer Stufe
fertig wird.

Daraus Schritt 62, ein Prototyp auf einer eigenen Branch. Die größte Insel zerfällt in eine feste Zahl von Gebieten,
unabhängig von der Zahl der Threads, und ein Thread rechnet ein Gebiet mit allen seinen Farben ohne Sperre dazwischen.
Ein Körper an einer Grenze bekommt in jedem Gebiet, das ihn berührt, eine Kopie mit einem Anteil seiner Masse, wie bei
den Knoten, und nach jedem Durchgang des Lösers mitteln die Kopien. Gesperrt wird dann nur noch zwischen den
Durchgängen, nicht mehr zwischen den Farben. Was der Prototyp zeigt, entscheidet so:

- **Übernehmen**: Der Prototyp wird übernommen, wenn alles davon gilt: Mit jeder Zahl von Threads und auf jeder
  Plattform kommen dieselben Ergebnisse heraus. Die Tests bleiben grün, mit neuen Hashes, wo sie Endstände festhalten,
  und beim Abriss bleibt nichts in der Luft hängen. Der Löser dauert in der Zeitleiste auf dem Ryzen mit 8 Threads
  mindestens 0,10 ms pro Schritt mit Team kürzer als mit `kollaps-24`. Mit einem Thread kostet er auf der VM gepaart
  höchstens 5 % mehr.
- **Verwerfen**: Sonst bleibt der Löser, wie er ist, der Prototyp bleibt auf seiner Branch, und es kommt Schritt 60.

Auf `kollaps-25` ist Schritt 62 gebaut und gemessen, siehe „Gebiete der größten Insel“ in
[Optimierungen.md](Optimierungen.md). Der Prototyp rechnet große Inseln in acht Gebieten, mit vier Teilschritten mit 36
statt rund 232 Sperren pro Schritt. Die Ergebnisse ändern sich, bleiben aber mit jeder Zahl von Threads, in beiden
Breiten und auf Windows, Linux und macOS gleich. Die CI ist grün, auch mit ASan, UBSan und TSan, und beim Einsturz
bleibt nichts in der Luft hängen. Mit einem Thread kostet der Löser auf der VM aber gepaart in zwei Messungen im Mittel
8,1 und 10,7 % mehr, erlaubt sind 5 %. Die Bedingungen selbst kosten kaum mehr, die Zeit liegt im Aufbau: Ein Thread
baut die Gebiete und ihre breiten Bedingungen in jedem Schritt neu, 0,67 ms pro Schritt, rund 100 ns pro Kontakt der
großen Inseln, und mit Gebieten braucht er die Pakete, 0,19 ms, die er sonst nicht braucht. Mit 4 Threads kostet der
Löser auf der VM dagegen 5,1 % weniger, dort warten die Threads an den Sperren viel länger als auf dem Ryzen.
„Verwerfen“ tritt ein. Der Löser bleibt, wie er ist, der Prototyp liegt im Verlauf von `kollaps-25`, Commit `6fa0f53`,
und es kommt Schritt 60 mit den Regeln oben. Auch die Regel für den Ryzen hätte der Prototyp wohl verfehlt, eine
Aufnahme braucht es deshalb nicht: Auf den Ryzen übertragen wie der Aufbau der Knoten kostete der Aufbau der Gebiete
rund 0,19 ms pro Schritt auf dem aufrufenden Thread. Selbst wenn die Gebiete alle 0,255 ms sparten, die die Threads dort
an den Sperren verlieren, blieben höchstens rund 0,065 ms, verlangt sind 0,10 ms. Beide Regeln hängen am selben Hebel,
einem rund halb so teuren Aufbau. Derselbe Aufbau, schneller gerechnet, gäbe dieselben Gebiete und Spuren und damit
bitgleich dieselben Ergebnisse wie der Prototyp.

Auf `kollaps-26` ist Schritt 60 gemessen, siehe „Das Erstarren zerlegt“ in [Optimierungen.md](Optimierungen.md). Beim
Abriss bei ×2 sind pro Schritt 564 Stücke ruhig, und 13,4 erstarren, mit je 19 Kontakten. Mit 4 Threads kostet das
Erstarren auf der VM 335 µs pro Schritt, ein Erstarren 16,7 µs, davon 55 % das Löschen der Kontakte, 25 % der Umzug in
den statischen Satz und 11 % die Formen. Ein Fünftel des ganzen Erstarrens ist das gesammelte Einsortieren in den
statischen Baum. Jedes Stück für sich, ohne etwas zu ändern, was ein anderes liest, rechnen großzügig gerechnet 17 %,
auf dem Ryzen 0,008 ms pro Schritt. „Vorziehen“ tritt nicht ein, also „Nichts davon“. Daraus Schritt 63, das Anlegen der
Kontakte für neue Paare. Mit 8 Threads rechnet es der aufrufende Thread nach der Paarsuche auf den Workern in 0,049 ms
pro Schritt allein. Es schreibt die Kontakte, ihre Ids, die Menge der Paare und die Listen der Körper in fester
Reihenfolge. Die Kontakte parallel anzulegen, die erste Stufe von Schritt 9, lohnte auf `physik-41` nicht. Was jeden
neuen Kontakt für sich rechnet, könnte aber schon in der Paarsuche stehen. Was die Messung der Teile zeigt, entscheidet
so:

- **Vorziehen**: Rechnen Teile des Anlegens jeden neuen Kontakt für sich und ändern nichts, was das Anlegen eines
  anderen liest, und sparen sie, in der Paarsuche auf den Workern vorbereitet, mit 8 Threads nach der Messung auf der
  VM mindestens 0,015 ms pro Schritt, werden sie dorthin gelegt, bitgleich, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst sind die Teile, die der aufrufende Thread um das Team herum allein rechnet, alle ausgemessen.
  Dann kommt ein neuer Blick auf das ganze Bild, „Skalierung messen“ auf dem Ryzen beim Abriss bei ×2 und ×4, und die
  nächsten Schritte folgen aus dem, was dort mit 8 Threads am meisten kostet.

Auf `kollaps-27` ist Schritt 63 gemessen, siehe „Das Anlegen der Kontakte zerlegt“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×2 entstehen pro Schritt 776 neue Kontakte. Mit 4 Threads legt der
aufrufende Thread sie auf der VM in 380 µs pro Schritt an, ein Kontakt kostet 461 ns. Die Hälfte davon wartet die
Schleife auf die rund 15 verstreuten Zeilen, die ein neuer Kontakt anfasst und die sie selbst schreibt: die Formen, die
Körper, die Köpfe ihrer Listen, den Kontakt selbst und den Platz in der Menge der Paare. Jeden Kontakt für sich rechnen
großzügig gerechnet 10 %, mit dem Sortieren auf dem Ryzen 0,007 ms pro Schritt. „Vorziehen“ tritt nicht ein, also
„Nichts davon“. Damit sind die Teile, die der aufrufende Thread um das Team herum allein rechnet, ausgemessen: der
Überlauf außerhalb der Knoten in Schritt 56, der Aufbau der Knoten in 57 und 59, die Zustandswechsel in 58, das
Erstarren in 60 und das Anlegen in 63. Keiner lässt sich mit 0,015 ms pro Schritt oder mehr auf die Worker legen, und
die 0,255 ms an den Sperren des Teams holt nur ein Umbau, der mit einem Thread zu viel kostet, siehe 61 und 62. Daraus
Schritt 64, ein neuer Blick auf das ganze Bild auf dem Ryzen: „Skalierung messen“ mit `kollaps-27` beim Abriss bei ×2
und ×4. Seit `kollaps-19`, zuletzt gemessen am 26. Oktober bei ×2, ändert keine Runde an der Bibliothek mehr als Zonen
für Tracy, die ohne `NEBENAN_PROFILE` nicht mitgebaut werden. Die Tabelle zeigt also vor allem, wie sich die Zeit mit 8
Threads verteilt, jetzt auch bei ×4. Was sie zeigt, entscheidet so:

- **Zerlegen**: Liegt ein Posten mit 8 Threads bei ×2 oder ×4 mindestens 0,05 ms pro Schritt über einem Achtel seiner
  Zeit mit einem Thread und haben ihn die Schritte 56 bis 63 nicht schon zerlegt, kommt als Nächstes, ihn auf der VM
  nach seinen Teilen zu messen, der größte zuerst.
- **Nichts davon**: Sonst bleibt in der Physik mit 8 Threads nur, was ein großer Umbau holt, und als Nächstes kommt die
  Grafik.

Am 27. Oktober hat der Nutzer den Umbau der Buchhaltung von Box3D, Schritt 9, ganz verworfen. Die Zustandswechsel, das
Erstarren und das Anlegen der Kontakte kosten auf dem Ryzen mit 8 Threads zusammen rund 0,16 ms pro Schritt, und die
Schritte 58, 60 und 63 zeigen, dass ihre Schleifen vor allem auf die Zeilen warten, die sie selbst schreiben. Mehr holte
auch ein Neubau nicht, für Monate Arbeit und andere Ergebnisse. Als großer Umbau bleiben die dauerhaften Gebiete: Box3D
führte die Gebiete der großen Inseln wie die Farben im Graphen über die Schritte. Jeder Körper gehörte fest zu einem,
jeder Kontakt bekäme Gebiet und Farbe, wenn er zu berühren beginnt, und neu geschnitten würde nur selten. Dann fiele der
Aufbau weg, an dem der Prototyp von Schritt 62 mit einem Thread scheiterte, und die Sperren zwischen den Farben blieben
weg. Ob diese Sperren mit 8 Threads der größte Posten bleiben, zeigt Schritt 64.

Auf dem Ryzen gemessen am 27. Oktober, Schritt 64, siehe „Skalierung auf dem Ryzen mit `kollaps-27`“ in
[Optimierungen.md](Optimierungen.md). Mit 8 Threads kostet ein Schritt bei ×2 2,253 ms und bei ×4 1,641 ms. Über einem
Achtel ihrer Zeit mit einem Thread liegen bei ×2 und ×4 die Bedingungen des Lösers mit 0,373 und 0,313 ms, die
Kontakt-Schleife mit 0,083 und 0,152 ms und die Einschläge mit 0,058 und 0,089 ms, der Aufbau der Knoten, die
Zustandswechsel, das Anlegen und das Erstarren mit je 0,03 bis 0,07 ms und bei ×4 die Prüfungen des Updates mit
0,051 ms. Die Bedingungen, die Knoten, die Zustandswechsel, das Anlegen und das Erstarren haben die Schritte 56 bis 63
zerlegt, also tritt „Zerlegen“ für die Kontakt-Schleife ein, daraus Schritt 65. Bei ×4 kostet ein Kontakt in ihren
Blöcken mit einem Thread 124 ns, mit 2 Threads 163 ns und mit 8 Threads 180 ns, in den Prüfungen wie in der Narrowphase,
bei ×2 mit 8 Threads nur 9 % mehr als mit einem. Bei ×4 kostet auch eine Prüfung auf Ruhe im Update mit 2 Threads 53 %
mehr als mit einem, und die Pakete des Lösers brauchen pro Einheit der Arbeit mit 2 Threads 57 % und mit 8 Threads 96 %
mehr als ein Thread allein in den Farben. Bei ×4 rechnen sie mit 8 Threads 72 % der Arbeit, das ist rund die Hälfte
dessen, was die Bedingungen dort über einem Achtel liegen. Eine Vermutung: Mit einem Thread hält ein Kern seine Daten
warm, mit mehreren landet jeder Block in jedem Schritt auf einem anderen Kern und fängt kalt an. Was die Messung zeigt,
entscheidet so:

- **Beheben**: Steigt auf der VM bei ×4 der Preis eines Kontakts mit dem zweiten Thread ebenso, und holt eine Änderung,
  die bitgleich bleibt, davon mindestens die Hälfte zurück, etwa eine Zuordnung, die jedem Thread in jedem Schritt
  möglichst dieselben Blöcke gibt, wird sie gebaut, wo sie ebenso hilft auch in den Prüfungen des Updates und in den
  Paketen, und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommen die Einschläge, mit 8 Threads bei ×4 0,089 ms über einem Achtel ihrer Zeit mit einem
  Thread, nach ihren Teilen gemessen.

Auf der VM gemessen am 27. Oktober auf `kollaps-28`, Schritt 65, siehe „Die Kontakt-Schleife bei ×4 auf der VM“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×4 kostet ein Kontakt in den Blöcken der Kontakt-Schleife dort mit
1, 2 und 4 Threads 463, 470 und 487 ns, mit 2 Threads 1,6 % mehr als mit einem, in einem zweiten Lauf 2,0 %, nicht 32 %
wie auf dem Ryzen, und in der Narrowphase gar nicht mehr. Auf der VM steigt der Preis also nicht ebenso, „Beheben“ tritt
nicht ein. Im Benchmark unter Trommelfeuer bei ×4, wo zwischen den Schritten nichts gezeichnet wird, steigt der Preis
mit 2 und 4 Threads um 6 bis 21 und 14 bis 22 %. Feste Blöcke für jeden Thread holen davon nichts Sicheres zurück. Läuft
die Schleife allein auf dem aufrufenden Thread, während der Rest mit 2 Threads rechnet, steigt sie trotzdem um 6 %: Mit
dem zweiten Thread kostet nicht die Verteilung der Blöcke, sondern was die übrigen Stufen auf dem anderen Kern
geschrieben haben. Die Prüfungen des Updates, die gleich nach dem Löser laufen, steigen auf der VM dagegen auch, eine
Prüfung auf Ruhe mit 2 Threads um 23 %. Nichts gebaut. Nach der Regel kommen jetzt die Einschläge, daraus Schritt 66.
Was ihre Messung zeigt, entscheidet so:

- **Verteilen**: Rechnet der aufrufende Thread einen Teil eines Einschlags allein, der sich bitgleich auf die Worker
  legen oder sparen lässt, und bringt das hochgerechnet auf den Ryzen mit 8 Threads bei ×4 mindestens 0,02 ms pro
  Schritt, wird es gebaut und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst kommen die Prüfungen des Updates bei ×4, mit 8 Threads 0,051 ms über einem Achtel ihrer Zeit
  mit einem Thread, nach ihren Teilen gemessen.

Auf der VM gemessen am 27. Oktober auf `kollaps-29`, Schritt 66, siehe „Die Teile eines Einschlags“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×4 fallen pro Schritt 1,43 Granaten, und ein Einschlag kostet auf
der VM 304 bis 318 µs, mit 1 und 4 Threads gleich viel. 72 bis 74 % davon rechnet der aufrufende Thread allein, auf dem
Ryzen hochgerechnet 0,076 ms pro Schritt. Am meisten kostet der Stoß der Explosion in Box3D mit 27 bis 28 %, auf dem
Ryzen 0,028 ms pro Schritt, dann die Zellen mit 22 %, die auf dem Ryzen mit 8 Threads schon die Worker mitrechnen, die
Abfrage der getroffenen Teile mit 10 %, das Teilen der Akteure mit 7 % und die Box3D-Formen der neuen Teile mit 6 %. Die
Explosion geht im Mittel 71,5 Formen durch, je rund 1 µs für den Abstand zum Punkt und die projizierte Fläche, und das
ließe sich auf die Worker legen. Mit den 7 µs, die sie auf dem Ryzen zum Anlaufen brauchen, brächte das nach einer
Simulation Explosion für Explosion aber nur 0,013 bis 0,014 ms pro Schritt, selbst mit Workern, die sofort
bereitstünden, 0,020 bis 0,022 ms. Formen zu überspringen, deren Box schon zu weit weg liegt, spart 0,003 ms. Jeder
andere Teil liegt unter 0,011 ms. „Verteilen“ tritt nicht ein, nichts gebaut. Nach der Regel kommen jetzt die Prüfungen
des Updates bei ×4, daraus Schritt 67. Ihr Preis pro Stück steigt mit dem zweiten Thread auf dem Ryzen um 53 % und,
anders als in der Kontakt-Schleife, auch auf der VM um 21 bis 23 %, siehe Schritt 65. Was ihre Messung zeigt,
entscheidet so:

- **Beheben**: Holt eine Änderung, die bitgleich bleibt, auf der VM bei ×4 mindestens die Hälfte des Anstiegs pro Stück
  mit 2 und 4 Threads zurück, wird sie gebaut und auf dem Ryzen nachgemessen.
- **Nichts davon**: Sonst sind alle Posten zerlegt, die mit 8 Threads über einem Achtel ihrer Zeit mit einem Thread
  liegen. Dann bleibt als Nächstes der große Umbau, die dauerhaften Gebiete auf einer eigenen Branch, über den der
  Nutzer entscheidet.

Auf der VM gemessen am 27. Oktober auf `kollaps-30`, Schritt 67, siehe „Die Prüfungen des Updates bei ×4 auf der VM“ in
[Optimierungen.md](Optimierungen.md). Beim Abriss bei ×4 kosten die Prüfungen auf der VM pro bewegtem Stück in ihren
Blöcken mit 2 und 4 Threads 11 und 17 % mehr als mit einem, im Benchmark unter Trommelfeuer bei ×4 15 und 24 %. Prüft
der aufrufende Thread allein, während der Rest der Welt mit 2 und 4 Threads rechnet, sind es nur 2 und 3 %: Den Anstieg
machen die Prüfungen, die gleichzeitig laufen, nicht die Daten des Lösers. Zwei Stellen fanden sich, an denen die
Threads in Cache-Zeilen schreiben, die die anderen lesen. Im Akteur liegen `isFree`, `isStatic` und `isRubble`, die der
Blick auf die Auflage bei jedem berührten Stück liest, in einer Zeile mit `checkedAsleep`, das jede Prüfung schreibt,
auch wenn es sich nicht ändert, und mit `restPose`, je nach Lage des Arrays auch mit `restTime`. Und die Arenen der
Worker liegen zu je 24 Bytes nebeneinander, drei in einer Zeile, und jede Allokation schreibt ihren Füllstand. Beides
getrennt, bitgleich, steigt der Preis im Benchmark noch um 11 und 14 %, mit festen Blöcken für jeden Thread dazu um 7
und 14 %, beim Abriss um 5 und 10 %. Das holt mit 2 Threads rund die Hälfte zurück, mit 4 Threads aber nur 39 bis 47 %,
und auf dem Ryzen brächte es hochgerechnet rund 0,011 ms pro Schritt. „Beheben“ tritt nicht ein, nichts gebaut. Damit
sind alle Posten zerlegt, die mit 8 Threads über einem Achtel ihrer Zeit mit einem Thread liegen, und keiner gab nach
den Regeln mehr her. Was bleibt, ist der große Umbau, die dauerhaften Gebiete, Schritt 68, über den der Nutzer
entscheidet. Will er ihn, gilt dieselbe Regel wie für den Prototyp von Schritt 62: Übernommen wird er nur, wenn die
Ergebnisse mit jeder Zahl von Threads und auf jeder Plattform gleich bleiben, die Tests grün bleiben, nichts in der Luft
hängen bleibt, der Löser auf dem Ryzen mit 8 Threads mindestens 0,10 ms pro Schritt mit Team kürzer dauert und mit einem
Thread höchstens 5 % mehr kostet.

Der Nutzer hält die Trennung trotzdem für lohnend, daraus Schritt 69. Gebaut am 27. Oktober auf `kollaps-31`, siehe „Die
Prüfungen des Updates auf eigenen Zeilen“ in [Optimierungen.md](Optimierungen.md). Was die Prüfungen von jedem bewegten
Stück schreiben, liegt am Ende des Akteurs auf zwei eigenen Cache-Zeilen, jede Allokation beginnt auf 64 Bytes, die
Arenen der Worker und die Aufgaben der Prüfungen liegen auf eigenen Zeilen, und mit Workern behält jeder Thread einen
festen Abschnitt der Blöcke, in dem er die Blöcke mit großen Stücken zuerst nimmt, bevor er den anderen von hinten
hilft. Die Welt endet bitgleich. Beim Abriss auf der VM bei ×4 kosten die Prüfungen auf den Threads mit 1 Thread 1,6 %
mehr und mit 2 und 4 Threads 2,8 und 6,2 % weniger, mit 4 Threads 0,553 statt 0,590 ms pro Schritt, und ein Stück in den
Blöcken kostet mit 4 Threads 11,6 % weniger. Im Benchmark unter Trommelfeuer bei ×4, mit halb so vielen bewegten Stücken
pro Update, kosten sie 3,3, 1,0 und 2,2 % mehr. Mit einem Thread liest eine Prüfung jetzt fünf statt vier Zeilen eines
Akteurs. Ob das bleibt, entscheidet der Ryzen, Schritt 70:

- **Behalten**: Kosten die Prüfungen auf den Threads, `checkMoving` im Bericht von „Skalierung messen“, beim Abriss bei
  ×4 mit 8 Threads weniger als mit `kollaps-30` direkt davor oder danach und mit einem Thread höchstens 5 % mehr, bleibt
  `kollaps-31`.
- **Zurücknehmen**: Sonst kommt die Trennung auf einer neuen Branch wieder heraus.

Danach bleibt der große Umbau, die dauerhaften Gebiete, Schritt 68, über den der Nutzer entscheidet.

Der Nutzer will ihn. Gebaut am 27. und 28. Oktober auf `kollaps-32`, siehe „Dauerhafte Gebiete“ in
[Optimierungen.md](Optimierungen.md). Eine Insel mit mindestens 1 024 Kontakten und einem Achtel der Arbeit aller wachen
Inseln bekommt ihre Gebiete einmal und behält sie, solange sie wach und groß genug bleibt, wie die Farben im Graphen.
Jeder Kontakt liegt im Array seines Gebiets in seiner Farbe, sobald er zu berühren beginnt, und neu geschnitten wird
nur, wenn ein Gebiet ein Viertel mehr Kontakte hält als ihr Mittel, höchstens alle 32 Schritte. Ein Schritt zählt nur
noch die Kopien und legt die Kontakte in breite Bedingungen, deren offene Spuren die nächsten Farben auffüllen,
bitgleich zu einer Farbe pro breiter Bedingung. Das rechnet ein Worker, während der aufrufende Thread die Knoten baut.
Die Ergebnisse ändern sich, bleiben aber mit jeder Zahl von Threads, in beiden Breiten und auf Windows, Linux und macOS
gleich, die Tests sind grün, auch mit ASan, UBSan und TSan, und nichts bleibt in der Luft hängen. Auf der VM, nach einem
Neustart ein rund doppelt so schneller Rechner, kostet der Löser beim Abriss bei ×2 gepaart mit einem Thread 0,2 % mehr,
erlaubt sind 5 %, und mit 4 Threads 12,2 % weniger, 2,09 statt 2,38 ms pro Schritt. Der Aufbau auf dem aufrufenden
Thread kostet mit 4 Threads 0,04 ms mehr. Im Benchmark unter Trommelfeuer ist die größte Insel kleiner, dort kostet der
Löser mit 2 und 4 Workern bis zu 6 % mehr: Mit so wenigen Threads rechnet ein Thread eine solche Insel ohne Gebiete
allein, ganz ohne Sperren. Mit einem Worker kostet der Löser dort selbst 3 bis 7 % mehr, mit dem anderen Verlauf der
Simulation bis zu 9 %. Dass Gebiete das Einschlafen bremsen, zeigt sich nicht. Die Regel mit einem Thread ist wie in
Schritt 62 beim Abriss gemessen und hält dort, im Benchmark nicht überall. Was die Messung dort zeigt, Schritt 71,
entscheidet so:

- **Übernehmen**: Kostet `solve` im Bericht von „Skalierung messen“ beim Abriss bei ×2 mit 8 Threads mit `kollaps-32`
  mindestens 0,10 ms pro Schritt weniger als mit `kollaps-31` direkt davor oder danach, bleiben die dauerhaften Gebiete.
  Beim Abriss rechnen große Inseln in 876 von 880 Schritten in Gebieten, der Schnitt über alle Schritte ist also der
  über die Schritte mit Team.
- **Verwerfen**: Sonst kommen sie auf einer neuen Branch wieder heraus, wie der Prototyp von Schritt 62, und als Nächstes
  kommt die Grafik.

Auf dem Ryzen gemessen am 28. Oktober, Schritt 70 und 71, siehe „Skalierung auf dem Ryzen mit `kollaps-30` bis
`kollaps-32`“ in [Optimierungen.md](Optimierungen.md). Bei ×4 kosten die Prüfungen des Updates mit `kollaps-31` mit 8
Threads 0,088 statt 0,077 ms pro Schritt und mit einem Thread 0,8 % mehr, bei ×2 mit 8 Threads 0,110 statt 0,105 ms. In
den Blöcken kostet ein Stück etwas weniger, aber mit den festen Abschnitten warten die Threads nach ihrem letzten Block
länger, bei ×4 zusammen 0,161 statt 0,065 ms. „Zurücknehmen“ tritt ein, auf `kollaps-33` ist die Trennung wieder heraus,
bitgleich. Der erste Lauf von `kollaps-31` bei ×2 war gestört, auch wo sich nichts geändert hat. Nach der Regel von
Schritt 71 stünden dort 0,314 ms, gegen den sauberen Lauf von `kollaps-30` mit demselben Löser 0,099 ms. Ein zweiter
Lauf aller drei Stände bei ×2 war sauber: `solve` kostet mit 8 Threads mit `kollaps-32` 0,860 statt 0,967 ms, 0,107 ms
weniger, über alle sauberen Läufe 0,102 ms. „Übernehmen“ tritt ein, knapp, die dauerhaften Gebiete bleiben. Bei ×4
bringen sie mit 8 Threads nichts, und mit einem Thread kostet der Löser auf dem Ryzen bei ×2 6 % und bei ×4 11 % mehr,
auf der VM, wo die Regel gilt, 0,2 %.

Mit den Gebieten liegen beim Abriss bei ×2 mit 8 Threads die Bedingungen am weitesten über einem Achtel ihrer Zeit mit
einem Thread, 0,248 ms pro Schritt, vor dem Update mit 0,151, der Kontakt-Schleife mit 0,090, dem Aufbau mit 0,086 und
den Zustandswechseln mit 0,079 ms, die die Schritte 56 bis 67 alle schon zerlegt haben. In den Bedingungen geht die
meiste Zeit in die Durchgänge der Gebiete, 0,425 ms pro Schritt mit Team, und dort warten die Threads 31 % ihrer Zeit
auf das langsamste Gebiet, nachdem ihr eigenes fertig ist, bis zu 0,13 ms pro Schritt. Geschnitten wird nach der Zahl
der Kontakte und neu erst, wenn das größte Gebiet ein Viertel mehr hält als ihr Mittel. Ein Durchgang rechnet in einem
Gebiet aber seine Kopien, seine Knoten, seinen übrigen Überlauf und seine breiten Bedingungen. Daraus Schritt 72: Auf
der VM wird gemessen, wie lange jedes Gebiet in jedem Durchgang rechnet, mit einem Thread einzeln gestoppt, und für den
Ryzen mit 8 Threads simuliert, wie lange die Threads auf das langsamste warten, woran das liegt und was ein anderer
Schnitt holt, etwa nach der Arbeit statt nach den Kontakten oder früher neu. Was die Messung zeigt, entscheidet so:

- **Ausgleichen**: Holt ein anderer Schnitt der Gebiete in der Simulation auf dem Ryzen mit 8 Threads beim Abriss bei ×2
  mindestens 0,03 ms pro Schritt und kostet er auf der VM gepaart mit einem Thread höchstens 2 % mehr, wird er gebaut
  und auf dem Ryzen nachgemessen. Die Ergebnisse ändern sich dabei, bleiben aber mit jeder Zahl von Threads und auf
  jeder Plattform gleich.
- **Nichts davon**: Sonst bleibt in der Physik mit 8 Threads nur, was ein neuer großer Umbau holt, und als Nächstes
  kommt die Grafik.

Gemessen am 28. Oktober auf der VM, siehe „Die Durchgänge der Gebiete zerlegt“ in [Optimierungen.md](Optimierungen.md),
im selben Verlauf wie `kollaps-32` auf dem Ryzen. Mit dem Median jedes Gebiets über die Durchgänge derselben Art warten
die Threads mit 8 Threads nachgestellt 28,7 % ihrer Zeit in den Durchgängen, auf dem Ryzen 31 %. Was ein Gebiet in einem
Durchgang kostet, folgt fast ganz seinen Kontakten, ein Schnitt nach der Arbeit brächte also kaum etwas. Direkt nach
einem Schnitt hält das größte Gebiet einer Insel das 1,16-Fache des Mittels, nach 1 bis 3 Schritten schon das
1,31-Fache, nach 16 bis 31 Schritten das 1,79-Fache, und neu geschnitten wurde frühestens nach 32. Den größten Teil
machen Inseln, die dazukommen: Alle ihre Körper nahmen das Gebiet des Körpers, an dem sie hingen. Ein Schnitt kostet bei
rund 4 200 Kontakten 615 µs auf dem aufrufenden Thread, 150 ns pro Kontakt, fast alles im Umzug der Kontakte. Wie lange
die Threads warten, hängt stark am Verlauf, mit dem alten Verfahren in ruhigen Verläufen 21 %, in wilden 28 bis 30 %.
Verglichen sind deshalb nur wilde Verläufe mit 730 bis 870 Granaten, wie der auf dem Ryzen, mit dem alten Stand im
Mittel 28,8 %. Ein Neuschnitt ab 4 Schritten allein senkt das Warten auf 19 bis 22 %, kostet aber 70 bis 104 statt 27 µs
pro Schritt an Schnitten, auf dem Ryzen blieben netto rund 0,02 ms. Legt der Schnitt seine Ebenen in der Insel ab und
nimmt jeder Körper, der mit seiner Insel dazukommt, das Gebiet, in dem er liegt, warten die Threads in vier Verläufen
22,2 bis 23,6 %, ohne einen Schnitt mehr, rund 0,033 ms. Beides zusammen 18 bis 21 %, netto rund 0,035 ms. Mit einem
Thread kostet der Löser auf der VM gepaart mit beidem zusammen aber 5,1 % mehr, pro berührendem Kontakt 3,1 %, erlaubt
sind 2 %. Mit dem Platzieren allein kostet er 0,9 % weniger. „Ausgleichen“ tritt für das Platzieren ein, gebaut auf
`kollaps-34`, der Neuschnitt bleibt bei 32 Schritten. Die Ergebnisse ändern sich, bleiben aber mit jeder Zahl von
Threads gleich, die Tests sind grün mit allen Hashes. Was die Messung auf dem Ryzen zeigt, Schritt 73, entscheidet so:

- **Behalten**: Kostet `solve` im Bericht von „Skalierung messen“ beim Abriss bei ×2 mit 8 Threads pro Einheit der
  Arbeit, `solve` durch „work per sampled step“, mit `kollaps-34` mindestens 3,5 % weniger als mit `kollaps-33` direkt
  davor oder danach, bei 0,86 ms rund 0,03 ms pro Schritt, bleibt das Platzieren nach dem Schnitt. Pro Einheit der
  Arbeit, weil jeder Stand einen anderen Verlauf nimmt. Beide Stände werfen dort wilde Verläufe, 829 und 865 Granaten,
  die Rechnung ist auf jeder Plattform dieselbe.
- **Zurücknehmen**: Sonst kommt es auf einer neuen Branch wieder heraus, und als Nächstes kommt die Grafik.

Auf dem Ryzen gemessen am 28. Oktober, Schritt 73, siehe „Skalierung auf dem Ryzen mit `kollaps-33` und `kollaps-34`“ in
[Optimierungen.md](Optimierungen.md). Beide Stände werfen dort wie auf der VM 829 und 865 Granaten. Bei ×4 kostet
`solve` mit 8 Threads pro Einheit der Arbeit mit `kollaps-34` 1,2 % mehr, im Rahmen der Schwankungen. Der erste Lauf von
`kollaps-34` bei ×2 war mit 2, 4 und 8 Threads gestört, auch wo sich nichts geändert hat: Die Kontakt-Schleife kostete
mit 8 Threads 171,4 statt 130,5 ns pro Kontakt, mit einem Thread so viel wie im sauberen Lauf danach. Nach der Regel
stünden dort 26 % mehr. Ein zweiter Lauf von `kollaps-34` bei ×2 war sauber. Darin kostet `solve` mit 8 Threads 0,790
statt 0,849 ms, bei 5,6 % weniger Arbeit pro Stichprobe, pro 1 000 Einheiten 23,6 statt 24,0 µs, 1,5 % weniger. Gegen
die beiden sauberen Läufe von `kollaps-32` mit demselben Löser und Verlauf sind es 2,7 und 3,1 %. In den Durchgängen der
Gebiete warten die Threads 26 statt 31 % ihrer Zeit, und die Durchgänge kosten pro Einheit der Arbeit 7 % weniger, rund
0,03 ms pro Schritt, wie hochgerechnet. Was im Löser an den Körpern hängt, wiegt im Verlauf von `kollaps-34` aber mehr:
Die größte Insel hat 6,6 % weniger Bedingungen, aber nur 1,5 % weniger Körper. „Zurücknehmen“ tritt ein, auf
`kollaps-35` ist das Platzieren wieder heraus, die Bibliothek ist dieselbe wie auf `kollaps-33`, bitgleich.

Damit holt in der Physik mit 8 Threads nur noch ein neuer großer Umbau etwas, und als Nächstes kommt die Grafik, nach
Punkt 10 der Reihenfolge in [Optimierungen.md](Optimierungen.md): nur bewegte Transformationen hochladen (Nr. 74) und
räumliche Render-Seiten mit Frustum-Culling (Nr. 49 bis 51). Womit sie anfängt, entscheidet der Nutzer.

## Figuren

Auf `active-ragdoll-1`, neu von `kollaps-36` aus, nach den Kapiteln 26 bis 28 der Roadmap des Spiels und dem Quellcode
von Overgrowth: Figuren, die animiert stehen, als aktive Ragdolls fallen und wieder aufstehen, alles in der Bibliothek,
siehe Figuren im README. Die Branches `ragdoll-*` sind nicht fortgesetzt, sie dienten als Fundus. Eine Figur steht mit
kinematischen Knochen, die ihrem Clip folgen. Ein harter Treffer, eine Explosion, Trümmer, eine Last von oben oder
Boden, der wegbricht, machen sie zum aktiven Ragdoll, mit den Geschwindigkeiten ihrer Knochen und dem Treffer genau
einmal, verteilt auf die Knochen wie in Overgrowth. Die Gelenke ziehen als Federn mit begrenztem Drehmoment, einer
Ergänzung in Box3D, zur Haltung zum Fallen, so stark, wie die Figur schnell ist. Liegt sie still, steht sie mit einem
Clip vom Bauch oder vom Rücken auf, sobald sie Boden, Platz und nichts auf sich hat. Die Clips eines Menschen baut die
Bibliothek selbst. Zehn Tests prüfen die Grenze der Federn, die Clips, Stand, Zucken, Sturz von vorn und hinten mit
Aufstehen, das Warten auf Platz und den Abbruch, den Boden, Trümmer, Last und Explosion, eigene Clips und die Bits mit
einem und vier Threads. Welche Wege dabei verworfen wurden, steht unter „Figuren nach Overgrowth“ und den Zeilen danach
in [Optimierungen.md](Optimierungen.md).

Gemessen mit `nebenan_benchmark figures` auf der VM, nach Kapitel 26.8 der Roadmap des Spiels mit 1, 10 und 50 Figuren:
Stehend kosten 50 Figuren mit einem Thread 0,14 ms im Box3D-Schritt und 0,075 ms im Update, 1,5 µs pro Figur. Fallen
alle 50 zugleich, kostete der Schritt auf `active-ragdoll-1` 1,9 bis 2,4 ms, mit vier Threads 1,4 ms. Das lag am Löser:
Solange ein Gelenk in den Farben lag, rechnete er ohne ganze Inseln pro Thread und ohne Gebiete, Farbe für Farbe mit
einer Sperre nach jeder Stufe, auch für die Trümmer der Stadt. Auf `active-ragdoll-2` rechnet jedes Paket seine Gelenke
selbst, bis aufs Bit wie über die Farben, und große Inseln behalten ihre Gebiete, auch mit einem Ragdoll darin, Schritt
F8. Im Median aus fünf Läufen im Wechsel gegen `active-ragdoll-1` kostet der gemeinsame Sturz von 50 Figuren mit vier
Threads jetzt 1,17 statt 1,30 ms pro Schritt, der Löser darin 0,80 statt 0,99 ms. Der Benchmark misst jetzt auch die
Stadt aus 16 Häusern unter Beschuss mit zwei Figuren an jedem Haus, 32 auf den Straßen, die Granaten, Trümmer und
einstürzende Stockwerke umwerfen. Dort liegt in 1 059 der 1 200 Schritte ein Ragdoll wach. Mit vier Threads rechnet der
Löser in 1 199 Schritten mit Paketen und in 784 mit Gebieten, vorher in keinem, und braucht 2,61 statt 3,09 ms, 15,5 %
weniger, der ganze Box3D-Schritt 6,00 statt 6,27 ms. Mit einem Thread kostet der Schritt so viel wie vorher. Die
Ergebnisse der Stadt mit Figuren ändern sich, weil ihre Trümmer jetzt in Gebieten rechnen, mit jeder Zahl von Threads
gleich. Alle übrigen Szenen des Benchmarks enden gleich wie auf `active-ragdoll-1`, und die Städte ohne Figuren kosten
so viel wie vorher, im Median zwischen 4,2 % weniger und 5,2 % mehr pro Box3D-Schritt, in beide Richtungen. Die Demo
zeigt die Figuren in der Szene „Figuren“, dort misst der Nutzer auf dem Ryzen. Seit `active-ragdoll-3` hat sie dazu die
Szene „Stadt mit Figuren“, die Stadt aus 20 Häusern mit 40 Figuren, und den Knopf „Mit und ohne F8 messen“, der sie mit
und ohne Gelenke in den Paketen beschießt, Schritt F13. Den Schalter dafür hat Box3D, `b3WorldDef::enableJointPacks`,
aus rechnet der Löser bis aufs Bit wie auf `active-ragdoll-1`. Auf dem Ryzen sind damit in der Stadt mit Figuren der
Löser 9,9 % und ein Bild 4,8 % schneller, nur die langsamsten Bilder werden langsamer, Schritt F14. Auf
`active-ragdoll-4` zeigt sich, dass das zum größten Teil an der Welt liegt, die mit F8 anders endet. Sicher mehr kosten
nur die ersten Schnitte der Gebiete in den langsamen Schritten, und die kosten jetzt bitgleich ein Drittel weniger. Was
in diesen Schritten außerdem auffiel, steht als F15 bis F17 in der Tabelle. Auf `active-ragdoll-5` wacht ein großes Teil
eines Gebäudes durch ein bewegtes Stück nur noch auf, wenn es auf ihm erstarrt ist, F17. Was das Update in den
langsamsten Bildern kostet, ist vor allem das Budget, F18. Auf `active-ragdoll-6` kostet ein Typwechsel dort rund ein
Fünftel weniger, bitgleich. Was das Budget dort sonst kostet, steht als F19 und F20 in der Tabelle. Auf
`active-ragdoll-7` wählt das Budget alle Stücke, bevor eines erstarrt, F19, und große Stücke erstarren dort nur noch, wo
sie auch in der Ruhe erstarren würden. Auf `active-ragdoll-8` merken sich große Stücke den Schutt an ihrer Seite, den
sie für ihr Gleichgewicht brauchen, und nur verkeilt erstarren sie nicht mehr, F21. Auf `active-ragdoll-9` behalten
erstarrende Stücke ihre Kontakte mit bewegten, F20, und die Probe am Ende zählt auf 48 neuen Folgen von Granaten, denn
auf die 16 alten waren F17 bis F21 abgestimmt. Was sie dabei fand, steht als F22 in der Tabelle. Auf `active-ragdoll-10`
folgt Schutt dem, worauf er liegt, auch wenn das zerbricht oder in einem Einsturz spät an die Reihe kommt, F22. Was
danach noch in der Luft liegt, hat das Budget fast immer an einer steilen Fläche oder in der Bewegung eingefroren, F23.
Auf `active-ragdoll-11` erstarrt ein kleines Stück, das sich bewegt, nur noch auf Flächen flacher als 60°, und ein
bewegtes Stück hält, was auf ihm erstarrt, von dort, wo es liegt, F23. Von dem, was dann noch in der Luft liegt, hat das
Budget die Hälfte in der Bewegung eingefroren, meist mit dem Schwerpunkt neben den Punkten, auf denen es auflag, F24.
Auf `active-ragdoll-12` erstarrt ein kleines Stück in Bewegung mit dem Schwerpunkt neben seiner Auflage nur noch, wenn
höchstens 30 cm darunter etwas liegt, F24. Was danach noch fällt, steht als F25 in der Tabelle. Auf `active-ragdoll-13`
halten bewegte Stücke ein kleines Stück in Bewegung, das sie braucht, auch wenn es zugleich auf etwas Festem liegt, und
unter einem Stück zählt nur noch Festes, F25. Was danach noch fällt, erstarrte meist langsam oder in der Ruhe neben
seiner Auflage an einer steilen Fläche, F26. Auf `active-ragdoll-14` zeigte sich, dass diese Stücke klemmen, meist von
bewegten Nachbarn gedrückt. Sie von den Nachbarn halten zu lassen kostete viel mehr Auftauen, als es Stücke rettete:
verworfen. Auf `active-ragdoll-15` fängt sich eine stehende Figur mit Schritten, wenn ein Treffer sie nicht umwirft, sie
ihn über ihren Füßen aber nicht abfangen kann, F9. Auf `active-ragdoll-16` stehen ihre Füße auf dem Boden unter ihnen,
auf einer Stufe, an einem Hang und auf Schutt, F10. Auf `active-ragdoll-17` rückt eine liegende Figur ohne Platz zum
Aufstehen zurück oder zur Seite oder dreht sich, statt zu warten, F11. Auf `active-ragdoll-18` liegen die Gelenke eines
Ragdolls im Schutt in den Gebieten der großen Insel, und der aufrufende Thread rechnet sie nicht mehr allein, F12. Auf
`active-ragdoll-19` bricht ein Ragdoll, der gegen eine zerstörbare Wand fliegt, nach Energie und Material durch oder
prallt ab, mit genau einem Einschlag pro Teil der Wand und Schritt, F28. Auf `active-ragdoll-20` fängt sich eine Figur
nach einem harten Treffer mit dynamischen Knochen und echten Schritten wie auf `ragdoll-11`, statt schlaff umzufallen,
und fällt nur, wo kein Schritt reicht, F29. Auf `active-ragdoll-21` wacht ein Ragdoll, der auf seinen Füßen ruht,
stehend auf, statt für immer auf Boden unter dem Becken zu warten, F30. Auf `active-ragdoll-22` schwingen Hals, Kopf und
Arme bei einem leichten Treffer einzeln, nach der Trefferlage, F31. Auf `active-ragdoll-23` rudern die Arme einer Figur,
die sich fängt, in Fallrichtung, wo ein Schritt zu kurz kommt, und hängen sonst locker, und die Balance gibt nicht mehr
auf, bevor Schritte und Arme es versucht haben, F35. Auf `active-ragdoll-24` schwingen die Arme geplant aus, halten,
solange die Figur tritt, und schwingen zurück, statt zu rucken, F35. Auf `active-ragdoll-25` können die Gelenke mit
genäherten Muskeln ziehen, als Schalter, der aus bleibt: Gemessen fängt die Figur mit ihnen nicht besser, und ihr Rumpf
ruckt nur bei manchen Treffern weniger, F45. Auf `active-ragdoll-26` verteilt sich ein harter Treffer entlang des
Skeletts: Ein Treffer auf einen Arm oder ein Bein bewegt das Glied, nicht den Rumpf daneben, und ein Tritt gegen ein
Schienbein lässt das andere Bein stehen, F46. Auf `active-ragdoll-27` bewegen sich die Gelenke so weit wie die eines
Menschen, mit einer eigenen Grenze in jede Richtung: Die Schulter hebt den Arm über den Kopf und nur 40° zurück, F47.
Auf `active-ragdoll-28` dreht ein Treffer neben der Mitte des Rumpfs ihn um seine Längsachse, ein leichter so weit, wie
es die Trägheit des Oberkörpers will, ein harter um sein ganzes Moment um diese Achse, F50. Auf `active-ragdoll-29`
haben Gelenke eine Zusatzträgheit, die Armature von MuJoCo, und Hals und Kopf des Menschen halten mit ihr ruhiger, F48.
Auf `active-ragdoll-30` gibt eine stehende Figur einem langsamen Drücken nach, statt eine Wand zu bleiben, F64 Stufe 1,
und die Demo hat einen Stock, um das von Hand auszuprobieren. Auf `active-ragdoll-31` gibt ein Arm einem Drücken aus
jeder Richtung nach, und die Demo hält den Stock an einem Ende in der Hand, die Maus schwingt das andere, und man sieht
sich dabei um. Auf `active-ragdoll-32` geht der Stock mit dem Blick wie eine Waffe in einem Spiel. Auf
`active-ragdoll-33` hält man ihn wie einen Stab mit beiden Händen, sein hinteres Drittel reicht hinter das Auge, und er
hängt straff am Blick. Auf `active-ragdoll-34` richtet sich die Balance nach den Füßen statt nach dem Becken, F59, ein
Fuß knapp über dem Boden zählt als Stütze, und von 168 leichten Treffern fällt keiner mehr. Schnellere Schritte nach
härteren Stößen, F60, halfen nicht. Auf `active-ragdoll-35` zeichnet die Demo die Figuren wie Gliederpuppen, jeden
Körperteil in eigener Farbe und Form, und der Mensch der Bibliothek steht ein wenig in den Knien, die Beine wie ein A,
und fängt Treffer auf Brust und Bauch besser. Auf `active-ragdoll-36` hat er Körper und Haltung der Vorlage, Arme von
81 cm mit einer Armatur an der Schulter, und ein Arm, den etwas hält oder hebt, gibt locker nach, während die Figur die
Balance hält. Die Demo zeichnet Arme und Beine als Röhren und den Rumpf aus Ringen. Auf `active-ragdoll-37` hat sein
Rumpf vier Stücke über dem Becken wie die Ringe der Vorlage, F63, die beiden der Brust schiebt ein Treffer als einen
Brustkorb, ein leichter Treffer beugt ihn vor allem im Bauch, und beim Aufstehen springt kein Knochen mehr.

Weg zur besten aktiven Ragdoll für das Spiel, besprochen mit dem Nutzer am 1. November 2026: Gemessen an den Kapiteln 3
und 26 bis 30 der Roadmap des Spiels fehlen am meisten Griffe und Würfe, die Wand, Übergänge aus der Bewegung, die
Landung, eigene Rigs, Werkzeuge und das Zurückholen für Replays und PvP, F52 bis F58. Gelernte Steuerungen wie DReCon,
SuperTrack oder PHC sind bewusst nicht dabei: Sie widersprechen 26.22, sind auf allen Plattformen schwer bitgleich zu
halten und brauchen eine große Trainingsumgebung. Reihenfolge nach Wunsch des Nutzers: F50, dann F48, dann der Inspektor
F52, damit jede weitere Runde in der Demo zu sehen ist, dann eigene Rigs F53, damit Griffe, Wand und Landung gleich für
die Rigs des Spiels entstehen, danach F54 bis F58 und die übrigen Schritte.

Bessere Balance, besprochen mit dem Nutzer am 1. November 2026: Die Balance rechnet mit dem Capture Point eines
umgekehrten Pendels, wie es Roboter für Push Recovery tun, und das bleibt ihr Kern. Sie nimmt aber ihre Richtung aus dem
Becken, F59, macht jeden Schritt 0,3 s lang, plant immer nur den nächsten und hält das Becken mit einer Kraft von außen
aufrecht: Ohne sie fällt der Mensch der Bibliothek schon bei 50 N s aus allen vier Richtungen. Ausbau in vier Stufen,
F59 bis F62: die Richtung aus den Füßen, die Dauer jedes Schritts nach dem Stoß, die Balance aus eigener Kraft und
zuletzt die Planung mehrerer Schritte. Neue Reihenfolge nach Wunsch des Nutzers: nach F48 als Runde 30 F59 und F60
zusammen, als Runde 31 F61 auf einer eigenen Branch, dann der Inspektor F52 und die eigenen Rigs F53.

Mehr Stücke im Rumpf, gewünscht vom Nutzer am 1. November 2026, F63: Ein Rumpf aus mehr Stücken übereinander, wie in
Videos anderer aktiver Ragdolls, soll sich im Bauch biegen können wie ein Mensch. Vorgeschlagen als Runde 31, nach F59
und F60 und vor F61, die den Rumpf über die Hüfte regelt und so gleich auf dem neuen Rumpf abgestimmt wird. Damit rückt F61
auf Runde 32.

Kampf mit Widerstand, gewünscht vom Nutzer am 1. November 2026, F64 und F65: Ein geblockter Schlag soll in Schulter und
Ellbogen nachgeben, ein Fuß soll einen Kopf langsam zur Seite drücken und einen Körper wegschieben können, und ein harter
Tritt soll anders wirken als ein leichter, aus der Physik und ohne Reaktionsclips je Stärke. Heute berühren sich zwei
stehende Figuren nicht, Box3D lässt kinematische Körper nicht mit kinematischen kollidieren. Die Architektur bleibt:
eine berechnete Zielpose aus Clips und prozeduralen Schichten und ein Physik-Skelett, das ihr folgt, ohne zweite
Armature, kinematisch, solange nichts stört. Dazu kommt ein aktiv-dynamischer Zustand bei Kontakt, F64, und ein Ziel für
Hand oder Fuß, F65. Die freie Steuerung eines Glieds mit der Maus ist nur ein Werkzeug der Entwicklung in der Demo, das
Spiel nutzt dieselbe Schnittstelle für seine Kampfclips. Vorgeschlagen nach F61, deren Balance sich auch gegen eine
anhaltende Kraft stemmen kann.

Anspannung und Entspannung, gewünscht vom Nutzer am 1. November 2026, F66: Derselbe Tritt soll anders wirken, je
nachdem, ob der Getroffene ihn kommen sah und sich anspannte oder überrascht und locker war. Heute zieht jedes Gelenk
einer Figur mit derselben Steifigkeit und Grenze, ob sie gewappnet ist oder nicht. Das Spiel soll die Spannung je
Körperbereich setzen können, und die Bibliothek hebt oder senkt damit die Gelenke. Vorgeschlagen zusammen mit F64, wo
die Spannung am meisten zählt: wie fest ein Block hält und wie hart ein Fuß drückt.

Stock zum Ausprobieren, gewünscht vom Nutzer am 1. November 2026, Runde 30: Der Nutzer will Treffer und Drücken mit
einem Stock selbst ausprobieren, den er schwingen oder langsam bewegen kann. Das kommt vor die Balance: die erste Stufe
von F64 in der Bibliothek, ein langsames Drücken lässt eine stehende Figur sich fangen, und ein Stock als Werkzeug der
Demo, nur aus einem Körper und einem Motorgelenk von Box3D. Damit rücken F59 und F60 auf Runde 31, F63 auf Runde 32 und
F61 auf Runde 33.

Besserer Stock, gewünscht vom Nutzer am 1. November 2026, Runde 31: Der Stock soll an einem Ende fest in der Hand liegen
und am anderen geschwungen werden, sehr langsam bis sehr schnell, und man soll sich umsehen können, ohne die rechte
Taste zu halten, auch während man schwingt. Beim Ausprobieren fiel ein Fehler der Bibliothek auf: Ein Stock, der langsam
in den Ellbogen drückt, galt am Unterarm als Last von oben und ließ die Figur eine Wand bleiben. Jetzt gibt ein Arm
einem Drücken aus jeder Richtung nach, ein Teil von F64. Damit rücken F59 und F60 auf Runde 32, F63 auf Runde 33 und F61
auf Runde 34.

Stock wie eine Waffe, gewünscht vom Nutzer am 1. November 2026, Runde 32: Der Stock soll mit dem Blick schwingen wie
eine geschwungene Waffe in einem Spiel, statt dass die Maus ihn erst frei vor dem Auge schwingt. Jetzt dreht die Maus
immer den Blick, und der Stock geht mit ihm, sein Spielraum bleibt als Wahl im Menü. Dabei fiel auf, dass der dünne
Stock nach einem harten Schwung endlos um die Hand kreisen konnte, das behebt die Demo mit seiner Trägheit um die
Längsachse. Damit rücken F59 und F60 auf Runde 33, F63 auf Runde 34 und F61 auf Runde 35.

Stock wie ein Stab, gewünscht vom Nutzer am 1. November 2026, Runde 33: Die Handhabung fühlte sich noch komisch an. Der
Stock soll sich dem Blick besser anpassen, ohne einen vorher animierten Hieb, und wie ein Stab mit beiden Händen
gehalten werden, nicht am Ende, sondern etwa bei zwei Dritteln, sein hinteres Ende hinter dem Auge. Gemessen lag der
Stock von Runde 32 bei einem Ruck des Blicks um 1,5 rad in 0,15 s bis 47° neben ihm und schwang danach 44° darüber
hinaus: Seine Federn dämpften gegen die ruhende Welt statt gegen die Hand, am Ende gehalten drehte er sich träge, und
gezeichnet wurde er ein Bild zu spät. Jetzt halten ihn die Hände ein Drittel vom hinteren Ende, das Gelenk läuft ihnen
um die Hälfte des Wegs voraus, den seine Dämpfung ihn zurückhält, und die Demo zeichnet ihn an den Händen von jetzt: 6°
und 1,5°. Die Bibliothek ändert sich nicht, der Stock bleibt ein Werkzeug der Demo aus einem Körper und einem
Motorgelenk von Box3D. Bekommt später eine Figur der Bibliothek eine Waffe in die Hand, etwa mit F64 und F65, gehört
dieser Griff in die Bibliothek. Damit rücken F59 und F60 auf Runde 34, F63 auf Runde 35 und F61 auf Runde 36.

Bessere Balance, Runde 34, F59 und F60 nach der Reihenfolge des Nutzers: F59 ist gebaut. Die Balance richtet sich nach
den Füßen, nicht nach dem Becken, plant jeden Schritt vom Standfuß aus und dreht das Becken über die Füße zurück. Nach
leichten Treffern steht die Figur im Mittel um 7,9 statt 13,3° gedreht, höchstens um 36 statt 67°. Beim Nachverfolgen
der Stürze zeigte sich aber ein anderer Grund als vermutet: Nach einem langen ersten Schritt schwebt der hintere Fuß der
breiten Stellung 0 bis 2,5 cm über dem Boden und verliert immer wieder seine Kontakte. Die Balance zählte ihn als in der
Luft und ließ ihn nach außen treten. Jetzt steht ein Fuß, der nicht schwingt, auch bis 3 cm über dem Boden. Von 168
leichten Treffern fällt so keiner mehr statt vier, an 4 464 Treffern auf Brust und Bauch von 50 bis 89 N s 14 statt 42,
mit 1,24 statt 2,38 Schritten. F60 ist verworfen: Das Schwungbein folgt seinem Ziel etwa 0,1 s zu spät, Schritte von
0,25 und 0,2 s kamen zu kurz, und von den leichten Treffern fielen 22 statt keinem. Ein Schwungbein, das schneller
folgt, wäre die Voraussetzung für einen neuen Versuch. Damit folgt F63 als Runde 35 und F61 als Runde 36.

Figuren wie im Bild, gewünscht vom Nutzer am 1. November 2026, Runde 35: Der Nutzer schickte das Bild einer roten
Gliederpuppe in einer Wüste und Ansichten von der Seite, von vorn oben und von vorn. Die Figuren sollen so aussehen:
jeder Körperteil in eigener Farbe und Form, der Rumpf aus Stücken, die dicht sitzen und sich ein wenig überlappen, die
Brust oben flach mit runden Schultern, das Becken kein Kasten, sondern ein rundes Polster, und eine Haltung wie im Bild,
ein wenig in den Knien, entspannt und bereit, die Beine nicht gerade nebeneinander, sondern wie ein A. Die Formen und
Farben zeichnet die Demo, die Formen der Physik bleiben. Die Haltung gehört in die Bibliothek, weil ein Spiel denselben
Stand braucht: Der Clip `nb_clipIdle` stellt die Füße mit der IK der Beine flach und je 2 cm weiter außen als die
Hüften, das Becken steht 0,96 statt 0,98 m hoch, der Rumpf neigt sich ein wenig über die Füße, und die Arme hängen etwas
vom Körper ab. So fängt die Figur Treffer auf Brust und Bauch besser: Von 50 bis 89 N s fallen 11 statt 14, von 90 bis
139 N s 677 statt 798, auf Arme, Beine und Kopf 326 statt 362. Ohne die 2 cm fielen auf dem Rumpf noch weniger, 0 und
645, mit 4 cm mehr, 24 und 720, siehe „Stand in den Knien“ in [Optimierungen](Optimierungen.md). Dabei zeigten sich
zwei Lücken der Balance: Eine Figur stand still mit dem Capture Point knapp neben den Sohlen und war nie ruhig, und eine
mit dem Fuß unter einem Brocken trat ohne Ende. Jetzt ist eine Figur bis 3 cm daneben ruhig, und wer 8 s lang nicht zur
Ruhe kommt, fällt, siehe „Nicht ewig fangen“. Damit rücken F63 auf Runde 36 und F61 auf Runde 37.

Figuren wie die Vorlage, Teil 2, gewünscht vom Nutzer am 1. November 2026, Runde 36: Arme und Beine sind in der Vorlage
runde Röhren, die Arme länger, die Beine kommen aus einer breiten Hüfte, die Schultern sind größere Kugeln, die Brust
steht vor, der Rumpf ist aus runden Ringen gestapelt statt aus Blöcken, und die Figur steht wie die blaue Gliederpuppe
von der Seite, der Oberarm aus der Schulter nach hinten gewinkelt und der Unterarm nach vorn. Hebt man einen Arm, soll
er locker nachgeben, und die Figur soll die Balance halten. Die Formen zeichnet die Demo, Maße, Haltung und der lockere
Arm gehören in die Bibliothek. Der neue Körper fängt Treffer auf den Rumpf schlechter: Von 90 bis 139 N s fallen 885
statt 676 der 2 064 Treffer, von 50 bis 89 N s 15 statt 11, auf die Glieder 272 statt 326. Gerade Oberarme oder die Arme
von Runde 35 fingen besser, sehen aber nicht aus wie die Vorlage. Dabei fiel auf, dass der längere Arm beim Rudern um
seine eigene Achse schlug, bis 35 rad/s, eine Armatur der Schulter beruhigt ihn. Siehe „Arme und Beine als Röhren“,
„Körper wie die Vorlage“, „Haltung wie die Vorlage“, „Armatur der Schulter“ und „Ein gehaltener Arm wird locker“ in
[Optimierungen](Optimierungen.md). Zwei grüne Bilder einer kräftigeren Figur sind nach dem Nutzer für später, F67, das
dritte, eine schlanke Figur im Gehen, für jetzt. Vorgeschlagen ist F68, die Balance auf den neuen Körper abzustimmen.
Damit rücken F63 auf Runde 37 und F61 auf Runde 38.

Rumpf aus vier Stücken, entschieden vom Nutzer am 2. November 2026, Runde 37, F63: Der Rumpf der Vorlage ist aus fünf
Ringen gestapelt, dem Becken, drei Stücken in der Mitte und der oberen Brust, und die Demo zeichnete sie schon so, die
beiden der Brust aber an einem Knochen. Statt der fünf Stücke über dem Becken, die F63 vorschlug, bekommt jeder Ring
einen Knochen, vier über dem Becken, die Brust teilt sich in eine untere und eine obere, zusammen so schwer wie vorher
und mit dem Schwerpunkt, wo er war. Die Zahl der Stücke kann man später noch ändern. Die Wirbelsäule beugt sich nun vor
allem im Bauch, ihre vier Gelenke sind je viermal so steif wie die anderen, und die beiden Teile der Brust nehmen einen
Treffer zusammen als ein Brustkorb: Jeder für sich ließ die Figur von 50 bis 89 N s 42-mal statt 15-mal fallen, als ein
Brustkorb 19-mal. Die Balance abzustimmen bleibt F68. Beim Messen zeigte sich eine alte Lücke: Eine Figur, die in eine
Wand aus Brettern fliegt, kann über der unteren Kante des Lochs liegen bleiben, ohne Boden unter dem Becken, und wartet
dort für immer, auf `active-ragdoll-36` in 4 von 96 Bruchmustern, mit dem Rumpf aus vier Stücken in 10, siehe F69. Siehe
„Rumpf aus vier Stücken“, „Brustkorb als Ganzes“, „Beugung im Bauch“ und „Aufstehen ohne Sprung“ in
[Optimierungen](Optimierungen.md). Wie mit dem Nutzer besprochen, folgt F68 als Runde 38, F61 rückt auf Runde 39.

Balance für den Körper der Vorlage, Runde 38, F68, wie mit dem Nutzer besprochen: Der Körper der Vorlage verlor vor
allem Treffer von hinten, von 110 bis 139 N s fielen dort 70 von 138 statt 8 auf `active-ragdoll-35`, und 785 der 910
Stürze von 90 bis 139 N s kamen vom Sinken des Schwerpunkts. Drei Ursachen: Die Höhe im Stand, unter deren drei Viertel
die Figur als gefallen gilt, kam aus den Drehpunkten der Knochen in der Ruhehaltung, 1,02 statt 0,94 m, weil Box3D
kinematischen Körpern keinen Schwerpunkt gibt. Mit dem Stand der Vorlage sinkt der Schwerpunkt bei einem langen Schritt
schneller, und der Fuß, der nur 8 cm hob, setzte früh und kurz auf. Und die Arme, die jetzt nach hinten hängen, hielten
0,3 rad Abstand zur Grenze der Schulter und schwangen bei einem Sturz nach vorn nur 0,3 statt 1,3 rad. Jetzt kommt die
Höhe aus der ersten Haltung des Clips im Stand, der Fuß hebt 12 cm, und die Arme schwingen bis an die Grenze der
Schulter. Von 50 bis 89 N s, von 90 bis 139 N s und auf die Glieder fallen 2, 665 und 214 statt 19, 910 und 290, auf
`active-ragdoll-35` 11, 676 und 326, und die Tests bekommen ihre Werte von vor Runde 36 zurück. Siehe „Balance für den
Körper der Vorlage“, „Höhe im Stand“, „Fuß höher beim Schritt“ und „Arme bis an die Grenze der Schulter“ in
[Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 39 F61, die Balance aus eigener Kraft, F69 nach Wunsch
des Nutzers davor oder danach.

Balance aus eigener Kraft, Stufe 1, Runde 39, F61, wie mit dem Nutzer besprochen: über mehrere Runden, in jeder ein
Baustein, jeder gegen die Stütze gegen die Welt gemessen, die Standard bleibt, bis die eigene Balance gleich gut fängt.
Ohne die Stütze fiel der Mensch der Bibliothek schon nach 114 von 168 leichten Treffern, mit 50 statt 250 N m nach 56,
mit 250 N m nach keinem. Die Stütze nimmt die Drehung heraus, die ein Treffer hoch auf der Brust dem Körper gibt,
25 N m s nach 60 N s von vorn. Aus eigener Kraft kann das nur der Boden, und der schiebt dabei den Schwerpunkt in Richtung
der Drehung. Darum rechnet der Capture Point jetzt die Drehung mit, für jede Figur: Mit der Stütze gegen die Welt fallen
von 90 bis 139 N s 628 statt 665 und an den Gliedern 175 statt 214, die Figur fängt sich dort mit 2,0 statt 2,3
Schritten, und von vorn auf die Brust fängt sie von 58 bis 70 N s ein Schritt statt drei. Die Grenze fürs Aufgeben steigt
dafür von 1,1 auf 1,2 m. Die Stütze kann das Becken jetzt auch gegen die Oberschenkel der tragenden Beine halten statt
gegen die Welt, `uprightAgainstWorld`, nach SIMBICON. Damit fallen von den leichten Treffern 87 statt 110, die meisten
durch Sinken und Kippen. Mehr Reibung hilft nicht, der Boden macht dann aus der Drehung Geschwindigkeit. Siehe „Balance
aus eigener Kraft, Stufe 1“, „Capture Point mit Drehung“, „Stütze durch die Beine“ und „Grenze fürs Aufgeben“ in
[Optimierungen](Optimierungen.md). Im Benchmark mit 10 Figuren bleibt jetzt eine über der Kante ihrer Mauer liegen, F69.
Vorgeschlagen ist als Runde 40 F69 oder Stufe 2 von F61, nach Wunsch des Nutzers.

Balance aus eigener Kraft, Stufe 2, Runde 40, F61, wie mit dem Nutzer besprochen, gegen das Sinken: Mit der Stütze durch
die Beine fielen nach Runde 39 von 168 leichten Treffern 87, davon 50, weil der Schwerpunkt zu tief sank. Die Beine
zogen ihre Gelenke nur zur Haltung der IK, und ihre weichen Federn sackten ein, bevor sie den Körper trugen. Jetzt
tragen die Beine am Boden den Körper über ihnen, nach den virtuellen Kräften von Coros 2010: Ein Bein schiebt den Körper
zu seinem Schwerpunkt, so stark, wie es sein Gewicht trägt, von dort, wo der Boden den Fuß drückt, unter dem
Schwerpunkt, solange der über der Sohle liegt, sonst von ihrem Rand, und Hüfte, Knie und Knöchel ziehen um so viel
weiter, wie ihre Federn dafür brauchen, in ihren Grenzen. Das gilt für jede Figur. Mit der Stütze durch die Beine fallen
von den leichten Treffern 43 statt 87, von 50 bis 89 N s 683 statt 1 204, von 90 bis 139 N s 1 550 statt 1 801 und an
den Gliedern 571 statt 818, ganz ohne Stütze 56 statt 101. Mit der Stütze gegen die Welt, die Standard bleibt, fallen
weiter keine leichten Treffer, von 50 bis 89 N s 3 statt 2, von 90 bis 139 N s 577 statt 628 und an den Gliedern 171
statt 175. Auf die obere Brust fängt sie sich von vorn bis 118 N s, auch bei 106, von den Seiten bis 85 und bei 91 statt
bis 94 N s, ein Grenzfall, bei dem der Schwerpunkt schon vorher nur 5 mm über der Grenze blieb. Verworfen: der Schub vom
Knöchel aus, senkrecht nach oben, von der Mitte der Sohle und im Kegel der Reibung, und die Stütze durch die Beine mit
60 oder 150 N m, siehe „Balance aus eigener Kraft, Stufe 2“ und „Beine tragen den Körper“ in
[Optimierungen](Optimierungen.md). Von den 43 leichten Treffern, die mit der Stütze durch die Beine noch fallen, sinken
20 nach einem Treffer von der Seite: Das Becken kippt über dem Standbein, die Stütze drückt das Bein nach außen, der Fuß
rutscht 24 cm, und der Körper sinkt. Das ist Kippen, Stufe 3. Im Benchmark liegt die Figur über der Kante ihrer Mauer,
F69, jetzt mit 50 Figuren statt mit 10, und in der Stadt stürzen fünf statt sechs Etagen ein. Vorgeschlagen ist als
Runde 41 Stufe 3 von F61 oder F69, nach Wunsch des Nutzers.

Euphoria und Overgrowth im Vergleich, am 2. November 2026 auf Frage des Nutzers, übernommen werden nur Ideen: Euphoria
von NaturalMotion, in GTA und Red Dead, ist eine vollständige Körper-KI. Sobald es übernimmt, ist der ganze Körper
Physik, jedes Gelenk ein Motor wie ein Muskel, und Verhaltensregler für Gleichgewicht, Ausfallschritte, Abstützen,
Greifen und Abfangen beim Sturz steuern sie. Die Balance kommt im Kern aus eigener Kraft, mit Schritten und
Körpereinsatz, und jede Reaktion fällt anders aus. Euphoria ist proprietär, ob Spiele dazu verdeckte Hilfskräfte nutzen,
ist nicht offen dokumentiert. Der Preis: teuer je Figur, schwer abzustimmen und kaum vorhersagbar. Overgrowth von
Wolfire ist das Gegenteil: Im Stand gibt es keine Physik-Balance. Die Figur ist animiert, über einen Controller und
prozedurale Animation, und steht, weil die Animation es sagt. Treffer sind Animationen mit prozeduralen Zusätzen, erst
ein schwerer Treffer macht sie zur Ragdoll mit etwas Muskelspannung, damit sie nicht wie tot fällt, danach steht sie mit
einer Animation wieder auf. Nebenan liegt dazwischen: im Stand animiert wie Overgrowth, nach harten Treffern dynamisch
mit Schritten wie Euphoria, dazu die begrenzte Stütze gegen die Welt, und F61 baut daneben die Balance aus eigener Kraft
auf. Empfehlung für das Kampfspiel, nach Kapitel 26.1 der Roadmap des Spiels, „Fake it Realism“ statt vollständiger
Körper-KI, und der Matrix in 26.4: Die Overgrowth-Basis bleibt, animiert im Stand, präzise, schnell steuerbar und
billig. Physisch abgefangen werden nur mittlere und starke Treffer, wie heute. Die Stütze gegen die Welt bleibt im PvP
Standard: Ob ein Treffer umwirft, soll vom Treffer abhängen, nicht von Millimetern der Physik, und mit reiner Eigenkraft
würde das zum Glücksspiel. Nach 94 N s von der Seite hängt der Fang auf `active-ragdoll-40` an 5 mm Höhe des
Schwerpunkts. Die Bausteine der eigenen Kraft werden trotzdem weitergebaut, soweit sie auch die Stütze gegen die Welt
verbessern: Je mehr die Beine selbst tragen, desto weniger muss sie tun und desto echter sieht es aus. Mit Runde 40
fallen mit ihr von 90 bis 139 N s 577 statt 628 Treffer, und nach 60 N s von vorn steht der Schwerpunkt 4 statt 9 cm
tiefer als im Stand. Die Balance aus eigener Kraft als Standard, Stufe 4 von F61, ist nicht das Ziel für den PvP-Modus,
sondern eine Option, etwa für einen realistischen Modus. Auch für den Riesen sieht Kapitel 11 der Roadmap des Spiels
eine assistierte Körperbalance vor. Entschieden wird das vom Nutzer.

Blade & Sorcery als vierte Referenz, am 2. November 2026 nach einer Recherche des Nutzers, neben Euphoria, Overgrowth
und Exanima, dessen Figuren nach Angaben seiner Entwickler ganz physikalisch bewegt werden. Übernommen werden nur Ideen:
Blade & Sorcery verbindet Kampfanimationen mit physikalisch simulierten Körpern, und ein Körper wird nicht erst beim Tod
zur Physik. Animationen geben Angriffe, Ausweichen, Parieren und Fortbewegung vor, die Physik bringt Reaktionen auf
Treffer, Kollisionen, Greifen und Schubsen, und bei Kontrollverlust, schweren Treffern oder Tod übernimmt die Ragdoll.
Nach der Recherche beschreibt die Dokumentation des SDK Federn und Dämpfer, die Körperteile mit begrenzter Kraft zu den
Positionen und Drehungen der Animation ziehen, dazu Zustände wie Stehen, Destabilisiert, Kinematisch und Eingefroren.
Treffer wirken in vier Stufen, den Push Levels 0 bis 3, je nach Art und Stärke bis zur vollen Ragdoll, und seit Update
10 schaltet das Spiel die volle Physik eines Gegners erst ein, wenn er am Kampf beteiligt ist. Offen ist, ob die Gegner
im Stand mit Schritten aus eigener Kraft balancieren oder ob Animation und Hilfskräfte sie halten. Die Angaben stammen
aus dem Wiki, der Steam-Community, SteamDB und der Dokumentation des SDK. Die Federn, die Zustände und das Einschalten
der Physik sind am selben Abend in der Dokumentation des SDK selbst nachgelesen, siehe den nächsten Absatz, die übrigen
Angaben nicht. Bewertung für Nebenan: Die Bibliothek tut im Kern schon dasselbe. Im Stand ist eine Figur animiert, beim
Abfangen ziehen ihre Gelenke mit begrenzter Kraft zur Haltung des Clips wie die Federn von Blade & Sorcery, und dessen
Stufen entsprechen ihren Schwellen: Zucken unter `knockdownImpulse`, Abfangen mit Schritten, Umwerfen, dazu `pushForce`
für langsames Schieben, und der Matrix in Kapitel 26.4 der Roadmap des Spiels. Federn, die Körperteile zu Positionen der
Animation in der Welt ziehen, sind eine Stütze von außen wie die gegen die Welt, siehe den nächsten Absatz, und sprechen
nicht gegen die Empfehlung, sie im PvP als Standard zu behalten. Was Nebenan fehlt, ist der Rest von F64: Kämpfer, die
während ihrer Kampfclips dynamisch sind, solange sie sich berühren, so dass man sie schieben, greifen und ihre Schläge
blocken kann, mit Motoren, die den Clips genau genug folgen. Dafür ist Blade & Sorcery die beste Referenz, und die
Physik nur für Figuren einzuschalten, die am Kampf beteiligt sind, passt dazu und spart Rechenzeit. Für den PvP-Modus
gilt weiter: Wann eine Figur stolpert oder fällt, bestimmen Impuls, Richtung und feste Schwellen, nicht Millimeter der
Physik.

Nachgelesen in der Dokumentation des SDK von Blade & Sorcery am 2. November 2026, auf den Seiten Ragdoll und Ragdoll
Part: Federn ziehen die Körperteile zu ihren Positionen in der Animation, `springPositionForce`, und drehen sie zu ihren
Drehungen in der Animation, `springRotationForce`, gedämpft und begrenzt durch `maxPositionForce` und
`maxRotationForce`, mit Faktoren für jedes Teil. Die Teile hängen mit passiven Kugelgelenken mit Grenzen,
`CharacterJoint`, an ihrem Elternteil. Die Zustände heißen Inert, Destabilized, Frozen, Standing, Kinematic, NoPhysic
und Disabled, beschrieben sind sie nicht. Destabilisiert werden die Drehfedern mit eigenen Faktoren schwächer, getrennt
danach, ob die Figur den Boden berührt. `hipsAttached` heftet die Hüfte an einen Punkt der Welt, und `physicToggle`
schaltet die Physik nur in einem Radius um den Spieler und um andere Ragdolls ein, für eine einstellbare Dauer. Wo die
Federn ansetzen, sagt die Dokumentation nicht. Ein Teil zu einer Position ziehen können die passiven Gelenke aber nicht,
und Kräfte zwischen den Teilen verschieben den Schwerpunkt nicht: Es ist eine begrenzte Kraft von außen an jedem Teil.
So weit die Dokumentation reicht, ist die offene Frage damit beantwortet: Die Gegner stehen nicht durch eigene Balance.
Federn ziehen sie zur Animation, wie die Stütze gegen die Welt das Becken der Bibliothek hält, nur an jedem Teil und
auch für die Position. Schieben und greifen lassen sie sich, während sie ihre Kampfanimation weiter ausführen, weil eine
Kraft über der Grenze ein Teil auslenkt und die Animation es danach zurückzieht. Das ist einfach und vorhersagbar und
passt zum PvP: Ob ein Teil nachgibt, entscheidet eine feste Grenze, nicht Millimeter der Physik. Die Bibliothek hat
davon schon die Gelenke, die beim Fangen mit begrenzter Kraft zur Haltung des Clips ziehen, seit Runde 20, mit Muskeln
seit Runde 25 und mit Reaktionen je Körperteil seit Runde 22, und die Stütze gegen die Welt für das Becken. Es fehlt der
Rest von F64: Im Stand ist eine Figur der Bibliothek ganz kinematisch, und zwei stehende Figuren berühren sich nicht,
weil Box3D kinematische Körper nicht miteinander kollidieren lässt. Der Weg nach Blade & Sorcery steht bei F64. Die
Arbeit an F61 bleibt für das Aussehen nach einem Treffer wertvoll, aber was die Gegner von Blade & Sorcery trägt, ist
sie nicht.

Wer wann zur Animation gezogen wird, Nebenan und Blade & Sorcery im Vergleich, am 2. November 2026 auf Frage des
Nutzers. Beide sind aktive Ragdolls, aber verschiedener Art. Blade & Sorcery führt seine Gegner wie an Fäden: Ihr Körper
ist im Kampf physikalisch, die Animation zieht ihn von außen, und er steht, weil die Federn ihn halten. Eine Figur von
Nebenan ist im Stand keine Physik, sondern Animation, und erst nach einem harten Treffer eine aktive Ragdoll, die sich
aus eigener Kraft mit Schritten fängt, mit einer Stütze nur am Becken. Nach einem Treffer ist Nebenan weiter, im
laufenden Kampf Blade & Sorcery. F64 bringt beides zusammen.

| Lage | Blade & Sorcery, nach der Dokumentation des SDK | Nebenan bis Runde 42 | Nebenan mit F64 |
| --- | --- | --- | --- |
| Stand und Kampf ohne Treffer | Physikalischer Körper: Begrenzte Federn ziehen jedes Teil von außen zur Animation, Position und Drehung. Man kann ihn schieben und greifen, und er stößt mit anderen zusammen | Reine Animation, kinematisch: Die Knochen werden auf die Haltung des Clips gesetzt, nicht gezogen. Ohne Physik berühren sich zwei stehende Figuren nicht. Ein Schieben ab `pushForce` weckt die Figur, dann fängt sie sich | Wie Blade & Sorcery: Kämpfer in Kontakt oder nahe am Gegner werden dynamisch und folgen ihrem Clip mit begrenzten Federn |
| Leichter Treffer | Die Federn geben nach und ziehen das Teil zurück. Welche Reaktion ein Treffer auslöst, wählen nach dem Wiki die Push Levels | Ein berechnetes Zucken auf der Animation, siehe F31 und F50 | Die Federn geben nach und ziehen das Teil zurück |
| Harter Treffer, Schub, Explosion | Destabilized: Die Drehfedern werden schwächer, der Gegner taumelt oder fällt. Dass er sich mit Schritten fängt, steht nirgends | Aktive Ragdoll mit eigener Balance: Die Gelenke ziehen mit begrenzter Kraft zur Haltung des Clips, die Figur fängt sich mit Schritten nach dem Capture Point, die Arme rudern, der Oberkörper nimmt die Drehung auf, und eine begrenzte Stütze hält nur das Becken | Wie bis Runde 42: Die Federn lassen los, und die Figur fängt sich mit Schritten |
| Sturz | Ragdoll, danach Aufstehen nach einer Kurve, `standUpCurve` | Ragdoll mit Restspannung, danach Aufstehen mit einem Clip | Wie bis Runde 42 |

Ein Tritt gegen einen Gegner, Schritt für Schritt, am 2. November 2026 auf Frage des Nutzers. Komplexe Kampfbewegungen
gehen schon heute: Das Spiel spielt eigene Clips mit `nbFigure_PlayClip`, und die Figur ist dann die Animation, Knochen
für Knochen. Was fehlt, ist der Kontakt zwischen zwei Kämpfern: Bis Runde 42 geht ein Fuß durch den Block, weil beide
Figuren kinematisch sind. F64 und F65 schließen das.

| Schritt | Bis Runde 42 | Mit F64 und F65 |
| --- | --- | --- |
| Die Kampfbewegung | Das Spiel spielt eigene Clips mit `nbFigure_PlayClip`, die Figur ist die Animation, Knochen für Knochen. Ein Tritt tritt, wie der Clip es will, die Fuß-IK lässt den Fuß los, sobald der Clip ihn hebt | Kommen sich zwei Kämpfer nahe, werden beide dynamisch und folgen ihren Clips mit begrenzten Federn, so genau, dass ein Schlag präzise bleibt |
| Der Tritt trifft den Block | Beide Figuren sind kinematisch, und Box3D lässt kinematische Körper nicht miteinander kollidieren: Der Fuß geht durch den Block. Das Spiel erkennt den Treffer selbst und spielt etwa einen Clip für den Block | Der Fuß trifft den Block physikalisch. Ist der Tritt stärker als die Federn des Arms, geben Schulter und Ellbogen nach, sonst hält der Block den Fuß auf, und das eigene Bein spürt die Gegenkraft |
| Der Tritt trifft ohne Block | Das Spiel ruft `nbFigure_ApplyHit` mit dem Impuls des Tritts an der Trefferstelle auf. Unter `knockdownImpulse` zuckt der Gegner, darüber wird er zur aktiven Ragdoll | Der Fuß trifft den Körper, und der Tritt wirkt nach Geschwindigkeit und Masse hinter dem Fuß |
| Danach | Der Gegner folgt nicht mehr der Bewegung des Clips durch die Welt, sondern dem Stoß und seiner eigenen Balance: Schritte nach dem Capture Point, rudernde Arme, der Oberkörper nimmt die Drehung auf. Seine Gelenke ziehen weiter mit begrenzter Kraft zur Haltung des Clips, so behält er Körperspannung und fällt nicht schlaff um. Ruhig wird er wieder Animation | Bringt ihn der Tritt aus dem Gleichgewicht, lässt er die Federn zur Animation los und fängt sich mit Schritten wie bis Runde 42: derselbe Körper mit denselben Gelenkmotoren, nur ohne die Fäden zur Animation |

Runde 43 baut davon den ersten Teil von F64: dynamisch und mit begrenzten Federn dem Clip folgen, solange ein Gegner nah
ist. F65 lässt danach das Spiel Hand und Fuß führen, so dass ein Tritt nach der Kraft wirkt, die hinter ihm steckt.

Balance aus eigener Kraft, Stufe 3, Runde 41, F61, wie mit dem Nutzer besprochen, gegen das Kippen: Mit der Stütze durch
die Beine fielen nach Runde 40 von 168 leichten Treffern 43, 20 davon nach einem Treffer von der Seite, bei dem das
Becken über dem Standbein kippte. Geplant war, dass die Hüfte den Oberkörper gegen die Drehung aufrecht hält und die
Arme nach dem Drehimpuls schwingen. Gemessen half beides nicht: Gegen die Neigung aufrecht gehalten, drückte der
Oberkörper das Becken weiter um, und es fielen 82 statt 43, die Arme änderten nichts. Es hilft das Gegenteil, die
Hüftstrategie von Horak und Nashner 1986: Die Wirbelsäule beugt den Oberkörper mit der Neigung des Beckens weiter, um
das Doppelte der Neigung und höchstens 0,5 rad, verteilt auf ihre vier Gelenke. Das Drehmoment, das den Oberkörper
weiterdreht, dreht das Becken zurück, und richtet sich das Becken auf, streckt sich der Oberkörper mit. Das gilt für
jede Figur. Mit der Stütze durch die Beine fallen von den leichten Treffern 17 statt 43, von 50 bis 89 N s 385 statt
683, von 90 bis 139 N s 1 456 statt 1 550 und an den Gliedern 465 statt 571, ganz ohne Stütze 21 statt 56. Mit der
Stütze gegen die Welt, die Standard bleibt, fallen weiter keine leichten Treffer, von 50 bis 89 N s keiner statt 3, von
90 bis 139 N s 385 statt 577 und an den Gliedern 133 statt 171. Auf die obere Brust fängt sie sich von den Seiten bis
103 und 109 N s statt bis 85, schräg bis 112 bis 118 statt 100 bis 103, von vorn und hinten wie vorher bis 118 und
130 N s. Bis 90 N s bleibt ihr Kopf höher als vorher, die Brust neigt sich aber weiter, bei 50 N s bis 0,40 statt
0,23 rad: Die Figur nimmt einen Treffer sichtbar mit dem Oberkörper auf. Die Stärke ist gemessen: Bis zum Doppelten
fängt jede Stufe mehr, darüber fallen mit der Stütze durch die Beine von 90 bis 139 N s wieder mehr, und das Fangen
dauert länger. Verworfen ist auch eine Stütze durch die Beine, die nur hält, was der Standfuß gegen den Boden hält,
siehe „Balance aus eigener Kraft, Stufe 3“ und „Oberkörper nimmt die Drehung auf“ in [Optimierungen](Optimierungen.md).
Nebenbei gefunden: Das Parallelgelenk von Box3D, das beide Stützen bildet, hält höchstens die Hälfte seines Drehmoments,
die 250 N m von `uprightTorque` wirken als 125, F70. Von den 17 leichten Treffern, die mit der Stütze durch die Beine
noch fallen, sinken 12, davon 9 nach Treffern von der Seite ab 72 N s, 4 kippen, und einer fällt nach dem fünften
Schritt. Im Benchmark stehen mit 50 Figuren am Ende alle, und in der Stadt fangen sich 10 statt 4 Figuren, sechs statt
fünf Etagen stürzen ein, und ein Bild kostet das 1,06-fache. Vorgeschlagen ist als Runde 42 F69, F70, Stufe 4 von F61
oder der Rest von F64 auf dem Weg von Blade & Sorcery, nach Wunsch des Nutzers.

Die Stütze hält, was ihre Zahl sagt, Runde 42, F70, nach dem, was am meisten Sinn macht, wie der Nutzer es wollte: F64
soll Kämpfer mit begrenzten Federn ihrem Clip folgen lassen, und vorher musste feststehen, dass die Grenzen von Box3D
halten, was ihre Zahl sagt. Geprüft sind alle Gelenke, die die Bibliothek mit Grenzen nutzt. Kugelgelenk, Scharnier und
Motorgelenk legen ihre Impulse auf Achsen der Länge eins und halten ihre Grenzen genau, nur das Parallelgelenk der
Stütze hielt die Hälfte. Seine Grenze in Box3D ist jetzt doppelt so hoch, der Standard von `uprightTorque` 125 statt
250 N m, und alles rechnet bitgleich wie vorher: alle Tests mit ihren Prüfsummen, die Treffer auf Brust, Bauch und
Glieder mit beiden Stützen und ohne Stütze, und jede Szene des Benchmarks mit einem und mit vier Threads. Der neue Test
`ParallelJointTorqueTest` misst das Drehmoment des Gelenks an seiner Grenze, vorher 9,89 von 20 N m, jetzt 19,77.
Wirklich 250 N m, der zweite Weg, sind nicht Standard geworden: Mit der Stütze gegen die Welt fallen dann von 90 bis
139 N s 252 statt 385 Treffer, mit der Stütze durch die Beine aber von den leichten Treffern 29 statt 17, und ob ein
Treffer umwirft, hinge noch mehr von der Stütze ab. Ein Spiel, das es will, setzt `uprightTorque` auf 250. Vorgeschlagen
ist als Runde 43 der Rest von F64 auf dem Weg von Blade & Sorcery.

Kämpfer machen die Bewegung selbst, Runde 43, F64 Stufe 2, auf dem Weg von Blade & Sorcery. Während der Runde sagte der
Nutzer, worauf es ankommt: Die Animation gibt nur die Keyframes vor, der aktive Ragdoll soll die Bewegungen wirklich
machen. So ist es jetzt gebaut, für jede Figur mit `nbFigureDef::engageRadius`, Standard 0 und damit aus:

- **Wann:** Steht das Becken einer anderen Figur näher als dieser Radius, folgt eine stehende Figur ihrem Clip mit
  dynamischen Knochen. Eine Sekunde nachdem die letzte gegangen ist und wieder auf ihrem Clip, ist sie kinematisch.
  `nbFigureInfo::following` sagt es, `nbFigureStats::followingCount` zählt sie.
- **Wie:** Der Clip gibt nur die Haltung. Jedes Gelenk zieht mit voller Kraft zu ihr und weiß, wie schnell der Clip es
  dreht: Box3D hat dafür eine Zielgeschwindigkeit der Gelenkfedern bekommen, `b3SphericalJoint_SetTargetVelocity` und
  `b3RevoluteJoint_SetTargetSpeed`. Ohne sie dämpft eine Feder jede schnelle Bewegung gegen den Stillstand. Dazu zieht
  ein Motorgelenk jeden Knochen zu seiner Lage im Clip, so stark, wie der Clip ihn bewegt, Masse mal Beschleunigung,
  und mit seinem Gewicht mal `followStrength`, Standard 1, gegen Störungen. Becken und Beine halten so auch ihre
  Drehung.
- **Wie genau:** Im Idle bleibt jeder Knochen 2,6 mm am Clip, die Faust eines Schlags, der in 0,12 s ausfährt, 4,1 cm.
- **Was ein Schlag tut:** Er trifft die Deckung oder den Körper. Durch eine kinematische Figur geht die Faust bis
  65 cm vor, in eine folgende bis 58 cm, und die Kraft dahinter drückt den Getroffenen 8 cm zurück. Der Treffer zählt
  mit dem Impuls seines Kontakts, nach denselben Regeln wie bei einer kinematischen Figur: Er zuckt, hier um 0,28 rad,
  tritt nach oder fängt sich, und wer schlägt, spürt den Treffer auch. Ein Treffer des Spiels mit 30 N s lässt eine
  folgende Figur zwei Schritte machen, einer mit 60 N s sie sich fangen, danach folgt sie wieder.
- **Wann sie loslässt:** Wird ihr Becken 20 cm oder 0,5 rad aus dem Clip gedrückt, bleibt ein Knochen eine halbe
  Sekunde 25 cm daneben, etwa eine Faust, die unter dem Arm des anderen klemmt, oder drücken andere Figuren sie bei
  zwei Prüfungen hintereinander mit mehr als ihrem halben Gewicht waagerecht, dann lässt sie los und fängt sich mit
  ihrer eigenen Balance, `nbFigureInfo::letGoCount`. Danach folgt sie wieder. Läuft eine Figur mit 1 m/s in eine
  andere, lassen beide los, fangen sich, und keine fällt.
- **Was sie kostet:** Eine folgende Figur kostet auf der Cloud-VM mit einem Thread rund 0,065 ms pro Schritt statt
  0,005 ms kinematisch, neun Zehntel davon im Löser von Box3D, der ihre 19 dynamischen Knochen mit Gelenken und
  Antrieben rechnet, das Update der Bibliothek 7 statt 2 µs. 100 Kämpfer in 50 Paaren kosten 6,8 ms pro Schritt mit
  einem Thread und 3,4 ms mit vier, kinematisch 0,5 und 0,6 ms. Wer allein steht oder weiter als sein Radius von allen
  anderen, bleibt kinematisch und kostet nichts dazu.
- **Was gleich bleibt:** Ohne `engageRadius` rechnet alles bitgleich wie vorher, alle Tests mit ihren Prüfsummen und
  jede Szene des Benchmarks, auch mit der neuen Zielgeschwindigkeit in Box3D. Gepaart gegen `active-ragdoll-42`, vier
  Läufe im Wechsel mit vier Threads und zwei mit einem, enden alle Szenen mit denselben Prüfsummen, mit einem und mit
  vier Threads. Die Stadt mit Figuren kostet 0,98 und 0,97 der Zeit davor, die Ragdolls im Schutt 0,95 und 0,97. Die
  kleinen Szenen mit 1, 10 und 50 Figuren schwankten in zwei solchen Vergleichen paarweise zwischen 0,5 und 2,0 der
  Zeit davor. Dieselben 50 Figuren ohne den Benchmark drumherum kosten zwölfmal im Wechsel 0,92 der Zeit davor, und
  Valgrind zählt für 50 Figuren, die stehen, zucken und fallen, 0,03 % mehr Instruktionen. Die waren es erst, nachdem
  das Kugelgelenk die Zielgeschwindigkeit mit einem Merker statt mit drei Vergleichen prüft, vorher 0,5 %.
- **Im Benchmark:** `nebenan_benchmark figures` misst jetzt auch 25 Kämpferpaare, die 5 s einander gegenüber folgen,
  5 s lang Treffer des Spiels mit Schritten abfangen, ohne loszulassen, und dann auseinandergehen. 50 Kämpfer kosten
  3,4 ms pro Schritt mit einem Thread und 1,6 ms mit vier, Box3D-Schritt und Update, und stehen am Ende alle wieder
  kinematisch, mit einem und mit vier Threads mit derselben Prüfsumme.

Die Demo hat dafür die Szene „Kampf“: zwei Paare, in jedem schlägt einer alle zwei Sekunden geradeaus, im einen hält der
andere die Deckung, im anderen steht er offen, und über jeder Figur steht „folgt dem Clip“. Der Stock trifft sie wie
jeden Körper. Der Weg dahin hat sieben Varianten verworfen, mit Gründen unter „Kämpfer machen die Bewegung selbst“ in
[Optimierungen](Optimierungen.md): Drehantriebe an jedem Knochen zittern, Antriebe mit fester Grenze lassen die Faust
37 cm hinter dem Clip, Gelenkfedern ohne Zielgeschwindigkeit 9 statt 4 cm, mit der Stärke 2 drücken sich zwei Kämpfer
mit bis zu 2 000 N und fallen beim Zusammenstoß beide, mit 0,5 kleben die Füße am Boden, nur über die Physik bewegt ein
Schlag den Getroffenen um 6 mm, und Drücken anderer Figuren als Stoß nach `pushForce` hielt zwei sich fangende Figuren
20 s und länger aneinander. Offen bleiben: F65, also Hand und Fuß nach einem Ziel führen. Eine Figur allein immer
physikalisch folgen lassen, wenn der Nutzer das will. Und Schritte, wenn das Spiel einen folgenden Kämpfer über den
Boden bewegt, statt rutschender Füße, die heute mit der Stärke 1 noch gleiten. Und die Suche nach dem Gegner prüft
jede Figur mit Radius gegen alle anderen, für Tausende Kämpfer in einer Welt bräuchte sie ein Gitter. Vorgeschlagen
ist als Runde 44, nach Wunsch des Nutzers, F65, Schritte für einen folgenden Kämpfer, den das Spiel bewegt, oder eine
Figur, die auch allein immer folgt.

Durchsicht fremder aktiver Ragdolls am 31. Oktober 2026, auf Wunsch des Nutzers: BIMOS-Active-Ragdolls (Unity, VR),
Kickback (Godot, nach Euphoria), Active-Ragdoll-Character, Unity.Humanoid.ActiveRagdoll, ActiveRagdoll von ashleve und
seine Rust-Portierung active-ragdoll-rs sowie active-ragdolls von sergioabreu-g. Übernommen werden sollen nur Ideen,
F31 bis F44, keine Zahlen: Was BIMOS und Kickback einstellen, steht dort nur zum Vergleich. Ihre Figuren haben andere
Massen, Antriebe und Physik, die Werte der Bibliothek werden an ihren eigenen Figuren gemessen und abgestimmt, auf
Wunsch des Nutzers. Nicht übernommen und warum: Kickbacks Muskeln überschreiben die Geschwindigkeit der Körper und kennen
keine Masse, ein Treffer ist nach drei Schritten zu 96 % weg, seine Schwerkraft schwindet mit der Kraft, und sein
Stolpern schiebt die Wurzel der Figur geskriptet, mit Schritten nach der Strecke statt nach der Balance. Das sagt sein
eigenes Audit. Die Bibliothek zieht mit begrenzten Drehmomenten und tritt dorthin, wohin der Capture Point zeigt.
ActiveRagdoll von ashleve und Unity.Humanoid.ActiveRagdoll ziehen jeden Knochen mit Kräften zur Animation und nehmen der
ganzen Figur bei Kontakt die Kraft. Active-Ragdoll-Character hält die Balance, solange ein Strahl vom Becken nach unten
Boden trifft. BIMOS trägt 85 % des Gewichts mit einer Kraft am Becken und steht laut seiner eigenen Doku eher
hochgezogen auf als ein Aufstehen aus Motion Capture, die Bibliothek steht mit Clips auf. Der GTA-Gist ist geleakter
Code eines frühen Prototyps aus GTA San Andreas ohne Gelenkantrieb und ohne Balance und darf nicht verwendet werden.
Physics-Based-Bodies ist ein Weichkörper ohne Skelett, DDGI ist Beleuchtung. Die Schlaghilfe von BIMOS lässt die Fäuste
seiner NPCs treffen, das ist Sache des Kampfsystems, die Angriffe einer Figur der Bibliothek kommen aus ihren Clips.

Zweite Durchsicht fremder Projekte am 2. November 2026, auf Wunsch des Nutzers: Physicanim (Unity, MIT),
godot-active-ragdoll (Godot 4, MIT), bevy-ragdoll (Bevy, Apache-2.0 oder MIT), Simbody (Apache-2.0), OpenMotion (Unreal,
nach eigener Angabe MIT) und zwei Ablagen mit Morpheme und Euphoria. Übernommen werden wie immer nur Ideen, keine
Zahlen. Was davon nützt, steht als F71 bis F74 in der Tabelle und als Ergänzung bei F32, F40, F41, F43, F64 und F65.

- **Nicht angefasst:** `NaturalMotion-MorphemeRuntime-5.2-vs2022` nennt sich selbst eine veränderte kommerzielle Ausgabe
  von NaturalMotions morpheme:runtime 5.2, `Morpheme-Euphoria-Fixed` eine für Morpheme angepasste Euphoria, mit
  Binärdateien zum Disassemblieren, beide ohne Lizenz. Das ist proprietärer Code ohne Erlaubnis, wie der durchgesickerte
  Code aus GTA: nicht gelesen, nichts übernommen. Was über Euphoria öffentlich bekannt ist, kennt die Roadmap schon aus
  Kickback und aus Vorträgen.
- **Physicanim:** Gelenkantriebe zur Animation, das Becken mit einer Feder an das animierte Becken gebunden, ein Sturz,
  wenn das Becken zu weit neben der Mitte der Füße liegt, und beim Aufstehen ziehen Federn und Grenzen langsam wieder
  an. Das kann die Bibliothek alles schon, nichts Neues.
- **godot-active-ragdoll:** Am nächsten an Runde 43. Sein Puppenmodus führt jeden Körper mit der Geschwindigkeit der
  Animation und einer Korrektur nach dem Abstand, die Füße fest, die Hände weich, und liegt das Becken zu weit weg,
  setzt er alles auf die Animation zurück, wie die Antriebe der Kämpfer und `nbFigure_Move`. Neu für uns: Ein Treffer
  schwächt Antriebe und Muskeln des getroffenen Glieds und seiner Kinder, den Elternknochen weniger, hält sie eine Weile
  schwach und lässt sie dann wieder anziehen, ein Treffer gegen ein Bein lässt auch das andere Knie nachgeben, und die
  Wirbelsäule behält eine Mindestspannung, siehe F32. Es misst, wie weit der Schwerpunkt der Körper waagerecht neben dem
  der Animation liegt, und gibt das dem Spiel, damit dessen Fortbewegung dorthin tritt, F71. Zum Sterben lässt es die
  Muskeln gruppenweise nach, zuerst die Beine, dann Rumpf und Kopf, zuletzt die Arme, F40. Ein leichter Treffer macht
  dort nur Rumpf, Kopf und Arme dynamisch, und die Beine laufen mit der Animation weiter, F72. Beim Wechsel zur Physik
  nimmt es Paare von Knochen, die sich in der Haltung tief überlappen, aus der Kollision, bis sie sich getrennt haben,
  und für eine Weile den Körper, der die Figur umwirft, etwa ein Fahrzeug. Bei uns ist beides noch nicht aufgefallen, im
  Blick behalten. Fällt eine Figur dort nach hinten, zieht sie das Kinn ein, das tut das Einrollen der Bibliothek schon.
  Dass es Gelenke als Geschwindigkeitsmotoren statt als Federn mit Drehmoment rechnet, weil diese bei leichten Körpern
  unruhig werden, lösen bei uns die weichen Gelenke von Box3D.
- **bevy-ragdoll:** Dort läuft die Schwächung eines Treffers den Baum entlang, zu den Kindern weiter als zu den Eltern,
  um so weiter, je stärker der Treffer, mit einer Untergrenze je Körperteil, siehe F32. Treffer kurz hintereinander
  schwächen stärker, F41, und die Kraft kommt je Körperteil nach eigener Verzögerung zurück, so in einer gewählten
  Reihenfolge. Seine Antriebe ziehen mit dem Abstand vom Ziel schwächer, statt bei großen Fehlern große Stöße zu geben:
  eine weichere Form unserer Regel, bei 20 cm loszulassen, F64. Ist die Obergrenze dynamischer Figuren voll, nimmt es
  deterministisch die älteste heraus und meldet das dem Spiel. Bei uns ginge das nur für folgende Kämpfer, die dabei
  kinematisch stehen bleiben, nie für einen fallenden Ragdoll, der in der Luft erstarren würde, F43. Sein Gelenkregler
  ist ein stabiler PD-Regler, der einen Teil der Dämpfung implizit mit dem Zeitschritt nimmt, das tun bei uns die
  weichen Gelenke von Box3D schon.
- **Simbody:** Mehrkörperdynamik in inneren Koordinaten nach Featherstone, in O(n), mit Vorwärts-, inverser und
  gemischter Dynamik und vorgegebenen Bewegungen, die Grundlage von OpenSim. Den Löser von Box3D ersetzt das nicht. Die
  Idee daraus ist die inverse Dynamik: Aus dem Clip rechnet sie für jedes Gelenk das Drehmoment, das seine Bewegung samt
  allem, was es trägt, braucht, und die Gelenke eines folgenden Kämpfers bekämen es vorab mit, F73. Dann könnten Federn
  und Antriebe weicher sein, ohne dass ein Schlag nachläuft, und ein Arm, der etwas trifft, gäbe nach wie ein Arm. Mit
  einem Optimierer wie CMA-ES, den Simbody mitbringt, ließen sich außerdem mehrere Zahlen der Balance zugleich gegen die
  gemessenen Treffertabellen abstimmen, F74.
- **OpenMotion:** Profile, die einen Teil des Skeletts physikalisch machen, etwa alles unter einem Knochen, und ein
  Gewicht je Knochen zwischen Animation und Physik, wie F72. Motion Matching wählt die Animation, das macht ein Spiel
  selbst, keine Physik. Sein FABRIK käme für F65 in Frage, wenn eine Hand ihr Ziel auch über die Wirbelsäule erreichen
  soll, Arme und Beine löst die Zwei-Knochen-IK der Bibliothek schon.

Etwas kürzere Arme, Runde 44, auf Wunsch des Nutzers, der dabei auch daran erinnerte, dass die Balance noch nicht fertig
ist. Oberarm und Unterarm des Menschen der Bibliothek sind 35 und 26 statt 37 und 27 cm lang, die Hand bleibt 17 cm,
zusammen 78 statt 81 cm, die Massen bleiben. Mit der Stütze gegen die Welt, dem Standard, fängt die Figur fast genau wie
vorher: Von den leichten Treffern und von 50 bis 89 N s fällt weiter keiner, von 90 bis 139 N s weiter 385, an den
Gliedern 135 statt 133. Mit der Stütze durch die Beine fallen von den leichten Treffern 25 statt 17, von 50 bis 89 N s
389 statt 385, von 90 bis 139 N s 1 449 statt 1 456 und an den Gliedern 479 statt 465: Der kürzere Arm hilft beim Rudern
etwas weniger. Der gerade Schlag der Kämpfer reicht 62 statt 65 cm, und wen er trifft, der zuckt nur noch, statt zwei
Schritte zurückzutreten, sein Stoß lag knapp über der Schwelle für einen Schritt. Gepaart gegen `active-ragdoll-43`,
vier Läufe im Wechsel mit vier Threads und zwei mit einem, rechnen alle Szenen ohne Figuren dasselbe, und die mit
Figuren enden mit einem und mit vier Threads gleich. Bei 10 Figuren hinter brechender Deckung liegt am Ende eine unter
Trümmern der Mauer und wartet, bis sie frei ist, bei 50 steht die letzte 15 s später wieder. In der Stadt mit Figuren
stürzen 4 statt 6 Etagen ein, und ein Bild kostet dort das 0,83- und 0,86-fache. Siehe „Etwas kürzere Arme“ in
[Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 45 die Balance: Stufe 4 von F61, die Stütze durch die
Beine so gut machen wie die gegen die Welt, zuerst an den 25 leichten Treffern, die mit ihr noch fallen, und dem Sinken
nach Treffern von der Seite.

Sohlen, die greifen, Runde 45, ein Baustein von Stufe 4 von F61, auf Wunsch des Nutzers zurück bei der Balance.
Nachverfolgt fielen mit der Stütze durch die Beine viele, weil ein Fuß rutschte: Nach 80 N s von der Seite rutschte der
Standfuß 28 cm, während der andere 70 cm hinaustrat, und die Figur sank. Nach 74 N s schräg von vorn schob die Stütze,
die das Becken gegen den Oberschenkel aufrichtet, den Standfuß in einer Zehntelsekunde 36 cm unter dem Körper weg, und
die Figur kippte nach hinten. Zwei Bausteine in der Bibliothek: Jeder Knochen kann eine eigene Reibung haben,
`nbBoneDef::friction`, und die Füße des Menschen der Bibliothek haben die einer Gummisohle, 0,9 statt der 0,6 des Rigs.
Und die Stütze durch ein Bein lässt nach, solange sein Fuß rutscht, ab 0,3 m/s, ab 0,8 m/s ist sie weg. Mit der Stütze
durch die Beine fallen von den leichten Treffern 9 statt 25, von 50 bis 89 N s 240 statt 389, von 90 bis 139 N s 1 453
statt 1 449 und an den Gliedern 405 statt 479. Mit der Stütze gegen die Welt, dem Standard, fallen weiter keine leichten
und keine von 50 bis 89 N s, von 90 bis 139 N s 362 statt 385 und an den Gliedern 122 statt 135, und von den Seiten
fängt sie jeden Treffer auf die obere Brust bis 110 N s. Verworfen ist ein Schwungbein, dessen Gelenke ihrem Ziel mit
dessen Geschwindigkeit folgen: Es fing weniger. Was bleibt: Von den 9 leichten Treffern, die mit der Stütze durch die
Beine noch fallen, kippen 7 nach Treffern von vorn oder schräg von vorn nach hinten. Nach 66 N s von vorn wird der
Körper beim ersten Schritt zurück schneller, mit der Stütze durch die Beine bis 1,49 m/s, mit der gegen die Welt bis
1,23 m/s, denn wer das Becken über das Standbein aufrichtet, schiebt die Hüfte mit nach hinten. Der Schritt reicht mit
55 cm von der Hüfte nicht bis zum Capture Point, der zweite muss fangen, und die Figur stolpert seitlich weg. Die Stütze
gegen die Welt dreht das Becken mit einer Kraft von außen zurück, die ein Körper nicht hat. Die Stütze durch die Beine
wird sie deshalb mit besseren Schritten nur annähernd erreichen. Gepaart gegen `active-ragdoll-44`, vier Läufe im
Wechsel mit vier Threads und zwei mit einem, rechnen alle Szenen ohne Figuren dasselbe, und die mit Figuren enden mit
einem und mit vier Threads gleich. Bei 10 Figuren hinter brechender Deckung stehen am Ende alle 10 statt 9, dafür kostet
ein Schritt, während die Deckung bricht, das 1,6- und 2,6-fache, denn eine Figur mehr fällt dort und steht wieder auf.
Bei 50 liegt am Ende eine über der Kante ihrer Mauer und wartet auf Boden unter dem Becken, siehe F69, auf
`active-ragdoll-44` stand die letzte 15 s später wieder. In der Stadt mit Figuren stürzen 6 statt 4 Etagen ein, und ein
Bild kostet dort das 1,14- und 1,13-fache. Siehe „Sohlen, die greifen“ und „Die Hüfte lässt nach, wenn der Fuß rutscht“
in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 46 weiter die Balance mit F62: mehrere Schritte
planen, damit ein erster Schritt, der zu kurz kommen muss, so landet, dass der zweite fängt.

Mehrere Schritte planen, Runde 46, F62, auf Wunsch des Nutzers. Gebaut als Varianten und an allen 5 912 Treffern
gemessen, nicht in die Bibliothek übernommen. Zuerst nachgesehen, warum der zweite Schritt nicht fängt: Der Planer sagt
voraus, wo der Capture Point bei der Landung liegen wird, mit dem Pendel vom Druckpunkt unter dem Standfuß aus.
Nachgemessen an 1 118 Landungen mit der Stütze durch die Beine und 523 mit der gegen die Welt läuft er beim ersten
Schritt im Mittel 13 und 49 cm weniger weit als vorhergesagt, denn die Drehung des Körpers, die der Capture Point seit
Runde 39 mitzählt, wächst nicht wie die Geschwindigkeit des Schwerpunkts, und die Stütze gegen die Welt nimmt sie
heraus. Ab dem zweiten Schritt liegt die Vorhersage im Mittel 13 bis 18 cm daneben, vor allem seitlich, und kein
Pendelmodell erklärt mehr als drei Viertel der Bewegung. In der Spur von Runde 45 sagte der Planer beim zweiten Schritt
den Capture Point 5 cm links der Mitte voraus, er landete 32 cm rechts, und der dritte Schritt hätte die Beine kreuzen
müssen. Auf so ein Modell lassen sich zwei Schritte nicht planen: Wählt der Planer unter fünf seitlich versetzten
Landepunkten den, nach dem der zweite Schritt am besten fängt, fallen mit der Stütze durch die Beine von 50 bis 89 N s
312 statt 240, wählt er so auch den Fuß, 1 082, und mit der Stütze gegen die Welt von 90 bis 139 N s 989 statt 362. Eine
genauere Vorhersage für den ersten Schritt fängt ebenfalls weniger: Ein Schritt, der zu weit geht, fängt noch, ein zu
kurzer nicht, und die zu lange Vorhersage wirkt wie ein Sicherheitsabstand. Folgeschritte seitlich weiter zu setzen
lässt mit der Stütze durch die Beine von den leichten Treffern 4 statt 9 fallen, von 90 bis 139 N s aber 21 mehr.
Gefunden dabei: Mit der Stütze gegen die Welt gibt die Figur bei harten Treffern sofort auf, wenn der Planer den ersten
Schritt für hoffnungslos hält, und das tut er auf der zu langen Vorhersage. Mit der Grenze bei 1,5 statt 1,2 m fängt sie
von 90 bis 139 N s 22 Treffer mehr, mit 3 bis 6 Schritten, und keinen weniger, und von den übrigen 340 fallen 324 erst
nach einem Schritt, auf `active-ragdoll-45` fielen 278 sofort. Das ändert, wie harte Treffer aussehen, darum entscheidet
der Nutzer in der Demo. Siehe „Mehrere Schritte planen“ und die drei Zeilen danach in [Optimierungen](Optimierungen.md).
Vorgeschlagen ist als Runde 47 F69: Eine Figur, die über einer Kante liegt, rutscht herunter und steht auf. Seit Runde
45 liegt im Benchmark mit 50 Figuren wieder eine so über ihrer Mauer.

Taumeln vor dem Sturz, Runde 47, nach der Option aus Runde 46, auf Wunsch des Nutzers nach der Roadmap des Spiels
entschieden. Dort steht in 26.4 für einen starken Tritt „Stolper-/Bail-Animation einleiten; bei überschrittener
Stabilitäts- oder Treffergrenze kontrolliert in vollständige Ragdoll übergehen“ und erst für einen extremen der
sofortige Ragdoll, 28.3 will nach einem Schritt, der nicht reicht, „einen weiteren Schrittversuch oder ein Bail“, 30.5
nach misslungener Balance „Stolpern, Knockdown oder aktive/passive Ragdoll“, und 30.8 keine „zufälligen oder unlesbaren
Niederlagen“. Bis `active-ragdoll-46` fehlte die mittlere Stufe: Was die Figur nicht fing, ließ sie sofort fallen, auf
die obere Brust von den Seiten schon ab 115 N s. Jetzt gibt die Balance erst auf, wenn der erste Schritt den Capture
Point nach ihrer Vorhersage mehr als 1,5 statt 1,2 m draußen ließe. Mit der Stütze gegen die Welt fängt sich die Figur
von vorn bis 130 statt 120 N s und von hinten bis 135 statt 130, taumelt darüber in einem Band von 5 bis 15 N s einen
Schritt oder zwei und fällt, und erst ab 130 N s von den Seiten, 140 von vorn und 155 von hinten fällt sie sofort. Von
90 bis 139 N s fallen 340 statt 362, und von den übrigen fallen 324 nach einem Schritt statt 278 sofort. Mit der Stütze
durch die Beine und ohne Stütze fallen genauso viele wie vorher. Gepaart gegen `active-ragdoll-46` rechnen die Szenen
ohne Figuren, eine Figur allein und die Kämpfer dasselbe, und die übrigen enden mit einem und mit vier Threads gleich.
Wo der Benchmark alle Figuren mit 120 N s umstößt, fallen weiter alle, manche erst nach einem Schritt, und danach laufen
die Szenen anders: Bei 50 Figuren stehen am Ende alle 50, die Figur über ihrer Mauer aus Runde 45 liegt dort nicht mehr,
und bei 10 Figuren fallen, während die Deckung bricht, 4 statt 5, ein Schritt kostet dort das 0,36- und 0,20-fache, kein
Gewinn der Runde. In der Stadt mit Figuren stürzen weiter 6 Etagen ein, und ein Bild kostet das 1,02- und 1,00-fache.
Siehe „Taumeln vor dem Sturz“ in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 48 F69: Eine Figur, die
über einer Kante liegt, rutscht herunter und steht auf. Im Benchmark liegt seit dieser Runde keine mehr so, gemessen
wird F69 an den Bruchmustern der Wand.

Vergleich der Ansätze der Balance, Runde 48, auf Wunsch des Nutzers: F69 ist ihm gerade nicht so wichtig, die Balance
viel mehr, und er will den besten der Ansätze weiterverfolgen. Gemessen auf `active-ragdoll-47`, jeder Ansatz an den 168
leichten Treffern, den 3 984 Treffern auf Brust und Bauch von 50 bis 139 N s und den 1 280 auf Arme, Beine und Kopf.
Dazu jeder dieser 5 264 Treffer noch einmal mit dem Treffpunkt 1 mm daneben: Wie oft kippt das Ergebnis? Wie oft fängt
sich die Figur nach einem stärkeren Treffer aus derselben Richtung und auf dieselbe Stelle, nachdem ein schwächerer sie
umwarf, „umgekehrt“? Und wie viel Drehimpuls gibt ihr die Stütze gegen die Welt, im Mittel über die gefangenen Treffer
von 50 bis 89 N s, gemessen am Drehmoment ihres Gelenks in jedem Update? Die Tabelle zählt die gefallenen Treffer, dann
diese drei Zahlen:

| Ansatz | leicht | 50–89 N s | 90–139 N s | Glieder | umgekehrt | kippt bei 1 mm | Hilfe der Welt |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| gegen die Welt, 125 N m, Standard | 0 | 0 | 340 | 115 | 16 | 14 | 86 N m s |
| gegen die Welt, 150 N m | 0 | 0 | 244 | 91 | 15 | 9 | 88 N m s |
| gegen die Welt, 90 N m | 0 | 0 | 505 | 167 | 20 | 16 | 80 N m s |
| gegen die Welt, 60 N m | 0 | 1 | 793 | 225 | 36 | 34 | 71 N m s |
| gegen die Welt, 30 N m | 0 | 63 | 1 178 | 302 | 75 | 55 | 52 N m s |
| durch die Beine, F61 | 9 | 240 | 1 453 | 405 | 334 | 120 | – |
| Beine und Welt mit 125 N m | 0 | 0 | 345 | 125 | 12 | 14 | 79 N m s |
| Beine und Welt mit 90 N m | 0 | 0 | 494 | 162 | 20 | 10 | 75 N m s |
| Beine und Welt mit 60 N m | 0 | 5 | 725 | 215 | 38 | 31 | 67 N m s |
| Beine und Welt mit 30 N m | 1 | 38 | 1 085 | 285 | 83 | 41 | 47 N m s |
| ohne Stütze | 17 | 364 | 1 574 | 451 | 463 | 166 | – |
| gegen die Welt mit Muskeln, F45 | 0 | 1 | 336 | 116 | 18 | 12 | 84 N m s |
| durch die Beine mit Muskeln | 6 | 252 | 1 445 | 408 | 288 | 151 | – |

Die Stütze gegen die Welt ist mit Abstand der beste Ansatz für das Spiel. Sie fängt alle leichten und mittleren Treffer,
lässt starke nach 26.4 taumeln und fallen, und ihr Ergebnis hängt am wenigsten vom Zufall ab: 14 von 5 264 Treffern
kippen, wenn der Treffpunkt 1 mm wandert, durch die Beine 120, ohne Stütze 166. Mit der Stütze durch die Beine fallen
schon leichte Treffer, was 26.4 ausschließt. Aus eigener Kraft kann eine Figur gegen den Boden nur so viel Drehmoment
aufbringen, wie der Druck unter ihren Sohlen hergibt, mehr bringt nur ein Schritt, und den setzt die Balance noch nicht
schnell und genau genug. Beine und Welt zusammen fangen so viel wie die Welt allein, mit 8 % weniger Hilfe der Welt, und
die Muskeln ändern fast nichts. Die Stärke der Stütze entscheidet, wie viel die Figur fängt, und ist damit die Zahl, mit
der ein Spiel einstellt, wie standfest eine Figur ist. Empfohlen bleibt die Stütze gegen die Welt, wie unter „Euphoria
und Overgrowth im Vergleich“, die Stütze durch die Beine bleibt Option für einen realistischen Modus, siehe F61.

Eine Schwäche der Stütze gegen die Welt zeigt ein neuer Test mit zwei Treffern auf die obere Brust: zuerst 60 N s, dann
0,1 bis 0,3 s später, während die Figur tritt, ein zweiter aus derselben Richtung, aus acht Richtungen und drei
Drehungen der Figur. Die Stütze hilft gleich stark, ob die Figur ruhig steht oder schon taumelt. Von je 72 zweiten
Treffern mit 20 bis 70 N s fällt sie zusammen ein einziges Mal, nach 80 N s 7-mal, durch die Beine schon nach 20 N s
14-mal, nach 50 N s 46-mal und nach 80 N s immer. 30.9 der Roadmap des Spiels verlangt: „Kick bei gutem Stand führt zu
Stabilisierung, bei schlechter Stütze zu Sturz“. Zwei Wege probiert: Hält die Stütze nur so stark, wie die Füße stehen,
auf einem Fuß halb und in der Luft gar nicht, fallen nach zweiten Treffern von 60, 70 und 80 N s 10, 14 und 30, aber
auch nach einem einzelnen Treffer von 90 bis 139 N s 522 statt 340, denn beim Fangen tritt die Figur. Halbiert dagegen
ein Treffer, der die Figur trifft, während sie sich schon fängt, ihre Stütze, fallen 8, 12 und 28, und jeder Treffer auf
eine stehende Figur endet bitgleich wie vorher. Mit einem Viertel statt der Hälfte fallen 16, 30 und 47. Siehe
„Vergleich der Stützen“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 49 F75:
Ein Treffer auf eine Figur, die schon taumelt, schwächt ihre Stütze gegen die Welt, damit Folgetreffer sie umwerfen.

Eigene Kraft, Runde 49, auf Wunsch des Nutzers: F75 und andere Ideen zurück, der Kern ist die Balance aus eigener Kraft,
die Stütze durch die Beine. Mit einer Sonde, die den Grund jedes Sturzes festhält, auf `active-ragdoll-48` nachgemessen:
Von den 240 Stürzen nach 50 bis 89 N s auf Brust und Bauch kippten 129, alle im Treten und in die Richtung des Stoßes,
55 sanken und 48 kamen nach dem fünften Schritt, von den 1 453 nach 90 bis 139 N s kippten 996. In einer Spur nach
74 N s schräg von vorn wurde der Schwerpunkt im ersten Schritt auf dem Standbein von 1,0 auf 1,8 m/s schneller, weil er
schon hinter dem Standfuß lag. Der Schritt landete 30 cm vor dem Capture Point, den die Vorhersage richtig sah, aber
nicht erreichen konnte, und jeder weitere kam wieder zu kurz, bis 2,2 m/s. Varianten an allen 5 912 Treffern: Ein
Kippwinkel von 0,8 oder 1 rad und ein Schwerpunkt bis 70 % vor dem Sturz änderten nichts, die Stürze sind echt.
Schnellere Schritte, längere und ein Fuß, der früh losschwingt, fingen weniger, stärkere Gelenke mehr, am meisten das
Knie, ein stärkerer Knöchel weniger. Gebaut: Die Stütze durch ein Bein hält so stark wie das Gelenk der Hüfte, 300 statt
125 N m, und ohne die Stütze gegen die Welt stemmt die Figur ihre Knie mit 375 statt 225 N m, tritt ein Update nach
einem deutlichen Stoß und macht bis zu zehn Schritte statt fünf. Mit eigener Kraft fallen von den leichten Treffern
keiner statt 9, von 50 bis 89 N s 50 statt 240, von 90 bis 139 N s 1 063 statt 1 453 und an den Gliedern 265 statt 405,
und 80 statt 120 Ergebnisse kippen, wenn der Treffpunkt 1 mm wandert. Auf die obere Brust fängt sie sich von vorn bis
110, von hinten bis 105, von links bis 95 und von rechts bis 80 N s, bisher fiel sie ab 66, 94, 72 und 86 N s. Mit der
Stütze gegen die Welt endet jeder Treffer bitgleich wie vorher. Für alle hätte das stärkere Knie auch den fallenden
Ragdoll steifer gemacht, eine umgestoßene Figur lag im Test erst nach mehr als 2 s flach, und um mit dem stärkeren Knie
so viel zu fangen wie vorher, bräuchte die Stütze gegen die Welt 90 statt 125 N m, mit denen zwei Kämpfer stürzen, die
zusammenstoßen. Gepaart gegen `active-ragdoll-48` rechnen alle Szenen des Benchmarks dasselbe. Siehe „Mit eigener Kraft
fangen“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Von den 50 Stürzen nach 50 bis 89 N s sinken jetzt
24, 14 davon nach Treffern von hinten, und 14 kippen. Vorgeschlagen ist als Runde 50 weiter die eigene Kraft: warum die
Figur nach einem Stoß von hinten im Treten nach vorn einsinkt.

Clip als Absicht, Physik als Ausführung, am 3. November 2026 mit dem Nutzer besprochen, nach einer Recherche des Nutzers
zu aktiven Ragdolls mit eigener Balance. Einen Salto kann eine Figur auf drei Arten ausführen. Kinematisch setzt der
Clip Lage und Drehung jedes Knochens, und ein Treffer bringt den Körper nicht aus der Bahn. Bei der physikalischen
Clip-Verfolgung gibt der Clip die Ziele der Gelenke, Muskeln ziehen mit begrenzter Kraft dorthin, und ein Treffer stört
die Bewegung. Und ein Controller rechnet die Ziele der Gelenke aus dem Zustand des Körpers. Nebenan nutzt heute alle
drei: Im Stand, in Kampfclips und beim Aufstehen ist eine Figur kinematisch, und ein harter Treffer macht sie im selben
Moment dynamisch, mit dem Schwung, den der Clip ihren Knochen gerade gibt. Nah am Gegner folgt sie mit `engageRadius`
ihrem Clip physikalisch. Nach einem harten Treffer ziehen ihre Gelenke zur Haltung des Clips, und die Balance verändert
Beine, Arme und Rumpf nach dem Zustand. Das Zielbild des Nutzers ist das für jede Bewegung: Die Gelenke folgen den
Drehungen des Clips, Lage und Drehung des Beckens erreicht der Körper selbst, Balance und Flug verändern die Ziele nach
dem Zustand, und Treffer wirken auf die dynamischen Knochen. Ein Clip, der einen Salto zeigt, garantiert dann keinen
Salto: Springen die Beine zu schwach ab, kommt die Figur nicht hoch genug, und reicht der Drehimpuls nicht, dreht sie
nicht ganz herum. Die Falle dabei steckt in `nbBeginFollowing`: Es hängt jeden Knochen eines folgenden Kämpfers über ein
Motorgelenk an einen festen Anker der Welt, das so stark ziehen darf, wie der Clip den Knochen beschleunigt, bis 981
m/s², und dazu sein Gewicht trägt. Einen Salto machten diese Fäden, nicht die Beine. Sie waren in Runde 43 der
Kompromiss, damit Kämpfer überhaupt schon physikalisch folgen. Ohne sie und ohne die Stütze gegen die Welt fängt sich
die Figur noch nicht gut genug, von 1 920 Treffern mit 50 bis 89 N s fallen 50 statt keinem. Der Weg zum Zielbild ist
F76, in dieser Reihenfolge: zuerst die eigene Kraft so gut wie die Stütze gegen die Welt, F61, dann Folgen ohne Fäden
als Option, dann Laufen aus eigener Kraft, wenn das Spiel die Figur bewegt, und zuletzt Absprung, Flug und Landung. Zwei
Grenzen bleiben. Die Akrobatik des Spielers bleibt nach Kapitel 2 der Roadmap des Spiels kinematisch, bis ein harter
Treffer sie physikalisch macht: Ein Salto, der auf Knopfdruck misslingt, weil der Absprung nicht genau genug gesteuert
ist, wirkt wie ein Fehler. Und eine physikalische Figur kostet rund 13-mal so viel wie eine kinematische, 0,065 statt
0,005 ms pro Schritt, siehe Runde 43, fern vom Geschehen bleibt eine Figur darum kinematisch, wie 26.17 es will. Die
Recherche empfiehlt dieselben Bausteine, die die Bibliothek hat: Clip, Muskeln mit Grenze, Balance mit Capture Point und
Schritten, und kein versteckter Halt am Becken, also die eigene Kraft. Nicht übernommen und warum: Figuren, die immer
physikalisch sind, für alle, siehe oben. Gelernte Steuerungen wie DeepMimic oder Isaac Lab, die für Box3D neu trainiert
werden und auf jeder Plattform bitgleich rechnen müssten. Und Assets für Unity, die nur dort laufen.

Zwei Optionen statt immer physikalisch, am selben Tag vom Nutzer entschieden: Eine Figur, die in jeder Bewegung
physikalisch ist, wie F76 sie wollte, ist verworfen. Es bleiben zwei Optionen, und ein Spiel wählt je Aktion. Erstens
kinematisch mit Umschalten, wie heute: Der Clip bewegt die Figur direkt, erst ein Treffer, ein Stoß oder ein Gegner in
der Nähe macht sie dynamisch. Zweitens physikalische Clip-Verfolgung, F77: Während einer Aktion, etwa einem Angriff im
Nahkampf oder einem Salto, ist die Figur dynamisch, ihre Gelenke ziehen zu den Winkeln des Clips, und Box3D rechnet
Kontakte und Kräfte. Normale Fortbewegung bleibt kinematisch. Den Unterschied spürt man an Kollisionen mit Gegenständen:

| Lage | Kinematisch mit Umschalten | Physikalische Clip-Verfolgung |
| --- | --- | --- |
| Der Fuß streift eine kleine Kiste | Der Fuß schiebt die Kiste weg und spürt nichts: Was die eigenen Knochen beiseiteschieben, zählt heute nicht als Druck | Der Fuß gibt nach, das Knie beugt sich, Hüfte und Drehung der Figur spüren den Widerstand, und der Clip läuft weiter |
| Die Hand berührt eine Wand | Box3D paart kinematische Körper nicht mit festen: Die Hand geht durch die Wand, außer das Spiel prüft selbst und schaltet um | Die Hand bleibt an der Wand hängen |
| Der Kopf trifft einen Balken | Wie die Hand an der Wand, umgeschaltet wird erst über das Spiel | Der Kopf wird sofort gebremst |
| Leichter Treffer | Ein berechnetes Zucken auf der Animation | Die Gelenke geben sofort nach |
| Starker Treffer | Die Figur wird dynamisch und fängt sich oder fällt | Die Physik läuft schon, die Balance übernimmt |
| Salto ohne Hindernis | Genau wie animiert | Kann vom Clip abweichen, wenn Absprung oder Drehimpuls nicht reichen |

Die Frage, die zwischen beiden entscheidet: Soll eine Figur bei einer kleinen Kollision ihre Animation möglichst
fortsetzen, während nur die betroffenen Körperteile nachgeben? Dann spricht das für die Clip-Verfolgung, und schaltet
bei jeder Kollision ohnehin die ganze Figur um, ist ihr Vorteil klein. Für die Clip-Verfolgung gilt die Falle von oben:
Die Fäden von `nbBeginFollowing` ziehen jeden Knochen nach einem Stoß zurück an seine Lage im Clip und halten die Figur
künstlich. Der Clip soll die Gelenke steuern, nicht die Lage der Knochen in der Welt, und ein Salto braucht dann einen
Absprung, der physikalisch reicht. Den Weg dahin beschreibt F77.

Kinematisch mit Umschalten reicht, am selben Tag mit dem Nutzer geklärt, und damit bleibt die Bibliothek beim Hybrid.
Mit kinematischer Animation und einem guten Umschalten sieht eine Figur praktisch so aus wie mit physikalischer
Clip-Verfolgung. Macht sie einen Salto nach ihrem Clip und trifft ihr Fuß eine Kiste, wird sie im Moment des Kontakts
dynamisch, mit den Lagen und Geschwindigkeiten ihrer Knochen. Der Fuß wird gebremst, das Knie gibt nach, die Kiste
fliegt weg, und die Gelenke ziehen weiter zur Haltung des Saltos im Clip. Mit Clip-Verfolgung wäre sie nur schon vorher
dynamisch, ab dem Kontakt können beide dieselbe Steuerung nutzen. Ohne Kontakt läuft die kinematische Figur genau wie
animiert und kostet ein Dreizehntel. F77 ist darum nicht nötig. Zwei Dinge muss das Umschalten dafür können, F78. Es
muss den Kontakt erkennen, bevor oder sobald er geschieht, denn eine kinematische Figur spürt keine Kontaktkräfte. Und
nach dem Umschalten ziehen die Gelenke zu den Winkeln des Clips, nicht die Knochen zu ihrer Lage in der Welt. Die Figur,
die sich nach einem Treffer fängt, tut das schon. Nur die Fäden der folgenden Kämpfer von F64 ziehen die Knochen an ihre
Lage im Clip zurück und übersteuern so eine Kollision. Welche Lage welchen Zustand braucht:

| Lage | Zustand |
| --- | --- |
| Stehen, Gehen, Rennen | Kinematisch |
| Salto ohne Kontakt | Kinematisch, genau wie animiert |
| Salto, dessen Fuß eine Kiste streift | Kinematisch, beim Kontakt dynamisch, die Gelenke folgen weiter dem Clip, F78 |
| Nahkampf mit Körperkontakt | Dynamisch, solange ein Gegner nah ist, wenn das Spiel `engageRadius` setzt, sonst wie der Salto: beim Kontakt umschalten |
| Treffer, der die Balance fordert | Dynamisch mit Balance, `nb_figureBalancing` |
| Schwerer Treffer oder Sturz | Ragdoll |
| Aufstehen | Mit einem Clip |

Der Ablauf: Eine Figur ist kinematisch. Ein Treffer oder eine Kollision macht sie dynamisch, und ihre Gelenke folgen
weiter dem Clip. Fängt sie sich, ist sie wieder kinematisch, fängt sie sich nicht, fällt sie als Ragdoll und steht mit
einem Clip auf.

Der Fuß schwingt mit seinem Tempo, Runde 50, F61, wie mit dem Nutzer vereinbart weiter an der eigenen Kraft, die auch
nach dem Umschalten trägt. Nachverfolgt an den Stürzen, die nach einem Stoß von hinten einsanken: Im ersten Schritt wird
der Schwerpunkt auf dem Standbein von 1,1 auf 1,9 m/s schneller, wie es das Pendel sagt, und der zweite Schritt muss den
hinteren Fuß gut 1 m weit tragen. Dieser Fuß rollt beim Abheben mit 2,5 m/s nach vorn ab, der sanfte Beginn jedes
Schritts aus der Ruhe bremste ihn auf 0,6 m/s, und danach hing er bis zu 40 cm hinter seinem Plan und setzte 30 bis
40 cm vor seinem Ziel auf. Gebaut, nur ohne die Stütze gegen die Welt: Der Fuß schwingt mit dem Tempo weiter, das er
beim Beginn des Schritts entlang seines Wegs hat, auf einer kubischen Kurve bis zur Ruhe am Ziel, höchstens mit dem
Dreifachen des Wegs durch die Zeit des Schritts, so viel, wie eine solche Kurve trägt, ohne über das Ziel
hinauszuschießen. Mit eigener Kraft fallen von den leichten Treffern weiter keiner, von 50 bis 89 N s 34 statt 50, von
90 bis 139 N s 1 030 statt 1 063 und an den Gliedern 260 statt 265, und 75 statt 80 Ergebnisse kippen, wenn der
Treffpunkt 1 mm wandert, ohne Stütze 8, 150, 1 438 und 356 statt 12, 184, 1 471 und 365. Mit der Stütze gegen die Welt
ist jedes Ergebnis Byte für Byte gleich. Gefunden dabei: Bei 32 der 34 Stürzen, die von 50 bis 89 N s bleiben, rutscht
der Standfuß mit 1 bis 5 m/s nach hinten, bei gefangenen Treffern meist unter 1 m/s. Das Bein, das gerade aufgesetzt
hat, richtet das Becken auf und streckt sich, das Knie in 0,07 s von 0,37 auf 0,12 rad, und steht der Fuß hinter dem
Körper, stößt es ihn dabei zurück. Gebremst haben das ein Standbein, das das Becken langsamer hebt, nur so hoch, wie es
reicht, oder nur halb aufrichtet, und eines, das nachgibt, sobald sein Fuß rutscht, alle fingen weniger: Das Aufrichten
durch das Standbein ist nötig. Auch ein weniger gedämpftes Schwungbein, ein Fuß, der sich erst nahe am Ziel senkt, oder
früher oben ist, und weitere oder höhere Schritte fingen weniger. Siehe „Der Fuß schwingt mit seinem Tempo“ und die
Zeilen danach in [Optimierungen](Optimierungen.md). Gepaart gegen `active-ragdoll-49`, zwei Läufe im Wechsel mit vier
Threads und einer mit einem, rechnen alle Szenen des Benchmarks dasselbe, ihre Figuren stehen mit der Stütze gegen die
Welt, und ein Bild der Stadt mit Figuren kostet das 0,99-, 0,98- und 0,96-fache, bei gleicher Arbeit. Vorgeschlagen ist
als Runde 51, nach Wunsch des Nutzers, weiter die eigene Kraft, ein Standbein, dessen Kräfte auf den Boden im Reibkegel
bleiben, während es das Becken aufrichtet, oder F78, das Umschalten bei Kollisionen mit Gegenständen, das der Hybrid
braucht.

Nicht stärker, als der Fuß hält, Runde 51, F61, auf Wunsch des Nutzers weiter an der eigenen Kraft, der erste Vorschlag
von Runde 50. Gemessen an den Kräften der Kontakte unter dem Standfuß nach Stößen von hinten: Die Stütze durch das
Standbein zog mit der vollen Kraft der Hüfte und schob den Fuß so stark über den Boden, wie die Reibung hielt, mit 73 %
seiner Last, wo das Pendel des Körpers 13 bis 25 % verlangte. Der Fuß kroch zurück, während der Körper weiterlief, und
ein Fuß, der gerade zum Standfuß wurde, trug noch nichts. Gebaut, nur ohne die Stütze gegen die Welt: Die Stütze durch
ein Bein zieht höchstens mit der Last der Kontakte seines Fußes mal ihrer Reibung, über die Länge des Beins. Die Last
kommt aus den Stößen der Kontakte im ganzen Schritt der Welt, die Reibung so, wie Box3D sie für jeden Kontakt mischt,
dafür gibt Box3D sie jetzt heraus, `b3Contact_GetFriction`. Mit eigener Kraft fallen von den leichten Treffern weiter
keiner, von 50 bis 89 N s 28 statt 34, von 90 bis 139 N s 1 000 statt 1 030 und an den Gliedern 257 statt 260, und von
den 28 sinken 8 statt 17. Zusammen über vier Lagen des Treffpunkts, wie er ist und um 1, −1 und 2 mm verschoben, fallen
von 50 bis 89 N s 119 statt 122, von 90 bis 139 N s 3 999 statt 4 108 und an den Gliedern 1 020 statt 1 045: Sicher
gewonnen hat sie bei den harten Treffern und an den Gliedern, bei den mittleren liegt der Unterschied im Zufall. Wandert
der Treffpunkt um 1, −1 und 2 mm, kippen 84, 68 und 88 statt 75, 75 und 81 Ergebnisse, im Mittel fast gleich viele. Ohne
Stütze und mit der Stütze gegen die Welt ist jedes Ergebnis Byte für Byte gleich. Mit dem 0,6- bis 1-fachen der Reibung
fallen gleich viele, von allen 5 432 Treffern 1 278 bis 1 285 statt 1 324, gebaut ist das 1-fache, die Grenze, die der
Boden selbst setzt. Die Hälfte der Reibung und weniger, nur der Rest, den das Tragen des Körpers übrig lässt, und ein
Standbein, das das Becken nach und nach statt sofort aufrichtet, fingen weniger, die Last aus dem letzten Teilschritt
statt aus dem ganzen Schritt ließ einen leichten Treffer fallen. Siehe „Nicht stärker, als der Fuß hält“ und die Zeilen
danach in [Optimierungen](Optimierungen.md). Gepaart gegen `active-ragdoll-50`, zwei Läufe im Wechsel mit vier Threads
und einer mit einem, rechnen alle Szenen des Benchmarks dasselbe, ihre Figuren stehen mit der Stütze gegen die Welt, und
ein Bild der Stadt mit Figuren kostet das 0,99-, 1,03- und 0,96-fache, bei gleicher Arbeit. Vorgeschlagen ist als Runde
52, nach Wunsch des Nutzers, weiter die eigene Kraft an den Stürzen, die jetzt am häufigsten sind: 13 der 28 kippen mit
dem Stoß, 12 davon nach Stößen von hinten oder schräg von hinten, meist im dritten Schritt. Oder F78, das Umschalten bei
Kollisionen mit Gegenständen, das der Hybrid braucht.

Die Hüfte bremst das Kippen, Runde 52, F61, auf Wunsch des Nutzers weiter an der eigenen Kraft. Nachverfolgt an den
Stürzen, bei denen die Figur nach Stößen von hinten im dritten Schritt kippte: Der schwingende Fuß folgt seinem Ziel
spät, und weil sein Ziel sich an den Knochen darüber hält, wie die IK sie will, kippte er in der zweiten Hälfte des
Schritts mit den Zehen um 0,2 bis 0,4 rad nach unten und blieb 6 bis 7 cm unter seinem Plan. Die Zehen trafen zur Hälfte
des Schritts den Boden, der Fuß landete 25 bis 50 cm vor seinem Ziel und hinter dem Schwerpunkt, und das nächste
Standbein schob ihn beim Strecken nach hinten weg. Gebaut, nur ohne die Stütze gegen die Welt: Der schwingende Fuß zieht
in der Mitte des Schritts die Zehen um 0,15 rad an und landet flach, und auf Anregung des Nutzers bremst die Stütze
durch ein Bein das Kippen des Beckens in der Welt, mit `uprightDampingRatio` wie die Stütze gegen die Welt. Ihr Gelenk
dämpfte nur das Drehen des Beckens gegen den Oberschenkel, und der kippt mit. Zusammen über vier Lagen des Treffpunkts
fallen von den Treffern von 50 bis 89 N s 73 statt 119, von 90 bis 139 N s 3 919 statt 3 999 und an den Gliedern 1 036
statt 1 020, von den leichten weiter keiner, und die Stütze gegen die Welt rechnet bitgleich. Von den 23 Stürzen der
Tabelle von 50 bis 89 N s kippen 4 statt 13, und doppelt so viele Treffer wie vorher, in den vier Lagen 66 statt 34,
enden, weil die Figur sich fängt, aber 8 s lang nicht zur Ruhe kommt. Ein Knie, das nur beim Nachgeben gestemmt ist, wie
Muskeln beim Bremsen stärker halten als beim Strecken, fing weniger: Das Standbein braucht das kräftige Strecken, auch
wenn es den Körper nach der Landung mit 3 kN und 0,65 m/s nach oben wirft. Der Fuß, der sich nach seinem Weg senkt, aus
Runde 50, fing 152 harte Treffer mehr, aber 25 mittlere und einen leichten weniger, ein höherer Fuß ebenso. Ein Fuß, der
flach gegen das Schienbein gehalten wird, wie es gerade steht, und Zehen, die bis zur Landung angezogen bleiben, fingen
viel weniger: Jede Korrektur des Fußes muss bei der Landung vorbei sein. Ein Druckpunkt unter den Sohlen, der mit dem
Capture Point wandert, auch eine Anregung des Nutzers, fing nicht mehr, im Stand ebenso viel und immer mit zwei leichten
Treffern, die fallen. Siehe „Die Hüfte bremst das Kippen“ und die Zeilen danach in [Optimierungen](Optimierungen.md).
Gepaart gegen `active-ragdoll-51`, zwei Läufe im Wechsel mit vier Threads und einer mit einem, rechnen alle Szenen des
Benchmarks dasselbe, ihre Figuren stehen mit der Stütze gegen die Welt, und ein Bild der Stadt mit Figuren kostet das
0,92-, 1,02- und 1,02-fache, bei gleicher Arbeit. Vorgeschlagen ist als Runde 53 F79, die Drehung im Körper, nach der
der Nutzer gefragt hat, oder weiter die eigene Kraft an den Figuren, die sich fangen und nicht zur Ruhe kommen, oder
F78.

Drehung im Körper, Runde 53, F79, auf Wunsch des Nutzers. Gemessen an einer Tabelle aus 192 Treffern je Stütze, auf die
Mitte der oberen Brust, vor die rechte Schulter und auf den Bauch, aus acht Richtungen mit 30 bis 100 N s: Der Körper
drehte sich als ein Block. Nach den Treffern, bei denen die Figur tritt, drehten Schultern und Hüften im Schritt im
Mittel höchstens um 3° gegeneinander, mit eigener Kraft um 5°, und die Wirbelsäule drehte nur, was ein Treffer ihr gab,
F50. Die Balance hielt das Becken in der Richtung der Füße, F59. Beim Menschen dreht das Becken im Schritt mit dem
Schwungbein, und der Oberkörper dreht dagegen, so weit, dass der Körper als Ganzes nicht mitdreht: Gerechnet aus den
Knochen des Menschen der Bibliothek haben Becken und Schwungbein um die Hüfte des Standbeins 1,31 kg m² Drehträgheit um
die Senkrechte, der Oberkörper um seine Wirbelsäule 0,81, das 1,6-fache. Gebaut, für jede Figur und jede Stütze: Das
Becken dreht zum vorderen Fuß, um ein Viertel des Winkels der Linie zwischen den Knöcheln gegen die Querrichtung der
Figur, höchstens 0,25 rad, `NB_PELVIS_TURN` und `NB_PELVIS_TURN_MAX`, und während ein Fuß schwingt, läuft die Linie zu
dem Punkt seines Bogens, an dem die Balance ihn will. Die Wirbelsäule dreht den Oberkörper um das 2,5-fache dagegen,
`NB_TRUNK_COUNTER`, verteilt wie die Drehung aus einem Treffer, in der Welt anderthalbmal so weit in die Gegenrichtung,
und im Stand klingt die Drehung in 0,3 s ab, `NB_TURN_RELAX`. Jetzt drehen Schultern und Hüften im Schritt im Mittel bis
24°, mit eigener Kraft bis 29°, höchstens 42 und 47°, und wenn die Figur wieder steht, sind Brust und Becken wie vorher
um 3° gegeneinander gedreht. Über vier Lagen des Treffpunkts fallen von allen 4 × 5 264 Treffern auf Brust, Bauch und
Glieder mit eigener Kraft 4 573 statt 5 028, mit der Stütze gegen die Welt 1 503 statt 1 814 und ohne Stütze 5 973 statt
7 457, von den 168 leichten keiner, keiner und 4 statt 11. Mit eigener Kraft fallen von den mittleren, 50 bis 89 N s,
126 statt 73, die harten und die an den Gliedern fängt sie öfter. Gemessen und verworfen: Der Oberkörper einmal, zweimal
und dreimal so weit zurückgedreht wie das Becken fing 5 150, 4 661 und 4 486 mit eigener Kraft nicht, dreimal so weit
mit einem leichten Treffer, der fällt. Ohne das Abklingen fing die Figur gleich viel, blieb aber im Stand um 13 bis 16°
verdreht. Ein weiter gedrehtes Becken, Arme, die gegen die Beine schwingen, nur der Oberkörper, der gegen den Schritt
dreht, und Füße, die sich mit dem Körper drehen, fingen nicht mehr oder ließen ein bis drei leichte Treffer fallen. Eine
Drehung nach dem Weg, den der Schwungfuß seit dem Abheben nach vorn ging, fing mit eigener Kraft 32 mehr und mit der
Stütze gegen die Welt 4 weniger: Gebaut ist die einfachere nach der Linie zwischen den Knöcheln, die keinen Zustand
braucht als das Abklingen. Die Tests: Der Abfang-Test gibt 114 statt 112 N s von rechts, 112 fängt die Figur jetzt.
Welche Bruchmuster die Figur durch die Wand lassen, ist Zufall, an den 32 Mustern von 3 cm seitlich 8 statt 10, und der
Test des Schutts hinter der Wand nimmt das dritte Muster, wo sie in beiden Runden zweimal aufsteht. Unter dem Block von
80 kg beginnt die gedrückte Figur aufzustehen, während er noch von ihr rutscht, er bricht das ab und bleibt auf ihren
Beinen liegen, und sie wartet, wie 26.15 der Roadmap des Spiels es will. Siehe „Drehung im Körper“ und die Zeilen danach
in [Optimierungen](Optimierungen.md). Gepaart gegen `active-ragdoll-52`, zwei Läufe im Wechsel mit vier Threads und
einer mit einem, rechnen alle Szenen ohne Figuren, die Kämpfer und die Ragdolls im Schutt dasselbe wie vorher, und die
Szenen mit getroffenen Figuren enden anders, mit einem und mit vier Threads gleich. Von den 50 Figuren hinter ihren
Mauern stehen am Ende 48 statt 50: Eine steht gerade auf, und auf einer liegt ein Stück ihrer Mauer von 22 kg, unter dem
sie wartet, auch eine Minute später noch. In der Stadt mit Figuren fangen sich 8 statt 5, und am Ende stehen 27 wie
vorher, 4 statt 2 liegen und 1 statt 3 stehen auf. Wo die Figuren dasselbe tun, kostet ihr Update so viel wie vorher,
und ein Bild der Stadt mit Figuren kostet das 0,91-, 0,97- und 0,99-fache, bei anderer Arbeit, mit 5 statt 6
eingestürzten Stockwerken. Vorgeschlagen ist als Runde 54 weiter die eigene Kraft an den mittleren Treffern, die mit der
Drehung öfter fallen, oder F78.

Mittlere Treffer mit eigener Kraft, Runde 54, auf Wunsch des Nutzers. Seit Runde 53 fallen mit eigener Kraft von den
mittleren Treffern auf Brust und Bauch, 50 bis 89 N s, über vier Lagen des Treffpunkts 126 statt 73. 14 von ihnen fallen
in allen vier Lagen, die Runde 52 alle fing, aber erst nach 4 bis 13 Schritten, und 56 der 126 Stürze kommen im dritten
Schritt, in Runde 52 waren es 10 von 73. Gemessen, während ein Fuß schwingt: Das Becken folgt seiner Drehung zu 0,7, die
Brust dreht in der Welt um das 1,8-fache dagegen, und der Boden dreht die Figur im Mittel mit 32,1 N m um die
Senkrechte, in Runde 52 mit 31,7 und ohne die Gegendrehung des Oberkörpers mit 36,3. Das 2,5-fache hält also den
Drehimpuls, wie Runde 53 es rechnete. Aber der Standfuß rutscht öfter, im dritten Schritt bei 39 statt 25 % der Schritte
mehr als 5 cm. Gebaut: Die Wirbelsäule dreht den Oberkörper um das Dreifache statt um das 2,5-fache der Drehung des
Beckens zurück, `NB_TRUNK_COUNTER`, höchstens so weit, wie ihre Gelenke es erlauben. Mit eigener Kraft fallen von den
leichten Treffern weiter keiner, von den mittleren 104 statt 126, von den harten 3 416 statt 3 505 und an den Gliedern
941 statt 942, von allen 4 × 5 264 Treffern 4 461 statt 4 573. Mit der Stütze gegen die Welt fallen 0, 1, 1 039 und 446
statt 0, 1, 1 060 und 442, 1 486 statt 1 503, und ohne Stütze 5, 370, 4 234 und 1 163 statt 4, 417, 4 363 und 1 193,
5 767 statt 5 973, ein leichter Treffer mehr. Schultern und Hüften drehen im Schritt im Mittel bis 28 statt 24°, mit
eigener Kraft bis 33 statt 29°. Die 73 mittleren von Runde 52 erreicht die Runde nicht, und warum das Dreifache besser
fängt, erklärt der Drehimpuls nicht: Der Boden dreht die Figur damit etwas stärker, mit 32,6 N m. Gemessen und
verworfen: Ein Schwungbein, das wie vor Runde 53 vom Becken ohne die Drehung aus greift, fing die 14 Treffer, über alle
Tabellen fielen aber 263 mittlere. 2,6-, 2,7-, 3,5- und viermal zurückgedreht fielen 119, 111, 166 und 217, 2,8-mal 102
und ein leichter Treffer: Ab dem 3,5-fachen reicht die Drehung der Wirbelsäule nicht mehr. Um die Achse der Wirbelsäule
statt um die Senkrechte gedreht fielen 201, und Federn, die dem bewegten Ziel der Balance folgen, fingen weniger, für
alle Gelenke fast nichts mehr. Eine Drehung, die nachlässt, wenn das Becken kippt oder der Standfuß rutscht, eine
geglättete, keine im Stand, ein Becken bis 0,15 rad, eine Ruderachse der Arme fest in der Welt und mit dem Dreifachen
ein anderes Abklingen oder ein anderer Anteil des Winkels fingen weniger oder ließen leichte Treffer fallen. Siehe „Mehr
Gegendrehung für die mittleren Treffer“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Die Tests ändern nur
ihre Hashes. Gepaart gegen `active-ragdoll-53`, zwei Läufe im Wechsel mit vier Threads und einer mit einem, rechnen alle
Szenen ohne Figuren, die Kämpfer und die Ragdolls im Schutt dasselbe, und die Szenen mit getroffenen Figuren enden
anders, mit einem und mit vier Threads gleich: Hinter ihren Mauern stehen am Ende alle 50 Figuren statt 48, in der Stadt
mit Figuren 28 statt 27, und ein Bild der Stadt mit Figuren kostet das 1,00-, 1,01- und 1,03-fache, bei anderer Arbeit.
Vorgeschlagen ist als Runde 55 weiter die eigene Kraft an den mittleren Treffern, am Standfuß, der in den späteren
Schritten öfter rutscht, oder F78.

Schieben, Festhalten und Abstützen gehören dazu, am 4. November 2026 mit dem Nutzer geklärt. Gegenseitiges Schieben,
Festhalten und Abstützen an Gegenständen gehören zum Spiel, nicht nur Treffer und Stürze. Die Entscheidung vom 3.
November bleibt: Normale Fortbewegung und Akrobatik ohne Kontakt laufen kinematisch, genau wie animiert, und nur wo
Körperkontakt möglich ist, folgt die Figur ihrem Clip mit dynamischen Knochen. Der Clip sagt, was die Figur tun will,
die Physik, was ihr Körper kann. Schieben, Festhalten und Abstützen fallen in die Zeile „Nahkampf mit Körperkontakt“ der
Tabelle unter „Kinematisch mit Umschalten reicht“: Dort ist die Figur schon dynamisch, solange ein Gegner nah ist,
`engageRadius`, F64. Eine dynamische Figur kostet rund 13-mal so viel wie eine kinematische, darum nur dort. Was die
Lagen verlangen:

| Lage | Was der Körper tut |
| --- | --- |
| Ein Gegner schubst | Der Körper gibt nach, die Beine stemmen sich über die Füße dagegen |
| Zwei drücken gegeneinander | Beide spüren einander über echte Kontaktkräfte, wer besser steht, schiebt |
| Ein Gegner hält einen Arm fest | Der Arm bleibt hängen, der übrige Körper arbeitet weiter, F54 |
| Eine Hand stützt sich an einer Wand ab | Der Arm gibt Kraft an die Wand, und die Balance zählt die Hand als Stütze, F80 |
| Ein Salto streift ein Hindernis | Das Glied gibt nach, die Drehung kann sich ändern, F78 |
| Ein starker Treffer | Die Balance fängt die Figur, sonst fällt sie |

Heute verfälschen das die Fäden von `nbBeginFollowing`: Sie hängen jeden Knochen eines folgenden Kämpfers über ein
Motorgelenk an einen Anker der Welt, tragen sein Gewicht und ziehen ihn zu seiner Lage im Clip. So steht die Figur, ohne
dass ihre Füße sie tragen, und was sie anstößt, zieht der Faden zurück in den Clip. Für echtes Schieben folgen nur noch
die Gelenke den Winkeln des Clips, mit begrenzter Kraft, und Füße und Balance tragen den Körper: Folgen ohne Fäden,
Schritt 2 von F77, jetzt als nächste Stufe von F64. Dieselben Clips bleiben, nur wie die Figur ihnen folgt, ändert sich.
Das braucht zuerst die eigene Kraft, F61: Von 4 × 1 920 mittleren Treffern auf Brust und Bauch fallen mit ihr nach Runde
54 noch 104, mit der Stütze gegen die Welt einer. Wer schiebt, braucht Halt unter dem Fuß, darum gehört auch der
Standfuß, der in späteren Schritten rutscht, Runde 55, dazu. Gemessen wird in der Demo derselbe Stoß gegen einen Kämpfer
mit und ohne Fäden, nebeneinander. Vorbilder zum Ansehen, nur als Idee: Overgrowth und die Euphoria-Figuren von GTA IV
und Red Dead Redemption 2 spielen Animationen und lassen erst bei einem Treffer die aktive Ragdoll übernehmen. Blade &
Sorcery, Exanima und Half Sword lassen Körper mit Muskeln kämpfen, die einander schieben und greifen. Gang Beasts zeigt,
wie weich und wackelig es wird, wenn alles physikalisch ist, für ein Spiel mit präziser Steuerung im PvP eine Warnung,
kein Vorbild.

Beide Arten ergänzen sich und werden beide gebaut, am selben Tag mit dem Nutzer geklärt. Eine Figur hat drei Zustände:
kinematisch für präzise Animation, Fortbewegung und Lagen ohne Körperkontakt, dynamische Clip-Verfolgung für Schieben,
Festhalten, Abstützen und andere Berührungen, und die aktive Ragdoll mit Balance für Treffer, Taumeln,
Ausgleichsschritte und Stürze. Die letzten beiden sind dieselbe Physik mit denselben Knochen, Gelenken und Muskeln, nur
anders gesteuert, so wie heute Folgen und Fangen dieselben Federn nutzen. Nichts wird weggeworfen: Die eigene Kraft, an
der die Runden seit 49 arbeiten, braucht die Figur in beiden. Dass sie Treffer fängt, macht die Clip-Verfolgung aber
noch nicht fertig. Die Reihenfolge: 1. die eigene Kraft so verlässlich, dass die Figur ihr Gewicht selbst trägt, F61, 2.
Folgen ohne Fäden, F64, 3. saubere Übergänge zwischen kinematischer Animation und dynamischer Steuerung, mit den Lagen,
Geschwindigkeiten und Muskeln der Knochen, F78 und Beginn und Ende des Folgens, 4. darauf Schieben, Festhalten, F54, und
Abstützen, F80, mit den Zielen für Hand und Fuß von F65.

Der Standfuß in späteren Schritten, Runde 55, auf Wunsch des Nutzers. Seit Runde 53 rutscht der Standfuß mit eigener
Kraft im dritten Schritt bei 40 statt 25 % der Schritte mehr als 5 cm, ab dem vierten bei 55 statt 44 %, und die
mittleren Treffer fallen meist im dritten Schritt. Über dieselben 329 Treffer einer Lage, die vor und seit Runde 53
einen dritten Schritt machen, rutscht er darin 6,6 statt 3,9 cm, es liegt also nicht an anderen Treffern. Er rutscht,
solange er allein trägt: Nach dem Aufsetzen federt das Standbein den Körper hoch, bis 0,6 m/s nach oben, und in der
Mitte des Schritts trägt der Standfuß oft fast nichts. Genau dort kehrte die Drehung des Beckens um, weil sie der Linie
zum schwingenden Knöchel auf seinem Bogen folgte, die am schnellsten dreht, wenn der Schwungfuß am Standfuß vorbeigeht:
in einem Zwölftel einer Sekunde, und der Oberkörper drehte in einer Neuntelsekunde um 90° zurück. Die Hüfte des
Standbeins drückte dabei in der Längsebene mit bis zu 289 N m und trat den Fuß, der kaum noch trug, nach hinten weg,
nach 89 N s auf die Brust im dritten Schritt 59 cm weit, in Runde 52 4 cm. Im ersten Schritt kehrt die Drehung nicht um.
Gebaut, `nbStepTurn`: Während ein Fuß schwingt, dreht das Becken von der Drehung mit dem Schwungfuß, wo er losging, zu
der mit ihm an seinem Ziel, glatt über die Zeit des Schritts. Mit eigener Kraft fallen von den leichten Treffern weiter
keiner, von den mittleren 72 statt 104, so wenige wie vor Runde 53 mit 73, von den harten 3 333 statt 3 416 und an den
Gliedern 948 statt 941, von allen 4 × 5 264 Treffern 4 353 statt 4 461. Ohne Stütze fallen 0, 243, 4 191 und 1 177 statt
5, 370, 4 234 und 1 163, 5 611 statt 5 767. Mit der Stütze gegen die Welt fallen 0, 0, 1 082 und 450 statt 0, 1, 1 039
und 446, 1 532 statt 1 486, 43 harte Treffer mehr, dort rutscht der Standfuß in späteren Schritten kaum, vorher wie
jetzt. Ab dem vierten Schritt rutscht er mit eigener Kraft bei 45 statt 55 % der Schritte mehr als 5 cm, so oft wie in
Runde 52, im dritten im Mittel 5,0 statt 6,0 cm, aber weiter bei 41 %. Schultern und Hüften drehen im Schritt mit
eigener Kraft bis 33,4 statt 34,0° gegeneinander. Gemessen und verworfen: Eine Drehung, die nur so weit folgt, wie der
Standfuß trägt, ein Standbein und eine Hüfte, die nur nach der Last ihres Fußes drücken, mehr Dämpfung der Höhe des
Beckens, ein Schwungfuß, der langsamer landet, eine Gegendrehung mit begrenzter Geschwindigkeit und eine breitere Linie
zwischen den Knöcheln ließen mehr fallen. Eine Drehung nach dem Weg des geplanten Knöchels statt nach der Zeit lässt in
der Summe weniger fallen, mit eigener Kraft 4 304, aber 81 mittlere, und ab dem vierten Schritt rutscht der Standfuß bei
53 % der Schritte. Siehe „Drehung über die Zeit des Schritts“ und die Zeilen danach in
[Optimierungen](Optimierungen.md). Die Tests ändern nur ihre Hashes. Gepaart gegen `active-ragdoll-54`, zwei Läufe im
Wechsel mit vier Threads und einer mit einem, rechnen alle Szenen ohne Figuren, die einzelne Figur hinter ihrer Mauer,
die Kämpfer und die Ragdolls im Schutt dasselbe, und die Szenen mit getroffenen Figuren enden anders, mit einem und mit
vier Threads gleich: Hinter ihren Mauern werden 4 statt einer der 10 Figuren umgeworfen, als die Mauern brechen, und 2
liegen am Ende noch, in der Stadt mit Figuren stehen am Ende 29 statt 28, und ein Bild der Stadt mit Figuren kostet das
0,92- bis 0,97-fache, bei anderer Arbeit, im Rauschen der VM. Vorgeschlagen ist als Runde 56 weiter die eigene Kraft,
Schritt 1 der Reihenfolge: der Standfuß, der im dritten Schritt nach dem Hochfedern noch rutscht, oder die harten
Treffer mit der Stütze gegen die Welt.

Der Standfuß im dritten Schritt, Runde 56, auf Wunsch des Nutzers. Nach Runde 55 rutscht der Standfuß mit eigener Kraft
im dritten Schritt weiter bei 41 statt 25 % der Schritte mehr als 5 cm. Wie weit die Drehung über den Schritt geht,
spielt dafür kaum eine Rolle, wohin der Schritt geht, schon: rückwärts rutscht er bei 43 statt 17 % der Schritte, zur
Seite weg vom Standfuß bei 47 statt 38 %, zu ihm hin bei 17 statt 8 % und nach vorn wie vorher bei 48 %. Bei Schritten
rückwärts gleitet seine Sohle in der Mitte des Schritts, wo die Gegendrehung des Oberkörpers am schnellsten umkehrt und
das Hochfedern nach dem Aufsetzen den Standfuß entlastet, in 48 statt 27 % der Updates schneller als 0,3 m/s, bei 0,60
statt 0,63 des Gewichts. Ohne Drehung bei Schritten rückwärts rutscht er im dritten Schritt nur noch bei 24 bis 29 % der
Schritte, aber von allen 4 × 5 264 Treffern fallen mit eigener Kraft 4 705 bis 4 796 statt 4 353, und ohne Stütze fallen
leichte Treffer. Dreht in diesen Schritten nur das Becken, fallen auf den neun Tabellen von den mittleren Treffern 58
statt 19 und von den harten 948 statt 835, dreht nur der Oberkörper zurück, 23 und 795: Die Gegendrehung des Oberkörpers
fängt harte Treffer, das gedrehte Becken nicht. Gebaut, `nbStepShare`: Das Becken nimmt bei einem Schritt nur seinen
Anteil an der Drehung, die ganze, wenn der Schwungfuß nach vorn oder zur Seite geht, keine bei einem Schritt gerade
zurück, dazwischen eins plus den Kosinus des Winkels zwischen seinem Weg und vorn. Der Anteil geht glatt über die Zeit
jedes Schritts von dem, den das Becken hatte, zu dem des Schritts und bleibt im Stand. Der Oberkörper dreht wie bisher
um die ganze Drehung zurück. Mit eigener Kraft fallen von den leichten Treffern weiter keiner, von den mittleren 81
statt 72, von den harten 3 139 statt 3 333 und an den Gliedern 936 statt 948, von allen 4 156 statt 4 353. Ohne Stütze
fallen 0, 170, 4 118 und 1 158 statt 0, 243, 4 191 und 1 177, 5 446 statt 5 611. Mit der Stütze gegen die Welt fallen 0,
0, 1 062 und 440 statt 0, 0, 1 082 und 450, 1 502 statt 1 532. Im dritten Schritt rutscht der Standfuß mit eigener Kraft
bei 34 statt 41 % der Schritte mehr als 5 cm, bei 372 statt 471 dritten Schritten, zur Seite weg bei 31 statt 47 %,
rückwärts aber weiter bei 49 %, und nach harten Treffern bei 62 statt 57 %. Im zweiten Schritt rutscht er bei 48 statt
43 % der Schritte, ab dem vierten wie vorher bei 45 %. Schultern und Hüften drehen im Schritt mit eigener Kraft bis 32,6
statt 33,4° gegeneinander, mit der Stütze gegen die Welt bis 27,2 statt 27,1°. Gemessen und verworfen: Becken und
Oberkörper ohne Drehung bei Schritten rückwärts oder nur, soweit der Schritt nach vorn geht, griffigere Sohlen, auch
zusammen damit, längere Schritte, weniger Drehung oder Gegendrehung, eine Umkehr, die bei Schritten rückwärts früher
oder nur halb kommt, eine Gegendrehung früher im Schritt, mit der Beugung oder mit bewegtem Ziel, eine Drehung im
Rahmen, in den der Standfuß beim Beginn des Schritts zeigte, Arme, deren Schwung in der Welt bleibt, und eine andere
Höhe des Beckens gegen das Hochfedern. Griffigere Sohlen mit 1,1 statt 0,9 fangen auf Runde 55 mehr, mit eigener Kraft
fallen 4 114, mit der Stütze gegen die Welt 1 507 und ohne Stütze 5 473, aber ohne Stütze ein leichter Treffer, und der
Standfuß rutscht im dritten Schritt weiter bei 39 %. Siehe „Das Becken mit seinem Anteil an der Drehung“ und die Zeilen
danach in [Optimierungen](Optimierungen.md). Die Tests ändern nur ihre Hashes. Gepaart gegen `active-ragdoll-55`, zwei
Läufe im Wechsel mit vier Threads und einer mit einem, rechnen alle Szenen ohne Figuren, die einzelne Figur hinter ihrer
Mauer, die Kämpfer und die Ragdolls im Schutt dasselbe, und die Szenen mit getroffenen Figuren enden anders, mit einem
und mit vier Threads gleich: Hinter ihren Mauern stehen am Ende wie vorher 8 der 10 und alle 50 Figuren, in der Stadt
mit Figuren wie vorher 29, nur eine liegt statt drei, und ein Bild der Stadt mit Figuren kostet das 1,00-, 1,02- und
0,96-fache, bei anderer Arbeit, im Rauschen der VM. Vorgeschlagen ist als Runde 57 weiter die eigene Kraft, Schritt 1
der Reihenfolge: der Standfuß bei Schritten rückwärts, der in der Mitte des dritten Schritts weiter bei der Hälfte der
Schritte rutscht, oder griffigere Sohlen, wenn geklärt ist, warum mit ihnen ein leichter Treffer fällt.

Der Standfuß bei Schritten rückwärts, Runde 57, auf Wunsch des Nutzers. Nach Runde 56 rutscht der Standfuß mit eigener
Kraft bei Schritten rückwärts im zweiten Schritt bei 58 % der Schritte mehr als 5 cm und im dritten bei 49 statt 17 % in
Runde 52. Bei Schritten gerade zurück dreht das Becken seit Runde 56 nicht mit, der Oberkörper aber weiter um das
Dreifache der Drehung zurück, und gegen seine Gegendrehung hält nur der Standfuß: In der Mitte des dritten Schritts
rückwärts, wo das Hochfedern nach dem Aufsetzen die Sohle auf 0,63 des Gewichts entlastet, muss der Boden ihn im Mittel
mit 33 statt 24 N m um die Hochachse halten, der Fuß liegt flach auf allen vier Ecken der Sohle, seine Reibung gegen
Drehen hält bis 46 N m, und er ist in 47 statt 28 % dieser Updates an ihrer Grenze. Gebaut, `NB_BACK_TWIST` und
`NB_SOLE_FRICTION`: Der Oberkörper dreht bei einem Schritt gerade zurück nur um 0,85 der Drehung zurück, bei den anderen
Schritten um 0,85 plus 0,15 mal dem Anteil des Beckens, `nbStepShare`, so springt die Gegendrehung nicht, wenn der
Schritt endet, und die Sohlen greifen mit 1,1 statt 0,9, auf dem Boden mit 0,6 mit 0,81 statt 0,73. Weniger Gegendrehung
allein lässt den Standfuß weniger rutschen, mit einem Viertel im dritten Schritt rückwärts bei 25 %, fängt aber weniger
harte Treffer, auf den neun Tabellen fallen von den 2 064 harten 866 statt 789, griffigere Sohlen allein fangen mehr,
756, lassen ihn aber bei 47 % rutschen. Mit eigener Kraft fallen von den leichten Treffern weiter keiner, von den
mittleren 48 statt 81, von den harten 3 013 statt 3 139 und an den Gliedern 885 statt 936, von allen 3 946 statt 4 156.
Ohne Stütze fallen 0, 149, 4 087 und 1 153 statt 0, 170, 4 118 und 1 158, 5 389 statt 5 446. Mit der Stütze gegen die
Welt fallen 0, 0, 1 062 und 425 statt 0, 0, 1 062 und 440, 1 487 statt 1 502. Bei Schritten rückwärts rutscht der
Standfuß mit eigener Kraft im zweiten Schritt bei 42 statt 58 % der Schritte mehr als 5 cm, im dritten bei 29 statt
49 %, über alle Schritte im zweiten bei 40 statt 48 %, im dritten bei 29 statt 34 %, ab dem vierten aber bei 53 statt
45 %, zur Seite weg bei 60 statt 53 %. Nach harten Treffern rutscht er im dritten Schritt bei 59 statt 62 %. In der
Mitte des dritten Schritts rückwärts ist er in 39 statt 47 % der Updates an der Grenze seiner Reibung gegen Drehen, die
dort bis 50 statt 46 N m hält. Schultern und Hüften drehen im Schritt mit eigener Kraft bis 31,5 statt 32,6°
gegeneinander, mit der Stütze gegen die Welt bis 26,3 statt 27,2°. Gemessen und verworfen: eine Gegendrehung, die bei
Schritten rückwärts früher fertig ist, erst ab dem zweiten oder dritten Schritt kleiner wird, nur so schnell folgt, wie
der Standfuß hält, oder langsamer folgt, ein Becken, das bei Schritten rückwärts seine Drehung behält, weniger
Gegendrehung ohne griffigere Sohlen, griffigere Sohlen allein und 0,5 bis 0,9 der Gegendrehung mit Sohlen von 1,0 oder
1,1. Mit 0,75, 0,8 und 0,9 und Sohlen von 1,1 rutscht der Standfuß rückwärts im dritten Schritt bei 32, 27 und 35 %, mit
eigener Kraft fallen 3 979, 3 971 und 3 926, ohne Stütze 5 462, 5 449 und 5 375, und je ein oder zwei leichte Treffer
fallen ohne Stütze oder mit eigener Kraft. Auf 17 Varianten nahe der gebauten, 0,5 bis 1 der Gegendrehung mit Sohlen von
0,9 bis 1,2, fallen ohne Stütze null bis drei der 168 leichten Treffer, auch mit den Sohlen von Runde 56: Ob einer
fällt, entscheidet hier der Zufall, gebaut ist der Wert, mit dem mit allen drei Stützen keiner fällt. Siehe „Weniger
Gegendrehung bei Schritten zurück und griffigere Sohlen“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Die
Tests ändern ihre Hashes, und der Test der Trümmer der Wand nimmt wieder das zweite Bruchmuster: Im dritten fliegt die
Figur mit den neuen Sohlen 1,15 m hinter die Wand, an ihren Stücken vorbei, und von 32 Mustern steht sie hinter der Wand
in 9 statt 11 auf und nach dem Block in 8 statt 9. Gepaart gegen `active-ragdoll-56`, zwei Läufe im Wechsel mit vier
Threads und einer mit einem, rechnen alle Szenen ohne Figuren dasselbe, und die Szenen mit Figuren rechnen mit den neuen
Sohlen anders, mit einem und mit vier Threads gleich: Hinter ihren Mauern stehen am Ende alle 10 statt 8 Figuren, von
den 50 werden 27 statt 29 umgeworfen, als die Mauern brechen, 7 statt 5 fangen sich, und am Ende stehen 49, eine liegt
noch. In der Stadt mit Figuren entstehen bis zu 63 780 statt 55 919 Kontakte, am Ende stehen wie vorher 29, und keine
liegt statt einer. Ein Bild der Stadt mit Figuren kostet das 0,95- und 0,94-fache, mit einem Thread das 1,10-fache, im
Rauschen der VM. Vorgeschlagen ist als Runde 58 weiter die eigene Kraft, Schritt 1 der Reihenfolge: der Standfuß ab dem
vierten Schritt, der bei 53 % der Schritte rutscht, zur Seite weg bei 60 %, oder die Schritte nach vorn, bei denen er im
zweiten Schritt bei 55 % rutscht.

Der Standfuß ab dem vierten Schritt, Runde 58, auf Wunsch des Nutzers. Nach Runde 57 rutscht der Standfuß mit eigener
Kraft ab dem vierten Schritt bei 53 statt 45 % der Schritte mehr als 5 cm, zur Seite weg bei 60 %, über vier Lagen des
Treffpunkts bei 51,5 % von 853 Schritten nach den mittleren Treffern, die die Figur fängt. 175 der 287 Schritte zur
Seite weg, bei denen er so rutscht, folgen auf zwei Schritte rückwärts und einen zum Standfuß hin: Bei den Schritten
rückwärts läuft der Schwerpunkt seitlich am Standfuß vorbei, die Figur kippt immer schneller zur Seite, und der vierte
Schritt muss weit zur Seite. Die Ursache des Rutschens liegt aber in jedem Schritt: Das Standbein hielt das Becken auf
Standhöhe, bis der Fuß landete, und das Schwungbein, das seinem Ziel etwa 0,1 s nachläuft, erreichte den Boden von dort
nicht. Fast jeder Schritt landet nach seinem geplanten Ende, ab dem vierten Schritt bei 1,23 seiner Dauer, in den
Updates danach ist der schwingende Knöchel im Mittel 3,9 cm über dem Boden und 11 cm vor seinem Ziel, und solange hielt
die Stütze das Becken aufrecht und das Standbein drückte es auf Standhöhe: Der Standfuß glitt ab dem vierten Schritt in
40 % dieser Updates schneller als 0,3 m/s, im ersten Schritt rückwärts und zur Seite in 32 %, mit der Stütze gegen die
Welt in 40 %, ohne Stütze nur in 11 %. Gebaut, `NB_DROP_PHASE`: Ab 0,7 der Schrittdauer senkt das Standbein das Becken,
ganz am geplanten Ende, so tief, wie es mit beiden Füßen an ihren Zielen aufrecht stehen kann, wie beide Beine im
breiten Stand, `nbReachableHeight`, nach dem Anteil, den der Schritt nicht nach vorn geht, `nbDropShare`, und nur,
solange eine Stütze das Becken aufrecht hält. Mit eigener Kraft fallen von den leichten Treffern weiter keiner, von den
mittleren 40 statt 48, von den harten 2 961 statt 3 013 und an den Gliedern 864 statt 885, von allen 3 865 statt 3 946.
Mit der Stütze gegen die Welt fallen 0, 0, 1 062 und 443 statt 0, 0, 1 062 und 425, 1 505 statt 1 487, ohne Stütze
rechnet die Figur bitgleich wie vorher. Nach den mittleren Treffern braucht die Figur mit eigener Kraft ab dem vierten
Schritt 410 statt 853 Schritte, und der Standfuß rutscht bei 201 statt 439 davon mehr als 5 cm, bei 49 statt 51 %, zur
Seite weg bei 87 statt 287, rückwärts bei 22 statt 44, im ersten Schritt bei 9 statt 19 % der Schritte, mit der Stütze
gegen die Welt bei 23 statt 43 %. Über alle Schritte rutscht er bei 2 889 statt 5 453 mehr als 5 cm, bei 14 statt 24 %,
und die Figur macht 20 191 statt 23 054 Schritte. Gemessen und verworfen: das Becken früher oder ab der Mitte des
Schritts tiefer, später, flacher, halb so tief, höchstens 4 oder 8 cm tiefer, erst ab dem zweiten, dritten oder vierten
Schritt, nur so tief, wie das Standbein reicht, nur so tief, wie das Schwungbein braucht, das auch bei jedem Schritt,
und das Absenken auch ohne Stütze. Bei jedem Schritt abgesenkt fielen mit eigener Kraft von den mittleren 66 statt 48:
Bei einem Schritt nach vorn wird der Fuß, der landet, vor dem Körper zum Standfuß des nächsten Schritts, sein Bein
richtet das abgesenkte Becken wieder auf, und nach Stößen, auf die die Figur nach vorn tritt, rutschte der Standfuß im
zweiten Schritt bis 8 cm weiter, bis 40 cm. Nur so tief, wie das Schwungbein braucht, und nicht nach vorn fallen 3 937
und 1 486, und ab dem vierten Schritt rutscht der Standfuß bei 266 der 586 Schritte. Ohne Stütze kippt das Becken mit
dem Körper über den Standfuß und bringt die Hüfte des Schwungbeins von selbst herunter: Abgesenkt fielen dort 5 273
statt 5 389, aber der Standfuß rutschte im ersten Schritt bei 16 statt 10 % der Schritte. Siehe „Becken spät im Schritt
tiefer“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Die Tests ändern ihre Hashes, und nach 80 N s nach
links schwingt der Arm, der rudert, gegen die Brust mit bis zu 15,7 statt 14,9 rad/s, denn das Becken kommt bei der
Landung des Schritts zur Seite auf 0,785 statt 0,793 m herunter: Der Test erlaubt 16 statt 15 rad/s, gerissen hatten die
Arme in Runde 23 mit 21. Gepaart gegen `active-ragdoll-57`, zwei Läufe im Wechsel mit vier Threads und einer mit einem,
rechnen alle Szenen ohne Figuren, die einzelne Figur hinter ihrer Mauer, die Kämpfer und die Ragdolls im Schutt
dasselbe, und die Szenen mit getroffenen Figuren enden anders, mit einem und mit vier Threads gleich: Hinter ihren
Mauern stehen am Ende wie vorher alle 10 Figuren, eine fängt sich, statt umgeworfen zu werden, von den 50 stehen 49, und
die eine steht gerade auf, statt zu liegen. In der Stadt mit Figuren stürzen 5 statt 6 Stockwerke ein, bei bis zu 35 654
statt 63 780 Kontakten, die Balance macht 25 statt 32 Schritte, fängt wie vorher 7, 22 statt 24 werden umgeworfen, und
am Ende stehen 31 statt 29. Ein Bild der Stadt mit Figuren kostet das 0,97- und 0,99-fache, mit einem Thread das
0,92-fache, bei anderer Arbeit und im Rauschen der VM. Vorgeschlagen ist als Runde 59 weiter die eigene Kraft, Schritt 1
der Reihenfolge: die Schritte nach vorn, die das Becken nicht absenken und bei denen der Standfuß im zweiten Schritt bei
58 % rutscht, oder ein Schwungbein, das seinem Ziel schneller folgt, denn fast jeder Schritt landet nach seinem
geplanten Ende.

Ideen von `ragdoll-sol`, Runde 59, auf Wunsch des Nutzers. ChatGPT Sol hat auf der Branch `ragdoll-sol` von Runde 51 aus
weitergearbeitet, und der Nutzer bat, die Ideen anzusehen und auszuprobieren, was sinnvoll ist, ohne alles zu
übernehmen. Im Kern antwortet Sol auf zwei Rückmeldungen des Nutzers: Der Körper wirkt steif, Verwringen, Beugen und
Mitkippen fehlen, und der Kopf wackelt nach Treffern zu stark. Sol lässt den Rumpf nachgeben und dämpft Hals und Kopf,
und die meisten übrigen Bausteine fangen die Stürze wieder auf, die der nachgebende Rumpf kostet. Jede Idee ist auf
Runde 58 nachgebaut und über alle Tabellen und vier Lagen des Treffpunkts mit allen drei Stützen gemessen. Bei uns
verdreht sich der Rumpf seit F79 nach 60 N s auf die Brust schon bis 40° gegen das Becken, beugt sich aber nur 15 bis
17°, und der Kopf dreht sich gegen den Hals mit bis zu 5,3 rad/s, so wie auf `ragdoll-sol` vor der Änderung. Gebaut sind
vier Ideen: Solange die Figur sich fängt, dämpfen Hals und Kopf achtmal so stark, `nbBoneDef::catchDamping`, und der
Kopf dreht sich nach 60 N s gegen den Hals mit höchstens 2,8 statt 5,3 rad/s und schwingt weniger als halb so stark
nach, siehe F48. Der erste Schritt mit eigener Kraft geht mit dem Fuß auf der Seite, zu der die Figur fällt,
`NB_FIRST_STEP_SIDE`. Steht sie mit der Stütze durch die Beine nach dem Fangen, drücken die Beine den Boden um die
Geschwindigkeit des Schwerpunkts mal 0,6 s vor ihm, und der Boden bremst die Restbewegung, `NB_STAND_BRAKE`. Und bei 30
Hz oder langsamer tritt sie nach einem Update mit dem Capture Point außerhalb statt nach zweien. Von allen 4 × 5 264
Treffern fallen mit eigener Kraft 3 752 statt 3 865, von den mittleren 18 statt 40, mit der Stütze gegen die Welt 1 532
statt 1 505, 28 harte mehr für den ruhigeren Kopf, ohne Stütze 5 258 statt 5 389, von den mittleren aber 189 statt 149,
von den leichten mit allen drei Stützen keiner. Nach den mittleren Treffern steht die Figur mit eigener Kraft nach 2,03
statt 2,24 s und 2,36 statt 2,65 Schritten still. Mit 30 Hz fallen auf den neun Tabellen auf Brust und Bauch mit eigener
Kraft 1 465 statt 1 739 und mit der Stütze gegen die Welt 409 statt 567, bei 60 Hz 752 und 266, siehe F81. Nicht
übernommen: der Rumpf, der nachgibt, wie Sol ihn baut, mit dem er sich bis 40° beugt, aber mit eigener Kraft fielen
5 991, mit der Stütze gegen die Welt 2 711, und leichte Treffer fielen, siehe F82, die Mitte der Stützfläche als
Flächenschwerpunkt, das Schrittziel mit Seitenabstand, die Zehen nur bei Schritten nach vorn und die geglättete
Hüftbremse, die geometrisch richtiger oder ruhiger gedacht sind, aber nicht mehr fangen, und Hals und Kopf viermal
gedämpft. Siehe „Ideen von ragdoll-sol zusammen“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Der Test
der Trümmer hinter der Wand nimmt jetzt das fünfte Bruchmuster: Im zweiten steht die Figur nach dem Block nicht mehr
auf, von 32 Mustern steht sie hinter der Wand in 10 und nach dem Block in 7 auf. Gepaart gegen `active-ragdoll-58`, zwei
Läufe im Wechsel mit vier Threads und einer mit einem, rechnen alle Szenen ohne Figuren, die 50 Kämpfer und die 20
Ragdolls im Schutt dasselbe wie vorher, und die Szenen mit getroffenen Figuren enden anders, mit einem und mit vier
Threads gleich. Die einzelne Figur hinter ihrer Mauer steht am Ende wie vorher, von den 10 hinter ihren Mauern werden 4
statt 3 umgeworfen, als die Mauern brechen, keine statt einer fängt sich, und am Ende stehen wie vorher alle, von den 50
werden 28 statt 27 umgeworfen, 5 statt 7 fangen sich, und am Ende stehen wie vorher 49, eine steht gerade auf. In der
Stadt mit Figuren stürzen 6 statt 5 Stockwerke ein, mit bis zu 86 968 statt 35 654 Kontakten, die Balance macht 27 statt
25 Schritte, fängt 9 statt 7, 22 werden wie vorher umgeworfen, und am Ende stehen 29 statt 31, eine fängt sich noch, und
zwei liegen. Ein Bild der Stadt mit Figuren kostet das 1,11- und 1,14-fache, mit einem Thread das 1,19-fache, bei mehr
Arbeit mit dem sechsten Stockwerk. Vorgeschlagen ist als Runde 60 der Rumpf, der nachgibt, F82, auf die Rückmeldung des
Nutzers an Sol, oder die Balance bei 30 Hz, F81.

Rumpf gibt nach, Runde 60, F82, auf die Rückmeldung des Nutzers an Sol: Der Körper wirkt steif, Verwringen, Beugen und
Mitkippen fehlen. Seit F79 verdreht sich der Rumpf nach 60 N s auf die Brust bis 40° gegen das Becken, beugt sich aber
nur 17 bis 19°. Wie auf `ragdoll-sol` mit 40 % der Kraft fielen mit eigener Kraft 4 981 statt 3 752, weil die
Hüftstrategie das Drehmoment der Wirbelsäule braucht. Gebaut ist deshalb eine Wirbelsäule, die weicher wird statt
schwächer: Nach einem Treffer ab der Hälfte von `knockdownImpulse` auf die Wirbelsäule ziehen ihre vier Gelenke 0,15 s
lang mit 0,3 ihrer Steifigkeit und nehmen sie in 0,35 s weich wieder auf, mit derselben Kraft,
`nbBoneDef::hitStiffness`. So weit, wie der Treffer von vorn oder von hinten kommt, nach dem Quadrat des Kosinus zur
Blickrichtung: Von der Seite bleibt die Wirbelsäule steif, denn ihre Gelenke lassen zur Seite nur 34° zu, weicher beugte
sie sich dort nur bis 3° weiter, und ohne Stütze fielen dann leichte Treffer von der Seite. Nach 60 N s von vorn und von
hinten beugt sich der Rumpf mit der Stütze gegen die Welt bis 25,1 und 30,8 statt 18,9 und 18,5°, nach 80 N s bis 23,1
und 35,4 statt 16,5 und 21,1°, mit eigener Kraft nach 60 N s bis 20,1 und 25,0 statt 16,8 und 16,5°. Von allen 4 × 5 264
Treffern fallen mit eigener Kraft 3 733 statt 3 752, von den mittleren 27 statt 18, mit der Stütze gegen die Welt 1 523
statt 1 532 und ohne Stütze 5 165 statt 5 258, von den leichten mit allen drei Stützen keiner, und Treffer auf die
Glieder rechnen wie vorher. Nach den mittleren Treffern steht die Figur mit eigener Kraft nach 2,09 statt 2,03 s still.
Gemessen und verworfen: weniger Kraft statt weicher, in jeder Form, eine Wirbelsäule, die nach jedem Treffer aus jeder
Richtung nachgibt, mit 0,5 bis 0,1 der Steifigkeit, zur Seite halb so weich, weicher nur bis zum Doppelten von
`knockdownImpulse`, kürzer weich und schneller zurück: Ohne Stütze fielen bei allen leichte Treffer, fast alle von der
Seite bei 74 bis 80 N s. Nur von vorn und hinten weich fängt sie mit 0,4 so gut wie mit 0,3 und beugt sich weniger, mit
0,2 fallen mit eigener Kraft 3 813. Vorher festgelegt war: keine leichten Stürze, mit jeder Stütze höchstens 1 % mehr
als Runde 59, und dann so weich wie möglich. Siehe „Rumpf gibt nach“ und die Zeilen danach in
[Optimierungen](Optimierungen.md). Die Tests ändern ihre Hashes, und der Test der Trümmer hinter der Wand nimmt wieder
das zweite Bruchmuster: Im fünften steht die Figur nach dem Block nicht mehr auf, von 32 Mustern steht sie hinter der
Wand in 12 und nach dem Block in 8 auf. Gepaart gegen `active-ragdoll-59`, zwei Läufe im Wechsel mit vier Threads und
einer mit einem, rechnen alle Szenen ohne Figuren, die 50 Kämpfer, die 20 Ragdolls im Schutt und die Stadt mit Figuren
dasselbe wie vorher: Explosionen werfen alle Knochen zugleich und gehen nicht über die Treffer, auf die die Wirbelsäule
nachgibt. Die Szenen mit Figuren hinter ihren Mauern enden anders, mit einem und mit vier Threads gleich. Am Ende stehen
dort wie vorher die eine, alle 10 und 49 der 50, eine steht gerade auf. Die eine steht nach dem Umwerfen 0,2 s später
auf, die 10 machen, als die Mauern brechen, 8 statt 5 Schritte, und von den 50 fangen sich dabei 4 statt 5, 29 statt 28
werden umgeworfen. Ein Bild der Stadt mit Figuren kostet das 1,03- und 1,08-fache, mit einem Thread das 1,00-fache, bei
derselben Arbeit im Rauschen der VM. Vorgeschlagen ist als Runde 61 die Balance bei 30 Hz, F81, oder das Mitkippen zur
Seite, F82.

Der Standfuß in späteren Schritten, Runde 61, auf Wunsch des Nutzers. Nach Runde 60 rutscht der Standfuß mit eigener
Kraft vor allem im zweiten Schritt: Auf den neun Tabellen auf Brust und Bauch, wie der Treffpunkt liegt, nach den
mittleren Treffern, die die Figur fängt, bei 43,0 % der Schritte mehr als 5 cm, im Mittel 7,1 cm, nach vorn bei 56 %,
rückwärts bei 60 %, im ersten Schritt bei 6,7 %, nach den harten Treffern im zweiten Schritt bei 71,8 %, im Mittel
17,8 cm. In einem verfolgten Treffer, 85 N s von vorn auf die Brust, steht das Standbein am Ende des ersten Schritts
zurück gestreckt und bis 38° schräg, sein Fuß trägt fast nichts mehr, und der Körper fällt mit bis zu 1,1 m/s. Das Bein,
das landet, fängt ihn mit bis zum 3,5-Fachen seines Gewichts, das Knie an der Grenze seines Drehmoments, drückt das
Becken von bis zu 17 cm unter Standhöhe hinauf und wirft den Körper mit 0,9 m/s wieder hoch. Ab der Mitte des zweiten
Schritts trägt sein Fuß kaum noch etwas, Hüfte und Knie schieben ihn vom Körper weg, und er rutscht 44 cm. Ebenso nach
vorn und zum Standfuß hin. Das Ziel, zu dem das Standbein reicht, folgte dem Fuß, solange er den Boden berührte, und so
hielt nichts einen Fuß, der wenig trug. Gebaut, `NB_PLANT_LOAD`: Solange eine Stütze das Becken aufrecht hält, folgt das
Ziel des Standfußes dem Fuß nur, solange er mindestens 0,7 des Gewichts trägt, sonst bleibt es, wo er zuletzt so viel
trug, und das Standbein zieht ihn dorthin zurück. Im verfolgten Treffer rutscht er im zweiten Schritt 14 statt 44 cm.
Mit eigener Kraft fallen von den leichten Treffern weiter keiner, von den mittleren 20 statt 27, von den harten 2 687
statt 2 848 und an den Gliedern 777 statt 858, von allen 4 × 5 264 Treffern 3 484 statt 3 733. Mit der Stütze gegen die
Welt fallen 0, 0, 875 und 407 statt 0, 0, 1 081 und 442, 1 282 statt 1 523, ohne Stütze rechnet die Figur bitgleich wie
vorher. Über vier Lagen des Treffpunkts rutscht der Standfuß nach den mittleren Treffern mit eigener Kraft ab dem
zweiten Schritt bei 761 statt 1 032 Schritten mehr als 5 cm, bei 31,8 statt 38,7 %, im zweiten bei 32,6 statt 42,8 % und
im Mittel 4,3 statt 7,0 cm, rückwärts bei 50,5 statt 60,7 %, ab dem vierten bei 19,0 statt 28,0 %, im dritten bei 31,0
statt 27,9 % von 462 statt 570 Schritten, im ersten aber bei 8,6 statt 6,6 %, bei Schritten zur Seite weg spät nach dem
geplanten Ende. Nach den harten Treffern rutscht er ab dem zweiten Schritt bei 53,9 statt 58,6 % der Schritte, im
zweiten bei 65,3 statt 69,4 % und im Mittel 12,5 statt 16,9 cm. Vorher festgelegt war: keine leichten Stürze, mit jeder
Stütze höchstens 1 % mehr als Runde 60, mit eigener Kraft nicht mehr mittlere als Runde 60, und dann die Schwelle, bei
der der Standfuß ab dem zweiten Schritt am seltensten rutscht: 0,7 mit 761 Schritten, 0,5 mit 806, und 1,0 mit 748 ließ
41 mittlere fallen. Gemessen und verworfen: ein Becken, das sich nach dem Aufsetzen langsamer hebt, auch nur bei
Schritten zurück, ein Standbein, das sich nicht streckt, solange sein Fuß kaum trägt, ein Standfuß, der den ganzen
Schritt stehen bleibt, auch im Stand, oder erst ab dem zweiten Schritt, und die Regel auch ohne Stütze. Langsamer
gehoben fielen mit eigener Kraft auf den neun Tabellen von den mittleren 21 bis 46 statt 8: Das Becken blieb tiefer, und
im dritten Schritt zur Seite weg rutschte der Standfuß weg. Den ganzen Schritt stehend fiel mit eigener Kraft ein
leichter Treffer, erst ab dem zweiten Schritt fielen mehr als vorher, und ohne Stütze fielen mit jeder Form einer bis
sechs der leichten Treffer, die meisten von der Seite. Siehe „Standfuß bleibt, wo er zuletzt trug“ und die Zeilen danach
in [Optimierungen](Optimierungen.md). Die Tests ändern ihre Hashes, `FigureCatchTest` gibt 124 statt 114 N s von rechts,
denn die Figur fängt mit der Stütze gegen die Welt jetzt alle bis 114 N s, wo sie bei 108 und 114 fiel, der rudernde Arm
in `FigureRowTest` schwingt bis 1,90 statt 1,83 rad, der Test erlaubt 2,0 statt 1,9, und `FigureNeckTest` vergleicht die
Umkehrungen des Kopfes ohne Armature mit denen mit ihr statt mit 40 % der Schritte: Der ganze Fang ist ruhiger, ohne
Armature kehrt der Kopf in 38 statt 56 % der Schritte um, mit ihr in 4 statt 10 %. Gepaart gegen `active-ragdoll-60`,
zwei Läufe im Wechsel mit vier Threads und einer mit einem, rechnen alle Szenen ohne Figuren, die einzelne Figur hinter
ihrer Mauer, die 50 Kämpfer und die 20 Ragdolls im Schutt dasselbe wie vorher, und die Szenen mit getroffenen Figuren
enden anders, mit einem und mit vier Threads gleich: Hinter ihren Mauern stehen am Ende wie vorher alle 10 Figuren, die,
als die Mauern brechen, 5 statt 8 Schritte machen, und alle 50 statt 49, von denen dabei 5 statt 4 sich fangen. In der
Stadt mit Figuren stürzen wie vorher 6 Stockwerke ein, bei bis zu 83 790 statt 86 968 Kontakten, die Balance macht 31
statt 27 Schritte, fängt 6 statt 9, 25 statt 22 werden umgeworfen, und am Ende stehen 28 statt 29, zwei stehen gerade
auf. Ein Bild der Stadt mit Figuren kostet das 0,99- und 0,95-fache, mit einem Thread das 0,96-fache, im Rauschen der
VM. Vorgeschlagen ist als Runde 62 weiter die eigene Kraft: der erste Schritt zur Seite weg, in dem der Standfuß spät
nach dem geplanten Ende jetzt öfter rutscht, oder das Hochfedern nach dem Aufsetzen, das die Regel nicht nimmt, oder die
Balance bei 30 Hz, F81.

Der erste Schritt zur Seite, Runde 62, auf Wunsch des Nutzers. Seit Runde 61 bewegt sich der Standfuß mit eigener Kraft
im ersten Schritt zur Seite weg öfter mehr als 5 cm: Auf den neun Tabellen auf Brust und Bauch, wie der Treffpunkt
liegt, nach den mittleren Treffern, die die Figur fängt, im allerersten Schritt nach dem Treffer bei 16,5 statt 11,0 %
der Schritte, fast immer spät nach dem geplanten Ende. Ein Protokoll, das den Weg des Standfußes danach teilt, ob er
dabei mindestens 5 % des Gewichts trägt, zeigt: Über den Boden glitt er seltener als in Runde 60, bei 7,0 statt 10,4 %
der Schritte mehr als 5 cm, länger wurde der Weg, auf dem er nichts trug. In verfolgten Treffern trägt er ab 0,8 der
Schrittdauer nichts mehr, das Standbein steht gestreckt und 38 bis 44° schräg, der Körper fällt mit bis zu 2,1 m/s zur
Seite, und das Bein trägt den Fuß 3 bis 4 cm über dem Boden mit, bis der Schwungfuß aufsetzt. Der setzt in fast jedem
Schritt erst nach dem geplanten Ende auf, mit eigener Kraft im Mittel bei 1,18 der Schrittdauer, mit der Stütze gegen
die Welt bei 1,22, denn das Schwungbein folgt seinem Ziel etwa 0,1 s spät: Im verfolgten Treffer ist der Fuß am
geplanten Ende noch 3,3 cm über dem Boden. Seit Runde 61 setzt er in manchen Treffern ein Update später auf, und so
lange trägt das Bein den Standfuß weiter mit. Gebaut, `NB_REACH_PHASE` und `NB_REACH_DEPTH`: Spät im Schritt greift das
Schwungbein unter den Boden, solange eine Stütze das Becken aufrecht hält ab 0,6 der Schrittdauer, am geplanten Ende
5 cm tief, ohne Stütze ab 0,9 und 3 cm tief, `NB_FREE_REACH_PHASE` und `NB_FREE_REACH_DEPTH`. Der Fuß setzt mit eigener
Kraft bei 1,08 statt 1,18 auf, mit der Stütze gegen die Welt bei 1,10 statt 1,22. Über vier Lagen des Treffpunkts bewegt
sich der Standfuß nach den mittleren Treffern mit eigener Kraft im ersten Schritt zur Seite weg bei 0,9 statt 16,5 % der
Schritte mehr als 5 cm, ab dem zweiten Schritt bei 514 statt 761 Schritten, im zweiten bei 24,8 statt 32,6 %, mit der
Stütze gegen die Welt im ersten Schritt zur Seite weg bei 6,4 statt 26,1 %, und die Figur braucht 2,08 statt 2,36 und
1,11 statt 1,18 Schritte. Ohne Stütze, wo der Standfuß im ersten Schritt auch vorher kaum über den Boden glitt, bei 13,9
statt 12,8 %, ab dem zweiten Schritt in weniger Schritten, 1 770 statt 1 892, aber in 34,0 statt 28,1 % von ihnen. Nach
den harten Treffern mit eigener Kraft ab dem zweiten Schritt bei 55,2 statt 53,9 %. Mit eigener Kraft fallen von den
leichten Treffern weiter keiner, von den mittleren 1 statt 20, von den harten 2 369 statt 2 687 und an den Gliedern 673
statt 777, von allen 4 × 5 264 Treffern 3 043 statt 3 484. Mit der Stütze gegen die Welt fallen 0, 0, 727 und 324 statt
0, 0, 875 und 407, 1 051 statt 1 282, ohne Stütze 0, 12, 3 825 und 1 117 statt 0, 165, 3 837 und 1 163, 4 954 statt
5 165. Vorher festgelegt war: keine leichten Stürze, mit jeder Stütze höchstens 1 % mehr als Runde 61, mit eigener Kraft
höchstens 20 mittlere, und dann die Form, bei der sich der Standfuß im ersten Schritt zur Seite weg am seltensten mehr
als 5 cm bewegt, ohne die Gewinne ab dem zweiten Schritt zu verlieren: 5 cm ab 0,6 mit 0,9 %, 8 cm ab 0,7 mit 1,6 % und
5 cm ab 0,7 mit 6,0 % auf den neun Tabellen. Ohne Stütze kippt das Becken mit dem Körper und bringt die Hüfte des
Schwungbeins von selbst nach unten, dort unterscheidet der erste Schritt die Formen kaum, und es entschied, was am
wenigsten fallen lässt: 5 cm ab 0,6 ließ von den harten Treffern auf den neun Tabellen 1 064 statt 964 fallen, 3 cm ab
0,9 948. Gemessen und verworfen: ein Becken, das spät im Schritt ungebremst sinkt, in vier Formen, und ein Standfuß, dem
sein Ziel wieder folgt, wenn das Bein es nicht mehr erreicht oder spät im Schritt. Ungebremst rutschte der Standfuß
seltener, aber mit beiden Stützen fielen mehr harte Treffer, mit der Stütze gegen die Welt auf den neun Tabellen 228 bis
257 statt 221. Das Ziel des Standbeins änderte am ersten Schritt zur Seite wenig, denn der Fuß wird dort in der Luft
mitgetragen. Siehe „Schwungfuß greift unter den Boden“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Die
Tests ändern nur ihre Hashes, und ein Stoß von 150 N auf die Stirn braucht 1 statt 3 Schritte, das Becken wandert 0,46
statt 1,07 m. Gepaart gegen `active-ragdoll-61`, vier Läufe im Wechsel mit vier Threads und einer mit einem, rechnen
alle Szenen ohne Figuren, die einzelne Figur hinter ihrer Mauer, die 50 Kämpfer und die 20 Ragdolls im Schutt dasselbe
wie vorher, nur der höchste gezählte Speicher der gestreamten Stadt liegt bei 188 statt 189 MB. Die Szenen mit
getroffenen Figuren enden anders, mit einem und mit vier Threads gleich: Hinter ihren Mauern stehen am Ende wie vorher
alle 10 Figuren, die, als die Mauern brechen, wie vorher 5 Schritte machen, sich einmal statt keinmal fangen und 1 statt
4 mal umgeworfen werden, und von den 50 stehen am Ende 49 statt 50, einer steht gerade auf, als die Mauern brechen
fangen sich 4 statt 5. In der Stadt mit Figuren stürzen wie vorher 6 Stockwerke ein, die Balance macht wie vorher 31
Schritte, fängt 9 statt 6, 24 statt 25 werden umgeworfen, und am Ende stehen 29 statt 28. Ein Bild der Stadt mit Figuren
kostet mit vier Threads im Mittel das 1,04-fache, in den vier Paaren das 1,13-, 0,98-, 1,06- und 1,00-fache, mit einem
Thread das 1,11-fache: Die Stadt endet anders, mit 38 899 statt 38 369 Trümmerteilen und 16 744 statt 15 869 wachen
Kontakten, und Box3D rechnet entsprechend mehr, das Update der Bibliothek kostet mit vier Threads gleich viel.
Vorgeschlagen ist als Runde 63 weiter die eigene Kraft: das Hochfedern nach dem Aufsetzen, oder ohne Stütze die späteren
Schritte, in denen der Standfuß jetzt öfter rutscht, oder die Balance bei 30 Hz, F81.

Das Hochfedern nach dem Aufsetzen, Runde 63, auf Wunsch des Nutzers, nichts gebaut. Auf den neun Tabellen auf Brust und
Bauch, wie der Treffpunkt liegt, folgt nach den mittleren Treffern, die die Figur mit eigener Kraft fängt, auf 369 erste
Schritte ein zweiter. In 78 % davon trägt der Standfuß des ersten ab 0,88 der Schrittdauer nichts mehr, der Körper
fällt, bis der Schwungfuß aufsetzt, und dann ist der Schwerpunkt im Mittel 10 cm unter seiner Standhöhe. Das Bein, das
landet, fängt ihn mit bis zum 3,5-Fachen des Gewichts und richtet ihn wieder auf, in den ersten 0,4 des zweiten Schritts
im Mittel mit bis zu 0,73 m/s, in 98 % der Schritte schneller als 0,3 m/s, mit der Stütze gegen die Welt mit 0,44 m/s,
ohne Stütze mit 0,58 m/s. In einem verfolgten Treffer, 85 N s von vorn auf die Brust, fällt das Becken beim Aufsetzen
mit 1,0 m/s, 12 cm unter Standhöhe, die Dämpfung `NB_HEIGHT_DAMPING` verlangt es 11 cm über Standhöhe, das Knie stemmt
an seiner Grenze, und danach drückt das Bein weiter, bis das Becken mit 0,74 m/s steigt. Bei 0,6 der Schrittdauer trägt
der Standfuß nur noch 0,57 des Gewichts, ab 0,78 nichts, und er rutscht. Gemessen in sieben Formen: das Steigen stärker
gedämpft, immer, nur über einer Geschwindigkeit oder nur im Stand, ein Abfangen, das das Becken höchstens wenig über der
Standhöhe verlangt, eine kleinere Absenkung spät im Schritt, ein Schwungfuß, der früher und tiefer greift, und ein
Schritt, der sich beeilt, sobald der Standfuß nicht mehr trägt. Was das Steigen bremst, nimmt das Hochfedern nur zum
Teil, mit 0,4 s statt 0,1 s Dämpfung auf 0,45 m/s, und lässt mit eigener Kraft mehr Treffer fallen: Über alle Tabellen
und vier Lagen fallen mit 0,15 s 3 232 statt 3 043, gedämpft nur über 0,4 m/s 3 240, ohne Stütze dabei zwei leichte
Treffer. Nur im Stand gebremst fallen mit der Stütze gegen die Welt von den mittleren Treffern 42 bis 49 statt keiner.
Was am freien Fall vor dem Aufsetzen ansetzt, ändert das Hochfedern kaum, ohne Absenkung steigt der Schwerpunkt mit 0,68
statt 0,73 m/s, und der Standfuß rutscht fast doppelt so oft. Die Figur braucht den schnellen Wiederaufstieg, um sich
mit eigener Kraft zu fangen, wie schon Runde 56 und 61 zeigten. Keine Form erfüllt die vorher festgelegte Regel, mit
jeder Stütze höchstens 1 % mehr Stürze als Runde 62 und keine leichten. Siehe „Hochfedern nach dem Aufsetzen bremsen“
und die Zeilen danach in [Optimierungen](Optimierungen.md). Die Bibliothek bleibt wie in Runde 62. Vorgeschlagen ist als
Runde 64 ohne Stütze die späteren Schritte, in denen der Standfuß seit Runde 62 öfter rutscht, oder die Balance bei
30 Hz, F81. Gegen das Hochfedern bliebe ein größerer Umbau auf eigener Branch, zum Beispiel eine kurze Doppelstütze nach
dem Aufsetzen, in der beide Beine abfangen.

Die späteren Schritte ohne Stütze, Runde 64, auf Wunsch des Nutzers, nichts gebaut. Ohne Stütze bewegt sich der Standfuß
seit Runde 62 nach den mittleren Treffern, die die Figur fängt, auf den neun Tabellen auf Brust und Bauch über vier
Lagen des Treffpunkts ab dem zweiten Schritt bei 34,0 statt 28,1 % der Schritte mehr als 5 cm. Pro Treffer ist das
seltener geworden: Nach den 7 504 Treffern, die Runde 61 und 62 beide fangen, bei 1 660 statt 1 885 Schritten, 0,221
statt 0,251 pro Treffer, denn die Figur braucht ab dem zweiten Schritt nur noch 4 959 statt 6 717 Schritte. Weggefallen
sind vor allem Schritte, in denen der Standfuß selten rutscht, zweite Schritte zur Seite weg vom Standfuß, 265 statt
599, und Schritte ab dem vierten zur Seite weg, 69 statt 427. Schlechter geworden sind die dritten Schritte rückwärts,
49,2 statt 28,3 %, und die zweiten zum Standfuß hin, 33,3 statt 25,8 %. In den dritten Schritten rückwärts dreht sich
der Standfuß dabei meist um mehr als 0,3 rad um die Hochachse, in 304 der 378 Schritte, in denen er rutscht: Er steht
auf dem Ballen und dreht sich mit dem Becken. In einem verfolgten Treffer, 80 N s schräg von vorn auf die Brust, dreht
sich das Becken im dritten Schritt um 38 Grad, der Fuß mit bis zu 10 rad/s mit, und er bewegt sich 8,7 cm, in Runde 61
1,5 cm. Dazu rutscht der Fuß aus, der seit Runde 62 unter den Boden greift: Er trifft früher auf den Boden, oft noch
schnell an seinem Ziel vorbei, nach 64 N s von vorn auf den Bauch am Ende des zweiten Schritts bei 1,22 der Schrittdauer
mit 2,5 m/s, in Runde 61 erst bei 1,39 mit 1,2 m/s, und rutscht als Standfuß des dritten Schritts noch 5,7 cm, in Runde
61 1,0 cm. Gemessen, alle nur ohne Stütze, mit den anderen Stützen bitgleich: später landen, nur bis zum Ziel oder nur
langsam unter den Boden greifen, weniger Gegendrehung des Oberkörpers bei Schritten zurück, weniger oder mehr
Gegendrehung überhaupt und ein Standfuß, den das Bein in der Richtung hält, in die er beim Beginn des Schritts zeigte.
Vorher festgelegt waren keine leichten Treffer, ohne Stütze höchstens 1 % mehr Stürze als Runde 63 und dann mindestens
10 % weniger solcher Schritte pro Treffer, den beide fangen. Keine Form schafft das: Später landen, erst unter 1,5 statt
2 m/s, nimmt 7 % und lässt 27 statt 12 mittlere Treffer fallen, 0,7 statt 0,85 der Gegendrehung bei Schritten gerade
zurück nimmt 1 %, bei den dritten Schritten rückwärts 44,5 statt 49,2 %, mit 4 936 statt 4 954 von allen 4 × 5 264
gefallen. Den Standfuß in seiner Richtung zu halten, lässt 231 mittlere fallen. Siehe „Spätere Schritte ohne Stütze“ und
die Zeilen danach in [Optimierungen](Optimierungen.md). Die Bibliothek bleibt wie in Runde 62. Nebenbei: Mit dem 3,5-
statt dem Dreifachen der Gegendrehung fallen ohne Stütze 4 832 statt 4 954 aller Treffer, aber 25 statt 12 mittlere.
Vorgeschlagen ist als Runde 65 die Balance bei 30 Hz, F81, oder ohne Stütze mehr Gegendrehung, die harte Treffer fängt,
ohne mittlere zu verlieren. Gegen das Hochfedern bliebe weiter ein größerer Umbau auf eigener Branch, eine kurze
Doppelstütze nach dem Aufsetzen.

Die Balance bei jedem Takt, Runde 65, F81, wie nach Runde 64 vorgeschlagen. Gemessen auf Runde 64 mit `stepRate` 20, 30,
40, 60 und 120 und allen drei Stützen, auf den neun Tabellen auf Brust und Bauch, bei 30 und 60 Hz über vier Lagen des
Treffpunkts, 17 856 Treffer je Stütze: Bei 30 Hz fallen mit eigener Kraft 3 935, mit der Stütze gegen die Welt 1 472 und
ohne Stütze 5 861, bei 60 Hz 2 370, 727 und 3 837. 174, 512 und 268 der Stürze bei 30 Hz kommen, weil die Figur acht
Sekunden lang keine Ruhe findet, bei 60 Hz 1, 2 und 2. Zwei Regeln arbeiten im Stand gegen eine Geschwindigkeit, die sie
am Anfang des Updates lesen, und ihr Ziel hält das ganze Update: die Beine, die die Höhe des Beckens mit 0,1 s dämpfen,
`NB_HEIGHT_DAMPING`, und das Becken, das sich gegen die Bewegung des Schwerpunkts schiebt. Bei 30 Hz wirken sie pro
Update doppelt so stark und schießen über. In einem verfolgten Treffer mit der Stütze gegen die Welt, 52 N s, stieg und
sank der Schwerpunkt der stehenden Figur von Update zu Update abwechselnd mit 0,05 bis 0,2 m/s, und die mittlere
Geschwindigkeit der Knochen sprang um die Grenze der Ruhe von 0,15 m/s, bis die Figur nach acht Sekunden aufgab und
fiel, bei 60 Hz stand sie nach 1,45 s wieder. Gebaut in der Bibliothek: Unter 60 Hz dämpfen die Beine die Höhe im Stand
um so viel schwächer, wie ein Update länger dauert, `NB_TUNED_STEP`, und mit der Stütze gegen die Welt schiebt sich das
Becken im Stand um so viel schwächer gegen die Bewegung, so wirken beide pro Update so viel wie bei 60 Hz. Ohne Stütze
kippt das Becken mit dem Körper, und das Schieben ist alles, was sein Schwanken im Stand dämpft: Dort schiebt es sich um
so viel stärker, schwächer schwankte die Figur weiter und trat immer wieder neu. Bei 60 Hz und schneller rechnet alles
bitgleich wie vorher. Bei 30 Hz fallen über vier Lagen 3 781, 908 und 5 672, von den mittleren 72, 11 und 561 statt 165,
197 und 676, ohne Ruhe 18, keiner und 45. Am mittleren Treffpunkt fallen bei 40 Hz 645, 289 und 1 132 statt 1 071, 1 158
und 1 284, bei 20 Hz 1 796, 427 und 2 676 statt 3 127, 3 074 und 3 594. Von den 168 leichten Treffern fallen bei 30 Hz
keiner, keiner und 4 statt 2, 3 und 9, bei 40 Hz keiner, keiner und 4 statt 23, 24 und 10. Vorher festgelegt waren bei
60 und 120 Hz höchstens 1 % mehr Stürze je Stütze und keine leichten, dann bei 30 Hz über vier Lagen die wenigsten
mittleren Stürze über die drei Stützen, dann die wenigsten überhaupt, und keine leichten. Ohne Stütze fallen weiter 4
leichte, dieselben wie vorher, nach drei bis fünf Schritten zur selben Zeit: Diese Runde ändert das Treten nicht. Nach
dem ersten Maß der Regel allein, den mittleren Stürzen bei 30 Hz, läge eine feste Dämpfung von 0,07 s unter 60 Hz mit
635 statt 644 vorn. Sie lässt aber bei 30 Hz ohne Stütze 6 statt 4 leichte Treffer fallen, mit der Stütze gegen die Welt
925 statt 908 aller Treffer, und mit ihr bei 40 Hz 437 statt 289 und bei 20 Hz 799 statt 427 am mittleren Treffpunkt.
Gebaut ist deshalb die Dämpfung nach dem Verhältnis der Takte. Gemessen und verworfen: die Geschwindigkeit über zwei
Updates gemittelt, die Höhe auch im Schritt schwächer oder im Stand anders gedämpft, das Becken bei allen Stützen gleich
gegen die Bewegung und Beine, die von dort greifen, wo das Becken ein halbes Update später ist. Siehe „Ruhig stehen bei
langsamem Takt“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Was bleibt, sind die harten Treffer: Am
mittleren Treffpunkt fallen mit eigener Kraft 368 nur bei 30 Hz und 13 nur bei 60 Hz, fast alle mit 80 N s und mehr,
nach im Mittel 3,9 Schritten, wo die Figur sie bei 60 Hz nach 5,3 Schritten fängt. Den ersten Takt nach dem Treffer als
Probe mit 1/60 s zu rechnen, mit der schwächeren Dämpfung im Stand, lässt mit eigener Kraft und ohne Stütze 3 % weniger
fallen, mit der Stütze gegen die Welt 37 % mehr: Früher zu antworten hilft wenig. Woran die Schritte bei 30 Hz
scheitern, ist noch nicht verfolgt. Vorgeschlagen ist als Runde 66, die harten Treffer bei 30 Hz Schritt für Schritt
gegen 60 Hz zu verfolgen, oder ohne Stütze mehr Gegendrehung, die harte Treffer fängt, ohne mittlere zu verlieren, aus
Runde 64.

Die harten Treffer bei 30 Hz, Runde 66, F81, nach dem Vorschlag von Runde 65. Eine Sonde zählt in den ersten drei
Schritten nach dem Treffer, wann der Schwungfuß abhebt, wann der Schritt endet und wie weit der Fuß dann von seinem Ziel
steht, bei 30 und 60 Hz für dieselben Treffer. Bei 30 Hz hebt der Fuß im Mittel etwa 30 ms später in der Schrittdauer
ab, und jeder Schritt endet 20 bis 35 ms später, mit allen drei Stützen: Die Ziele eines Schritts gelten ein ganzes
Update, im ersten Update steht der Fuß noch auf dem Boden, und bei 30 Hz hebt er so erst nach 33 statt 17 ms. Nach den
Treffern am mittleren Treffpunkt, die die Figur mit eigener Kraft nur bei 30 Hz nicht fängt, 368 von 4 464, landet der
Fuß im dritten Schritt im Median 28 statt 9 cm neben seinem Ziel, und nur 68 % dieser Schritte landen, bei 60 Hz alle.
Mit eigener Kraft und ohne Stütze beginnt der erste Schritt bei 30 Hz 16 ms später, weil das erste Update erst nach 33
ms kommt, mit der Stütze gegen die Welt bei beiden Takten nach 33 ms, und er wählt fast immer denselben Fuß. Gebaut in
der Bibliothek: Solange eine Stütze das Becken aufrecht hält, nimmt die Balance unter 60 Hz die Ziele eines Schritts um
das Doppelte der Zeit voraus, die ein Update länger als 1/60 s dauert, bei 30 Hz um ein ganzes Update, `NB_STEP_LEAD`.
Bei 60 Hz und schneller rechnet sie bitgleich wie vorher. Über vier Lagen fallen bei 30 Hz mit eigener Kraft 2 993 statt
3 781, mit der Stütze gegen die Welt 699 statt 908 und ohne Stütze wie vorher 5 672, von den mittleren 28, keiner und
561 statt 72, 11 und 561. Am mittleren Treffpunkt fallen bei 40 Hz 547 und 243 statt 645 und 289, bei 20 Hz 1 310 und
217 statt 1 796 und 427. Von den 168 leichten Treffern fallen bei 20 Hz 4, keiner und 57 statt 15, 1 und 57, bei 30 und
40 Hz wie vorher keiner, keiner und 4. Vorher festgelegt war: bei 60 und 120 Hz bitgleich, bei 30 Hz über vier Lagen in
Summe mindestens 2 % weniger Stürze, keine Stütze mehr als 1 % mehr, höchstens 5 mittlere mehr und keine leichten mehr,
bei 20 und 40 Hz keine Stütze mehr als 2 % mehr und keine leichten mehr, dann die wenigsten Stürze bei 30 Hz. Das Ein-
und Anderthalbfache des Vorlaufs ließ 2 990 und 2 909 mit eigener Kraft und 819 mit der Stütze gegen die Welt fallen,
das Anderthalbfache bei 20 Hz mit der Stütze gegen die Welt 5 statt einen leichten Treffer. Ohne Stütze ließ jeder
Vorlauf bei 30 Hz 7 bis 14 statt 4 leichte Treffer fallen, vor allem von der Seite, dort bleiben die Ziele. Auch Landen
und Planen nach dem vorausgenommenen Schritt ließ mehr fallen. Siehe „Schrittziele bei langsamem Takt vorausnehmen“ und
die Zeilen danach in [Optimierungen](Optimierungen.md). Auf Wunsch des Nutzers in derselben Runde die neuen Ideen der
Branch `ragdoll-sol` auf Runde 65 gemessen, am mittleren Treffpunkt: Seitliche Schritte früher absenken lässt bei 60 Hz
mit eigener Kraft 786 statt 595 fallen, kurz auf beiden Füßen ausregeln ändert fast nichts, das belastete Fußziel nur
bis 0,55 der Schrittdauer zu halten lässt bei 60 Hz 624 statt 595 fallen, und den Boden am Ende des Schritts zu
erreichen gibt es seit Runde 58 und 62. Nur den Rumpf nach harten Treffern früher wieder tragen zu lassen hilft etwas,
bei 60 Hz ohne Stütze 940 statt 952, bei 30 Hz mit eigener Kraft 923 statt 950. Es ändert 60 Hz und braucht die ganze
Messung, vorgeschlagen als Runde 67. Danach bleibt bei 30 Hz ohne Stütze der Abstand zu 60 Hz, und bei 20 Hz fallen ohne
Stütze weiter 57 leichte Treffer.

Der Rumpf nach harten Treffern, Runde 67, F82, nach dem Vorschlag von Runde 66, eine Idee der Branch `ragdoll-sol`
(dbcdbfa). Seit Runde 60 behält die Wirbelsäule nach einem Treffer ab der Hälfte von `knockdownImpulse` 0,15 s lang nur
0,3 ihrer Steifigkeit, nach jedem solchen Treffer gleich lang. Sol lässt sie nach harten Treffern kürzer weich, damit
der Rumpf die Figur früher durch ihre Schritte trägt. Gebaut in der Bibliothek: Ohne die Stütze gegen die Welt, mit
eigener Kraft und ohne Stütze, werden die 0,15 s vom 1,5-Fachen von `knockdownImpulse` an kürzer, ab dem Doppelten um
die Hälfte, `NB_SOFT_HARD_CUT`. Mit der Stütze gegen die Welt rechnet die Figur bitgleich wie vorher, und an den
Gliedern, wo kein Gelenk nachgibt, ändert sich nichts. Von allen 4 × 5 264 Treffern fallen bei 60 Hz mit eigener Kraft
3 032 statt 3 043 und ohne Stütze 4 868 statt 4 954, von den mittleren 3 und 15 statt 1 und 12, von den leichten mit
allen drei Stützen keiner, über die drei Stützen zusammen 8 951 statt 9 048. Bei 30 Hz fallen über vier Lagen 2 958
statt 2 993 und 5 585 statt 5 672, von den leichten wie vorher keiner, keiner und 4. Weil sie keine Ruhe finden, fallen
ohne Stütze aber 56 statt 45. Vorher festgelegt war: bei 60 Hz keine Stütze mehr als 1 % mehr, je Stütze höchstens 5
mittlere mehr und keine leichten, über die drei Stützen zusammen mindestens 1 % weniger, bei 30 Hz keine Stütze mehr als
1 % mehr und keine leichten mehr, dann die wenigsten Stürze bei 60 Hz. So wie bei Sol, um drei Viertel, fallen 8 976,
0,8 % weniger, mit allen drei Stützen 8 986, weil dann mit der Stütze gegen die Welt 1 061 statt 1 051 fallen. Ganz ohne
die 0,15 s ab dem Doppelten oder schon vom Einfachen an kürzer fallen ohne Stütze von den mittleren 27 und 33 statt 12.
Der Gewinn ist klein und folgt der Kürzung nicht gleichmäßig: um die Hälfte 8 951, um drei Viertel 8 976, ganz 8 990.
Siehe „Rumpf nach harten Treffern kürzer weich“ und die Zeilen danach in [Optimierungen](Optimierungen.md).
Vorgeschlagen ist als Runde 68, ohne Stütze bei 30 Hz die Ziele nur bei Schritten vorauszunehmen, die nicht zur Seite
gehen, denn die leichten Treffer, die der Vorlauf in Runde 66 fallen ließ, kamen vor allem von der Seite, oder ohne
Stütze mehr Gegendrehung aus Runde 64.

Die Schrittziele ohne Stütze, Runde 68, F81, nach dem Vorschlag von Runde 67, nichts gebaut. Seit Runde 66 nimmt die
Balance unter 60 Hz die Ziele eines Schritts nur voraus, solange eine Stütze das Becken aufrecht hält, denn ohne Stütze
ließ jeder Vorlauf bei 30 Hz mehr leichte Treffer fallen, nach Runde 66 vor allem von der Seite. Gemessen ohne Stütze,
mit den anderen Stützen und bei 60 Hz und schneller bitgleich: der Vorlauf nach dem Anteil des Wegs des Schwungfußes
nach vorn oder hinten, dem Quadrat des Kosinus zur Blickrichtung, nur ab der Hälfte davon, nach seinem Quadrat und halb
so weit. Über vier Lagen fallen bei 30 Hz ohne Stütze 5 567, 5 834, 5 783 und 5 569 statt 5 585, von den 168 leichten
Treffern 12, 9, 8 und 8 statt 4, die zusätzlichen vor allem von vorn und schräg von vorn, meist erst nach 6 bis 16
Schritten, manche, weil die Figur acht Sekunden keine Ruhe findet. Vorher festgelegt waren bei 30 Hz mindestens 2 %
weniger Stürze, höchstens 5 mittlere und keine leichten mehr, bei 20 und 40 Hz höchstens 2 % mehr und keine leichten
mehr. Keine Form schafft das. Nachträglich, nicht nach der Regel, gemessen: Für alle Schritte fallen 4 757 und 10
leichte, nur nach dem Anteil zur Seite 4 552 und 11, die leichten dann alle von der Seite, 68 bis 80 N s, meist nach
etwa einer Sekunde und drei bis vier Schritten. Ohne Vorlauf fallen von dort schon 70, 74, 78 und 80 N s. Der Gewinn des
Vorlaufs ohne Stütze kommt also von den Schritten zur Seite, und von dort kommen auch die leichten Treffer, die er
fallen lässt. Bei 20 Hz fallen mit den meisten Formen weniger leichte Treffer, 26 bis 42 statt 56, nur ab der Hälfte 55.
Siehe „Schrittziele ohne Stütze nur nach vorn und hinten vorausnehmen“ und die Zeile danach in
[Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 69, die leichten Treffer von der Seite, die mit dem
Vorlauf ohne Stütze bei 30 Hz fallen, Schritt für Schritt gegen dieselben ohne Vorlauf zu verfolgen. Fängt die Figur
sie, nimmt der Vorlauf zur Seite ohne Stütze bei 30 Hz fast ein Fünftel der Stürze.

Der Vorlauf ohne Stütze, Runde 69, F81, nach dem Vorschlag von Runde 68, nichts gebaut. Verfolgt sind die leichten
Treffer von der Seite, die mit dem Vorlauf für alle Schritte ohne Stütze bei 30 Hz fallen und ohne ihn nicht, 68, 74 und
80 N s von der einen Seite, 72 und 76 N s von der anderen. In allen fünf landet der Nachziehschritt, der zweite, in dem
der ferne Fuß wieder neben den Standfuß kommt, nur 3 bis 5 cm neben dem Standfuß statt der geplanten 16 cm und 6 bis
16 cm davor: Der Fuß schießt über sein Ziel hinaus. Ohne Vorlauf landet er 3 bis 10 cm daneben, bei 60 Hz 5 bis 14 cm.
Im nächsten Schritt hebt der Schwungfuß dann kaum ab und bleibt stehen, der Standfuß verliert den Boden, das Becken
rollt über den Schwungfuß, und die Figur kippt. Gemessen ohne Stütze, mit den anderen Stützen und bei 60 Hz und
schneller bitgleich, für alle Schritte und nur nach dem Anteil zur Seite: ohne den Vorlauf im ersten Update eines
Schritts, mit einem Vorlauf, der bis zum geplanten Ende des Schritts ausklingt, und beides. Ohne den Vorlauf im ersten
Update bleibt der Gewinn, über vier Lagen bei 30 Hz 4 744 und 4 877 statt 5 585 Stürze, aber 8 statt 4 leichte Treffer
fallen. Klingt er aus, fallen nur 3 bis 6 leichte, aber 5 317 bis 5 442 Stürze, und bei 20 Hz mehr leichte. Den Gewinn
bringt also das frühere Aufsetzen am Ende des Schritts, und dort fallen auch die leichten Treffer. Vorher festgelegt
waren wie in Runde 68 bei 30 Hz mindestens 2 % weniger Stürze, höchstens 5 mittlere und keine leichten mehr, bei 20 und
40 Hz höchstens 2 % mehr und keine leichten mehr. Am nächsten kommt der Vorlauf nur zur Seite, der ausklingt: bei 30 Hz
5 420 Stürze, 566 mittlere und 3 leichte, bei 20 Hz aber 2 734 statt höchstens 2 724 und 58 statt 56 leichte. Siehe
„Nachziehschritt mit Vorlauf ohne Stütze“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist
als Runde 70, den Nachziehschritt selbst zu richten, so dass er den Abstand zum Standfuß hält, bei jedem Takt, denn er
landet auch bei 60 Hz zu nah. Das ändert 60 Hz und braucht die ganze Messung über alle Tabellen.

Der Nachziehschritt, Runde 70, F81, nach dem Vorschlag von Runde 69, nichts gebaut. Bei 60 Hz ohne Stütze, nach 72 N s
von der Seite, legt der ferne Fuß im Nachziehschritt 93 cm in 0,3 s zurück, hinkt seinem Ziel bis 25 cm nach, erreicht
5,6 m/s und schießt am Ende 12,6 cm über das Ziel hinaus, 5 cm neben den Standfuß statt 18 cm. Das Ziel hält den
Abstand, der Fuß nicht. Die Füße einer Figur stoßen aneinander, so landet er an oder auf dem Standfuß. Eine Sonde über
die neun Tabellen auf Brust und Bauch am mittleren Treffpunkt zählt die Treffer, nach denen ein Fuß einmal auf dem
anderen steht: bei 60 Hz mit eigener Kraft 11, mit der Stütze gegen die Welt keiner und ohne Stütze 72, von denen 0,
keiner und 23 fallen, bei 589, 184 und 941 Stürzen, bei 30 Hz 7, keiner und 128, von denen 3, keiner und 79 fallen, bei
740, 176 und 1 394. Gemessen, bei jedem Takt und mit jeder Stütze: Kommt der Schwungfuß dem Standfuß innerhalb einer
Fußlänge seitlich näher als `footGap`, schiebt sein Ziel ihn um so viel hinaus, nach dem Abstand jetzt oder nach dem
Abstand ein Update später, mit seiner Geschwindigkeit, allein und mit dem Vorlauf ohne Stütze von Runde 68. Mit dem
Abstand ein Update später und dem Vorlauf fallen ohne Stütze bei 30 Hz über vier Lagen 4 621 statt 5 585, von den
mittleren 270 statt 561, von den leichten Treffern einer statt 4, bei 20 Hz 18 statt 56: Ohne Fuß auf Fuß fängt die
Figur die leichten Treffer von der Seite auch mit Vorlauf. Bei 60 Hz fallen über alle 4 × 5 264 Treffer mit eigener
Kraft 3 040 statt 3 032, mit der Stütze gegen die Welt 1 053 statt 1 051 und ohne Stütze 4 839 statt 4 868. Mit der
Stütze gegen die Welt fallen bei 30 Hz aber 710 statt 699, 1,6 % mehr, und bei 40 Hz ohne Stütze 6 statt 4 der leichten
Treffer, wieder von der Seite nach drei bis vier Schritten. Vorher festgelegt waren bei 60 und 30 Hz keine Stütze mehr
als 1 % mehr, höchstens 5 mittlere und keine leichten mehr, bei 20 und 40 Hz ohne Stütze höchstens 2 % mehr und keine
leichten mehr, über 60 und 30 Hz zusammen mindestens 1 % weniger. Keine Form schafft das, die anderen scheitern früher,
mit dem Abstand jetzt an 26 statt 15 mittleren Treffern ohne Stütze bei 60 Hz. Siehe „Fuß auf Fuß im Nachziehschritt“
und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 71, den Schwungfuß nur ohne
Stütze vom Standfuß fernzuhalten, so dass die anderen Stützen bei jedem Takt bitgleich bleiben, mit dem Vorlauf ohne
Stütze, und vorher die leichten Treffer von der Seite zu verfolgen, die damit bei 40 Hz noch fallen.

Der Schwungfuß ohne Stütze, Runde 71, F81, nach dem Vorschlag von Runde 70. Verfolgt sind bei 40 Hz die leichten Treffer
von der Seite, die mit dem Fernhalten nach dem Abstand ein Update später und dem Vorlauf ohne Stütze fallen, 74, 76 und
78 N s von der einen Seite und 78 N s von der anderen. Im Nachziehschritt läuft der ferne Fuß mit bis zu 5,4 m/s auf den
Standfuß zu, und sein Ziel schiebt ihn erst hinaus, wenn er ein Update, 25 ms, vor dem Abstand ist. So schnell hält das
Bein ihn nicht auf: Er landet knapp 8 cm neben dem Standfuß statt 16, und der nächste Schritt scheitert wie in Runde 69.
Bei 30 Hz dauert ein Update 33 ms, das reichte. Wann der Fuß bremsen muss, hängt an seiner Geschwindigkeit, nicht am
Takt. Gebaut in der Bibliothek, nur ohne Stütze, so dass eigene Kraft und Stütze gegen die Welt bei jedem Takt bitgleich
bleiben: Läuft der Schwungfuß innerhalb einer Fußlänge vor oder hinter dem Standfuß auf ihn zu und käme er ihm seitlich
in 1/15 s, oder in einem Update, wenn das länger dauert, näher als `footGap`, schiebt sein Ziel ihn um so viel hinaus,
`NB_KEEP_TIME`. Dazu nimmt die Balance unter 60 Hz die Ziele eines Schritts auch ohne Stütze voraus, `NB_STEP_LEAD`.
Ohne Stütze fallen bei 30 Hz über vier Lagen 4 551 statt 5 585, von den mittleren 256 statt 561, am mittleren Treffpunkt
bei 20 Hz 1 803 statt 2 671 und bei 40 Hz 951 statt 1 135, von den 168 leichten Treffern bei 20, 30 und 40 Hz 21, 3 und
keiner statt 56, 4 und 4, bei 60 Hz von allen 4 × 5 264 Treffern 4 864 statt 4 868, von den mittleren 7 statt 15, von
den leichten keiner. Mit eigener Kraft und der Stütze gegen die Welt rechnet die Figur bitgleich wie vorher, geprüft bei
60 Hz über alle Tabellen und bei 30 Hz über vier Lagen. Die leichten Treffer, die bei 30 Hz noch fallen, kommen nicht
mehr von der Seite, sondern schräg von vorn, 52, 62 und 66 N s, und weil sie keine Ruhe finden, fallen über vier Lagen
68 statt 56. Vorher festgelegt waren ohne Stütze bei 60 Hz höchstens 1 % mehr Stürze, höchstens 5 mittlere mehr und
keine leichten, bei 30 Hz über vier Lagen mindestens 2 % weniger, höchstens 5 mittlere und keine leichten mehr, bei 20
und 40 Hz höchstens 2 % mehr und keine leichten mehr, eigene Kraft und Stütze gegen die Welt bitgleich, dann die
wenigsten Stürze bei 30 Hz. Ein Update voraus fallen bei 40 Hz 6 leichte Treffer, ohne den Vorlauf bei 20 Hz 57 leichte
und bei 30 Hz über vier Lagen 5 411 Treffer. Auch mindestens 1/30 und 1/20 s statt 1/15 s bestehen, mit 4 621 und 4 565
Stürzen bei 30 Hz, 1/20 s nur 14 dahinter und bei 60 Hz, bei 20 Hz und an den leichten Treffern sogar besser. Siehe
„Schwungfuß ohne Stütze fernhalten, mit Vorlauf“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Gepaart
gegen `active-ragdoll-70`, drei Läufe im Wechsel mit vier Threads und zwei mit einem, rechnet der Benchmark alle Szenen
bitgleich wie vorher, und ein Bild der Stadt mit Figuren kostet mit vier Threads das 0,99-, 1,00- und 0,99-fache, mit
einem das 1,01- und 1,01-fache. Auf denselben Tabellen über vier Lagen bleibt bei 30 Hz der Abstand zu 60 Hz mit eigener
Kraft, 2 958 statt 2 359, und ohne Stütze, 4 551 statt 3 751, von den mittleren 256 statt 7, ohne Ruhe 68 statt keinem.
Mit der Stütze gegen die Welt fallen bei 30 Hz 699 statt 727. Vorgeschlagen ist als Runde 72, ohne Stütze bei 30 Hz die
mittleren Treffer, die nur bei 30 Hz fallen, Schritt für Schritt gegen dieselben bei 60 Hz zu verfolgen.

Die mittleren Treffer ohne Stütze bei 30 Hz, Runde 72, F81, nach dem Vorschlag von Runde 71, nichts gebaut. Ohne Stütze
fallen auf den neun Tabellen auf Brust und Bauch über vier Lagen 255 mittlere Treffer nur bei 30 Hz, bei 60 Hz fängt die
Figur sie alle. Bei beiden Takten Schritt für Schritt verfolgt, zerfallen sie in drei Gruppen. In 134 bleibt die Figur
bei 60 Hz nach einer Landung stehen, weil der Capture Point höchstens `NB_STEP_MARGIN`, 3 cm, außerhalb liegt. Bei 30 Hz
liegt er dort weiter draußen, im Median 10 cm, in 40 Fällen nur 3 bis 6 cm, und sie tritt sofort weiter: Der nächste
Schritt nimmt ihr die breite Doppelstütze, die den Körper bei 60 Hz bremste, der Körper fällt weiter, und der Capture
Point läuft davon, in einem verfolgten Treffer in einem Schritt von 5 auf 33 cm. In 91 bleibt die Figur bei beiden
Takten nach derselben Landung stehen und fällt bei 30 Hz später, mit gleichen nächsten Schritten, ohne einen
Unterschied, der sich durchzieht. 30 fallen vorher. Bei 30 Hz landet jeder Schritt in Echtzeit später, der erste um 17
bis 34 ms, im Median 33, denn der erste Schritt beginnt erst mit dem ersten Update nach dem Treffer, und jede Landung
zeigt sich erst im nächsten Update. Bei der zweiten Landung liegt der Capture Point bei 30 Hz 3 bis 5 cm weiter draußen.
Gemessen, nur unter 60 Hz, so dass 60 Hz und schneller mit jeder Stütze bitgleich bleiben: Nach einer Landung bleibt die
Figur stehen, statt sofort weiterzutreten, solange der Capture Point höchstens `NB_STEP_MARGIN` mal 1 + a (60 dt − 1)
außerhalb liegt, mit a = 1 und 2 bei 30 Hz 6 und 9 cm, ohne Stütze oder mit jeder Stütze, und ohne Stütze mit a = 1 auch
im Stand. Ohne Stütze fallen bei 30 Hz über vier Lagen 4 527 und 4 486 statt 4 551, von den mittleren 239 und 223 statt
256, am mittleren Treffpunkt bei 20 Hz 1 709 und 1 657 statt 1 803, bei 40 Hz 956 und 958 statt 951. Mit a = 2 fallen
bei 40 Hz aber 3 leichte Treffer schräg von vorn, mit jeder Stütze und a = 1 bei 20 Hz mit eigener Kraft 5 statt 4, und
mit der Schwelle auch im Stand fallen ohne Stütze bei 30 Hz 4 672 und 7 leichte Treffer. Vorher festgelegt waren bei
30 Hz ohne Stütze mindestens 2 % weniger Stürze, höchstens 5 mittlere und keine leichten mehr, mit den anderen Stützen
höchstens 1 % mehr, bei 20 und 40 Hz mit jeder Stütze höchstens 2 % mehr und keine leichten mehr. Keine Form schafft
das: Stehen statt Weitertreten nimmt bei 30 Hz höchstens 1,4 % der Stürze. Siehe „Mittlere Treffer ohne Stütze nur bei
30 Hz“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 73, den ersten
Fangschritt schon beim Treffer zu planen statt erst im ersten Update danach. Dann beginnt er bei jedem Takt gleich früh,
bei 30 Hz 33 ms und bei 60 Hz 17 ms eher. Das ändert 60 Hz und braucht die ganze Messung über alle Tabellen mit allen
Stützen.

Der erste Fangschritt beim Treffer, Runde 73, F81, nach dem Vorschlag von Runde 72, nichts gebaut. Ein Treffer, der die
Balance beginnt, macht die Knochen dynamisch und gibt ihnen den Impuls sofort, den ersten Fangschritt entscheidet aber
erst die Balance nach dem nächsten Schritt der Physik. An 80 Treffern je Takt und Stütze beginnt er mit eigener Kraft
und ohne Stütze im ersten Update, bei 60, 40, 30 und 20 Hz 17, 25, 33 und 50 ms nach dem Treffer, mit der Stütze gegen
die Welt bei 60 und 40 Hz erst im zweiten, nach 33 und 50 ms. Gemessen, mit jeder Stütze und bei jedem Takt: Gleich nach
dem Treffer misst sich die Figur und beginnt den ersten Schritt, wenn der Capture Point mehr als 3 oder 10 cm außerhalb
liegt, oder der Treffer zählt als ein Update mit dem Capture Point draußen. Bei 60 Hz fallen von allen 4 × 5 264
Treffern mit eigener Kraft 2 662 und 2 643 statt 3 032, mit der Stütze gegen die Welt 482 und 477 statt 1 051, von den
harten auf Brust und Bauch 124 statt 727, ohne Stütze aber 5 005 und 4 958 statt 4 864, von den mittleren 26 und 27
statt 7. An den Gliedern fallen mit jeder Stütze mehr, mit eigener Kraft 786 und 767 statt 673. Bei 30 Hz fallen über
vier Lagen mit eigener Kraft 3 170 statt 2 958 und ohne Stütze 5 029 und 5 028 statt 4 551, bei 20 Hz von den leichten
Treffern mit eigener Kraft 23 und 21 statt 4. Zählt der Treffer als Update, fallen bei 60 Hz mit der Stütze gegen die
Welt 669, sonst ähnlich. Vorher festgelegt waren bei 60 Hz keine Stütze mehr als 1 % mehr, höchstens 5 mittlere und
keine leichten mehr, bei 30 Hz über die drei Stützen zusammen mindestens 2 % weniger und keine mehr als 1 % mehr, bei 20
und 40 Hz höchstens 2 % mehr und keine leichten mehr. Keine Form schafft das. Beim Treffer bleiben die Ziele der Gelenke
im ersten Schritt der Physik die des Stehens, während die Zeit des Schritts schon läuft, bei 30 Hz 33 ms, bei 20 Hz
50 ms, bei 60 Hz nur 17. Vermutlich kommt daher, dass langsame Takte verlieren. Siehe „Erster Fangschritt beim Treffer“
und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 74, den ersten Schritt beim
Treffer samt seiner Haltung zu planen, so dass das Bein schon im ersten Schritt der Physik schwingt, und die Treffer auf
die Glieder und ohne Stütze zu verfolgen, die damit öfter fallen. Mit der Stütze gegen die Welt, dem Standard, fiele bei
60 Hz gut die Hälfte weniger.

Der erste Fangschritt beim Treffer samt seiner Haltung, Runde 74, F81, nach dem Vorschlag von Runde 73, nichts gebaut.
Mit dem Schritt beim Treffer von Runde 73 fallen an den Gliedern vor allem Treffer auf den Unterarm öfter, mit eigener
Kraft 119 statt 38, und auf das Schienbein, 99 statt 68: Der Stoß bewegt erst das Glied und erreicht den Rumpf nur über
dessen Gelenk, der Schritt aber ist sofort für den Schwerpunkt geplant. Gemessen, mit jeder Stütze, bei 60 Hz über alle
Tabellen und bei 20, 30 und 40 Hz: Beginnt der erste Schritt beim Treffer, bekommen die Gelenke gleich die Haltung der
Balance, ihre Ziele um die Länge des letzten Schritts der Welt vorausgenommen wie in einem Update, so dass das Bein
schon im ersten Schritt der Physik schwingt. Das für alle Treffer, nur für Treffer auf den Rumpf, vom Becken bis zur
Brust, oder nur für diese und nur mit einer Stütze, und zum Vergleich der Schritt beim Rumpftreffer ohne die Haltung.
Nur für Rumpftreffer fallen bei 60 Hz von allen 4 × 5 264 Treffern mit eigener Kraft 2 489 statt 3 032, mit der Stütze
gegen die Welt 498 statt 1 051 und ohne Stütze 4 492 statt 4 864, zusammen 7 479 statt 8 947, an den Gliedern genau
dieselben wie vorher. Die Haltung bringt die mittleren Treffer ohne Stütze von 26 wieder auf 7. Bei 40 Hz fallen 490, 94
und 910 statt 549, 243 und 951, bei 30 Hz über vier Lagen 2 727, 339 und 4 381 statt 2 958, 699 und 4 551, mit eigener
Kraft und ohne Stütze aber 59 und 314 mittlere statt 20 und 256 und 3 und 10 leichte statt keinem und 3. Bei 20 Hz
fallen am mittleren Treffpunkt 1 466, 526 und 1 831 statt 1 340, 217 und 1 803, von den leichten 11 und 33 statt 4 und
21. Vorher festgelegt waren bei 60 und 30 Hz keine Stütze mehr als 1 % mehr, höchstens 5 mittlere mehr und keine
leichten mehr, bei 60 Hz über die drei Stützen mindestens 2 % weniger, bei 20 und 40 Hz höchstens 2 % mehr und keine
leichten mehr. Bei 60 Hz schaffen das alle Formen mit Haltung, bei 30 und 20 Hz keine. Die leichten Treffer brauchen bei
20 Hz mit dem Schritt beim Treffer mehr Schritte, mit eigener Kraft über alle 168 zusammen 644 statt 505. Von den neu
fallenden kommen mit eigener Kraft 5 von 10 in 8 s nicht zur Ruhe, nach 12 bis 21 Schritten, ohne Stütze fallen 16 von
21 schon in den ersten 2 s nach 3 bis 6 Schritten. An 47 Treffern mit 50 bis 80 N s auf die Brust landet der erste
Schritt bei 20 Hz im Median 0,30 statt 0,40 s nach dem Treffer, mit dem Capture Point 2,2 statt 0,6 cm draußen, bei
60 Hz 0,32 statt 0,33 s danach und so weit drinnen wie vorher. Siehe „Erster Fangschritt beim Rumpftreffer samt Haltung“
und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 75, die leichten und mittleren
Treffer bei 20 und 30 Hz zu verfolgen, die mit dem Schritt beim Treffer mehr Schritte brauchen, und die Stütze gegen die
Welt bei 20 Hz, und daraus eine Form zu finden, die bei jedem Takt hilft. Mit der Stütze gegen die Welt, dem Standard,
fiele bei 60 Hz gut die Hälfte weniger.

Der Schritt beim Treffer bei langsamen Takten, Runde 75, F81, nach dem Vorschlag von Runde 74, nichts gebaut. Mit dem
Schritt beim Rumpftreffer samt Haltung von Runde 74 bekommt der Schwungfuß sofort die Ziele des Schritts, unter 60 Hz um
`NB_STEP_LEAD` vorausgenommen, bei 20 Hz 22 % des Schwungs und 7,7 cm hoch. Nach einem harten Treffer trägt der Fuß im
ersten Schritt der Physik noch und bleibt am Boden, und der Körper kippt schneller: Von den 328 Treffern, die bei 20 Hz
mit der Stütze gegen die Welt nur damit fallen, neigt sich der Rumpf 50 ms nach dem Treffer 0,15 statt 0,10 rad, der
Schwungfuß setzt bei 242 von 296 mitten im Schwung wieder auf, kommt bis zur Landung 0,40 statt 0,70 m weit und landet
0,50 statt 0,20 m vor seinem Ziel, dann kippt die Figur. Leichte Treffer bei 20 Hz mit eigener Kraft neigen sich nach
50 ms 0,11 statt 0,07 rad, mittlere bei 30 Hz nach 33 ms 0,091 statt 0,065 rad. Gemessen, mit jeder Stütze bei 20, 30,
40 und 60 Hz: Beim Treffer bleibt der Schwungfuß am Anfang des Schritts stehen, ohne Vorlauf oder mit dem halben, oder
er bleibt stehen und der Schritt läuft ab dem ersten Update nach dem Treffer wie vorher, nur beim Treffer entschieden
und gehalten. Ohne Vorlauf und mit dem halben ist bei 60 Hz alles wie in Runde 74, denn dort gibt es keinen Vorlauf.
Ohne Vorlauf fallen bei 20 Hz mit der Stütze gegen die Welt immer noch 421 statt 217, mit dem halben 509. Mit dem
Schritt wie vorher fallen bei 20 Hz mit jeder Stütze weniger, 1 173, 190 und 1 537 statt 1 340, 217 und 1 803, bei 30 Hz
über vier Lagen 2 785, 689 und 4 467 statt 2 958, 699 und 4 551, bei 60 Hz von allen 4 × 5 264 Treffern 2 905, 694 und
4 757 statt 3 032, 1 051 und 4 864. Dafür fallen bei 60 Hz ohne Stütze 20 statt 7 mittlere Treffer, bei 20 Hz von den
leichten mit eigener Kraft 14 statt 4 und mit der Stütze gegen die Welt 3 statt keinem, und bei 30 Hz ohne Stütze von
den mittleren 293 statt 256 und von den leichten 7 statt 3. Vorher festgelegt war dieselbe Regel wie in Runde 74. Keine
Form schafft sie, alle lassen bei 20 und 30 Hz mehr leichte Treffer fallen. Die neu fallenden leichten Treffer geraten
fast alle in einen Spagat: Mit dem Schritt wie vorher stehen die Knöchel bei allen 12 neu fallenden mit eigener Kraft
bei 20 Hz bei einer Landung 0,87 bis 1,37 m seitlich auseinander. Das gibt es auch ohne den Schritt beim Treffer: Von
den 125 leichten Treffern, die bei 20 Hz mit eigener Kraft schreiten, fällt keiner der 79, deren Knöchel bei jeder
Landung höchstens 0,9 m seitlich auseinander stehen, von den 46, deren Knöchel bei einer Landung weiter auseinander
stehen, fallen 4, nach Landungen mit 1,03 bis 1,36 m. Die Balance plant Schritte in der Reichweite der Hüfte,
`NB_MAX_STEP_LENGTH`, aber nicht nach dem Abstand der Füße. Siehe „Schritt beim Treffer ohne Vorlauf“ und die Zeilen
danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 76, den Spagat zu verfolgen und zu verhindern,
zuerst ohne den Schritt beim Treffer, so dass die Figur nach einem Schritt zur Seite nicht mit immer weiter auseinander
stehenden Füßen weitertritt. Danach lässt sich der Schritt beim Treffer erneut messen.

Die beste Körperform für die Balance, Runde 76, F83, auf Wunsch des Nutzers statt des Vorschlags von Runde 75, nichts
gebaut. Gesucht war die Form des Menschen der Bibliothek, mit der er bei derselben Größe und Masse, 1,8 m und 74 kg, die
meisten Treffer fängt, mit Ideen aus anderen Projekten, ohne ihre Zahlen: DeepMimic gibt seinem Menschen flache
Kastenfüße, die vor das Fußgelenk reichen, der Mensch von MuJoCo hat die Hüftgelenke innen am Becken und das Fußgelenk
weiter vorn über der Sohle, Jolt und godot-active-ragdoll bauen ihre Figuren aus Kapseln, mit kleinen und leichten Füßen
oder ganz ohne. Nach dem Capture Point gibt eine größere Sohle mehr Raum, und ein tieferer Schwerpunkt verschiebt ihn
nach demselben Stoß weniger weit. Gemessen, mit jeder Stütze bei 20, 30 und 40 Hz und die drei besten auch bei 60 Hz
über alle Tabellen, alles andere der Bibliothek wie es ist: die Hüften 20 statt 26 cm auseinander wie vor Runde 36, die
Füße 30 statt 26 cm lang und 11 statt 9 cm breit, das Fußgelenk 6,5 statt 4 cm vor der Ferse, 3 kg vom Rumpf in die
Beine, und schmale Hüften mit großen Füßen. Mit großen Füßen fallen bei 60 Hz von allen 4 × 5 264 Treffern mit eigener
Kraft 2 711 statt 3 032, mit der Stütze gegen die Welt 874 statt 1 051 und ohne Stütze 4 459 statt 4 864, zusammen 8 044
statt 8 947, bei 30 Hz über vier Lagen 2 543, 437 und 4 089 statt 2 958, 699 und 4 551, zusammen 7 069 statt 8 208, bei
20 Hz 1 181, 219 und 1 705 statt 1 340, 217 und 1 803 und bei 40 Hz 490, 179 und 872 statt 549, 243 und 951. Dafür
fallen ohne Stütze bei 60 Hz 24 statt 7 mittlere Treffer, und von den leichten bei 20 Hz mit der Stütze gegen die Welt 2
statt keinem und bei 40 Hz ohne Stütze einer. Schmale Hüften mit großen Füßen lassen bei 30 Hz nur 6 913 fallen und bei
60 Hz 8 086, aber mit eigener Kraft bei 60 Hz 15 statt 3 und bei 30 Hz 46 statt 20 mittlere und einen leichten, ohne
Stütze bei 60 Hz 32 statt 7 mittlere. Schmale Hüften allein lassen bei 30 Hz 7 892 fallen, bei 60 Hz 8 787, ohne Stütze
56 statt 7 mittlere. Das Fußgelenk weiter vorn hilft nicht, bei 30 Hz fallen 8 213, mit der Stütze gegen die Welt 787
statt 699, und 3 kg mehr in den Beinen schaden, bei 30 Hz fallen 10 245, die Beine sind schwerer bei derselben Kraft der
Gelenke. Vorher festgelegt war dieselbe Regel wie in Runde 74 und 75, keine Form schafft sie. Die Balance kennt die
Größe der Füße nur zum Teil: Sie setzt den Knöchel eines Schritts seitlich mindestens `footGap` neben den des
Standfußes, 0,6 des Abstands der Hüften, und ohne Stütze den schwingenden Fuß vom Standfuß fern, solange er weniger als
`NB_KEEP_LENGTH` vor oder hinter ihm steht, 26 cm, so lang wie die Füße heute. Zwischen den Sohlen bleiben so mit großen
Füßen 4,6 statt 6,6 cm, mit schmalen Hüften 3 cm und mit beidem 1 cm, und gerade ohne Stütze fallen mit allen drei
Formen mehr mittlere Treffer. Siehe „Große Füße“ und die Zeilen danach in [Optimierungen](Optimierungen.md).
Vorgeschlagen ist als Runde 77, `footGap` und `NB_KEEP_LENGTH` aus Hüften und Sohlen abzuleiten, für den Menschen der
Bibliothek mit denselben Werten wie heute, so dass die Balance zu jedem Körper passt, auch zu den Rigs eines Spiels,
F53, und dann die großen Füße erneut zu messen. Der Spagat von Runde 75 bleibt danach zu verfolgen.

Der Körper von Sol, Runde 76, F83, auf Wunsch des Nutzers, nichts gebaut. Auf der Branch `ragdoll-sol` hat Sol am 6.
November einen zweiten Menschen in die Bibliothek gebaut, `nbMakeBalancedHumanoidRig` mit eigenen Clips: Oberschenkel
und Schienbeine 4 % länger, Bauch, Rippen und Brust um so viel kürzer, dass er 1,8 m groß bleibt, die Sohlen 12 statt
9 cm breit und 11 statt 4 cm hinter dem Knöchel, nach dem Fuß des Menschen von MuJoCo. Er steht 3,6 cm höher, und weil
die Balance ihren Maßstab aus der Standhöhe nimmt, rechnet sie mit einer 3,7 % größeren Figur. Auf seiner Branch fielen
mit eigener Kraft bei 60 Hz von 1 200 harten Treffern auf die Mitte 341 statt 464. Übertragen auf den Stand von Runde
71, mit denselben Knochen wie auf `ragdoll-sol`, gemessen mit jeder Stütze bei 20, 30, 40 und 60 Hz: Sols Körper, nur
seine Füße, nur seine Beine und sein Rumpf, und sein Körper mit der Balance in der alten Größe. Mit Sols Körper fallen
bei 60 Hz von allen 4 × 5 264 Treffern mit eigener Kraft 2 524 statt 3 032, mit der Stütze gegen die Welt 855 statt
1 051 und ohne Stütze 3 648 statt 4 864, zusammen 7 027 statt 8 947, keiner der leichten, bei 30 Hz über vier Lagen
2 522, 652 und 3 503 statt 2 958, 699 und 4 551, zusammen 6 677 statt 8 208, bei 40 Hz 458, 182 und 700 statt 549, 243
und 951, weniger als mit den großen Füßen. Dafür fallen mit eigener Kraft bei 30 Hz 164 statt 20 mittlere Treffer und
bei 20 Hz 14 statt 4 leichte, mit der Stütze gegen die Welt bei 20 Hz 294 statt 217 und ohne Stütze bei 60 Hz 18 statt 7
mittlere. Die neuen Stürze mit eigener Kraft kommen spät, die mittleren bei 30 Hz im Median nach 5,5 statt 3,5 s und 16
statt 10 Schritten: Die Figur kommt nicht mehr zur Ruhe. Mit Sols Füßen steht der Schwerpunkt im Stand über 64 statt
39 % der Sohle von der Ferse aus, 9,4 statt 15,9 cm hinter den Zehen, denn die Sohle reicht 7 cm weiter nach hinten, und
die Knöchel stehen wie vorher 4 cm hinter dem Becken, `NB_STANCE_BACK`. Nur Sols Füße lassen bei 60 Hz 7 519 und bei
30 Hz 6 988 fallen, mit eigener Kraft bei 30 Hz 112 mittlere, nur seine Beine und sein Rumpf 8 301 und 7 619, ohne
Stütze bei 60 Hz 45 mittlere, sein Körper mit der Balance in der alten Größe 7 124 und 6 684, mit eigener Kraft bei
30 Hz 73 mittlere und bei 20 Hz 5 leichte. Vorher festgelegt war dieselbe Regel wie für die anderen Formen, keine
schafft sie. Siehe „Körper von ragdoll-sol“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen
ist als Runde 77, die Maße der Balance, die heute für die Füße des Menschen der Bibliothek festliegen, aus dem Körper
abzuleiten, `footGap`, `NB_KEEP_LENGTH` und den Stand der Füße unter dem Becken, für den Menschen der Bibliothek mit
denselben Werten wie heute, und dann die großen Füße und Sols Füße mit dem Schwerpunkt über verschiedenen Stellen der
Sohle zu messen.

Die Maße der Balance aus dem Körper, Runde 77, F83, nach dem Vorschlag von Runde 76, nichts gebaut. Drei Maße der
Balance gelten heute nur für die Füße des Menschen der Bibliothek: Ein Schritt setzt seinen Knöchel mindestens `footGap`
neben den des Standfußes, 0,6 des Abstands der Hüften, ohne Stütze hält `nbKeepApart` den schwingenden Fuß fern, solange
er weniger als `NB_KEEP_LENGTH` vor oder hinter dem Standfuß steht, 26 cm, und die Clips stellen die Knöchel
`NB_STANCE_BACK` hinter das Becken, 4 cm. Aus dem Körper abgeleitet: `footGap` mindestens die breiteste Sohle und
6,5 cm, die Länge zum Fernhalten die der längsten Sohle, und die Knöchel so, dass die Mitte der Sohle unter dem Becken
steht wie heute. Für den Menschen der Bibliothek bleibt damit alles bitgleich, alle 171 Ergebnisse bei 20, 30 und 40 Hz
und die Ausgaben der Tests. Gemessen, mit jeder Stütze bei 20, 30, 40 und 60 Hz: die großen Füße von Runde 76 damit, der
Schwerpunkt im Stand über 40 % der Sohle von der Ferse aus, und mit den Knöcheln 3 cm weiter hinten, über 48 %, und Sols
Füße damit, mit dem Schwerpunkt über 42, 52 und 64 % der Sohle. Mit den Ableitungen lassen die großen Füße bei 60 Hz
8 211 statt 8 044 fallen, ohne Stütze 41 statt 24 mittlere, bei 30 Hz 7 128 statt 7 069: Die mittleren Treffer ohne
Stütze fallen also nicht deshalb, weil die Sohlen zu dicht stehen, wie Runde 76 vermutete. Mit und ohne die Ableitungen
lassen die großen Füße dieselben drei leichten Treffer fallen, bei 20 Hz mit der Stütze gegen die Welt zwei schräg von
hinten mit 58 N s nach rund 4 s und 9 und 10 Schritten, bei 40 Hz ohne Stütze einen von vorn mit 60 N s nach 2 s und 5
Schritten. Mit Sols Füßen kostet der Schwerpunkt weit vorn über der Sohle bei langsamen Takten, und er bringt bei 60 Hz
den Gewinn: Mit eigener Kraft fallen bei 30 Hz über 42, 52 und 64 % der Sohle 24, 31 und 76 mittlere Treffer, bei 60 Hz
über alle Stützen 8 091, 7 719 und 7 598. Über 52 %, mit den Knöcheln unter dem Becken, schafft Sols Fuß den Teil der
Regel für 60 Hz, mit 7 719 statt 8 947 und ohne Stütze 9 mittleren. Mit der Stütze gegen die Welt fallen mit Sols Füßen
bei 20 Hz aber 343 bis 367 statt 217, wo der Schwerpunkt auch steht, 149 bis 172 Treffer, die vorher standen, aus allen
Richtungen, im Median nach 2 s und 5 Schritten. Vorher festgelegt war dieselbe Regel wie in Runde 74 bis 76, und die
Ableitungen allein wären gebaut worden, wenn sie den großen Füßen helfen. Keine Form schafft die Regel, und den großen
Füßen helfen die Ableitungen nicht. Siehe „Maße der Balance aus dem Körper“ und die Zeilen danach in
[Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 78, die Treffer zu verfolgen, an denen die großen Füße
scheitern: die drei leichten bei 20 und 40 Hz und die 24 mittleren ohne Stütze bei 60 Hz. Sonst lassen sie bei 60 Hz ein
Zehntel weniger fallen und bei 30 Hz rund ein Siebtel.

Die Treffer, an denen die großen Füße scheitern, Runde 78, F83, nach dem Vorschlag von Runde 77, nichts gebaut. Verfolgt
wurden die 24 mittleren Treffer ohne Stütze bei 60 Hz und die drei leichten bei 20 und 40 Hz, die die großen Füße von
Runde 76 fallen lassen, und die 10 mittleren, die der Mensch der Bibliothek bei 60 Hz fallen lässt. Zwei Muster: Am Rand
stehen. Nach einem Schritt oder gleich nach dem Treffer steht die Figur mit dem Capture Point knapp außerhalb der
Stütze, 0 bis 3 cm, weniger als `NB_STEP_MARGIN`, und macht keinen Schritt. Das Becken kippt und sinkt 0,15 bis 0,4 s
lang, bis der Abstand 3 cm überschreitet oder ein Fuß abhebt, und der späte Schritt fängt nicht mehr. So fallen 14 der
24 mittleren Treffer der großen Füße, der leichte bei 40 Hz und 7 der 10 mittleren des Menschen der Bibliothek, 4 ohne
Stütze und 3 mit eigener Kraft. Der Spagat. Nach einem breiten Schritt, die Knöchel 0,89 m seitlich auseinander,
schwingt der Körper über das gelandete Bein, das Becken steigt mit 0,3 bis 0,6 m/s, und das hintere Bein, schon an der
Grenze seiner Reichweite, hebt seinen Fuß in 0,1 s um 3,8 cm, mehr als `NB_NEAR_GROUND`. Die Stütze schrumpft auf den
anderen Fuß, und der gehobene Fuß tritt nach außen, wohin der Körper nun fällt, jede Landung weiter außen, bis 1,35 m.
So fallen die beiden leichten Treffer bei 20 Hz mit der Stütze gegen die Welt, schräg von hinten mit 58 N s, und 2
mittlere bei 60 Hz. Der Mensch der Bibliothek macht in diesen leichten Treffern dieselben vier Schritte und bleibt bei
1,28 m stehen. 6 mittlere Treffer der großen Füße fallen sonst im breiten Stand, 2 anders. Gemessen, mit jeder Stütze
bei 20, 30, 40 und 60 Hz, mit den Füßen von heute und mit den großen: ein Schritt, sobald der Capture Point im Stand
0,15 s in Folge außerhalb der Stütze liegt, ebenso nach 0,3 s, nach 0,15 s nicht mit der Stütze gegen die Welt, ein Fuß,
der nicht schwingt, steht bis 6 statt 3 cm über dem Boden, und die erste Form mit der vierten. Nach 0,15 s fängt die
Figur mit eigener Kraft bei 60 Hz alle 3 mittleren Treffer, die vorher fielen, bei gleich vielen Stürzen insgesamt, und
die großen Füße lassen ohne Stütze 13 statt 24 mittlere fallen und bei 40 Hz keinen leichten. Mit eigener Kraft fallen
aber bei 30 Hz 28 statt 20 mittlere und bei 20 Hz ein leichter Treffer mehr, mit den großen Füßen 4 statt 3: Aus dem
breiten Stand hebt der Schritt nach der Zeit den äußeren Fuß, die Figur fällt über den fernen Standfuß, und der Fuß
tritt 0,58 m weiter nach außen, von dort in den Spagat. Mit der Stütze gegen die Welt macht die Figur mit der Regel in
manchen Treffern mehr Schritte, fällt aber nicht öfter. Mit 6 cm fangen beide Körper mit der Stütze gegen die Welt bei
20, 30 und 40 Hz mehr, die großen Füße auch die beiden leichten bei 20 Hz, ohne Stütze aber viel weniger, bei 30 Hz
fallen 5 022 statt 4 551: Ein Fuß, der bis 6 cm über dem Boden schwebt, zählt dann als Stütze, während der Körper über
ihn fällt. Vorher festgelegt war, eine Form zu bauen, die den Menschen der Bibliothek in den Grenzen der Regel von Runde
76 lässt und mit der die großen Füße bei jedem Takt weniger fallen lassen, keine schafft das. Erst nach der Messung
zusammengesetzt, nicht vorher festgelegt: der Schritt nach der Zeit nur ohne Stütze und 6 cm nur mit der Stütze gegen
die Welt. Aus den gemessenen Stützen zusammengesetzt bliebe der Mensch der Bibliothek damit überall in den Grenzen, mit
8 940 statt 8 947 Stürzen bei 60 Hz und 8 178 statt 8 208 bei 30 Hz, und die großen Füße ließen bei jedem Takt weniger
fallen, bei 60 Hz 8 029 statt 8 044, und keinen der drei leichten Treffer, an denen sie scheiterten. Nach der Regel von
Runde 76 fehlte ihnen dann ein mittlerer Treffer ohne Stütze bei 60 Hz, 13 statt höchstens 12. Siehe „Schritt nach der
Zeit am Rand“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 79, diese beiden
Formen je nach Stütze vorher festzulegen und an Treffern zu prüfen, die zu ihrer Wahl nicht beigetragen haben, an neuen
Punkten des Treffers und bei anderen Takten. Größer ist danach ein Schritt aus dem breiten Stand heraus, den es auch in
Runde 75 gab: erst das Gewicht über den Fuß nahe dem Capture Point, dann den anderen Fuß heranholen.

Die Formen von Runde 78 je nach Stütze, Runde 79, F83, nach dem Vorschlag von Runde 78, nichts gebaut. Runde 78 hatte
erst nach der Messung aus ihren Stützen zusammengesetzt, dass der Schritt nach 0,15 s am Rand nur ohne Stütze und der
Boden bis 6 cm nur mit der Stütze gegen die Welt beiden Körpern helfen: Ohne Stütze schiebt nichts den Capture Point
zurück, mit eigener Kraft tragen die Beine den Schwerpunkt zurück, und mit der Stütze gegen die Welt bleibt das Becken
oben, so dass der hintere Fuß eines breiten Stands wieder aufsetzt. Gebaut, wie es in die Bibliothek käme, erkannt an
den Gelenken der Stützen, und vorher festgelegt geprüft an Treffern, die zu dieser Wahl nicht beigetragen haben: bei 60
und 30 Hz mit dem Punkt des Treffers um −2 und +3 mm verschoben, die Tabellen nutzen 0, ±1 und +2 mm, und bei 25 und 50
Hz am mittleren Punkt samt den leichten Treffern. Auf den Tabellen von Runde 78 gleicht es der Zusammensetzung Byte für
Byte. Auf den neuen Treffern lässt der Mensch der Bibliothek damit bei 60 Hz 4 455 statt 4 465 fallen, bei 30 Hz 4 077
statt 4 091, bei 25 Hz 2 494 statt 2 497 und bei 50 Hz 1 699 statt 1 709, die großen Füße 4 021 statt 4 040, 3 522 statt
3 531, 2 354 statt 2 358 und 1 518 statt 1 525, ohne Stütze bei 60 Hz 7 statt 15 mittlere, und von den leichten Treffern
bei 25 und 50 Hz 8 statt 10. Beim Menschen der Bibliothek fällt aber bei 25 Hz ohne Stütze ein leichter Treffer mehr, 7
statt 6, und vorher festgelegt war, dass auf den neuen Treffern kein leichter mehr fällt. Von vorn mit 54 N s steht die
Figur nach ihren Fangschritten 1,6 s, ohne zu treten, der Capture Point wandert 1,3 cm hinaus, der Schritt nach der Zeit
tritt nach außen, und aus dem breiten Stand pendelt die Figur bis 1,12 m auseinander, bis sie nach 8 s als ruhelos
fällt. Auf den Tabellen von Runde 78 bleibt der Mensch der Bibliothek in allen Grenzen, mit 8 940 statt 8 947 Stürzen
bei 60 Hz und 8 178 statt 8 208 bei 30 Hz, und den großen Füßen fehlt nach der Regel von Runde 76 ein mittlerer Treffer
ohne Stütze bei 60 Hz, 13 statt höchstens 12. Siehe „Schritt nach der Zeit ohne Stütze, Boden bis 6 cm mit der Stütze
gegen die Welt“ in [Optimierungen](Optimierungen.md). Wie in Runde 75 und 78 endet der Treffer, an dem es scheitert, im
Spagat. Vorgeschlagen ist als Runde 80 der Schritt aus dem breiten Stand heraus: Steht die Figur breiter, als ihre Beine
aufrecht reichen, verlagert sie zuerst das Gewicht über den Fuß nahe dem Capture Point und holt dann den anderen Fuß
heran, statt mit dem äußeren Fuß weiter nach außen zu treten. Darauf die Formen dieser Runde erneut.

Der Schritt aus dem breiten Stand, Runde 80, F83, nach dem Vorschlag von Runde 79, nichts gebaut. Zuerst gemessen, mit
beiden Körpern, jeder Stütze, bei 30 und 60 Hz: Ein Stand, den die Beine nur mit dem Becken unter der Standhöhe
erreichen, ist das gewöhnliche Ende eines Fangs. Bei der letzten Landung eines gefangenen Treffers muss das Becken bei
60 Hz im Median mit eigener Kraft 6 mm, mit der Stütze gegen die Welt 80 mm und ohne Stütze 0 mm tiefer, bei einem
Zehntel der Fänge mindestens 75, 146 und 37 mm, und viele gefangene Treffer stehen mit den Knöcheln 1,0 bis 1,2 m
auseinander. Gebaut als Variante: Steht die Figur langsam, mit dem Capture Point innerhalb, in einem Stand, für den das
Becken mehr als 3 oder 6 cm unter die Standhöhe muss, schiebt sie das Becken zur Mitte der Sohle, der der Capture Point
am nächsten liegt, und sobald er in dieser Sohle liegt, setzt sie den anderen Fuß daneben, höchstens einmal bis zum
nächsten Fangschritt, nach 1 s gibt sie auf. Schon an einzelnen Treffern zeigte sich, dass das die Stürze von Runde 78
und 79 nicht erreicht: Im Spagat bei 20 Hz hebt der hintere Fuß 0,1 s nach der Landung ab, bevor die Figur langsam genug
ist, in einem Stand von 1,28 m bringt die Verlagerung den Capture Point in 1 s nicht über die nahe Sohle, und in einem
Stand, der 3 cm zu breit ist, folgen dem Nachsetzschritt zwei Fangschritte. Auch die Länge der Schritte nach der Zeit
von Runde 78 trennt die Treffer, die sie retten, nicht von denen, die sie verlieren, 0,12 bis 0,66 gegen 0,11 bis 0,77
m. Gemessen nach vorher festgelegter Regel wie in Runde 78: Mit eigener Kraft fallen bei 30 Hz 38 und 39 statt 20
mittlere Treffer und bei 20 Hz 5 und 6 statt 4 leichte, ohne Stütze bei 60 Hz 12 und 16 statt 7 mittlere, bei 60 Hz über
alle Stützen 8 983 und 8 988 statt 8 947, und die großen Füße lassen bei 60 Hz 8 080 und 8 096 statt 8 044 fallen, bei
20 Hz mit der Stütze gegen die Welt 4 statt 2 leichte. Mit den Formen von Runde 79 zusammen fallen mit eigener Kraft auf
deren neuen Treffern bei 30 Hz 1 507 statt 1 487. Jeder Schritt mehr aus dem Stand ist eine Gelegenheit zu fallen, keine
Form schafft die Regel. Siehe „Schritt aus dem breiten Stand“ und die Zeilen danach in
[Optimierungen](Optimierungen.md). Was in den Runden 78 bis 80 ohne Kosten half, ist allein der Boden bis 6 cm mit der
Stütze gegen die Welt: Der Mensch der Bibliothek lässt damit auf den Tabellen bei 60, 30, 20 und 40 Hz 1 047, 696, 202
und 234 statt 1 051, 699, 217 und 243 fallen und auf den neuen Treffern von Runde 79 bei 60, 25 und 50 Hz 520, 187 und
205 statt 523, 195 und 209, bei 30 Hz 347 statt 346, die großen Füße fangen damit die beiden leichten Treffer bei 20 Hz.
Gewählt wurde diese Form aber nach diesen Zahlen. Vorgeschlagen ist als Runde 81, sie allein vorher festzulegen und an
Treffern zu prüfen, die noch keine Runde gesehen hat, bei 35 und 45 Hz und mit dem Punkt des Treffers um −3 und +4 mm
verschoben. Danach ruht F83: Die großen Füße bleiben ein Vorschlag, ihnen fehlen ohne Stütze bei 60 Hz die mittleren
Treffer, 24 statt höchstens 12, und bei 40 Hz ein leichter Treffer.

Der Boden bis 6 cm mit der Stütze gegen die Welt, Runde 81, F83, nach dem Vorschlag von Runde 80, gebaut auf
`active-ragdoll-81`. Was in den Runden 78 bis 80 ohne Kosten half, allein und vorher festgelegt: Mit der Stütze gegen
die Welt steht ein Fuß, der nicht schwingt, auch, solange seine Sohle bis 6 statt 3 cm über dem Boden schwebt, erkannt
am Gelenk dieser Stütze, mit eigener Kraft und ohne Stütze wie vorher bis 3 cm. Weil diese Form nach den Tabellen von
Runde 78 und den neuen Treffern von Runde 79 gewählt war, wurde sie an Treffern geprüft, die noch keine Runde gesehen
hatte: bei 60 und 30 Hz mit dem Punkt des Treffers um −3 und +4 mm verschoben, bei 35 und 45 Hz am mittleren Punkt samt
den leichten Treffern. Vorher festgelegt war, dass mit der Stütze gegen die Welt auf keinem Satz mehr als 1 % und 5
mittlere Treffer mehr fallen, zusammen weniger als vorher, bei 35 und 45 Hz kein leichter mehr, dass mit eigener Kraft
und ohne Stütze alles Byte für Byte gleich bleibt und dass auf den Tabellen die Grenzen der Regel von Runde 76 halten.
Die Regel hält: Mit der Stütze gegen die Welt fallen auf den neuen Treffern bei 60, 30, 35 und 45 Hz 520, 346, 248 und
213 statt 521, 349, 249 und 221, zusammen 1 327 statt 1 340 von 29 344, wie vorher keiner der mittleren und keiner der
leichten, und auf den Tabellen bei 60, 30, 20 und 40 Hz 1 047, 696, 202 und 234 statt 1 051, 699, 217 und 243, von den
mittleren bei 20 Hz keiner statt 6 und bei 40 Hz 9 statt 20. Mit eigener Kraft und ohne Stütze bleibt alles bitgleich.
Die großen Füße lassen damit auf den neuen Treffern mit der Stütze gegen die Welt 1 019 statt 1 026 fallen. Gebaut
rechnet die Bibliothek auf den 171 Tabellen bei 20, 30 und 40 Hz Byte für Byte wie die gemessene Variante. In der Stadt
mit Figuren des Benchmarks enden über 48 Folgen von Granaten 1 001 statt 1 000 Fänge im Sturz und 319 statt 341 im
Stand, knapp im Rauschen, und am Ende stehen 1 236 statt 1 215 der 1 536 Figuren. Siehe „Boden bis 6 cm mit der Stütze
gegen die Welt“ in [Optimierungen](Optimierungen.md). Damit ruht F83: Die großen Füße bleiben ein Vorschlag, ihnen
fehlen ohne Stütze bei 60 Hz die mittleren Treffer, 24 statt höchstens 12, und bei 40 Hz ein leichter Treffer.

Der Boden bis 6 cm in der Stadt, Runde 82, F83, nach dem Vorschlag von Runde 81, nichts gebaut. In der Stadt mit Figuren
des Benchmarks endeten mit dem Boden bis 6 cm über 48 Folgen von Granaten 319 statt 341 Fänge im Stand, bei 1 001 statt
1 000 Stürzen. Geprüft, ob das an der Regel liegt: In einer Kopie der Bibliothek lässt sie sich für jede Figur einzeln
und ab jedem Update schalten, mit ihr rechnet die Stadt wie `active-ragdoll-81`, ohne sie wie `active-ragdoll-80`, bis
auf den Hash gleich. Sie greift in einem Fang, wenn ein Fuß, der nicht schwingt, mit der Stütze gegen die Welt 3 bis 6
cm über dem Boden schwebt: ohne die Regel in 190 von 1 372 Fängen, mit ihr in 181 von 1 356, im Median 26 Updates nach
dem Beginn des Fangs. 16 der 22 fehlenden Fänge im Stand liegen in Fängen, in denen sie nie greift. Vorher festgelegt
wurde jeder Fang, in dem sie greift, aus beiden Läufen ab dem ersten Update, in dem sie greift, mit der Regel für diese
eine Figur umgeschaltet wiederholt, bis dahin bit für bit gleich, und kostete sie nach einem einseitigen Vorzeichentest
auf 5 % Fänge, würde das nachverfolgt und behoben. Sie kostet keine: Über beide Läufe verliert die Figur mit der Regel 4
Fänge und gewinnt 8, kosten würde sie erst ab 5,7 mehr verlorenen als gewonnenen. Von den 266 verschiedenen Fängen, 105
davon in beiden Läufen bis zum Greifen gleich, enden 254 mit und ohne die Regel gleich. In 7 fängt sich die Figur nur
mit ihr, in 3 nur ohne sie, in 8 stürzt sie nur ohne die Regel, in 2 nur mit ihr. Die 22 fehlenden Fänge sind Zufall
einer Stadt, die nach jedem anderen Schritt einer Figur anders einstürzt. Siehe „Boden bis 6 cm in der Stadt“ in
[Optimierungen](Optimierungen.md). F83 ruht weiter.

Wo die eigene Kraft hinter der Stütze gegen die Welt bleibt, Runde 83, F61, auf die Frage des Nutzers, ob die Balance
fertig ist. Gemessen mit einer Kopie der Balance, die bei jedem Fang aufschreibt, woran sie aufgibt, wie weit der Rumpf
kippt, wie tief der Schwerpunkt sinkt und wie breit die Füße stehen, und die die Tabellen Treffer für Treffer gleich
rechnet. Bei 60 Hz fallen auf den neun Tabellen auf Brust und Bauch über vier Lagen des Treffpunkts, 17 856 Treffern,
mit eigener Kraft 2 359 und mit der Stütze gegen die Welt 727. 1 632 fängt nur die Stütze, umgekehrt keinen, und 1 629
davon kommen mit 90 N s und mehr. Mit eigener Kraft kippt der Rumpf bei ihnen in den ersten ein, zwei Schritten im
Median auf 0,47 bis 0,62 rad, mit der Stütze auf 0,24 rad. Danach erreicht nach einer Landung kein Schritt mehr den
Capture Point, 586-mal, der Schwerpunkt sinkt unter drei Viertel seiner Höhe im Stand, 514-mal, oder der Rumpf kippt
über 0,6 rad, 507-mal, 23-mal reichen die meisten Schritte nicht, zweimal kommt die Figur nicht zur Ruhe. Ein Stoß von
100 N s auf die obere Brust gibt dem Körper um seinen Schwerpunkt rund 40 N m s Drehimpuls. Ein Fuß hält gegen den Boden
seitlich etwa 30 und nach vorn und hinten etwa 90 N m, bevor er kippt, die Stütze gegen die Welt 125 N m in jede
Richtung, auch auf einem Fuß. Diese Stürze sind darum zum größten Teil die eines Körpers. Von 1 920 mittleren Treffern
von 50 bis 89 N s am mittleren Treffpunkt fallen mit eigener Kraft bei 60 Hz einer, bei 30 Hz 6 und bei 20 Hz 174. Bei
20 Hz kippen davon 77 im dritten Schritt, 46 sinken nach rund sechs Schritten, 42 kommen in 8 s mit 18 Schritten nicht
zur Ruhe und 9 erreicht kein Schritt mehr, die Demo bietet aber 30, 60 und 120 Hz an. Fänge aus eigener Kraft enden bei
60 Hz in einem gewöhnlichen Stand: Nach mittleren Treffern stehen die Knöchel im Median 0,37 m auseinander, bei 0,4 %
mehr als 0,8 m, nach im Median 2 Schritten und 1,6 s, den weitesten Stand von im Median 1,04 m gibt es nur unterwegs.
Mit der Stütze gegen die Welt bleibt die Figur dagegen nach meist einem Schritt bis zur Ruhe im Ausfallschritt stehen,
im Median 0,71 m breit und 1,35 s lang, erst danach setzt die Bibliothek ihre Füße animiert unter den Körper. Für
leichte und mittlere Treffer bei 30 bis 120 Hz ist die Balance aus eigener Kraft damit nah an dem, was ein Körper kann.
Offen bleiben der Ausfallschritt mit der Stütze gegen die Welt, der Griff zum Boden beim Sturz zur Seite, F35, und harte
Treffer aus eigener Kraft. Begonnen ist in dieser Runde der Griff zum Boden. Siehe „Wo die eigene Kraft hinter der
Stütze bleibt“ in [Optimierungen](Optimierungen.md).

Der Griff zum Boden beim Sturz zur Seite, Runde 83, F35, nichts gebaut. Nach vorn stützt sich eine fallende Figur mit
den Armen ab, `nb_clipBrace`, zur Seite und nach hinten rollt sie sich nur ein, `nb_clipCurl`. Vorher festgelegt: Bei
Stürzen zur Seite liegt die Hand oder der Unterarm der Fallseite mindestens 20 Punkte öfter als erster Teil des
Oberkörpers am Boden, der Kopf höchstens zwei Drittel so oft und im Median nicht schneller, nach vorn und nach hinten
kommen Arm oder Hand nicht seltener zuerst auf und der Kopf nicht schneller, die Figur steht so oft und so schnell
wieder auf, und jeder Sturz kommt im selben Update. Gemessen an allen Stürzen der neun Tabellen auf Brust und Bauch, mit
eigener Kraft und mit der Stütze gegen die Welt, bis die Figur wieder steht. Ein Griff, der sich sein Gewicht mit dem
Einrollen teilt, nimmt dem Kinn den Schutz: Bei 60 Hz kommt die Hand in 43,3 statt 34,9 % der Stürze zur Seite zuerst
auf, der Kopf trifft aber mit 4,47 statt 4,26 m/s auf, nach hinten mit 3,59 statt 3,12 m/s, in allen vier Formen. Nach
dem Einrollen nur in den Arm der Seite und in den Kopf gemischt, schlägt der Kopf an Stürzen, auf die keine Form
abgestimmt war, bei 40 Hz und bei 60 Hz an zwei anderen Treffpunkten, nur noch in 11,7 statt 20,0 % der Stürze zur Seite
zuerst auf, die Hand kommt aber nur in 45,1 statt 36,6 % zuerst auf. Der Griff beginnt zu spät: Wenn die Balance
aufgibt, sieht die Kugel entlang der Geschwindigkeit den Boden noch nicht, erst im Median 11 bis 12 Updates danach ist
der Griff auf 0,5, und im Median 17 Updates nach dem Aufgeben schlägt der Oberkörper auf. Mit der Kraft des Sturzes im
Arm kam die Hand nicht früher. Greift der Arm der Seite, die dem Boden zugewandt ist, schon vom Aufgeben an, an wieder
neuen Stürzen bei 50 Hz und bei 60 Hz an zwei weiteren Treffpunkten geprüft, kommt die Hand in 47,5 statt 34,7 % zuerst
auf und der Kopf in 11,4 statt 22,3 %, mit eigener Kraft in 13,3 statt 27,2 %. Nach vorn kommen Arm oder Hand in 95,9
statt 93,4 % zuerst auf, nach hinten in 62,6 statt 50,5 %, Aufstehen und Stürze bleiben gleich. Die Hand kommt damit
12,8 statt 20 Punkte öfter zuerst auf, nichts gebaut. Beim ersten Kontakt steht der Arm im Median 0,6 rad ab, gewollt
sind 1,3: Er beginnt sich erst zu heben, wenn die Balance aufgibt, und bis zum Aufschlag bleiben im Median 0,28 s. Damit
die Hand zuerst unten ist, müsste der Arm schon greifen, während die Figur noch tritt und der Sturz kommt. Siehe „Griff
zum Boden, mit dem Einrollen geteilt“, „Griff zum Boden nach dem Einrollen“ und „Griff zum Boden zur Seite, die dem
Boden zugewandt ist“ in [Optimierungen](Optimierungen.md).

Eine Figur, die das Spiel steuert, Runde 84, F84, auf Wunsch des Nutzers: „wir sollten erstmal versuchen eine figur
stuerbar zu machen“. Gebaut in der Bibliothek, nach Kapitel 2 und 25.1 der Roadmap des Spiels: Mit
`nbFigureDef::steered` bewegt sich ein Mensch der Bibliothek als Kapsel, ein Programm sagt mit `nbFigure_Steer` nur, mit
welcher Geschwindigkeit er über den Boden will und wohin er schaut, und `nbFigureInfo` gibt `moveVelocity` und
`onGround` zurück. Die Kapsel reicht von der Stufenhöhe, 0,4 der Standhöhe, bis an den Kopf, so breit wie Schultern und
Arme im Stand. Sie gleitet mit `b3World_CollideMover`, `b3SolvePlanes` und `b3World_CastMover` an dem entlang, was sie
berührt, nach den Ideen des Movers der Beispiele von Box2D und von `move_and_slide` in Godot, ohne ihre Zahlen. Auf dem
Boden ist steilerer Grund als `maxSlope` eine senkrechte Wand, ein Fühler unter der Kapsel hält sie mit einer kritisch
gedämpften Feder von 5 Hz über dem Boden, ohne Boden fällt sie mit der Schwerkraft, und ist sie länger in der Luft, als
ein Fall von 1,8 m dauert, fällt sie als Ragdoll. Die Füße treten nach der Kapsel in einem Gang mit dem Takt aus der
Länge der Beine. Beim Gehen steht ein Fuß 0,6 seines Zyklus, beim Rennen 0,33, umgestellt, wo die Froude-Zahl 0,5
passiert. Ein Fuß landet, wo die Hüfte nach 0,4 seiner Standzeit über ihn kommt, beim Rennen nach 0,35, und ein
stehender Fuß behält Ort und Richtung, bis er abhebt. Die Arme schwingen gegen die Beine, der Rumpf neigt sich in die
Geschwindigkeit, der Kopf bleibt waagrecht.

Vorher festgelegt und erfüllt, siehe die Tests `Steer…` in `test/test_figure.c`: 1. Figuren ohne `steered` rechnen
bitgleich wie auf `active-ragdoll-83`: Die 132 Tests von vorher enden mit denselben Hashes, die sechs Hashes des
Benchmarks bleiben. 2. Ein Parcours gesteuerter Figuren mit Wand, Treppe, einem Treffer und einer stehenden Figur endet
mit 1, 2 und 4 Threads mit demselben Hash, festgeschrieben für jede Plattform. 3. In der Ebene erreicht die Figur
gewünschte 1,5 und 4,5 m/s auf 10 % in der Zeit ihrer Beschleunigung und 0,1 s, es sind 1,500 und 4,500 m/s, steht
wieder und dreht sich mit ihrer Drehrate, höchstens 10,000 rad/s. 4. An einer Wand, gerade und unter 30°, überlappt die
um 1 cm verkleinerte Kapsel nie etwas Festes, sie steht 5 mm in der Wand, und unter 30° gleitet sie mit 0,751 m/s
entlang. 5. Eine Rampe von 20° und Stufen von 15 cm hinauf hat sie in jedem Update Boden und kommt oben an, ein Hang von
60° hält sie auf, und von einer Kante von 1 m fällt sie 27 Updates lang, landet und geht weiter. 6. In der Ebene bewegt
sich ein stehender Fuß bei 1,5 und 4,5 m/s zwischen Landen und Abheben weniger als 1 cm. Seit der Fuß abrollt, zählt die
Kante seiner Zehen, die stehen bleibt, während sich die Ferse hebt, und sie bewegt sich nicht. 7. Von hinten mit 120 N s
getroffen, während sie rennt, fliegt ihr Becken mit der Geschwindigkeit von Lauf und Treffer weg, auf 20 %, es sind 5,30
statt 6,12 m/s, und wieder auf den Füßen geht sie nach 4,2 s weiter, mit 1,45 m/s.

Von der Seite sah der Gang zuerst wie ein Ausfallschritt aus: Die Wurzel stand den ganzen Schritt so tief, wie die Beine
am Ende der Standphase reichen, beim Gehen mit 1,5 m/s das Becken 0,84 bis 0,87 statt 0,96 m hoch. Jetzt hebt ein Fuß
hinter der Hüfte die Ferse und rollt über die Kante seiner Zehen, so weit sein Bein es braucht, bis 0,7 rad, beim Gehen
bis 0,55. Beim Gehen schwingt die Hüfte über das Standbein: Ab 0,4 des Schwungs senkt sie sich, so weit das Bein den
Landepunkt braucht, nach dem Aufsetzen steigt sie kritisch gedämpft wieder wie ein Knie, das nachgibt, und das Becken
geht zwischen 0,909 und 0,956 m. Beim Rennen federt es in der Standphase auf 0,867 m ein und steigt im Flug auf 0,940 m.
Ein schwingender Fuß zieht die Hüfte nie hinunter, und im Gang bleibt sie über dem Boden unter der Kapsel, statt mit dem
tieferen Fuß hinunterzugehen. Die Rampe hinauf liegt das Becken so 0,76 bis 0,94 m über dem Boden, mit dem tieferen Fuß
waren es 0,60 bis 0,76. Was dabei nicht ging, steht unter „Gang einer gesteuerten Figur“ in
[Optimierungen](Optimierungen.md).

Die Demo hat dazu die Szene „Spielfigur“: W A S D relativ zum Blick, Shift rennt, die Kamera folgt von hinten, auf einem
Parcours aus Rampe, Treppe, Plattform, einem Hang von 60°, zwei Blöcken, einer Ziegelwand und drei Figuren, und `--walk`
lässt die Figur ihn ohne Menü gehen. Eine gesteuerte Figur kostet im Update der Figuren auf der VM 21 statt 1,7 µs,
gemessen mit 20 Figuren. Fast alles davon liegt in den Abfragen der Kapsel: `b3World_CollideMover` rechnet pro Aufruf
knapp 17 Kollisionen mit Formen, fast nur mit den eigenen Knochen, die die Kapsel umschließt, und erst der Rückruf
verwirft sie. Siehe „Gesteuerte Figur auf eigenen Füßen“ in [Optimierungen](Optimierungen.md).

Stufe 2 der gesteuerten Figur, Runde 85, F84, auf Wunsch des Nutzers, der auf den Vorschlag von Runde 84 „weiter“
schrieb: der Vorfilter für die Abfragen und der Gang am Hang mit der Ferse zuerst, in einer Runde. Die Abfragen einer
Figur ließen ihre eigenen Knochen bisher erst im Rückruf weg, nachdem Box3D sie gerechnet hatte, und die Kapsel einer
gesteuerten Figur umschließt sie. Jetzt hat `b3QueryFilter` in Box3D `excludeGroup`: Eine Abfrage lässt die Formen
dieser Kollisionsgruppe weg, bevor sie sie rechnet, siehe [`extern/README.md`](../extern/README.md). Jede Figur hat ihre
eigene Gruppe, 2²⁰ plus ihr Index, ohne Selbstkollision negativ wie bisher, mit ihr positiv, was in Box3D kollidiert wie
ohne Gruppe, und ihre Abfragen lassen sie weg: die Kapsel, die Fühler, die Füße, die Balance und das Aufstehen. Die
Rückrufe lassen die Figur weiter weg, so findet jede Abfrage dasselbe wie vorher, und das ganze Protokoll der Tests
blieb wie auf `active-ragdoll-84`. Eine gesteuerte Figur kostet im Update der Figuren auf der VM 7,7 statt 22,1 µs, eine
stehende 1,5 statt 1,7 µs, mit 20 Figuren in vier Läufen im Wechsel. Mit negativen Gruppen allein brachte es nichts,
denn der Mensch der Bibliothek kollidiert von Haus aus mit sich selbst, `selfCollision`, und hatte gar keine Gruppe.

Für den Gang vorher festgelegt, gehend mit 1,5 m/s bei 60 Hz, in der Ebene, an Rampen von 10° und 20° und auf Stufen von
15 cm hoch und 30 cm tief, jeweils hinauf und hinunter: 1. Das Becken bewegt sich gegen den Rahmen, mit dem es steht,
pro Update um höchstens 3 cm. 2. Zwischen zwei Landungen geht es auf 20° um höchstens 10 cm auf und ab, auf 10° um 7 cm,
auf Stufen um 14 cm und in der Ebene um 4,7 cm wie vorher. 3. Auf 20° sind die Schritte entlang des Hangs höchstens 0,85
so lang wie in der Ebene, in der Ebene gleich lang wie vorher. 4. Gehend landet ein Fuß in der Ebene und an den Rampen
auf der Ferse, die Zehen 0,15 bis 0,35 rad über dem Boden, rennend flach wie vorher. 5. Ferse und Zehenkante eines
stehenden Fußes bewegen sich am Boden weniger als 1 cm. 6. Kein Boden geht verloren, auch rennend mit 4,5 m/s die Rampe
von 20° hinauf und hinunter, rennend landet kein Fuß im Hang, und in der Ebene springt das Becken rennend nicht mehr als
vorher, 5,3 cm. 7. Die Bedingungen von Runde 84 gelten weiter, mit 1, 2 und 4 Threads bitgleich, Figuren ohne `steered`
wie vorher.

Alle sind erfüllt, siehe die Tests `SteerFootTest` und `SteerSlopeTest` in `test/test_figure.c`:

| Bedingung | `active-ragdoll-84` | `active-ragdoll-85` |
| --- | --- | --- |
| 1. größter Sprung des Beckens pro Update | Ebene 0,6, 20° hinauf 2,8, hinunter 18,6, 10° hinunter 8,3, Stufen hinauf 4,3, hinunter 24,9 cm | 0,6, 0,9, 0,3, 0,4, 1,2, 1,9 cm |
| 2. größtes Auf und Ab zwischen zwei Landungen | 20° 19,2 und 21,0, 10° 8,4 und 11,7, Stufen 25,2 und 29,9, Ebene 4,7 cm | 2,3 und 2,0, 2,4 und 2,6, 1,5 und 13,2, 4,7 cm |
| 3. Schritt auf 20° gegen die Ebene | 0,753 gegen 0,708 m, 1,06 | 0,583 m, 0,82, in der Ebene 0,708 und 1,496 m wie vorher |
| 4. Landung | flach | gehend jede auf der Ferse mit 0,25 rad, rennend 0 |
| 5. ein stehender Fuß rutscht | 0 | höchstens 0,2 mm |
| 6. rennend 20° | ein Fuß landete bis 0,70 rad im Hang | kein Boden verloren, 0,003 rad, in der Ebene springt das Becken 5,3 cm wie vorher |

Die Figur misst die Steigung ihres Wegs aus dem Weg ihres Rahmens über die letzten 0,5 m. Bergauf landet ein Fuß weiter
in seiner Standphase, bergab näher unter dem Körper, um eine Standphase je Einheit Steigung, `NB_GAIT_LAND_SLOPE`: So
reicht das vordere Bein so weit hinauf, wie das hintere hinunterreicht, und die Hüfte muss für keines tiefer. Die
Schritte werden je Einheit Steigung um 0,8 kürzer, `NB_GAIT_STEEP`. Die Feder unter der Kapsel folgt der Ebene, die der
Fühler findet. Bisher hinkte sie am Hang hinterher: Rennend 20° hinunter schwebte der Rahmen gut 10 cm über dem Boden,
und beim Landen sprang die Hüfte um 15 cm. Vor dem Landen geht die Hüfte so tief, wie das Bein den wirklichen Landepunkt
braucht. Was tiefer liegt als ebener Boden, nimmt sie schon ab dem Abheben vorweg, und nach 0,4 seines Schwungs sucht
ein Fuß nicht mehr tiefer, so wechselt er die Treppe hinunter nicht spät auf eine tiefere Stufe. Gehend hebt ein Fuß ab
der Hälfte seines Schwungs die Zehen um 0,25 rad, `NB_GAIT_HEEL`, landet auf der Ferse und rollt in einem Zehntel seines
Zyklus auf die Sohle ab, die Ferse bleibt stehen. Solange er von der Ferse kommt, zählt er als Fuß, über den die Hüfte
noch nicht hinweg ist. Eine gesteuerte Figur kostet mit allem 7,7 µs wie mit dem Vorfilter allein. Im Benchmark bleiben
alle sechs Hashes, die Bildzeiten liegen im Wechsel bei 0,98 bis 1,01 von `active-ragdoll-84`.

Zuerst maß das Messprogramm das Becken gegen den Rahmen nach dem Update. Die Knochen erreichen in einem Schritt aber,
was das Update davor ihnen gab, und so stand dort auch die Bewegung des Rahmens selbst darin, auf Stufen bis 3 cm pro
Update. Gemessen wird gegen den Rahmen, mit dem das Becken gestellt wurde, auch die Werte von `active-ragdoll-84` oben.
Wo die Treppe hinunter noch bis 13 cm bleiben: Bei Schritten von gut 0,5 m landet ein Fuß mal eine, mal zwei Stufen
tiefer. Was sonst nicht ging, steht unter „Gang am Hang“ in [Optimierungen](Optimierungen.md).

Rest von Stufe 2 der gesteuerten Figur, Runde 86, F84, auf Wunsch des Nutzers, der auf den Bericht von Runde 85 „weiter“
schrieb: Treppen Stufe für Stufe. Bis `active-ragdoll-85` setzte die Figur ihre Füße auf einer Treppe dorthin, wo sie
sie auf einer Rampe gleicher Steigung hingesetzt hätte. Mit 1,5 m/s auf Stufen von 15 × 30 cm stand ein Fuß bei 3 von 17
Landungen mit weniger als 0,7 seiner Sohle auf seiner Stufe, die Zehen steckten bis 15 cm in der nächsten, ein Schritt
nahm mal eine, mal zwei, mal drei Stufen, und die Treppe hinunter ging das Becken um bis 13 cm auf und ab.

Vorher festgelegt, bei 60 Hz auf Treppen mit 20 bis 24 Stufen, gehend mit 1,0, 1,5 und 2,0 m/s auf Stufen von 15 × 30 cm
und mit 1,5 m/s auf 17 × 29, 15 × 33 und 19 × 25 cm, jeweils hinauf und hinunter: 1. Jeder Fuß, der auf der Treppe
landet, steht mit der ganzen Sohle auf einer Stufe. Solange er flach steht, liegen mindestens 10 der 11 Punkte seiner
Sohle höchstens 1,5 cm über oder unter seiner Stufe, und kein Punkt steckt tiefer als 1,5 cm in der nächsten. Auf 19 ×
25 cm, kürzer als die Sohle von 26 cm, reichen 0,8 der Sohle, und nichts steckt in einer Stufe. 2. Ab der dritten
Landung auf der Treppe bis zur vorletzten nimmt jeder Schritt gleich viele Stufen. 3. Das Becken geht pro Schritt gegen
die Linie der Treppe um höchstens 6 cm auf und ab und springt in der Welt pro Update um höchstens 3 cm. Rennend mit 3,0
und 4,5 m/s auf 15 × 30 cm, hinauf und hinunter: 4. Kein Boden geht verloren, die Figur bleibt animiert, kein Punkt
einer flach stehenden Sohle steckt tiefer als 1,5 cm in einer Stufe, und das Becken springt pro Update um höchstens 5
cm. 5. In der Ebene und an Rampen von 10°, 20° und 26,6° gibt das Messprogramm dieselben Zahlen wie auf
`active-ragdoll-85`, Figuren ohne `steered` bleiben bitgleich, gesteuert mit 1, 2 und 4 Threads bitgleich, und die
Bedingungen der Runden 84 und 85 gelten weiter. 6. Eine gesteuerte Figur kostet in der Ebene im Update der Figuren
höchstens 1 µs mehr als auf `active-ragdoll-85`.

Erfüllt sind 1, 2, 4, 5 und 6, und 3 bis auf einen Fall: Auf 19 × 25 cm hinunter geht das Becken um 7,3 statt höchstens
6 cm auf und ab. Siehe die Tests `SteerStairsTest` und `SteerSlopeTest` in `test/test_figure.c`:

| Bedingung | `active-ragdoll-85` | `active-ragdoll-86` |
| --- | --- | --- |
| 1. kleinster Anteil einer Sohle auf ihrer Stufe | 0,00 bis 0,55, steckt bis 30 cm in einer Stufe | 1,00, auf 19 × 25 cm 0,82, steckt nirgends |
| 2. Stufen pro Schritt, 1,5 m/s auf 15 × 30 cm | hinauf 3221222221, hinunter 212221221221 | 2222222222 und 2222222222, mit 1,0 m/s jeder Schritt 1, mit 2,0 m/s 2 |
| 3. größtes Auf und Ab gegen die Linie der Treppe | hinauf 8,3 bis 12,1, hinunter 10,5 bis 21,3 cm | 0,1 bis 0,8 cm, 3,5 bis 5,8 cm, auf 19 × 25 cm hinunter 7,3 cm |
| 3. größter Sprung in der Welt pro Update | 3,5 cm | 1,8 cm |
| 4. rennend, größter Sprung pro Update | hinauf 3,6 und 4,7, hinunter 6,3 und 18,4 cm, steckt bis 15 cm | 1,7 und 1,8, 2,2 und 2,8 cm, steckt nirgends, kein Boden verloren |
| 5. Ebene und Rampen | – | dieselben Zahlen, ohne `steered` und mit 1, 2 und 4 Threads bitgleich |
| 6. eine gesteuerte Figur in der Ebene | 7,7 µs | 8,4 µs, 0,65 µs mehr |

Bevor ein Fuß zu seinem Ziel schwingt, sucht `nbFindFoothold` dort mit Strahlen nach unten in der Mitte der Sohle und 1
cm hinter ihren beiden Enden eine Kante: Boden mehr als 2 cm neben der Ebene unter der Mitte, mit vier Halbierungen auf
ein Sechzehntel des Wegs genau gefunden. Sie zählt nur, wenn der Boden an ihr um 4 bis 35 cm springt, ein Knick in einer
Rampe ist keine Kante. Liegt eine unter der Sohle oder geht die Figur auf einer Treppe, misst sie die Stufe unter der
Mitte bis 45 cm nach vorn und hinten aus und setzt die Sohle in ihre Mitte, auf eine Stufe, die kürzer ist als die
Sohle, an die höhere Kante. Eine Stufe von 15 bis 50 cm Tiefe zeigt der Figur, dass sie auf einer Treppe geht, wie tief
deren Stufen sind und wie hoch. Landet ein Fuß ohne Kante unter dem Ziel, halbiert sich das Gewicht der Treppe, und
unter 0,2 endet sie. Auf der Treppe nimmt jeder Schritt gleich viele Stufen: so viele, wie ein Schritt auf einer Rampe
gleicher Steigung lang wäre, auf das nächste ihrer Verhältnisse gerundet, eine andere Zahl erst 10 % jenseits der
Grenze, und höchstens 50 cm hinauf oder hinunter. Der Takt wird um so viel schneller oder langsamer. Ein Fuß zielt auf
die Stufe so viele Stufen vor dem anderen Fuß, und damit er dort landet, wo die Hüfte so weit in seiner Standphase über
ihn kommt, wie der Gang es will, geht der Gang in diesem Schwung bis 40 % schneller oder langsamer. Ein gewählter
Fußpunkt bleibt im Schwung, solange er höchstens eine halbe Stufe neben dem liegt, wohin der Gang den Fuß setzt, sonst
wählt die Figur neu, etwa wenn sie langsamer wird.

Die Kapsel geht eine Treppe Stufe für Stufe hinauf, und ein Becken, das ihr folgt, wippt bei jeder Stufe. Auf der Treppe
folgt die Wurzel deshalb dem Boden zwischen den Fußpunkten hinter und vor der Hüfte, wo die Füße stehen, landen oder
zuletzt abhoben, `nbStairGround`, und weicht dabei höchstens 0,6 m/s von der Linie der Treppe ab, `NB_STAIR_FOLLOW`.
Fängt der Gang auf einer Treppe an oder hört er auf, geht die Wurzel von dort weiter, wo sie war, und der Unterschied
klingt mit 0,15 s ab, `nbCarryRoot`. Auf der Treppe entscheidet deren Steigung, nicht die des Rahmens, wo ein Fuß in
seiner Standphase landet, wie tief die Hüfte für den Landepunkt geht und wo ein Fuß seinen Boden sucht. Findet ein Fuß
an seinem Fußpunkt keinen Boden, sucht er dort bis zur Höhe einer Stufe tiefer, nur an einem gewählten Fußpunkt, so
bleiben Rampen bitgleich.

Wo es noch fehlt: Auf 19 × 25 cm hinunter geht das Becken um 7,3 cm auf und ab. Auf Treppen, die keine Messung vorher
sah, geht es treppab gehend mit 1,2 bis 1,8 m/s auf Stufen von 12 bis 18 cm Höhe und 27 bis 35 cm Tiefe um 4,7 bis 6,4
cm auf und ab, in 4 von 7 Fällen etwas mehr als 6 cm, und mit 2,5 m/s um 10,3 cm. Auf Stufen von 14 × 35 cm nimmt die
Figur mit 1,5 m/s eine Stufe pro Schritt, 253 Schritte pro Minute. Hält sie auf der Treppe 2 s an und geht weiter,
landet dabei ein Fuß ein- oder zweimal teils über einer Kante, und das Becken springt bis 4,3 cm in einem Update, auf
`active-ragdoll-85` bis 14,3 cm. Was nicht ging, steht unter „Treppen Stufe für Stufe“ in
[Optimierungen](Optimierungen.md). Eine gesteuerte Figur kostet in der Ebene im Update der Figuren auf der VM 8,4 statt
7,7 µs, eine stehende 1,5 µs wie vorher, mit 20 Figuren in vier Läufen im Wechsel. Im Benchmark bleiben alle sechs
Hashes, die Bildzeiten liegen im Wechsel bei 0,99 bis 1,02 von `active-ragdoll-85`.

Gehen in der Ebene, Runde 87, F84, auf Wunsch des Nutzers, der auf den Bericht von Runde 86 „weiter, aber das normale
gehen ohne treppe sollte erstmal sauber aussehen“ schrieb. Auf `active-ragdoll-86` ruckte die gesteuerte Figur schon
geradeaus: Mit 2,0 m/s änderte ein Schienbein seine Geschwindigkeit von einem Update zum nächsten um bis 9,2 cm. Beim
Losgehen, in Wendungen und beim Wechsel der Geschwindigkeit sprangen Knochen bis 12,1 cm und das Becken bis 6,8 cm in
einem Update, es drehte sich um bis 9,7° pro Update², und in einer Wendung sank es um bis 30,6 cm: Ein Fuß blieb hinter
oder neben der Hüfte stehen, bis sein Bein ihn nicht mehr erreichte, und die Wurzel ging so tief, wie das Bein es
brauchte. Dazu setzte die Figur die Ferse mit 30 bis 32° gebeugtem Knie auf und streckte es im Stehen nie weiter als bis
23°, die Füße landeten 26 cm auseinander, Becken und Brust drehten sich nicht, das Becken pendelte nicht zur Seite, der
schwingende Fuß ging 6 bis 7 cm über den Boden, und die Ellbogen blieben steif.

Vorher festgelegt, bei 60 Hz mit 4 Teilschritten in der Ebene, gesteuert wie in der Demo, gemessen daran, wie sehr ein
Knochen seine Geschwindigkeit von einem Update zum nächsten ändert: 1. Geradeaus gehend mit 1,0, 1,6 und 2,0 m/s ändert
kein Fuß seine Geschwindigkeit um mehr als 3 cm pro Update, kein anderer Knochen um mehr als 2 cm und das Becken um mehr
als 0,5 cm. 2. Beim Losgehen und Anhalten mit 1,6 m/s, beim Wechsel der Richtung um 45, 90 und 180°, aus dem Stand
seitwärts und von 1,0 auf 2,0 m/s und zurück: Füße höchstens 4 cm, andere Knochen 3 cm und das Becken 1 cm pro Update,
Becken, Brust und Kopf ändern ihre Drehung um höchstens 2° pro Update², und das Becken sinkt nie mehr als 6 cm unter
seinen tiefsten Punkt beim Gehen geradeaus mit 1,6 m/s. Geradeaus gehend, gegen den Gang des Menschen: 3. Das Knie ist
beim Aufsetzen der Ferse höchstens 12° gebeugt und in der Mitte der Standphase irgendwann höchstens 12°, im Schwung
beugt es sich auf 50 bis 70°, am meisten bei 65 bis 80 % des Zyklus. 4. Die Füße setzen 10 bis 18 cm auseinander auf,
von Knöchel zu Knöchel. 5. Das Becken pendelt 2 bis 5 cm zur Seite des Standbeins und dreht sich um 2 bis 6° zu jeder
Seite, die Brust gegenläufig um 1,5 bis 6°. 6. Das Becken geht mit 1,0 m/s um 1,5 bis 5 cm auf und ab, mit 1,6 m/s um 3
bis 6 cm und mit 2,0 m/s um 3 bis 7 cm. 7. Der schwingende Fuß geht zwischen 30 und 70 % seines Schwungs 1 bis 4 cm über
den Boden. 8. Der Ellbogen beugt sich im Gang um mindestens 10° mehr und weniger. Unverändert: 9. Rennend mit 3,0 und
4,8 m/s springt kein Knochen und nicht das Becken stärker als auf `active-ragdoll-86`. 10. Auf Treppen gelten die
Bedingungen 1, 2 und 4 von Runde 86 weiter und 3 in keinem Fall um mehr als 1 cm schlechter, an Rampen von 10° und 20°
die Bedingungen 1 und 2 von Runde 85. 11. Alle Tests laufen, Figuren ohne `steered` bleiben bitgleich, gesteuerte mit 1,
2 und 4 Threads. 12. Eine gesteuerte Figur kostet im Update der Figuren höchstens 1 µs mehr als auf `active-ragdoll-86`.

Erfüllt sind 4, 5 und 7 bis 12, nicht ganz 1, 2, 3 und 6: Mit 2,0 m/s geradeaus ändert ein Schienbein seine
Geschwindigkeit noch um 2,4 cm pro Update und das Becken um 0,7 cm. In Wendungen und seitwärts sind es bei den
Schienbeinen 3,2 bis 3,7 statt höchstens 3 cm, und der Fuß, der von 1,0 auf 2,0 m/s schneller wird, 4,8 statt 4 cm. Das
Knie ist beim Aufsetzen der Ferse 14 bis 16° gebeugt statt höchstens 12°, mit 2,0 m/s im Stehen nie weniger als 12,5°,
und mit 1,0 m/s geht das Becken um 1,4 statt mindestens 1,5 cm auf und ab. Gemessen mit demselben Messprogramm auf
beiden Ständen, siehe auch die Tests `SteerTurnTest`, `SteerFootTest` und `SteerStairsTest` in `test/test_figure.c`:

| Bedingung | `active-ragdoll-86` | `active-ragdoll-87` |
| --- | --- | --- |
| 1. größte Änderung pro Update geradeaus mit 1,0, 1,6 und 2,0 m/s: Fuß, anderer Knochen, Becken | 1,4, 1,8 und 2,4 cm; 2,3, 3,2 und 9,2 cm; 0,16, 0,52 und 0,82 cm | 1,2, 1,5 und 1,9 cm; 1,4, 1,8 und 2,4 cm; 0,11, 0,45 und 0,67 cm |
| 2. Losgehen und Anhalten, Wendungen um 45, 90 und 180°, seitwärts, Wechsel der Geschwindigkeit: Fuß, anderer Knochen, Becken | bis 12,1, 11,6 und 6,8 cm | bis 4,8, 3,7 und 0,9 cm |
| 2. Drehung pro Update², tiefster Punkt des Beckens unter dem beim Gehen | bis 9,7°, 30,6 cm | bis 1,8°, 4,7 cm |
| 3. Knie beim Aufsetzen, am wenigsten gebeugt im Stehen, am meisten im Schwung | 30 bis 32°, 23°, 59 bis 63° bei 85 % | 14 bis 16°, 11,6 bis 12,5°, 66 bis 68° bei 69 % |
| 4. Abstand der Füße quer | 25 bis 26 cm | 14 cm |
| 5. Becken zur Seite, Becken gedreht, Brust gegenläufig | 0, 0 und 0 | 2,8 bis 3,0 cm, 3,9 bis 4,1°, 2,8 bis 2,9° |
| 6. Auf und Ab des Beckens mit 1,0, 1,6 und 2,0 m/s | 2,4, 5,2 und 6,3 cm | 1,4, 4,2 und 5,5 cm |
| 7. schwingender Fuß über dem Boden | 6,4 bis 7,1 cm | 1,6 bis 2,6 cm |
| 8. Ellbogen mehr und weniger gebeugt | 2,3° | 14,6 bis 15,8° |
| 9. rennend mit 3,0 und 4,8 m/s: Fuß, anderer Knochen, Becken | 2,9, 10,7 und 1,7 cm, 5,9, 12,9 und 8,5 cm | 2,8, 4,4 und 1,2 cm, 3,8, 4,6 und 1,8 cm |
| 10. Treppen gehend, Auf und Ab gegen die Linie der Treppe hinauf und hinunter | 0,1 bis 0,8 und 3,5 bis 7,3 cm | 0,2 bis 0,7 und 3,0 bis 6,2 cm |
| 10. Rampen von 10° und 20°, Sprung des Beckens pro Update und Auf und Ab | 0,9 cm, bis 2,6 cm | 0,9 cm, bis 2,9 cm |
| 12. eine gesteuerte Figur im Update der Figuren | 8,2 µs | 8,8 µs |

Was die Ruckler machte: Ein Fuß hob erst ab, wenn seine Phase im Zyklus es sagte. Lief die Hüfte schneller von ihm weg,
in einer Wendung, seitwärts oder beim Schnellerwerden, erreichte das Bein ihn vorher nicht mehr, und die Wurzel ging so
tief, wie es das brauchte. Jetzt sieht die Figur voraus, wie lange ihr Bein einen Fuß am Boden noch erreicht,
`nbTimeToLose`: Die Hüfte geht den Weg der Kapsel, deren Geschwindigkeit mit der Beschleunigung oder dem Bremsen zu der
gewollten geht, entlang der Steigung des Gangs, und deren Blick sich mit der Drehrate dreht, `nbPath`, in zehn Schritten
über 0,5 s und auf ein Zweiunddreißigstel genau. Ein Fuß hinter der Hüfte, den das Bein im nächsten Update auch ganz
über die Zehen gerollt nicht mehr erreicht, hebt früher ab. Ein Fuß neben oder vor der Hüfte ist bald verloren, wenn
sein Bein die Wurzel mehr als 4 cm tiefer bräuchte, als der Gang sie hat, bevor der andere Fuß hinschwingen könnte:
Steht der andere, hebt er ab, schwingt der andere, eilt der bis zu 2,5-mal so schnell, um vorher zu landen,
`NB_GAIT_HURRY`. Die Eile wächst um höchstens das Zehnfache des Gangs pro Sekunde, und der Fuß beschleunigt und bremst
dabei mit höchstens 120 m/s². Gehend bleibt ein Fuß stehen, solange der andere schwingt, und ein Fuß, der spät abhebt,
holt das über seinen ganzen Schwung auf.

Der Blick drehte sich bis `active-ragdoll-86` gleich mit der ganzen Drehrate von 10 rad/s. Jetzt kommt er mit
`nbFigureSteering::turnAcceleration` auf sie und bremst so, dass er beim gewollten Blick zur Ruhe kommt, 100 rad/s² von
Haus aus, 0 dreht wie bisher. Ein schwingender Fuß zielte mit der Geschwindigkeit, die die Kapsel in diesem Update
hatte, und wo sie sich änderte, sprang sein Ziel. Jetzt zielt er mit einer kritisch gedämpften Feder, die ihr in 0,04 s
folgt, und solange die Figur gehen will, so weit voraus, wie die Feder einer gleichmäßig wachsenden Geschwindigkeit
nachhängt, `NB_GAIT_AIM_LAG`. Hält sie an, zielt er ohne diesen Vorhalt: Sein Ziel rutscht beim Bremsen nicht mehr
zurück, und der Körper kommt über ihm zur Ruhe.

Der hintere Fuß rollt im letzten Fünftel des Zyklus vor dem Abheben um 0,35 rad weiter über die Zehen, als sein Bein es
braucht, die Ferse hoch und das Knie gebeugt, wie bei Menschen, `NB_GAIT_PUSH`. Der Abdruck kommt herein, während die
Hüfte die ersten 10 cm über den Fuß hinausgeht, so springt die Rolle in einer Wendung nicht. Abgehoben rollt der Fuß
erst so schnell weiter, wie er am Boden rollte, bis 12 rad/s, und in der Luft kippt er um einen Punkt, der zwischen
Zehen und Ferse wandert, `NB_FOOT_ROCK`, so springt der Knöchel nicht, wenn die Rolle von den Zehen auf die Ferse
wechselt. Ein Viertel in den Schwung gehen die Zehen um 0,35 rad weiter hinunter, so beugt sich das Knie auf 66 bis 68°.
Der Fuß hebt sich nur noch 6 statt 9 cm, am höchsten ein Viertel in den Schwung, wo die Zehen über den Boden müssen, von
dem sie abstießen, und geht in der Mitte des Schwungs 2 cm über den Boden.

Die Beine strecken sich gehend bis 0,995 ihrer Länge statt 0,98, und die Wurzel geht so viel höher, `NB_LEG_REACH_WALK`,
so ist das Knie im Stehen nur noch 12° gebeugt statt 23°. Vor dem Aufsetzen geht die Wurzel so tief, wie das Bein den
Knöchel des Fußes auf seiner Ferse braucht, nicht den des flachen Fußes, auch noch für einen Fuß, der schon steht und
von der Ferse herunterkommt, und ab 0,4 des Schwungs mehr und mehr für den schwingenden Fuß, so muss sie beim Aufsetzen
nicht springen. Will der Gang die Wurzel tiefer als im Update davor, sinkt sie wie in einem Knie, das nachgibt, und
kommt danach gedämpft wieder hoch.

Gehend setzen die Füße um 0,45 näher unter dem Körper auf, `NB_GAIT_NARROW_WALK`. Das Becken dreht sich um 0,07 rad zu
jeder Seite, die Seite des vorderen Beins voraus, und pendelt 1,5 cm über das Standbein, am weitesten 0,3 des Zyklus
nach dem Aufsetzen. Die Wirbelsäule dreht die Brust gegenläufig um 0,05 rad, Hals und Kopf drehen zurück, so schaut der
Kopf geradeaus, und die Ellbogen beugen sich um 0,12 rad mehr, wenn der Arm nach vorn schwingt, und weniger, wenn er
zurückschwingt, `NB_GAIT_PELVIS_TURN`. Drehen und Pendeln des Beckens wirken, wo die Füße gestellt werden, nicht auf die
Clips, `nbPlaceFeet`, so bleibt ein stehender Fuß, wo er steht.

Oben an einer Treppe sah die Vorhersage die Hüfte mit der Treppe weiter steigen. Ein gerade auf dem Absatz gelandeter
Fuß galt so als bald verloren und hob nach vier Updates wieder ab, und rennend mit 3,0 m/s nahm ein Schritt mitten auf
der Treppe nur eine Stufe. Für einen Fuß neben oder vor der Hüfte steigt der Weg jetzt nicht: Er steht auf dem weitesten
Boden, den die Figur erreicht hat, und höher steigt die Kapsel nicht, bevor die Hüfte über ihn kommt. In der Ebene
ändert das nichts.

Nachgemessen, nicht vorher festgelegt: Die Übergänge oben beginnen alle an derselben Stelle im Gang. Beginnen sie an
acht Stellen, je ein Achtel des Zyklus von 0,925 s auseinander, ist der schlimmste Fall größer, auf beiden Ständen.
Anhalten zur falschen Zeit ließ das Schienbein zuerst sogar um 7,8 statt 3,9 cm in einem Update springen: Beim Bremsen
rutschte das Ziel des schwingenden Fußes mit dem Vorhalt mit etwa 4 m/s zurück und blieb bei 0,8 des Schwungs plötzlich
stehen, `NB_GAIT_AIM`. Ohne Vorhalt beim Anhalten sind es 2,8 cm. In Wendungen springt ein Schienbein an einzelnen der
acht Stellen noch bis 9,6 cm, das Becken bis 4,7 cm, und um 90° sinkt es einmal 12,8 cm. Größte Änderung pro Update über
die acht Stellen, Fuß, anderer Knochen und Becken, und wie tief das Becken unter das beim Gehen sinkt:

| Übergang | `active-ragdoll-86` | `active-ragdoll-87` |
| --- | --- | --- |
| Anhalten | 5,2, 3,9, 1,0 cm, 3,2 cm | 5,0, 2,8, 0,8 cm, 4,7 cm |
| Wendung um 45° | 4,9, 10,3, 6,6 cm, 24,3 cm | 4,6, 9,6, 2,7 cm, 4,4 cm |
| Wendung um 90° | 12,1, 10,1, 6,6 cm, 30,1 cm | 4,4, 9,0, 4,7 cm, 12,8 cm |
| Wendung um 180° | 14,5, 15,4, 11,1 cm, 30,6 cm | 6,6, 7,0, 2,2 cm, 7,0 cm |
| seitwärts losgehen und anhalten | 5,2, 4,9, 1,0 cm, 3,2 cm | 5,0, 3,4, 0,8 cm, 4,7 cm |
| von 1,0 auf 2,0 m/s und zurück | 5,6, 11,6, 6,8 cm, 24,5 cm | 5,3, 4,5, 1,6 cm, 7,7 cm |

Wo es noch fehlt: Das Knie ist beim Aufsetzen 14 bis 16° gebeugt. Gestrecktere Beine machten die Ruckler größer, siehe
„Beine bis 0,997 gestreckt“ in [Optimierungen](Optimierungen.md). In Wendungen und seitwärts ändert ein Schienbein seine
Geschwindigkeit noch um 3,2 bis 3,7 cm in einem Update, wenn ein Fuß in Eile landet, je nach Stelle im Gang bis 9,6 cm,
und mit 2,0 m/s geradeaus um 2,4 cm. Am meisten fehlt ein Schwung, der sein Ziel ändern kann, ohne dass der Fuß springt:
Er geht heute auf geradem Weg von dort, wo er abhob, zu seinem Ziel, und ändert sich das Ziel oder bleibt es bei 0,8 des
Schwungs stehen, ändert der Fuß seine Geschwindigkeit in einem Update. Die Treppe hinunter geht das Becken auf 17 × 29
cm um 6,2 statt 5,8 cm auf und ab, auf 19 × 25 cm um 5,9 statt 7,3 cm. Was nicht ging, steht unter „Sauberes Gehen in
der Ebene“ in [Optimierungen](Optimierungen.md). Eine gesteuerte Figur kostet im Update der Figuren auf der VM 8,8 statt
8,2 µs, eine stehende 1,6 µs wie vorher, mit 20 Figuren in vier Läufen im Wechsel. Im Benchmark bleiben alle Hashes, die
Bildzeiten liegen im Wechsel bei 0,98 bis 1,02 von `active-ragdoll-86`.

Übergänge an jeder Stelle im Gang, Runde 88, F84, auf den Vorschlag am Ende von Runde 87, den der Nutzer mit „ok“
annahm. Auf `active-ragdoll-87` waren Anhalten, Wendungen, Seitwärtsgehen und der Wechsel der Geschwindigkeit an einer
Stelle im Gang ruhig, an anderen nicht: Beginnen sie an acht Stellen, je ein Achtel des Zyklus von 0,925 s auseinander,
ändert ein Fuß seine Geschwindigkeit um bis 6,6 cm in einem Update, ein Schienbein um bis 9,6 cm und das Becken um bis
4,7 cm, und es sinkt bis 12,8 cm. Ein schwingender Fuß ging auf geradem Weg von dort, wo er abhob, zu seinem Ziel, und
änderte sich das Ziel oder blieb es bei 0,8 des Schwungs stehen, änderte er seine Geschwindigkeit in einem Update. In 6
der 48 Läufe fand der Gang nach dem Übergang nicht in seinen Takt zurück: Jeder Fuß wurde hinter der Hüfte verloren und
hob bis 0,27 Zyklen vor seiner Zeit ab, noch vor seinem Abdruck, mit fast gestrecktem Bein.

Vorher festgelegt, bei 60 Hz mit 4 Teilschritten in der Ebene, gesteuert wie in der Demo, jeder Übergang an den acht
Stellen: 1. Kein Fuß ändert seine Geschwindigkeit von einem Update zum nächsten um mehr als 4 cm, kein anderer Knochen
um mehr als 3 cm und das Becken um mehr als 1 cm, und das Becken sinkt nie mehr als 6 cm unter seinen tiefsten Punkt
beim Gehen geradeaus mit 1,6 m/s. 2. Geradeaus mit 1,0, 1,6 und 2,0 m/s: Füße höchstens 3 cm, andere Knochen 2 cm, das
Becken 0,5 cm pro Update. Unverändert: 3. Die Bedingungen 3 bis 8 von Runde 87 bleiben, wo sie erfüllt sind, und werden
sonst nicht schlechter. 4. Rennend mit 3,0 und 4,8 m/s springt kein Knochen und nicht das Becken stärker als auf
`active-ragdoll-87`. 5. Auf Treppen gelten die Bedingungen 1, 2 und 4 von Runde 86 weiter und 3 in keinem Fall um mehr
als 1 cm schlechter als auf `active-ragdoll-87`, jeder Fuß steht auf der Treppe irgendwann flach, an Rampen von 10° und
20° gelten die Bedingungen 1 und 2 von Runde 85. 6. Alle Tests laufen, Figuren ohne `steered` bleiben bitgleich,
gesteuerte mit 1, 2 und 4 Threads. 7. Eine gesteuerte Figur kostet im Update der Figuren höchstens 1 µs mehr als auf
`active-ragdoll-87`.

Erfüllt sind 3 bis 7, die erste beim Anhalten und seitwärts ganz, sonst nicht ganz, und die zweite wie auf
`active-ragdoll-87` nicht ganz: Mit 2,0 m/s geradeaus ändert ein Schienbein seine Geschwindigkeit um 2,4 cm pro Update
und das Becken um 0,7 cm, gleich wie vorher, denn geradeaus gehen und rennen bleiben nach dem Losgehen bitgleich. In den
Wendungen und beim Wechsel der Geschwindigkeit springen Schienbeine an einzelnen Stellen noch 4,3 bis 6,3 statt
höchstens 3 cm, in der Wendung um 180° ein Fuß 4,6 statt höchstens 4 cm, in den Wendungen um 90 und 180° und beim
Wechsel der Geschwindigkeit das Becken 1,5 bis 1,9 statt höchstens 1 cm, und in der Wendung um 90° und beim Wechsel der
Geschwindigkeit sinkt es 6,1 statt höchstens 6 cm. In der Wendung um 45° und seitwärts sinkt es tiefer als vorher, 5,1
und 5,9 statt 4,4 und 4,7 cm, unter der Grenze. Größte Änderung pro Update über die acht Stellen, Fuß, anderer Knochen
und Becken, und wie tief das Becken unter das beim Gehen sinkt, gemessen mit demselben Messprogramm auf beiden Ständen,
siehe auch `SteerInStepTest` in `test/test_figure.c`:

| Übergang | `active-ragdoll-87` | `active-ragdoll-88` |
| --- | --- | --- |
| Anhalten | 5,0, 2,8, 0,8 cm, 4,7 cm | 2,4, 2,5, 0,8 cm, 4,7 cm |
| Wendung um 45° | 4,6, 9,6, 2,7 cm, 4,4 cm | 3,1, 4,7, 1,0 cm, 5,1 cm |
| Wendung um 90° | 4,4, 9,0, 4,7 cm, 12,8 cm | 3,9, 5,3, 1,5 cm, 6,1 cm |
| Wendung um 180° | 6,6, 7,0, 2,2 cm, 7,0 cm | 4,6, 6,3, 1,9 cm, 5,0 cm |
| seitwärts losgehen und anhalten | 5,0, 3,4, 0,8 cm, 4,7 cm | 3,8, 3,0, 0,9 cm, 5,9 cm |
| von 1,0 auf 2,0 m/s und zurück | 5,3, 4,5, 1,6 cm, 7,7 cm | 3,7, 4,3, 1,6 cm, 6,1 cm |
| Updates über den Grenzen der ersten Bedingung in allen 48 Läufen, davon später als 1,5 s nach dem Wechsel | 162, 12 | 76, 3 |
| Läufe, die außer Takt enden | 6 | 0 |

Ein schwingender Fuß plant seinen Weg jetzt in jedem Update neu, `nbSwingPath`: eine kubische Bahn von dort, wo er ist,
so schnell, wie er sich bewegt, zu seinem Ziel, wo er zur Ruhe kommt, über die Zeit, die sein Schwung noch hat. Bleibt
das Ziel, ist das genau der Weg wie bisher. Ändert es sich, eilt der Schwung oder bleibt das Ziel bei 0,8 des Schwungs
stehen, biegt sich der Weg, und der Fuß ändert seine Geschwindigkeit nicht mehr in einem Update. Die Drehung zur
Richtung, mit der er landet, plant er ebenso, `nbSwingTurn`.

Von zwei Füßen am Boden hebt der zuerst ab, den sein Bein zuerst verliert, sobald einer abheben muss, und der andere
wartet, auch wo seine Phase sagt, er wäre dran. Vorher hob der fällige Fuß ab, der andere wurde verloren, während der
erste schwang, und die Wurzel sank, in der Wendung um 90° 12,8 cm. Ein Fuß, der so länger steht, als seine Phase sagt,
behält den Abdruck über die Zehen, bis er abhebt, `NB_GAIT_PUSH`.

Ein schwingendes Bein streckt sich höchstens 0,2 % seiner Länge weiter als seine Reichweite, `NB_LEG_SWING_SLACK`, und
sein Fuß geht dahin, wohin das Bein reicht. Vorher streckte die IK es, wo die Hüfte bremst oder sich von einem gerade
abgehobenen Fuß wegdreht, für ein Update ganz, und nahe der Streckung bewegt sich das Knie fünfmal so weit wie der
Knöchel: Es schnappte gerade und zurück, ein Schienbein sprang bis 9,6 cm. Geradeaus kommt kein schwingendes Bein so
weit.

Der Takt des Gangs hält Schritt mit den Füßen, `NB_GAIT_KEEP`: Hebt ein Fuß gehend mehr als ein Update vor seiner Zeit
ab, weil sein Bein ihn verliert, zeigt das, wie weit der Takt hinter den Füßen ist, und der Takt holt das kritisch
gedämpft mit 0,25 s auf. So hebt der nächste Fuß wieder nach seinem Abdruck ab, und kein Lauf endet mehr außer Takt.
Rennend driften die Füße schon auf `active-ragdoll-87` langsam gegen den Takt, mit 4,8 m/s um 0,003 Zyklen pro Schritt,
und dort hält der Takt seine eigene Zeit.

Wo es noch fehlt, nachgemessen an den größten Sprüngen: Kommt in einer Wendung ein Fuß von hinter der Hüfte neben sie,
hört er in einem Update auf, über die Zehen zu rollen, und die Wurzel geht so tief, wie sein Bein dann braucht, in der
Wendung um 180° 1,9 cm in einem Update. Beginnt ein Schwung zu eilen, hebt sich der Fuß mit dem Quadrat der Eile weniger
hoch und fällt im nächsten Update. Bleibt das Ziel bei 0,8 des Schwungs stehen, hört die Wurzel in einem Update auf, für
die Landung tiefer zu gehen, beim Wechsel von 1,0 auf 2,0 m/s 1,6 cm. Ein Fuß, der weit hinter der Hüfte um 1 rad über
die Zehen gerollt abhebt, senkt den Knöchel beim Abrollen schneller, als das Bein nachgibt, in der Wendung um 90° 5,3 cm
am Schienbein. Und fällt der Bedarf der Beine weg, kommt die Wurzel aus der Ruhe hoch, auch wo sie gerade mit 0,6 m/s
sank. Was nicht ging, steht unter „Übergänge an jeder Stelle im Gang“ in [Optimierungen](Optimierungen.md). Eine
gesteuerte Figur kostet im Update der Figuren auf der VM 9,1 statt 9,0 µs, eine stehende 1,5 µs wie vorher, mit 20
Figuren in vier Läufen im Wechsel. Im Benchmark bleiben alle Hashes. Er hat keine gesteuerte Figur und rechnet denselben
Code: Die Bildzeiten der Stadt liegen im Wechsel bei 0,98 bis 1,01 von `active-ragdoll-87`, die Schritte der drei Szenen
mit Einschlägen schwanken zwischen 0,95 und 1,08.

Hub, Rolle und Hüfte in den Übergängen, Runde 89, F84, auf den Vorschlag am Ende von Runde 88, den der Nutzer mit „ok
weiter“ annahm. Auf `active-ragdoll-88` sprangen über die acht Stellen im Gang noch Schienbeine bis 6,3 cm in einem
Update und das Becken bis 1,9 cm, in 76 Updates über den Grenzen der ersten Bedingung von Runde 88. Fast alle Sprünge
waren Knie: Nahe der Streckung bewegt sich ein Knie bis fünfmal so weit, wie sich der Abstand von Hüfte und Knöchel
ändert, und in den Wendungen hob ein eilender Fuß mit fast gestrecktem Bein ab oder setzte so auf. Dazu blieb das Ziel
eines Fußes bei 0,8 des Schwungs schlagartig stehen, und die Wurzel hörte im selben Update auf, für die Landung tiefer
zu gehen, die Rolle eines Fußes, der wieder neben die Hüfte kam, endete in einem Update, und begann ein Schwung zu
eilen, hob sich der Fuß sofort weniger hoch.

Vorher festgelegt, nach der Suche nach den Ursachen an Varianten und vor dem Bau, gemessen wie in Runde 88: 1. Über alle
acht Stellen jedes Übergangs ändert kein Fuß seine Geschwindigkeit von einem Update zum nächsten um mehr als 4 cm, kein
anderer Knochen um mehr als 3 cm und das Becken um mehr als 1 cm, und das Becken sinkt nie mehr als 6 cm unter seinen
tiefsten Punkt beim Gehen geradeaus mit 1,6 m/s. 2. In allen 48 Läufen springen Knochen in höchstens halb so vielen
Updates über diese Grenzen wie auf `active-ragdoll-88`, höchstens 38. Unverändert: 3. Geradeaus mit 1,0, 1,6 und 2,0 m/s
und rennend mit 3,0 und 4,8 m/s springt kein Knochen und nicht das Becken stärker als auf `active-ragdoll-88`. 4. Die
Bedingungen 3 bis 8 von Runde 87 bleiben, wo sie erfüllt sind, und werden sonst nicht schlechter. 5. Auf Treppen gelten
die Bedingungen 1, 2 und 4 von Runde 86 weiter und 3 in keinem Fall um mehr als 1 cm schlechter als auf
`active-ragdoll-88`, jeder Fuß steht auf der Treppe irgendwann flach, an Rampen von 10° und 20° gelten die Bedingungen 1
und 2 von Runde 85. 6. Kein Lauf endet außer Takt, und `SteerInStepTest` läuft. 7. Alle Tests laufen, Figuren ohne
`steered` bleiben bitgleich, gesteuerte mit 1, 2 und 4 Threads. 8. Eine gesteuerte Figur kostet im Update der Figuren
höchstens 1 µs mehr als auf `active-ragdoll-88`.

Erfüllt sind 2 bis 8, die erste beim Anhalten und seitwärts ganz wie auf `active-ragdoll-88` und für die Füße jetzt
überall, sonst nicht ganz: In den Wendungen und beim Wechsel der Geschwindigkeit springen Schienbeine an einzelnen
Stellen noch 3,1 bis 4,3 statt höchstens 3 cm, in den Wendungen um 90 und 180° das Becken 1,01 und 1,33 statt höchstens
1 cm, und in den Wendungen um 45 und 180° sinkt es 6,3 und 6,6 statt höchstens 6 cm, tiefer als vorher mit 5,1 und 5,0
cm, weil die Knie dort gebeugt sind. Das Ziel der Runde, die erste Bedingung auch in den Wendungen und beim Wechsel der
Geschwindigkeit, ist damit nicht erreicht. Geradeaus gehen und rennen springen gleich wie vorher, die Werte der
Bedingungen von Runde 87 sind dieselben, beim Losgehen weichen die Bahnen bis 2 cm ab, nach 3 s nur noch um 0,01 mm.
Größte Änderung pro Update über die acht Stellen, Fuß, anderer Knochen und Becken, und wie tief das Becken unter das
beim Gehen sinkt, mit demselben Messprogramm auf beiden Ständen, siehe auch `SteerKneeTest` in `test/test_figure.c`:

| Übergang | `active-ragdoll-88` | `active-ragdoll-89` |
| --- | --- | --- |
| Anhalten | 2,4, 2,5, 0,8 cm, 4,7 cm | 2,2, 2,0, 0,9 cm, 4,7 cm |
| Wendung um 45° | 3,1, 4,7, 1,0 cm, 5,1 cm | 3,0, 3,9, 1,0 cm, 6,3 cm |
| Wendung um 90° | 3,9, 5,3, 1,5 cm, 6,1 cm | 4,0, 3,7, 1,0 cm, 5,5 cm |
| Wendung um 180° | 4,6, 6,3, 1,9 cm, 5,0 cm | 3,9, 4,3, 1,3 cm, 6,6 cm |
| seitwärts losgehen und anhalten | 3,8, 3,0, 0,9 cm, 5,9 cm | 3,9, 2,9, 1,0 cm, 5,4 cm |
| von 1,0 auf 2,0 m/s und zurück | 3,7, 4,3, 1,6 cm, 6,1 cm | 3,8, 3,1, 1,0 cm, 5,8 cm |
| Updates über den Grenzen der ersten Bedingung in allen 48 Läufen, davon später als 1,5 s nach dem Wechsel | 76, 3 | 23, 0 |
| Läufe, die außer Takt enden | 0 | 0 |

Hebt ein Fuß ab, legt er fest, wie weit er früh hochgeht, mit der Eile, die sein Schwung dann hat: Eilt der Schwung
später mehr, hebt sich der Fuß nicht mehr in einem Update weniger hoch. Allein damit waren es 65 statt 76 Updates über
den Grenzen.

Kommt ein Fuß am Boden, der über die Zehen gerollt war, wieder neben die Hüfte, weil die Figur über ihn zurück wendet,
rollt er mit 0,06 s flach statt in einem Update, `NB_FOOT_UNROLL`, und die Wurzel geht so tief, wie sein Bein mit dem
Rest der Rolle braucht. In der Wendung um 180° springt das Becken so 1,3 statt 1,7 cm in einem Update, mit allen anderen
Teilen dieser Runde gemessen.

Das Ziel eines schwingenden Fußes bleibt bei 0,8 des Schwungs nicht mehr schlagartig stehen: Von 0,7 bis 0,9 behält es
mehr und mehr von dem, wohin es zuletzt zielte, `NB_GAIT_AIM_FADE`, und wo der Fuß landet, kommt weich zur Ruhe. So hört
auch die Wurzel nicht mehr in einem Update auf, für die Landung tiefer zu gehen. Mit den beiden Teilen davor waren es
damit 43 statt 61 Updates über den Grenzen, davon am Becken 4 statt 14. Runde 88 hatte ein Ziel verworfen, das zwischen
0,6 und 0,8 auslief: Es hing nach, und der Fuß landete, wo das Bein weiter reichen musste. Um 0,8 herum folgt es länger
und landet, wo es soll.

Bremst oder wendet die Kapsel, beugt eine gehende Figur die Knie, wie Menschen beim scharfen Wenden: Ihre Beine strecken
sich dann höchstens auf 96 statt 99,5 % ihrer Länge, `NB_LEG_REACH_BENT`, und die Wurzel geht so viel tiefer. Ganz
gebeugt ist sie, sobald die Geschwindigkeit, die die Kapsel will, von der, mit der die Füße zielen, quer oder entgegen
um 1 m/s abweicht, `NB_GAIT_BEND_SPEED`, kritisch gedämpft mit 0,08 s, und sie streckt sich mit 0,3 s wieder. Geradeaus
und beim Schnellerwerden beugt sie nicht. Nahe der Streckung bewegt sich ein Knie fünfmal so weit, wie sich der Abstand
von Hüfte und Knöchel ändert, bei 96 % eineinhalbmal: Hebt ein eilender Fuß ab oder setzt er auf, springt das Knie viel
weniger. Mit den Teilen davor sind es 23 statt 43 Updates über den Grenzen, an den Schienbeinen 19 statt 38, und ein
Schienbein springt in der Wendung um 180° bis 4,3 statt 6,3 cm. Im Test, an acht Stellen der Wendung um 180°, springen
die Knie in 14 statt 39 Updates um mehr als 3 cm und höchstens 4,7 statt 6,6 cm, `SteerKneeTest`. Dafür sinkt das Becken
in den Wendungen um 45 und 180° tiefer, bis 6,6 cm.

Wo es noch fehlt, nachgemessen an den übrigen Sprüngen: Wendet die Figur um, während ein Fuß weit hinter ihr steht und
der andere eilt, sinkt die Wurzel, um den stehenden Fuß zu erreichen, im Test an einer Stelle mit 0,9 m/s bis 12 cm, und
hebt er ab, hört sie in einem Update auf zu sinken, das Becken springt 1,9 cm. Sinkt sie mit ihrer Geschwindigkeit
weiter, springt es 1,4 cm, aber rennend springen Schienbeine und Becken stärker, wie schon in Runde 88. Und ein eilender
Fuß, der mit fast gestrecktem Bein abhebt, beschleunigt sofort voll, ein Knie springt dabei noch 3 bis 4 cm. Was nicht
ging, steht unter „Hub, Rolle und Hüfte in den Übergängen“ in [Optimierungen](Optimierungen.md). Eine gesteuerte Figur
kostet im Update der Figuren auf der VM 9,3 statt 9,1 µs, mit 20 Figuren in zweimal acht Läufen im Wechsel, einmal 0,1
µs weniger und einmal 0,4 µs mehr, eine stehende 1,5 µs wie vorher. Im Benchmark bleiben alle Hashes. Er hat keine
gesteuerte Figur und rechnet denselben Code: Die Bildzeiten der Stadt liegen im Wechsel bei 0,94 bis 1,01 von
`active-ragdoll-88`, die Schritte der drei Szenen mit Einschlägen bei 0,96 bis 1,00.

Der Körper fällt, die Beine fangen ihn, Runde 90, F84, auf Wunsch des Nutzers. Nach Runde 89 schrieb er „bei einem
menschlichen gang gibt es eine drehung im torso und schultern oder“ und „ein mensch fällt quasi mit dem oberkörper nach
vorne und die beine folgen, also rein bein ik sieht komisch aus“, und den Vorschlag dazu nahm er mit „weiter“ an. Auf
`active-ragdoll-89` fuhr die Hüfte waagerecht mit der Kapsel, die Beine reichten mit IK zu den Füßen, und der Oberkörper
neigte sich nur nach dem Tempo: Beim Losgehen wuchs die Neigung erst mit dem Tempo, beim Bremsen und in Wendungen neigte
er sich nicht. Gehend drehten Becken und Brust gegeneinander, das Becken um 4° und die Brust um 3° zu jeder Seite, ab
1,9 m/s immer weniger und rennend gar nicht. Laufen aus eigener Kraft, Schritt 3 von F76, hat der Nutzer verworfen: Der
Gang bleibt kinematisch, und wo die Figur ist, bestimmt das Spiel über die Kapsel.

Vorher festgelegt, nach der Suche in der Literatur und vor dem Bau, gemessen an der Linie vom Becken zum Hals gegen die
Senkrechte, nach vorn entlang der Blickrichtung und zur Seite, und an der Drehung von Becken, Brust und der Linie der
Schultergelenke um oben, bei 60 Hz mit 4 Teilschritten, gesteuert wie in der Demo. Der Oberkörper führt: 1. Beim
Losgehen aus dem Stand auf 1,6 m/s neigt er sich innerhalb von 0,6 s um 4 bis 12° weiter vor als im Gehen und ist 2 bis
2,5 s nach dem Wechsel bis auf 1° wieder so geneigt wie im Gehen. 2. Beim Losrennen auf 4,8 m/s 10 bis 30° weiter vor
als im Rennen, innerhalb von 0,8 s, und nach 2 s wieder wie im Rennen. 3. Beim Anhalten aus 1,6 m/s nie mehr als 1°
weiter vor als im Gehen, am wenigsten höchstens so weit vor wie im Stand und höchstens 8° dahinter, nach 2 s wie im
Stand. 4. Beim Anhalten aus 4,8 m/s lehnt er sich 3 bis 20° hinter den Stand zurück. 5. In Wendungen neigt er sich zur
Innenseite, um 90° gehend 3 bis 12°, rennend 8 bis 30°, um 45° gehend 1,5 bis 8°, und nie mehr als 2° nach außen. 6. Die
Neigung ändert sich um höchstens 0,5° pro Update², und Becken, Brust und Kopf ändern ihre Drehung um höchstens 2° pro
Update², Bedingung 2 von Runde 87. Drehung mit dem Tempo: 7. Gehend mit 1,0, 1,6 und 2,0 m/s dreht das Becken 2 bis 6°
zu jeder Seite und die Brust gegenläufig 1,5 bis 6°, Bedingung 5 von Runde 87. 8. Rennend mit 3,0 und 4,8 m/s dreht die
Brust 8 bis 17° zu jeder Seite, die Schultern mindestens 8°, das Becken 3 bis 8°, gegenläufig. 9. Mit 2,2 und 2,6 m/s
dreht die Brust mindestens 1,5°, gegenläufig zum Becken. Die Hüfte im Schritt: 10. Gehend mit 1,6 m/s ist das Becken
dort am langsamsten, wo es am höchsten ist, und sein Tempo schwankt um 3 bis 20 %. Unverändert: 11. An denselben 48
Läufen der Übergänge höchstens 23 Updates über den Grenzen der ersten Bedingung von Runde 89, je Übergang nicht mehr als
0,3 cm schlechter, kein Lauf außer Takt. 12. Geradeaus gehend und rennend springt kein Knochen mehr als 0,3 cm und das
Becken nicht mehr als 0,1 cm stärker als auf `active-ragdoll-89`. 13. Die Bedingungen 3 bis 8 von Runde 87 bleiben. 14.
Treppen und Rampen wie in Runde 89. 15. Alle Tests laufen, Figuren ohne `steered` bleiben bitgleich, gesteuerte mit 1, 2
und 4 Threads. 16. Eine gesteuerte Figur kostet höchstens 1 µs mehr.

Die Zahlen kommen vom Menschen. Gehend mit 5 km/h dreht das Becken im Mittel um 5,2° (Huthwelker u. a. 2023, 201
Gesunde), die Brust wenig und zwischen 0,85 und 1,52 m/s fast gleich, und mit dem Tempo drehen Becken und Brust von
gleichläufig zu gegenläufig (Lamoth u. a. 2002). Rennend mit 9 bis 12 km/h dreht das Becken um 10 bis 16°
(Perpiñá-Martínez u. a. 2023), die Brust mit 3,58 m/s um 22,6 bis 34,5° (eine Studie an Läufern) und die untere
Wirbelsäule gegen das Becken um 17,6 bis 24,7° (Tojima u. a. 2019), jeweils die ganze Spanne. Losgehen ist ein geführter
Fall nach vorn: Der Druckpunkt geht zurück, der Schwerpunkt nach vorn, und der Rumpf neigt sich. Wer mit a beschleunigt,
bringt den Schwerpunkt um tan α = a/g vor die Stütze: Losgehen erreicht das Tempo mit rund 1 bis 2 m/s², 6 bis 11°,
Losrennen mit 3 bis 5 m/s², 17 bis 27°, und ein Sprinter neigt den Rumpf beim Beschleunigen gegen Widerstand 36,5 statt
10,9° (ein Einzelfall). In Wendungen neigt sich der Rumpf zur Seite der Wendung, mit tan α = v²/(g r).

Erfüllt sind 1 bis 7 und 11 bis 16, die achte und die neunte nur für Brust und Schultern, weil das Becken rennend nicht
dreht, und die zehnte nicht, weil die Hüfte als Pendel nicht gebaut ist, siehe unten. Mit demselben Messprogramm auf
beiden Ständen, siehe auch `SteerLeanTest` in `test/test_figure.c`:

| | `active-ragdoll-89` | `active-ragdoll-90` |
| --- | --- | --- |
| 1. Losgehen auf 1,6 m/s: Neigung über der im Gehen, am meisten nach | 0,1°, 2,5 s | 5,8°, 0,42 s |
| 2. Losrennen auf 4,8 m/s: Neigung über der im Rennen, am meisten nach | 0,1°, 2,5 s | 14,8°, 0,63 s |
| 3. Anhalten aus 1,6 m/s: am weitesten vor und am wenigsten, im Stand 1,0° | 3,2° und 0,6° | 3,2° und −4,9° |
| 4. Anhalten aus 4,8 m/s: hinter dem Stand | 0,4° | 15,6° |
| 5. Zur Innenseite der Wendung um 90° gehend und rennend, um 45° gehend | 0, 0 und 0° | 6,3, 14,8 und 4,5° |
| 6. Neigung pro Update², Drehung von Becken, Brust und Kopf pro Update² in den Übergängen | 0,21°, 1,8° | 0,40°, 1,8° |
| 7. Gehend mit 1,0, 1,6 und 2,0 m/s: Becken, Brust | 3,9 bis 4,1°, 2,8 bis 2,9° | 3,9 bis 4,1°, 2,9 bis 3,4° |
| 8. Rennend mit 3,0 und 4,8 m/s: Brust, Schultern, Becken | 0, 0 und 0° | 12,2, 12,2 und 0° |
| 9. Brust mit 2,2 und 2,6 m/s | 1,5 und 0° | 7,6 und 12,2° |
| 10. Schwankung des Tempos der Hüfte im Schritt mit 1,6 m/s | 0 % | 0 % |
| 11. Updates über den Grenzen in den 48 Läufen der Übergänge, außer Takt | 23, 0 | 23, 0 |
| Drehung von Becken und Brust pro Update² beim Losrennen und Abbremsen | 1,7° und 1,2° | 0,1° und 0,3° |

Der Körper folgt der Geschwindigkeit der Kapsel über den Boden wie eine kritisch gedämpfte Feder von 0,3 s,
`NB_GAIT_LEAN_LAG`, und der Oberkörper neigt sich entlang ihrer Beschleunigung um atan(a/g), höchstens 0,5 rad,
`NB_GAIT_LEAN_MAX`: vor, wenn sie schneller wird, zurück, wenn sie bremst, und in einer Wendung zur Innenseite, der Kopf
bleibt waagerecht. Beim Losgehen auf 1,6 m/s beschleunigt der Körper so mit bis 2 m/s² wie ein Mensch, der losgeht, und
die Neigung ist nach 0,42 s am größten, während die Kapsel ihr Tempo schon nach 0,16 s hat. Mit 0,5 s lag sie beim
Losgehen nur 3,5° über der im Gehen, und rennend war sie nach 2 s noch 1,8° zu weit vorn.

Rennend dreht die Brust mit den Schultern um 0,21 rad zu jeder Seite gegen die Beine, `NB_RUN_CHEST_TURN`, im Lauf
12,2°, und zwischen Becken und Brust sind es damit ±12°, so viel wie die untere Wirbelsäule von Läufern gegen das
Becken. Das Becken dreht nicht: Dreht es um 5° zu jeder Seite, springen rennend mit 4,8 m/s ein Schienbein 5,2 statt 4,6
cm und das Becken 2,1 statt 1,8 cm in einem Update. Von Gehen zu Rennen gehen die Drehungen über, wie eine kritisch
gedämpfte Feder von 0,2 s folgt, wie weit die Figur rennt, `NB_RUN_TURN_LAG`: Beim Losrennen und Abbremsen geht das
Tempo in wenigen Updates durch die Spanne zwischen Gehen und Rennen, und auf `active-ragdoll-89` drehte das Becken dabei
um bis 1,7° pro Update², jetzt um 0,1°.

Die Hüfte als Pendel ist nicht gebaut. Schwang sie vor und zurück, so dass sie oben über dem Standfuß am langsamsten
war, mit 10 % Schwankung, erfüllte sie die zehnte Bedingung, aber in den 48 Läufen waren es 34 statt 23 Updates über den
Grenzen, mit dem geglätteten Tempo des Körpers 25, mit 6 % Schwankung 24, und mit einem Pendel, das ausblendet, solange
der Körper beschleunigt, 25: Das Pendel ändert sich, wenn der Takt schneller oder langsamer wird oder die Figur wendet,
und die Hüfte springt mit. Dabei geht sie mit 1,6 m/s nur 4 bis 7 mm vor und zurück. Die Füße mit dem Körper statt mit
der Kapsel zu planen, hätte nur mit dem Pendel Sinn gehabt, und schon in Runde 89 gab ein Becken, das der Kapsel nicht
folgte, mehr Sprünge.

Wo es noch fehlt: Rennend dreht das Becken nicht, beim Menschen um 10 bis 16°. Dafür müsste zuerst der Ruck weg, mit dem
die Wurzel im Flug für die Landung tiefer geht: Mit 4,8 m/s springt das Becken dort schon auf `active-ragdoll-89` 1,8 cm
in einem Update, weil der Bedarf des landenden Beins in ein oder zwei Updates wächst. Beim Losgehen nimmt die Kapsel mit
10 m/s² ihr Tempo in 0,16 s an, die Hüfte fährt mit, und nur der Oberkörper zeigt, dass der Körper fällt. Was nicht
ging, steht unter „Der Körper fällt, die Beine fangen ihn“ in [Optimierungen](Optimierungen.md). Eine gesteuerte Figur
kostet im Update der Figuren auf der VM 9,5 und 9,2 statt 9,3 und 9,1 µs, mit 20 Figuren in zweimal acht Läufen im
Wechsel, je 0,1 µs mehr, eine stehende 1,6 statt 1,5 µs, sie rechnet denselben Code. Im Benchmark bleiben alle Hashes.
Er hat keine gesteuerte Figur und rechnet denselben Code: Die Bildzeiten der Stadt liegen im Wechsel bei 0,97 bis 1,03
von `active-ragdoll-89`, die Schritte der drei Szenen mit Einschlägen bei 0,99 bis 1,00.

Die Wurzel landet ohne Ruck, Runde 91, F84, auf Wunsch des Nutzers. Den Vorschlag nach Runde 90, zuerst den Ruck im Flug
beim Rennen zu beheben und danach das Becken rennend gegen die Brust drehen zu lassen, nahm er mit „ok“ an. Auf
`active-ragdoll-90` wippte die Wurzel rennend, am höchsten im Flug, aber die Gangplanung senkte sie für die Landung nur
beim Gehen ab, zwischen Gehen und Rennen nur mit dem Anteil am Gehen, und rechnete das Wippen bei der Landung nicht mit.
Spät im Schwung erreichte das landende Bein seinen Fuß nicht mehr, und `nbLowerSteered` senkte die Wurzel in zwei
Updates um bis 3,3 cm, während sie noch stieg. Am weitesten muss das Bein dabei einige Updates vor der Landung reichen,
nicht bei ihr, weil die Hüfte hinter dem Fuß zurückbleibt. Mit 4,8 m/s sprang das Becken so 1,8 cm in einem Update.

Vorher festgelegt, nach der Messung von `active-ragdoll-90` und vor dem Bau, mit dem Sprung als zweiter Differenz des
Ursprungs eines Knochens von Update zu Update, bei 60 Hz mit 4 Teilschritten, gesteuert wie in der Demo. Der Flug: 1.
Geradeaus mit 2,2, 2,6, 3,0 und 4,8 m/s springt das Becken höchstens 1,0 cm in einem Update. 2. Dort springt kein Fuß
und kein anderer Knochen stärker als auf `active-ragdoll-90`, auf ganze Millimeter gerundet. 3. Rennend mit 3,0 und 4,8
m/s geht das Becken in jedem Schritt 6 bis 9 cm auf und ab, am höchsten im Flug und am tiefsten im Stand, und das Knie
ist beim Aufsetzen 15 bis 30° gebeugt und im Stand höchstens 2° stärker als vorher. Das Becken dreht beim Rennen: 4.
Rennend mit 3,0 und 4,8 m/s dreht das Becken 3 bis 8° zu jeder Seite gegen die Brust, mit einer Korrelation von
höchstens −0,5, die Brust 8 bis 17° und die Linie der Schultern mindestens 8°. 5. Mit 2,2 und 2,6 m/s dreht die Brust
mindestens 1,5°, gegenläufig zum Becken. 6. Die Bedingungen 1 bis 3 gelten auch mit dem drehenden Becken. Unverändert:
7. An den 48 Läufen der Übergänge höchstens 23 Updates über den Grenzen der ersten Bedingung von Runde 89, je Übergang
nicht mehr als 0,3 cm schlechter, kein Lauf außer Takt. 8. Gehend geradeaus mit 1,0, 1,6 und 2,0 m/s springt kein
Knochen mehr als 0,3 cm und das Becken nicht mehr als 0,1 cm stärker. 9. Beim Losrennen auf 4,8 m/s, beim Anhalten
daraus und in der Wendung um 90° rennend springt nichts stärker. 10. Die Bedingungen 1 bis 7 von Runde 90 bleiben
erfüllt. 11. Die Bedingungen 3 bis 8 von Runde 87 bleiben. 12. Treppen und Rampen wie in Runde 90. 13. Alle Tests
laufen, Figuren ohne `steered` bleiben bitgleich, gesteuerte mit 1, 2 und 4 Threads. 14. Eine gesteuerte Figur kostet
höchstens 1 µs mehr.

Die Zahlen kommen vom Menschen. Der Schwerpunkt eines Läufers ist mitten im Flug am höchsten und mitten im Stand am
tiefsten. Im Flug fällt er frei, bei 60 Hz mit 2,7 mm pro Update², und im Stand drückt der Boden mit bis zum
Zweieinhalbfachen des Gewichts, rund 4 mm pro Update²: Ein Sprung von 1 cm in einem Update liegt schon über dem, was ein
Mensch zeigt. Das Knie ist beim Aufsetzen leicht gebeugt und im Stand am meisten (Novacheck 1998), und Läufer drehen das
Becken um 10 bis 16°, die ganze Spanne, gegen den Brustkorb (Perpiñá-Martínez u. a. 2023, Schache u. a. 2002, siehe
Runde 90).

Erfüllt sind 1, 2, 4, 5 und 7 bis 14, die dritte und die sechste nur mit 3,0 m/s: Mit 4,8 m/s geht das Becken 4,2 bis
4,8 statt 6 bis 9 cm auf und ab, siehe unten. Mit demselben Messprogramm auf beiden Ständen, siehe auch `SteerRunTest`
in `test/test_figure.c`:

| | `active-ragdoll-90` | `active-ragdoll-91` |
| --- | --- | --- |
| 1. Sprung des Beckens geradeaus mit 2,2, 2,6, 3,0 und 4,8 m/s | 14,6, 12,7, 11,7 und 18,4 mm | 1,8, 2,9, 2,9 und 3,5 mm |
| 2. Sprung eines Fußes | 16, 27, 28 und 38 mm | 15, 23, 24 und 30 mm |
| 2. Sprung eines anderen Knochens | 37, 40, 44 und 46 mm | 22, 39, 37 und 36 mm |
| 3. Auf und ab in jedem Schritt mit 3,0 und 4,8 m/s | 7,3 bis 7,5 und 6,8 bis 7,5 cm | 6,4 bis 6,5 und 4,2 bis 4,8 cm |
| 3. Knie beim Aufsetzen mit 3,0 und 4,8 m/s, im Stand am meisten | 23 und 24 bis 26°, 49 und 58 bis 60° | 23 und 27 bis 30°, 49 und 58 bis 60° |
| 4. Rennend: Becken, Brust und Schultern, Korrelation | 0, 12,2 und 12,2°, −0,37 und −0,35 | 5,2, 12,2 und 12,2°, −1,0 |
| 5. Brust mit 2,2 und 2,6 m/s, Korrelation | 7,6 und 12,2°, −1,0 und −0,35 | 7,6 und 12,2°, −1,0 |
| 7. Updates über den Grenzen in den 48 Läufen, außer Takt | 23, 0 | 23, 0 |
| 8. Gehend mit 1,0, 1,6 und 2,0 m/s: Sprung eines anderen Knochens | 14,0, 17,9 und 24,3 mm | 14,0, 17,9 und 19,1 mm |
| 8. Gehend: Sprung des Beckens | 1,1, 4,5 und 6,7 mm | 1,1, 4,5 und 6,2 mm |
| 9. Losrennen, Anhalten daraus und Wendung um 90° rennend: Fuß, anderer Knochen, Becken | 64, 90 bis 128 und 21 mm | 64, 90 bis 128 und 21 mm |
| 10. Rennend zur Innenseite der Wendung um 90°, Drehung pro Update² in den Übergängen | 14,8°, 1,95° | 14,4°, 1,95° |
| 12. Sprung des Beckens in der Welt, Treppe hinunter mit 2,0, 3,0 und 4,5 m/s | 1,22, 2,52 und 3,22 cm | 1,07, 2,26 und 2,88 cm |
| 12. Sprung des Beckens in der Welt mit 4,5 m/s, eben und eine Rampe von 20° hinauf | 1,61 und 4,10 cm | 1,00 und 1,30 cm |

Rennend sieht die Gangplanung bis 0,16 s voraus, `NB_RUN_DESCEND`, wie tief die Wurzel für das landende Bein sein muss:
für jedes Update bis zur Landung dort, wo der Fuß auf seinem geplanten Weg dann ist, mit seinem Hub, der Bewegung der
Kapsel und dem Wippen und der Drehung des Beckens in diesem Update, und für den Landepunkt selbst. Jeder Bedarf zählt
mit einer weichen Rampe, die über die 0,16 s bis zu seinem Update von 0 auf 1 steigt, und mit einer zweiten, die ab 0,4
des Schwungs steigt. So geht die Wurzel gleichmäßig tiefer, statt in zwei Updates zu fallen. Ohne die zweite Rampe
zählte auch ein Fuß, der früh im Schwung noch weit hinter der Hüfte ist, und das Becken sprang 17 bis 39 cm. Senkte die
Gangplanung die Wurzel nur für den Landepunkt, ab 0,4 des Schwungs, sprang das Becken nur 2,6 und 9,5 mm, aber das
landende Knie mit 3,0 m/s 5,4 statt 4,1 cm und mit 4,8 m/s ein Knochen beim Abheben der Ferse 4,9 cm. Begann die zweite
Rampe später, bei 0,55 bis 0,75 des Schwungs, sprang das Becken mit 4,8 m/s wieder 10 bis 13 mm, und stieg die erste ab
0,55 des Schwungs bis zur Landung statt über die letzten 0,16 s, sprang das Knie mit 3,0 m/s 5,4 cm, und die Wurzel war
beim Aufsetzen zu tief.

In der zweiten Hälfte des Schwungs hebt sich der Fuß rennend nur um den Bogen mal 1 − 0,3 · (1 − Bogen),
`NB_RUN_LAND_SOFT`: Er setzt mit 70 % seines Tempos auf, und das landende Bein braucht die Wurzel in den letzten Updates
weniger tief. Mit 0,5 und 0,7 ging das Becken mit 4,8 m/s nur noch 3,6 bis 4,5 und 3,1 bis 4,2 cm auf und ab, und das
Knie war beim Aufsetzen bis 33 und 36° gebeugt. Ein Hub, der mit dem Quadrat des Bogens landet, so dass der Fuß ohne
Tempo aufsetzt, ließ das Becken 17,5 und 27,7 mm springen, und mit 0,15 und 0,2 lagen die Werte näher an den Grenzen.

Ein Fuß hinter der Hüfte rollt rennend schon so weit über die Zehen, wie sein Bein in den nächsten 0,08 s brauchen wird,
`NB_FOOT_ROLL_AHEAD`, mit derselben weichen Rampe und mit dem Wippen und der Drehung des Beckens bis dahin. So hebt sich
die Ferse, bevor das Bein gestreckt ist, statt auf einmal, wenn das Knie am Ende des Stands anhält. Mit 0,12 s beugte
sich das Knie im Stand bis 61 bis 63°, und hörte die Vorschau auf, sobald der Fuß ganz gerollt sein müsste, sprang er
beim Abheben 7,4 cm.

Rennend dreht das Becken um 0,09 rad zu jeder Seite, `NB_RUN_PELVIS_TURN`, 5,2°, die Seite des vorderen Beins voraus,
gehend wie bisher um 0,07 rad, und dazwischen nach dem, wie weit die Figur rennt. Mit der Brust, die gegenläufig um
12,2° dreht, sind es zwischen Becken und Brust ±17,5°. Mit 0,06 und 0,12 rad hielten alle Bedingungen ebenso. Wie weit
die Figur rennt, nehmen der Abstieg und die Rolle aus der Feder von `NB_RUN_TURN_LAG` wie die Drehungen in Runde 90: Mit
dem Anteil am Rennen direkt aus dem Tempo waren es in den 48 Läufen 24 statt 23 Updates über den Grenzen, und beim
Wechsel des Tempos sprang das Becken 11,6 statt 8,8 mm. Gehend mit 2,0 m/s, wo der Übergang zum Rennen schon beginnt,
springt ein Schienbein so 19,1 statt 24,3 mm, und die Figur hält dort eine Grenze von Runde 87 mehr.

Mit 4,8 m/s hält die Figur die dritte Bedingung nicht. Auf `active-ragdoll-90` ging das Becken dort 6,8 bis 7,5 cm auf
und ab, aber die Wurzel stieg noch, als das landende Bein seinen Fuß schon nicht mehr erreichte, und fiel dann in zwei
Updates. Jetzt geht sie vor der Landung so tief, wie das Bein es braucht, und kommt im Flug nicht mehr so hoch: 4,2 bis
4,8 cm. Mehr Wippen, 0,04 statt 0,035 von `NB_GAIT_BOB_RUN`, gab nur 4,6 bis 5,5 cm und beugte das Knie im Stand bis
63°, und lag der tiefste Punkt statt in der Mitte des Stands dort, wo die Hüfte über den Fuß geht, bei 0,35 bis 0,45 des
Stands, sprangen Ferse und Fuß beim Abheben 6,5 bis 7,4 cm. Nebenbei streckt sich das Knie mit 3,0 m/s am Ende des
Stands nur noch bis 33 statt 23°, das war keine Bedingung.

Wo es noch fehlt: Beim Losrennen und beim Anhalten aus dem Rennen springen Füße und Hände wie auf `active-ragdoll-90`,
losrennend ein Fuß 6,4 cm, eine Hand 9,0 cm und das Becken 2,1 cm in einem Update, anhaltend eine Hand 12,8 cm. Mit 4,8
m/s wippt das Becken weniger, als die dritte Bedingung verlangt. Was nicht ging, steht unter „Rennen ohne Ruck“ in
[Optimierungen](Optimierungen.md). Eine gesteuerte Figur kostet im Update der Figuren auf der VM 9,8 und 9,8 statt 9,1
und 8,9 µs, mit 20 Figuren in zweimal acht Läufen im Wechsel, gepaart je 0,9 µs mehr, eine stehende 1,5 µs wie vorher,
sie rechnet denselben Code. Im Benchmark bleiben alle Hashes. Er hat keine gesteuerte Figur und rechnet denselben Code:
Die Bildzeiten der Stadt liegen im Wechsel bei 1,00 bis 1,02 von `active-ragdoll-90`, die Schritte der drei Szenen mit
Einschlägen bei 0,99 bis 1,02.

Losrennen und Anhalten ohne Ruck, Runde 92, F84, auf Wunsch des Nutzers. Den Vorschlag nach Runde 91, die Sprünge beim
Losrennen und Anhalten zu beheben, nahm er mit „ok“ an: Kein Knochen sollte dabei stärker springen als im gleichmäßigen
Rennen, dort bis 3,7 cm in einem Update, das Becken höchstens 1 cm. Auf `active-ragdoll-91` sprangen beim Anhalten aus
4,8 m/s die Hände bis 14,1 cm in einem Update und das Becken bis 4,7 cm, beim Losrennen aus dem Stand ein Fuß 6,4 cm.

Wie weit die Figur rennt, kommt aus dem Tempo, und beim Losrennen und Anhalten geht es in drei bis vier Updates von 0
auf 1 oder zurück: Mit 10 m/s² dauert der Weg durch die Spanne zwischen Gehen und Rennen, 1,9 bis 2,45 m/s, auch hinter
der Feder von 1/6 s, mit der das Tempo des Gangs dem der Kapsel folgt, nur rund 0,05 s. Daran hingen die Beugung der
Ellbogen, gehend 0,2 und rennend 1,3 rad, wie tief die Wurzel rennend steht, das Wippen und wo im Takt es am tiefsten
ist, der Abstieg des Gehens vor einer Landung und Höhe und Form des Hubs. Beim Anhalten aus 4,8 m/s schaltete so in zwei
Updates der Abstieg des Gehens zu, 3,2 cm, und das Wippen brach weg, 2,0 cm: Die Wurzel fiel 5,8 cm in einem Update.

Vorher festgelegt, nach der Messung von `active-ragdoll-91` und der Suche nach den Ursachen mit Varianten, vor dem Bau
in der Bibliothek, an 56 Läufen: Losrennen aus dem Stand auf 4,8 und 3,0 m/s, von 1,6 auf 4,8 m/s, Anhalten aus 4,8 und
3,0 m/s, von 4,8 auf 1,6 m/s und eine Wendung um 90° mit 4,8 m/s, jeweils an acht Stellen im Gang, gemessen von 0,1 s
vor dem Wechsel bis 3 s danach, bei 60 Hz mit 4 Teilschritten und gesteuert wie in der Demo. Losrennen und Anhalten: 1.
Keine Hand springt mehr als 3,7 cm in einem Update. 2. Das Becken springt höchstens 1,0 cm. 3. Kein Fuß und kein anderer
Knochen springt mehr als 3,7 cm, so viel wie höchstens im gleichmäßigen Rennen mit 3,0 m/s. Unverändert: 4. Die 48 Läufe
der Übergänge im Gehen wie in Runde 91. 5. Gehend geradeaus springt kein Knochen mehr als 0,3 cm und das Becken nicht
mehr als 0,1 cm stärker. 6. Im gleichmäßigen Rennen mit 3,0 und 4,8 m/s ebenso, an acht Stellen des Takts gegen die
Updates, und Wippen und Knie bleiben. 7. Die Bedingungen 1 bis 7 von Runde 90 und D1 und D2 von Runde 91 bleiben
erfüllt. 8. Die Bedingungen 3 bis 8 von Runde 87 bleiben. 9. Treppen und Rampen wie in Runde 91. 10. Ein Fuß, der steht,
schwebt in keiner Szene mehr als 0,3 cm höher über dem Boden. 11. Alle Tests laufen, Figuren ohne `steered` bleiben
bitgleich, gesteuerte mit 1, 2 und 4 Threads. 12. Eine gesteuerte Figur kostet höchstens 1 µs mehr.

Die Zahlen kommen vom Menschen. Sein Schwerpunkt fällt nicht schneller als mit g, bei 60 Hz 2,7 mm pro Update², und der
Boden drückt ihn mit bis zum Zweieinhalbfachen des Gewichts hoch, rund 4 mm pro Update², siehe Runde 91: 1 cm ist schon
großzügig. Ein Läufer beugt die Ellbogen um rund 90°, ein Gehender wenig, und beim Losrennen kommt die Beugung mit den
ersten Schritten, nicht in einem Update.

Erfüllt sind 1 und 4 bis 12, nicht 2 und 3: Das Becken springt beim Anhalten aus 4,8 m/s noch bis 2,9 cm, ein Fuß beim
ersten Schritt aus dem Stand 6,4 cm und ein Schienbein bis 6,6 cm. Mit demselben Messprogramm auf beiden Ständen,
jeweils der größte Sprung in einem Update über die acht Stellen, siehe auch `SteerStartStopTest` in
`test/test_figure.c`:

| `active-ragdoll-91` → `active-ragdoll-92` | Hände | Becken | Füße | andere Knochen |
| --- | --- | --- | --- | --- |
| Losrennen aus dem Stand auf 4,8 m/s | 9,0 → 2,2 cm | 2,1 → 1,7 cm | 6,4 → 6,4 cm | 7,4 → 6,6 cm |
| Losrennen aus dem Stand auf 3,0 m/s | 4,9 → 1,9 cm | 1,8 → 1,8 cm | 6,4 → 6,4 cm | 6,6 → 6,6 cm |
| Von 1,6 auf 4,8 m/s | 6,1 → 2,2 cm | 2,7 → 1,6 cm | 4,2 → 4,2 cm | 6,5 → 6,4 cm |
| Anhalten aus 4,8 m/s | 14,1 → 3,1 cm | 4,7 → 2,9 cm | 5,6 → 3,5 cm | 7,1 → 5,1 cm |
| Anhalten aus 3,0 m/s | 8,4 → 2,8 cm | 4,0 → 2,7 cm | 3,8 → 2,4 cm | 4,7 → 4,1 cm |
| Von 4,8 auf 1,6 m/s | 3,5 → 1,4 cm | 1,4 → 0,7 cm | 3,5 → 3,4 cm | 5,3 → 5,3 cm |
| Wendung um 90° mit 4,8 m/s | 2,4 → 2,4 cm | 2,0 → 2,0 cm | 4,4 → 4,4 cm | 6,5 → 6,5 cm |

Wie weit die Figur rennt, folgt seit Runde 90 auch einer kritisch gedämpften Feder von 0,2 s, `NB_RUN_TURN_LAG`, an der
bisher die Drehungen von Becken und Brust hingen und seit Runde 91 der Abstieg des Rennens vor einer Landung und die
Rolle voraus. Jetzt hängen an ihr auch die Beugung der Ellbogen und wie sie schwingen, das Pendeln des Beckens, das
Wippen und wo im Stand es am tiefsten ist, und die Höhe, bis zu der ein Fuß abhebt. Wie tief die Wurzel rennend steht,
kommt mit der Feder und geht mit dem Tempo: Wird die Wurzel weniger tief, kommt sie ohnehin wie nach einer Landung mit
einer Feder hoch, siehe `nbLowerSteered`. Der Abstieg des Gehens vor einer Landung kommt umgekehrt mit der Feder und
geht mit dem Tempo. Die Form seines Hubs behält ein schwingender Fuß von dort, wo er abhob, und geht in der zweiten
Hälfte des Schwungs zu der über, die der Gang jetzt hat, `nbLiftShare`: So wechselt er sie nicht mitten im Schwung und
landet doch, wie der Gang jetzt landet. Der Takt der Schritte, wie lange ein Fuß steht und wo er landet, folgt weiter
dem Tempo.

Den Takt aus der Feder zu nehmen, ging nicht: Rennend und mit dem Anteil des Gehens am Stand standen die Füße 1,5 m
auseinander, und die Figur ging bis auf 42 cm Höhe des Beckens in die Hocke. Eine Wurzel, die höchstens mit g fällt und
mit dem Anderthalbfachen steigt, ließ das Becken höchstens 0,6 cm springen, aber die Knie bis 16 cm, weil die Beine die
Füße nicht mehr erreichten, mit 2 und 3 g das Becken 0,9 cm und die Knie bis 9,2 cm. Folgte auch die Form des Hubs der
Feder, sprang das Becken beim Anhalten aus 4,8 m/s nur 1,1 cm, aber beim Anhalten aus 3,0 m/s fiel ein Fuß beim Landen
aus 2,8 statt 1,6 cm in einem Update auf den Boden, gegen die zehnte Bedingung, ebenso, wenn ein Fuß die Form, mit der
er abhob, den ganzen Schwung behielt. Ein Wippen, dessen Lage im Takt fest bei einem Drittel des Stands liegt, ließ
gehend mit 2,0 m/s ein Schienbein 22,4 statt 19,1 mm springen. Mehr Rollen über die Zehen beim Rennen, 0,85 bis 1,2
statt 0,7 rad, ein Hub, der rennend ohne Steigung beginnt, und eine Wurzel, die vor dem Abheben eines Fußes
vorausschauend tiefer geht, machten das gleichmäßige Rennen oder die Wendung unruhiger.

Wo es noch fehlt: Beim ersten Schritt aus dem Stand zieht die Kapsel mit 10 m/s² davon, der schwingende Fuß jagt seinem
Ziel nach, kommt mit 7 m/s an und hält in zwei Updates an, 6,4 cm. Kurz nach einem Wechsel zum Rennen steht die Wurzel
am Ende des Stands bis 1,7 cm höher als später, der Fuß bräuchte mehr als 0,7 rad Rollen, und das gestreckte Bein lässt
beim Abheben das Knie bis 6,6 cm springen oder die Wurzel in einem Update 1,9 cm fallen. Beim Anhalten springt das
Becken noch bis 2,9 cm, wo Wippen und Abstiege des Rennens und des Gehens ineinander übergehen. Was nicht ging, steht
unter „Losrennen und Anhalten ohne Ruck“ in [Optimierungen](Optimierungen.md). Eine gesteuerte Figur kostet im Update
der Figuren auf der VM 9,8 und 9,7 statt 9,9 und 9,8 µs, mit 20 Figuren in zweimal acht Läufen im Wechsel, gepaart 0,08
und 0,04 µs weniger, eine stehende 1,5 statt 1,6 µs, sie rechnet denselben Code. Im Benchmark bleiben alle Hashes. Er
hat keine gesteuerte Figur und rechnet denselben Code: Die Bildzeiten der Stadt liegen im Wechsel bei 0,97 bis 1,01 von
`active-ragdoll-91`, die Schritte der drei Szenen mit Einschlägen bei 0,98 bis 1,01, bis auf einen Lauf mit dem Gewehr,
in dem die VM bei denselben Bruchstücken 2,0-mal so lang brauchte.

Erster Schritt und Abheben ohne Ruck, Runde 93, F84, auf Wunsch des Nutzers. Den Vorschlag nach Runde 92 nahm er an:
Kein Knochen sollte beim Losrennen, Anhalten und in den anderen Wechseln stärker springen als im gleichmäßigen Rennen,
bis 3,7 cm in einem Update, das Becken höchstens 1 cm, und alles Erreichte sollte bleiben. Auf `active-ragdoll-92`
sprang beim ersten Schritt aus dem Stand ein Fuß 6,4 cm in einem Update und ein Schienbein 6,6 cm, beim Anhalten aus 4,8
m/s das Becken bis 3,6 cm.

Gemessen wird jetzt an 24 statt acht Stellen im Gang: Acht Stellen haben die schlimmsten Fälle unterschätzt, beim
Anhalten aus 4,8 m/s springen auf `active-ragdoll-92` die Hände 3,9 statt 3,1 cm und das Becken 3,6 statt 2,9 cm. Die
Ursachen: Beim ersten Schritt aus dem Stand zielt der Fuß mit der Geschwindigkeit, die der Gang beim Abheben hat, fast
null, und mit dem Vorlauf des Gehens. Die Kapsel beschleunigt mit 10 m/s² und zieht davon, der Fuß jagt seinem Ziel mit
bis 7 m/s nach und hält in zwei Updates an. Der Abstoß des Gehens springt beim Losrennen, weil der Takt kürzer wird und
die Phase eines Fußes auf einmal tief im Abstoß liegt. Wechselt der Gang mitten im Schwung vom Rennen zum Gehen, wird
der Anteil des Schwungs am Zyklus kleiner, 0,4 statt 0,67, und ein Schwung, der rennend abhob, endet auf einmal früher.
Ein bis zwei Sekunden nach einem Wechsel zum Rennen rollt ein Fuß vor dem Abheben bis an den Rand von `NB_FOOT_ROLL`,
0,7 rad, und das Bein erreicht ihn im letzten Update nicht mehr. Und im gleichmäßigen Rennen heben die Füße ab, weil ihr
Bein sie verliert, ein Update bevor ihre Phase es sagt. Nichts bringt sie zum Takt zurück: Nach einem Wechsel laufen
links und rechts so ungleich weiter, wie der Wechsel sie ließ, mit 4,8 m/s bis 17 zu 22 Updates, die Figur hinkt, und
das Wippen, das am Takt hängt, wandert gegen die Füße, um 0,006 eines Takts pro Schritt. Das am stärksten gebeugte Knie
im Stand liegt so zwischen 48,9 und 60,3°.

Vorher festgelegt, nach der Messung von `active-ragdoll-92` und der Suche nach den Ursachen mit Varianten, vor dem Bau
in der Bibliothek, an 168 Läufen: die sieben Szenen von Runde 92 an 24 Stellen im Gang, von 0,1 s vor dem Wechsel bis 3
s danach, und das gleichmäßige Rennen mit 3,0 und 4,8 m/s von 3 bis 8 s nach dem Wechsel von 1,6 m/s, an 24 Stellen des
Takts gegen die Updates, bei 60 Hz mit 4 Teilschritten und gesteuert wie in der Demo. Wechsel: 1. Keine Hand springt
mehr als 3,7 cm in einem Update. 2. Das Becken springt höchstens 1,0 cm. 3. Kein Fuß und kein anderer Knochen springt
mehr als 3,7 cm. 4. Im gleichmäßigen Rennen wechseln die Füße an jeder der 24 Stellen gleichmäßig: Die Updates von einem
Aufsetzen zum nächsten des anderen Fußes sind nach beiden Seiten zusammen höchstens zwei benachbarte Zahlen, und das am
stärksten gebeugte Knie im Stand liegt über alle Schritte in 2°. Unverändert: 5. Die 48 Läufe der Übergänge im Gehen wie
in Runde 92. 6. Gehend geradeaus springt kein Knochen mehr als 0,3 cm und das Becken nicht mehr als 0,1 cm stärker. 7.
Im gleichmäßigen Rennen ebenso, an den 24 Stellen und im Lauf von Runde 90, das Wippen bleibt, und das Knie beim
Aufsetzen und im Stand liegt im Mittel über die Schritte auf 2° wie vorher. 8. Die Bedingungen 1 bis 7 von Runde 90 und
D1 und D2 von Runde 91 bleiben erfüllt. 9. Die Bedingungen 3 bis 8 von Runde 87 bleiben. 10. Treppen und Rampen wie in
Runde 92. 11. Ein Fuß, der steht, schwebt ab seinem zweiten Update am Boden in keiner Szene mehr als 0,3 cm höher, und
im ersten kommt er in keiner Szene aus mehr als 0,3 cm über dem Höchsten von `active-ragdoll-92` herab, 2,3 cm. 12. Alle
Tests laufen, Figuren ohne `steered` bleiben bitgleich, gesteuerte mit 1, 2 und 4 Threads. 13. Eine gesteuerte Figur
kostet höchstens 1 µs mehr.

Die vierte Bedingung kam mit der Suche nach den Ursachen dazu: Die Varianten für den ersten Schritt ließen die Figur
nach manchem Wechsel stärker hinken, mit 4,8 m/s bis 23 zu 16 Updates und das Knie im Stand bis 43°. Die siebte mittelt
das Knie über die Schritte, weil es auf `active-ragdoll-92` mit dem wandernden Wippen von Schritt zu Schritt fällt, in
3,5 s um 2°. Und was Runde 92 als Schweben eines stehenden Fußes zählte, bis 2,3 cm, ist das erste Update am Boden: Die
Knochen folgen dem Plan ein Update später und zeigen den Fuß noch dort, wo ihn sein Schwung im Update davor ließ. So
weit kommt er im letzten Update herab, mit 70 % des Tempos seines Bogens, `NB_RUN_LAND_SOFT`, je nachdem, wo zwischen
zwei Updates der Schwung endet. Ab dem zweiten Update schwebt kein Fuß höher als 0,1 mm. Die elfte Bedingung trennt
beides. Nach der zehnten von Runde 92, je Szene, hätte das Herabkommen sie in vier Szenen verfehlt, siehe unten.

Die Zahlen kommen vom Menschen, wie in Runde 92: Sein Schwerpunkt fällt nicht schneller als mit g, bei 60 Hz 2,7 mm pro
Update², und der Boden drückt ihn mit bis zum Zweieinhalbfachen des Gewichts hoch, rund 4 mm pro Update², 1 cm ist schon
großzügig. Ein Mensch rennt gleichmäßig, seine Schritte links und rechts dauern bis auf wenige Prozent gleich lang, und
mit 4,8 m/s ist ein Update schon 5 % eines Schritts. Sein Knie im Stand ändert sich von Schritt zu Schritt kaum.

Erfüllt sind 1 und 4 bis 13, nicht 2 und 3: Das Becken springt beim Anhalten aus 4,8 m/s noch bis 2,1 cm, aus 3,0 m/s
2,0 cm und beim Wechsel von 1,6 auf 4,8 m/s 1,8 cm, ein Fuß beim ersten Schritt aus dem Stand 4,0 cm und ein Schienbein
beim Wechsel von 1,6 auf 4,8 m/s bis 5,3 cm. Mit demselben Messprogramm auf beiden Ständen, jeweils der größte Sprung in
einem Update über die 24 Stellen, siehe auch `SteerStartStopTest` und `SteerEvenRunTest` in `test/test_figure.c`:

| `active-ragdoll-92` → `active-ragdoll-93` | Hände | Becken | Füße | andere Knochen |
| --- | --- | --- | --- | --- |
| Losrennen aus dem Stand auf 4,8 m/s | 2,2 → 2,2 cm | 1,7 → 1,5 cm | 6,4 → 4,0 cm | 6,6 → 4,2 cm |
| Losrennen aus dem Stand auf 3,0 m/s | 1,9 → 1,0 cm | 1,8 → 0,7 cm | 6,4 → 4,0 cm | 6,6 → 4,2 cm |
| Von 1,6 auf 4,8 m/s | 2,6 → 1,9 cm | 2,4 → 1,8 cm | 4,3 → 3,3 cm | 6,5 → 5,3 cm |
| Anhalten aus 4,8 m/s | 3,9 → 2,9 cm | 3,6 → 2,1 cm | 3,8 → 3,4 cm | 5,4 → 4,8 cm |
| Anhalten aus 3,0 m/s | 2,8 → 2,4 cm | 2,7 → 2,0 cm | 2,8 → 2,4 cm | 4,1 → 3,7 cm |
| Von 4,8 auf 1,6 m/s | 1,6 → 1,5 cm | 0,8 → 0,7 cm | 3,9 → 3,0 cm | 5,3 → 4,6 cm |
| Wendung um 90° mit 4,8 m/s | 3,0 → 2,3 cm | 2,5 → 0,9 cm | 4,4 → 3,1 cm | 6,5 → 4,1 cm |

Im gleichmäßigen Rennen mit 4,8 m/s wechseln die Füße an allen 24 Stellen zwischen 19 und 20 Updates, auf
`active-ragdoll-92` von 17 zu 22 bis 21 zu 18. Das am stärksten gebeugte Knie im Stand liegt bei 60,4 bis 60,9° statt
48,9 bis 60,3°, und in 20 s Rennen wandert der Takt nicht mehr gegen die Füße: Ein Fuß hebt immer bei derselben Phase
ab, auf 0,012 eines Takts genau, auf `active-ragdoll-92` wanderte diese Phase in derselben Zeit um 0,17. Ein Knochen
springt dort höchstens 2,7 statt 3,3 cm, das Becken 3,8 statt 3,6 mm, und das Becken geht 4,9 bis 5,1 statt 4,2 bis 4,8
cm auf und ab. Mit 3,0 m/s liefen die Füße schon gleichmäßig, dort bleibt alles. Die Übergänge im Gehen bleiben, 21
statt 23 Updates über den Grenzen, je Übergang höchstens 1 mm schlechter und das Becken 1 mm tiefer, ebenso das Gehen
geradeaus, die Neigung und Drehung von Runde 90 und die Treppen und Rampen: Rennend die Treppe hinunter geht das Becken
mit 3,0 m/s gegen die Linie der Treppe 4,8 statt 16,8 cm auf und ab, mit 4,5 m/s aber 17,4 statt 14,0 cm, und der zweite
Schritt nimmt dort fünf statt drei Stufen. In der Welt springt das Becken dabei wie vorher höchstens 2,75 cm, und keine
Sohle steckt in einer Stufe. Ein Fuß kommt im letzten Update vor dem Aufsetzen jetzt auch beim ersten Schritt aus dem
Stand und im gleichmäßigen Rennen mit 4,8 m/s aus 2,2 und 2,3 statt 0,7 cm herab, beim Anhalten aus 4,8 m/s und von 4,8
auf 1,6 m/s aus 2,1 statt 1,8 cm. Mit 4,8 m/s endete der Schwung auf `active-ragdoll-92` zufällig immer kurz nach einem
Update, jetzt endet er, wo die Phase des Fußes ihn landen lässt. Aus 2,0 bis 2,3 cm kam ein Fuß auch auf
`active-ragdoll-92` mit 3,0 m/s, in der Wendung und beim Wechsel von 1,6 auf 4,8 m/s schon herab, mit bis 1,4 m/s.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet` und `nbRollToReach`. Ein Fuß hinter der Hüfte rollt über die
Zehen, so weit sein Bein es braucht, bis `NB_FOOT_ROLL_MOST`, 1 rad. Wann er abhebt, entscheidet die Figur weiter mit
0,7 rad, `NB_FOOT_ROLL`, so erreicht ihn sein Bein auch im letzten Update am Boden. Anders als „Mehr Rollen beim Rennen“
in Runde 92 hebt er also nicht später ab. Losrennend plant ein schwingender Fuß mit der Beschleunigung der Kapsel: Er
zielt dorthin, wo sie beim Landen sein wird, wenn sie bis zur Geschwindigkeit, die das Spiel will, so weiter
beschleunigt, `nbFrameAhead`, und landet nicht weiter voraus, als der Gang bei dem Tempo landen würde, mit dem er zielt,
`nbGaitLead`. So plant er, sobald der Gang mehr als halb rennt, `NB_GAIT_PLAN_FROM`, oder die Figur schneller gesteuert
wird, als Menschen vom Gehen ins Rennen wechseln, voll 0,1 darüber, `NB_GAIT_PLAN_SPAN`, und nicht auf Treppen. Ebenso
geht die Hüfte gehend vor dem Landen so tief, wie das Bein den Fuß dort braucht, wo die Kapsel dann ist. Gehend plant
die Figur wie vorher. Der Abstoß des Gehens kommt nicht schneller als über die Vorschwungphase im Takt,
`NB_GAIT_PRESWING`, auch wo der Takt beim Losrennen kürzer wird. Ein Schwung dauert mindestens den Anteil des Zyklus,
mit dem er abhob, `nbSwingTime`, so endet ein Schwung, der rennend abhob, nicht auf einmal früher, wenn der Gang zum
Gehen wechselt oder die Figur anhält. Und rennend hält sich ein Fuß an den Takt: Hebt er ab, bevor seine Phase es sagt,
weil sein Bein ihn verliert, schwingt er so viel langsamer und landet, wenn seine Phase kommt, höchstens
`NB_GAIT_KEEP_MOST`, 0,3 eines Zyklus, später, so wie ein Fuß, der zu spät abhebt, schon schneller schwang. Gehend holt
wie bisher der Takt die Füße ein, `NB_GAIT_KEEP`.

Was nicht ging, steht unter „Erster Schritt und Abheben ohne Ruck“ in [Optimierungen](Optimierungen.md). Am meisten
hätte ein Vorlauf gebracht, der aus dem Tempo beim Landen kommt statt aus dem jetzigen: Beim ersten Schritt aus dem
Stand sprang ein Fuß dann 2,6 statt 4,0 cm und ein Schienbein 2,9 statt 4,2 cm, aber rennend auf der Treppe sank die
Figur 15,7 cm ein, gegen die vierte Bedingung von Runde 86. Jeder der fünf gebauten Teile trägt etwas bei, zwei mit
einem Preis: Ohne das Planen mit der Beschleunigung, nur mit dem begrenzten Vorlauf, spränge beim ersten Schritt aus dem
Stand ein Fuß nur 2,7 statt 4,0 cm, beim Anhalten aus 4,8 m/s aber das Becken 3,1 statt 2,1 cm und beim Wechsel von 1,6
auf 4,8 m/s ein Fuß 4,4 statt 3,3 cm. Ohne den Erhalt des Anteils am Zyklus spränge das Becken beim Anhalten 1,7 und 1,8
statt 2,1 und 2,0 cm, aber beim Wechsel von 4,8 auf 1,6 m/s ein Fuß 4,4 statt 3,0 cm, mehr als auf `active-ragdoll-92`.

Wo es noch fehlt: Beim ersten Schritt aus dem Stand erreicht der Fuß noch 6,3 m/s und bremst in drei Updates mit der
Grenze von `NB_GAIT_SWING_ACCEL`, 120 m/s², so springen ein Fuß 4,0 und ein Schienbein 4,2 cm. Beim Wechsel von 1,6 auf
4,8 m/s springt ein Schienbein bis 5,3 cm, beim Anhalten aus 4,8 m/s bis 4,8 cm, und das Becken springt beim Anhalten
noch 2,0 bis 2,1 cm, wo Wippen und Abstiege des Rennens und des Gehens ineinander übergehen. Eine gesteuerte Figur
kostet im Update der Figuren auf der VM 10,1 und 10,3 statt 10,2 und 9,8 µs, mit 20 Figuren in zweimal acht Läufen im
Wechsel, gepaart 0,07 µs weniger und 0,30 µs mehr, eine stehende 1,5 µs wie vorher, sie rechnet denselben Code. Im
Benchmark bleiben alle Hashes. Er hat keine gesteuerte Figur und rechnet denselben Code: Bildzeiten und Schritte liegen
im Wechsel bei 0,91 bis 1,11 von `active-ragdoll-92`, im Rauschen der VM.

Wechsel ganz ohne Ruck, Runde 94, F84, auf Wunsch des Nutzers. Den Vorschlag nach Runde 93 nahm er an: Das Becken sollte
beim Anhalten höchstens 1 cm in einem Update springen, kein Fuß und kein Schienbein mehr als 3,7 cm, auch nicht beim
ersten Schritt aus dem Stand, und rennend mit 4,5 m/s die Treppe hinunter sollte das Becken höchstens 14 cm wippen, mit
gleich vielen Stufen in jedem Schritt. Alles Erreichte sollte bleiben.

Die Ursachen, gemessen mit Varianten auf `active-ragdoll-93`: Wie weit der Gang rennt, kommt aus seinem Tempo, und das
fällt beim Bremsen mit 14 m/s² in drei Updates durch das Band von 1,9 bis 2,5 m/s, in dem der Gang vom Rennen zum Gehen
wechselt. Die Form des Hubs folgte in der zweiten Hälfte des Schwungs diesem Wert, `nbLiftShare`: Der geplante Fuß fiel
spät im Schwung in zwei Updates um 3 cm, der Abstieg des Rennens vor dem Landen, `NB_RUN_DESCEND`, sah ihn tiefer, die
Hocke sprang 1,7 cm und zurück, und das Becken sprang beim Anhalten aus 4,8 m/s 2,1 cm. Beim Losrennen ging ebenso der
Abstieg des Gehens mit dem Tempo in wenigen Updates, während der des Rennens mit der Feder von `NB_RUN_TURN_LAG` erst
kam. Beim Wechsel von 1,6 auf 4,8 m/s rollte ein Fuß hinter der Hüfte erst über die Zehen, wenn sein Bein ihn nicht mehr
erreichte, denn das Vorausrollen von Runde 91, `NB_FOOT_ROLL_AHEAD`, galt nur rennend und ging mit der Feder, die beim
Wechsel noch bei 0 ist. Die Kapsel beschleunigt davon, das Rollen setzte in einem Update mit 5 rad/s ein, und ein
Schienbein sprang bis 5,3 cm. Beim Aufsetzen nach dem Anhalten kommt mit dem Tempo auf einmal die Ferse des Gehens, die
Zehen gehen in einem Update 0,14 rad hoch, und ein Schienbein springt 4,7 cm. Beim ersten Schritt aus dem Stand läuft
das Ziel des Fußes erst bis 1,05 m voraus und kommt in den letzten fünf Updates auf 0,71 m zurück, weil der Vorlauf in
Sekunden im Schwung schrumpft, während der Gang vom Gehen ins Rennen wechselt. Der Fuß erreicht 6,3 m/s und bremst an
der Grenze, 4,0 cm.

Rennend auf einer Treppe hing alles davon ab, wie die Figur an ihr ankam. Mit dem Start um 0 bis 0,7 m nach hinten
verschoben stand rennend hinauf mit 3,0 m/s bei vier und mit 4,5 m/s bei zwei von acht Starts ein Fuß neben seiner Stufe
und steckte bis 11 und 15 cm in der nächsten: Die Figur sucht den Boden am Ziel eines Fußes von 0,4 m über der Höhe, die
sie dort erwartet, `NB_GROUND_ABOVE`, und das erste Ziel auf der Treppe liegt bis 0,45 m höher, solange die Kapsel noch
eben läuft. Hinunter mit 4,5 m/s nahmen die Schritte je nach Start drei oder fünf Stufen, und das Becken ging gegen die
Linie der Treppe 13 bis 21 cm auf und ab: Ein Schritt will dort drei Stufen, `NB_STAIR_CLIMB`, aber `nbFindFoothold`
stellte den Fuß nur auf diese Stufe, wenn sie die nächste vor oder hinter der war, die der Gang trifft. Auf
`active-ragdoll-92` war es genauso, hinunter mit 3,0 m/s bis 28 cm. Was Runde 93 als schlechter beim Hinunterrennen mit
4,5 m/s nannte, 17,4 statt 14,0 cm, ist einer dieser Starts.

Vorher festgelegt, nach der Messung von `active-ragdoll-93` und der Suche nach den Ursachen mit Varianten, vor dem Bau
in der Bibliothek, bei 60 Hz mit 4 Teilschritten und gesteuert wie in der Demo. Wechsel, die sieben Szenen von Runde 93
an 24 Stellen im Gang, 168 Läufe von 0,1 s vor dem Wechsel bis 3 s danach: 1. Das Becken springt höchstens 1,0 cm in
einem Update. 2. Kein Fuß und kein anderer Knochen außer den Händen springt mehr als 3,7 cm. 3. Keine Hand springt mehr
als 2,9 cm. Treppen von 15 × 30 cm, rennend mit 3,0 und 4,5 m/s hinauf und hinunter, der Start um 0 bis 0,7 m in
Schritten von 0,1 m nach hinten verschoben, 32 Läufe: 4. Jeder Fuß steht beim Landen mit mindestens 0,8 seiner Sohle auf
seiner Stufe und steckt nicht tiefer als 1,5 cm in einer. 5. Mit 4,5 m/s hinunter nimmt von der dritten Landung bis zur
vorletzten jeder Schritt gleich viele Stufen, und das Becken geht gegen die Linie der Treppe höchstens 14 cm auf und ab.
Unverändert: 6. Die 48 Läufe der Übergänge im Gehen kommen auf höchstens 23 Updates über den Grenzen der ersten
Bedingung von Runde 89, je Übergang springt kein Fuß, anderer Knochen oder Becken mehr als 0,3 cm stärker, das Becken
geht nicht mehr als 0,3 cm tiefer, und kein Lauf kommt aus dem Takt. 7. Gehend geradeaus mit 1,0, 1,6 und 2,0 m/s
springt kein Knochen mehr als 0,3 cm und das Becken nicht mehr als 0,1 cm stärker. 8. Im gleichmäßigen Rennen mit 3,0
und 4,8 m/s ebenso, an den 24 Stellen und im Lauf von Runde 90, die Füße wechseln an jeder Stelle gleichmäßig, das Knie
im Stand liegt in 2°, das Becken geht mit 3,0 m/s 6 bis 9 cm und mit 4,8 m/s nicht weniger als vorher auf und ab, und
das Knie beim Aufsetzen und im Stand liegt im Mittel auf 2° wie vorher. 9. Die Bedingungen 1 bis 7 von Runde 90 und D1
und D2 von Runde 91 bleiben erfüllt. 10. Die Bedingungen 3 bis 8 von Runde 87 bleiben. 11. Treppen beim Start von Runde
86: die Bedingungen 1, 2 und 4 von Runde 86, die dritte nirgends um mehr als 1 cm schlechter, und an Rampen von 10 und
20° die Bedingungen 1 und 2 von Runde 85. 12. Ein Fuß, der steht, schwebt ab seinem zweiten Update am Boden nirgends
mehr als 0,3 cm höher, und im ersten kommt er nirgends aus mehr als 2,6 cm herab. 13. Alle Tests laufen, Figuren ohne
`steered` bleiben bitgleich, gesteuerte mit 1, 2 und 4 Threads. 14. Eine gesteuerte Figur kostet höchstens 1 µs mehr.

Die Zahlen kommen vom Menschen wie in Runde 93: Sein Schwerpunkt fällt nicht schneller als mit g, bei 60 Hz 2,7 mm pro
Update², und der Boden drückt ihn mit bis zum Zweieinhalbfachen des Gewichts hoch, rund 4 mm pro Update², 1 cm ist schon
großzügig. 3,7 cm springt ein Knochen im gleichmäßigen Rennen mit 3,0 m/s. Und ein Mensch tritt auf einer Treppe mit der
ganzen Sohle auf eine Stufe und nimmt gleich viele Stufen pro Schritt, wo immer er an ihr ankommt.

Erfüllt sind 3, 4 und 6 bis 14, nicht 1, 2 und 5: Das Becken springt beim Wechsel von 1,6 auf 4,8 m/s noch 1,65 cm und
beim Anhalten aus 4,8 m/s 1,15 cm. Ein Fuß springt beim ersten Schritt aus dem Stand weiter 4,0 cm, ein Schienbein beim
Wechsel von 1,6 auf 4,8 m/s 4,9 cm, beim Anhalten aus 4,8 m/s 4,7 cm, von 4,8 auf 1,6 m/s 4,6 cm, beim Losrennen 4,2 cm
und in der Wendung 4,1 cm. Und rennend mit 4,5 m/s die Treppe hinunter geht das Becken bei einem Start 14,3 cm auf und
ab, mit drei Stufen in jedem Schritt. Mit demselben Messprogramm auf beiden Ständen, jeweils der größte Sprung in einem
Update über die 24 Stellen, siehe auch `SteerStartStopTest` und `SteerRunStairsTest` in `test/test_figure.c`:

| `active-ragdoll-93` → `active-ragdoll-94` | Hände | Becken | Füße | andere Knochen |
| --- | --- | --- | --- | --- |
| Losrennen aus dem Stand auf 4,8 m/s | 2,2 → 1,7 cm | 1,5 → 0,85 cm | 4,0 → 4,0 cm | 4,2 → 4,2 cm |
| Losrennen aus dem Stand auf 3,0 m/s | 0,97 → 1,04 cm | 0,7 → 0,7 cm | 4,0 → 4,0 cm | 4,2 → 4,2 cm |
| Von 1,6 auf 4,8 m/s | 1,9 → 1,8 cm | 1,75 → 1,65 cm | 3,3 → 3,2 cm | 5,3 → 4,9 cm |
| Anhalten aus 4,8 m/s | 2,9 → 2,9 cm | 2,1 → 1,15 cm | 3,4 → 3,4 cm | 4,8 → 4,7 cm |
| Anhalten aus 3,0 m/s | 2,4 → 2,0 cm | 2,0 → 0,85 cm | 2,4 → 2,4 cm | 3,65 → 3,65 cm |
| Von 4,8 auf 1,6 m/s | 1,5 → 1,5 cm | 0,7 → 0,7 cm | 3,0 → 3,05 cm | 4,6 → 4,6 cm |
| Wendung um 90° mit 4,8 m/s | 2,3 → 2,3 cm | 0,85 → 0,85 cm | 3,1 → 3,1 cm | 4,1 → 4,1 cm |

Rennend auf der Treppe steht bei allen 32 verschobenen Starts jeder Fuß beim Landen mit der ganzen Sohle auf seiner
Stufe, auf `active-ragdoll-93` bei sechs nicht. Hinauf mit 4,5 m/s nimmt jeder Schritt drei Stufen, auf
`active-ragdoll-93` bei einem Start fünf. Hinunter mit 4,5 m/s nimmt von der dritten Landung an jeder Schritt drei
Stufen, auf `active-ragdoll-93` bei zwei Starts fünf, und das Becken geht gegen die Linie der Treppe 13,2 bis 14,3 statt
13,2 bis 20,7 cm auf und ab, beim Start von Runde 86 13,6 statt 17,4 cm. Mit 3,0 m/s hinunter bleibt alles, wie es war,
bei drei Starts geht das Becken dort 17,6 bis 18,1 cm auf und ab. Gehen, gleichmäßiges Rennen und die Übergänge im Gehen
bleiben: 21 Updates über den Grenzen wie vorher, je Übergang höchstens 1 mm stärker, das Becken nirgends tiefer, und im
gleichmäßigen Rennen an allen 24 Stellen und im Lauf von Runde 90 jeder Sprung, das Wippen, die Knie und der Wechsel der
Füße wie vorher. Ebenso bleiben die Neigung und Drehung von Runde 90 und die Treppen und Rampen beim Start von Runde 86,
bis auf das Hinunterrennen mit 4,5 m/s. Ein Fuß kommt im ersten Update am Boden wie vorher aus höchstens 2,3 cm herab.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet` und `nbLowerSteered`, und in der Suche nach den Stufen. Die Form
des Hubs in der zweiten Hälfte des Schwungs folgt der Feder von `NB_RUN_TURN_LAG` statt dem Tempo des Gangs,
`nbLiftShare`, wie seine Höhe seit Runde 92, und der Abstieg des Gehens vor dem Landen geht beim Losrennen mit derselben
Feder, wie der Abstieg des Rennens kommt. Ein Fuß hinter der Hüfte rollt schon voraus, sobald der Gang nach seinem Tempo
rennt, nicht erst, wenn die Feder folgt, `NB_FOOT_ROLL_AHEAD`. Die Strahlen und die Kugel, die den Boden am Ziel eines
Fußes suchen, beginnen so viel höher, wie sie tiefer gehen dürfen, `nbMeasureTread` und `nbCastFootGround`, so findet
ein Fuß eine Stufe hinauf auch, solange die Kapsel noch eben läuft. Und auf einer Treppe geht ein Fuß bis zwei Stufen
weiter zu der, die ein Schritt will, `NB_STAIR_SHIFT`, statt nur bis zur nächsten. Der neue Test `SteerRunStairsTest`
rennt mit 3,0 und 4,5 m/s die Treppe hinauf und hinunter, den Start um 0 bis 0,6 m verschoben, und prüft, dass jeder
Fuß, der flach steht, mit Ferse und Zehen auf einer Stufe steht und hinunter mit 4,5 m/s jeder Schritt gleich viele
Stufen nimmt.

Was nicht ging, steht unter „Wechsel ganz ohne Ruck“ in [Optimierungen](Optimierungen.md). Rollte ein Fuß auch gehend
voraus, spränge beim Wechsel von 1,6 auf 4,8 m/s ein Schienbein nur 3,9 statt 4,9 cm, in den Wendungen im Gehen aber bis
3,4 statt 3,0 cm, über der Grenze von Runde 89. Kam die Ferse des Gehens mit der Feder, spränge das Becken beim Anhalten
aus 3,0 m/s nur 1,0 statt 2,0 cm, Schienbeine aber dort, im Wechsel zum Rennen, in der Wendung und im gleichmäßigen
Rennen bis 1,1 cm stärker. Und der Vorlauf aus dem Tempo beim Landen ließ beim ersten Schritt aus dem Stand einen Fuß
wie in Runde 93 nur 2,6 cm springen, aber rennend hinauf und, mit dem Boden von weiter oben, hinunter stand dann bei
manchen Starts ein Fuß neben seiner Stufe.

Wo es noch fehlt: Beim ersten Schritt aus dem Stand springt ein Fuß weiter 4,0 cm und ein Schienbein 4,2 cm. Das Becken
springt beim Wechsel von 1,6 auf 4,8 m/s 1,65 cm, wo der Abstieg des Gehens neu plant, wenn die Eile eines Fußes
wechselt, und beim Anhalten aus 4,8 m/s 1,15 cm, wo der Abstieg des Rennens am Anfang des Bremsens noch wirkt. Beim
Aufsetzen nach dem Anhalten springt ein Schienbein mit der Ferse des Gehens bis 4,7 cm, und von 4,8 auf 1,6 m/s und in
der Wendung 4,6 und 4,1 cm. Rennend mit 3,0 m/s die Treppe hinunter geht das Becken bei manchen Starts bis 18,1 cm auf
und ab. Eine gesteuerte Figur kostet im Update der Figuren auf der VM 8,0 und 7,8 statt 8,0 und 8,4 µs, mit 20 Figuren
in zweimal acht Läufen im Wechsel, gepaart 0,04 und 0,58 µs weniger, eine stehende 1,2 µs wie vorher, sie rechnet
denselben Code. Die VM rechnete an diesem Tag schneller als in Runde 93, die 10,1 und 10,3 µs von dort sind hier 8,0 und
8,4 µs. Im Benchmark bleiben alle Hashes. Er hat keine gesteuerte Figur und rechnet denselben Code: Die Bildzeiten der
fünf Städte liegen im Wechsel bei 0,82 bis 1,33 von `active-ragdoll-93`, im Mittel bei 1,01 bis 1,08, im Rauschen der
VM.

Treppe hinunter ruhig, erster Schritt weich, Runde 95, F84, auf Wunsch des Nutzers. Den Vorschlag nach Runde 94 nahm er
an: Rennend mit 3,0 und 4,5 m/s die Treppe hinunter sollte das Becken bei jedem Start höchstens 14 cm wippen, beim
ersten Schritt aus dem Stand kein Fuß mehr als 3,7 cm in einem Update springen und das Becken in keinem Wechsel mehr als
1 cm. Alles Erreichte sollte bleiben.

Die Ursachen, gemessen mit Varianten auf `active-ragdoll-94`: Auf einer Treppe folgt die Wurzel dem Boden zwischen den
Fußpunkten um die Hüfte, `nbStairGround`, aber höchstens 0,6 m/s schneller oder langsamer als die Neigung der Treppe,
`NB_STAIR_FOLLOW`. Das gilt, sobald die Figur eine Stufe gefunden hat, auch während die Kapsel noch auf dem ebenen
Podest läuft. Rennend hinunter muss die Wurzel dort mit 3,0 m/s mindestens 0,9 m/s sinken, mit 4,5 m/s mindestens 1,65
m/s, obwohl der Boden zwischen den Füßen kaum fällt. Am Rand der Treppe steht sie mit 3,0 m/s 0,42 statt 0,13 m unter
der Kapsel und holt das danach nur mit 0,6 m/s auf: Das Becken geht im ersten Schritt auf die Treppe je nach Start bis
18,1 cm gegen ihre Linie auf und ab. Beim ersten Schritt aus dem Stand ist das Ziel des Fußes, wo die Kapsel beim Landen
sein wird, und ein Vorlauf in Sekunden mal dem Tempo beim Landen. Der Vorlauf ist der des Gehens aus dem Stand, 0,30 s,
das Tempo beim Landen aber schon 2,9 m/s: Das Ziel läuft bis 1,09 m voraus und kommt in den letzten fünf Updates auf
0,70 m zurück, wenn der Vorlauf mit dem Tempo auf 0,09 s fällt. Der Fuß erreicht 6,3 m/s und bremst an der Grenze, 4,0
cm. Wählt ein Fuß spät im Schwung eine andere Stufe, etwa weil der andere Fuß abhebt und der Bezug für „so viele Stufen
weiter“ springt, mischt das weiche Auslaufen des Ziels, `NB_GAIT_AIM_FADE`, alte und neue Stufe, und er landet auf der
Kante. Und das Becken springt beim Wechsel von 1,6 auf 4,8 m/s 1,65 cm, wo der Abstieg des Gehens neu plant, wenn die
Eile eines Fußes wechselt, beim Anhalten aus 4,8 m/s 1,15 cm, wo am Anfang des Bremsens noch der Abstieg des Rennens
wirkt.

Vorher festgelegt, nach der Messung von `active-ragdoll-94` und der Suche nach den Ursachen mit Varianten, vor dem Bau
in der Bibliothek, bei 60 Hz mit 4 Teilschritten und gesteuert wie in der Demo. Wechsel, die sieben Szenen von Runde 93
an 24 Stellen im Gang, 168 Läufe: 1. Beim Losrennen aus dem Stand auf 3,0 und 4,8 m/s springt kein Fuß mehr als 3,7 cm
in einem Update. 2. Das Becken springt in keiner Szene mehr als 1,0 cm. 3. Keine Szene springt bei Füßen, Händen,
anderen Knochen oder Becken mehr als 0,3 cm stärker. Treppen von 15 × 30 cm, rennend mit 3,0 und 4,5 m/s hinauf und
hinunter, der Start um 0 bis 0,7 m nach hinten verschoben, 32 Läufe: 4. Hinunter geht das Becken gegen die Linie der
Treppe bei jedem Start höchstens 14 cm auf und ab. 5. Jeder Fuß steht beim Landen mit mindestens 0,8 seiner Sohle auf
seiner Stufe und steckt nicht tiefer als 1,5 cm in einer, und hinauf geht das Becken nicht mehr als 1 cm stärker auf und
ab. Unverändert gegen `active-ragdoll-94`, wie die Bedingungen 6 bis 14 von Runde 94: 6. die Übergänge im Gehen, 7.
Gehen geradeaus, 8. gleichmäßiges Rennen, 9. Neigung und Drehung von Runde 90 und 91, 10. die Bedingungen von Runde
87, 11. Treppen und Rampen beim Start von Runde 86, 12. das Schweben eines stehenden Fußes, 13. Tests und
Bitgleichheit, 14. die Kosten. Die Zahlen kommen vom Menschen wie in Runde 94, und ein Mensch, der eine Treppe
hinunterrennt, hält seine Hüfte so ruhig über ihrer Linie wie auf einer Rampe, wo immer er ankommt.

Erfüllt sind 3 bis 14, nicht 1 und 2: Beim ersten Schritt aus dem Stand springt ein Fuß weiter 4,0 cm, und das Becken
springt beim Wechsel von 1,6 auf 4,8 m/s weiter 1,65 cm und beim Anhalten aus 4,8 m/s 1,15 cm. Was diese Runde gebaut
hat, wirkt nur auf Treppen: Die sieben Szenen der Wechsel, das Gehen, das gleichmäßige Rennen und die Übergänge im Gehen
rechnen bitgleich wie auf `active-ragdoll-94`, an Rampen bleibt jeder Wert gleich. Gehend auf Treppen ändern sich beim
Start von Runde 86 zwei der zwölf Läufe um Millimeter: Hinauf auf Stufen von 15 × 33 cm mit 1,5 m/s geht das Becken
gegen die Linie der Treppe im Mittel 0,2 statt 0,1 cm und höchstens 0,3 statt 0,4 cm auf und ab, mit 137 statt 136
Schritten in der Minute, und hinunter auf Stufen von 19 × 25 cm zählt die Messung einen flachen Stand mehr, jeder Fuß
wie vorher mit mindestens 0,82 seiner Sohle auf seiner Stufe. Rennend gegen die Linie der Treppe, über die acht
verschobenen Starts, siehe auch `SteerRunStairsTest` in `test/test_figure.c`:

| Rennend auf der Treppe | `active-ragdoll-94` | `active-ragdoll-95` |
| --- | --- | --- |
| Hinunter mit 3,0 m/s | 4,8 bis 18,1 cm | 4,6 bis 5,7 cm |
| Hinunter mit 4,5 m/s | 13,2 bis 14,3 cm | 5,4 bis 7,0 cm |
| Hinauf mit 3,0 m/s | 7,0 bis 7,5 cm | 7,0 bis 7,1 cm |
| Hinauf mit 4,5 m/s | 7,0 bis 8,6 cm | 6,9 bis 7,1 cm |

Jeder Fuß steht dabei mit der ganzen Sohle auf seiner Stufe, wie auf `active-ragdoll-94`, und in der Welt springt das
Becken rennend auf der Treppe beim Start von Runde 86 in einem Update höchstens 1,9 statt 2,9 cm.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet`. Die Grenze von `NB_STAIR_FOLLOW` bezieht sich jetzt auf die
Neigung des Bodens zwischen den Fußpunkten vor und hinter der Hüfte, die `nbStairGround` dazu liefert, und nur ohne
beide auf die der Treppe. So geht die Wurzel nicht schon die Treppe hinunter oder hinauf, während die Kapsel noch auf
ebenem Boden läuft, und folgt doch nicht sprunghaft, wo die Fußpunkte wechseln. Und ab `NB_GAIT_AIM` weniger
`NB_GAIT_AIM_FADE` seines Schwungs hält ein Fuß seine Stufe, so landet er nicht zwischen zwei Stufen, wenn die Stufe,
die der Schritt will, sich spät ändert. Allein ändert das in den Läufen dieser Runde nur Millimeter, gehend hinauf auf
Stufen von 15 × 33 cm wie oben und rennend hinauf mit 3,0 m/s bei zwei Starts das Wippen im Mittel um 0,1 cm. Mit einem
anderen Vorlauf beim Losgehen stand ohne es rennend hinunter mit 4,5 m/s bei einem Start ein Fuß mit 0,0 der Sohle auf
seiner Stufe und 4 bis 6 cm tief in der nächsten, mit ihm jeder Fuß auf seiner Stufe. `SteerRunStairsTest` prüft jetzt
auch, dass das Becken rennend gegen die Linie der Treppe höchstens 10 cm auf und ab geht, auf `active-ragdoll-94` dort
bis 17,5 cm.

Was nicht ging, steht unter „Treppe hinunter ruhig“ in [Optimierungen](Optimierungen.md). Hielt ein Schwung, der
langsamer als mit 1,6 m/s abhob, den Vorlauf aus dem Tempo beim Landen, sprangen beim ersten Schritt aus dem Stand ein
Fuß 2,6 statt 4,0 cm und ein Schienbein 2,7 statt 4,2 cm, und der Wechsel von 1,6 auf 4,8 m/s blieb, wie er war. Rennend
die Treppe hinunter mit 4,5 m/s nahm beim Start von Runde 86 aber der erste Schritt auf der Treppe vier Stufen, das
Becken ging 14,6 cm auf und ab: Der andere Fuß fand am Rand des Podests keinen Boden, sein Fußpunkt blieb 0,53 m zu tief
bei der Schätzung aus der Neigung der Treppe, und der Schritt wählte seine Stufe von dort. Erst muss die Suche nach dem
Boden am Rand einer Stufe sicher werden.

Wo es noch fehlt: Beim ersten Schritt aus dem Stand springt ein Fuß weiter 4,0 cm und ein Schienbein 4,2 cm, und das
Becken springt beim Wechsel von 1,6 auf 4,8 m/s 1,65 cm und beim Anhalten aus 4,8 m/s 1,15 cm. Am Rand einer Stufe
findet ein Fuß bei manchem Anlauf keinen Boden. Eine gesteuerte Figur kostet im Update der Figuren auf der VM 8,3 und
8,1 statt 8,7 und 8,0 µs, mit 20 Figuren in zweimal acht Läufen im Wechsel, gepaart 0,37 µs weniger und 0,07 µs mehr,
eine stehende 1,2 µs wie vorher, sie rechnet denselben Code. Im Benchmark bleiben alle Hashes. Er hat keine gesteuerte
Figur und rechnet denselben Code: Die Bildzeiten der fünf Städte liegen im Wechsel bei 0,84 bis 1,10 von
`active-ragdoll-94`, im Mittel bei 0,95 bis 0,99, im Rauschen der VM.

Drehen ohne gekreuzte Beine, Runde 96, F84, auf Wunsch des Nutzers. Er nahm den Vorschlag nach Runde 95 zuerst an und
lenkte dann mit Bildern aus der Demo um: In Drehungen kreuzte die Figur die Beine zu einem X, ging tief in die Hocke und
machte gehend wie rennend riesige Ausfallschritte bis zum Spagat. Das sollte zuerst weg, das Gehen über Schutt, wo sie
sich hinsetzt, ein Knie auf die Spitzen der Brocken hebt und ein Bein waagrecht auf sie streckt, in einer eigenen
Runde.

Die Ursachen, gemessen mit Varianten auf `active-ragdoll-95`, gesteuert wie in der Demo: Die Tasten setzen die
Geschwindigkeit, die die Figur will, sofort, und sie schaut, wohin sie geht. Gemessen wurden Drehungen um 45, 90 und
135° nach links und rechts und um 180°, gehend mit 1,6 und rennend mit 4,8 m/s, je an 16 Stellen im Gang, dazu
Losgehen, Anhalten und die Wechsel zwischen Gehen und Rennen. Losgehen, Anhalten und die Wechsel waren sauber, die
Drehungen nicht. Die Kapsel ging sofort quer oder rückwärts, wohin die Tasten zeigten, und der Blick drehte mit 10
rad/s, eine Kehrtwende in 0,4 s: Der Körper lief seitlich vom stehenden Fuß weg, schneller, als ein Schritt ihn holen
konnte. Ein stehender Fuß behielt seine Richtung, bis er abhob, die Hüfte drehte sich bis 180° gegen ihn. Der
schwingende Fuß zielte dorthin, wo die Hüfte beim Landen sein wird, auch jenseits des stehenden Fußes, und die Knie
kreuzten sich bis 25 cm. Ein Fuß weit hinter der Hüfte, den sein Bein schon nicht mehr erreichte, hob nur ab, wenn
seine Phase es erlaubte, sonst ging die Wurzel ihm nach: rennend in der Kehrtwende bis 68 cm tief, das Standknie 150°
gebeugt und die Oberschenkel 177° auseinander.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten, vor dem Bau in der Bibliothek, mit den
Voreinstellungen der Bibliothek und den 16 Stellen jeder Drehung von oben: 1. Die Knie kreuzen sich in keiner Drehung,
quer zum Becken gemessen. 2. Das Becken geht gehend höchstens 12 cm und rennend höchstens 16 cm tiefer als im Stand.
3. Das Standknie beugt sich gehend höchstens 65° und rennend höchstens 72°. 4. Die Oberschenkel gehen gehend höchstens
70° und rennend höchstens 105° auseinander. 5. Losgehen, Anhalten und die Wechsel zwischen Gehen und Rennen rechnen
bitgleich wie auf `active-ragdoll-95`. Unverändert: 6. Gehen und Rennen geradeaus, Treppen und Rampen, jeder Wert,
auch rennend auf der Treppe mit 20 Starts über eine ganze Stufe. 7. Die Tests mit gcc und clang, ein Hash ändert sich
nur, wo ein Test eine gesteuerte Figur drehen lässt. 8. Gesteuert bitgleich mit 1, 2 und 4 Threads, Figuren ohne
`steered` wie vorher. 9. Eine gesteuerte Figur kostet höchstens 1 µs mehr. 10. Im Benchmark bleiben die Hashes, die
Bildzeiten im Rauschen der VM. Die Grenzen 2 bis 4 liegen etwas über dem, was die Figur geradeaus braucht, gehend und
rennend 6,8 und 11,9 cm, 43,5 und 63° und 47,6 und 82,3°.

Erfüllt sind alle zehn. Für 7 gilt eine Ausnahme: `SteerKneeTest` verlangt in der Kehrtwende jetzt, dass die Figur die
Knie zu 0,7 statt 0,8 ihrer Beugung beim Bremsen und Wenden beugt, siehe unten. Über beide Richtungen und 16 Stellen jeder Drehung, das Schlimmste von
`active-ragdoll-95` und von `active-ragdoll-96`, quer zum Becken gemessen:

| Wendung | Knie gekreuzt | Füße gekreuzt | Becken tiefer | Standknie | Schenkel auseinander |
| --- | --- | --- | --- | --- | --- |
| Gehend 45° | 6,5 cm → nie | 30,6 cm → 15,2 cm | 9,7 → 8,3 cm | 47,5 → 45,0° | 53,1 → 53,5° |
| Gehend 90° | 9,5 cm → nie | 40,9 cm → 19,2 cm | 10,2 → 10,9 cm | 60,1 → 59,6° | 62,3 → 63,6° |
| Gehend 135° | 12,5 cm → nie | 47,1 cm → 19,0 cm | 27,8 → 8,2 cm | 86,5 → 56,5° | 100,0 → 66,5° |
| Gehend 180° | nie → nie | 23,2 cm → 3,4 cm | 10,4 → 7,5 cm | 65,7 → 56,5° | 59,6 → 66,1° |
| Rennend 45° | 8,3 cm → nie | 31,2 cm → 2,5 cm | 11,4 → 11,5 cm | 62,6 → 62,6° | 88,5 → 93,2° |
| Rennend 90° | 11,0 cm → nie | 45,3 cm → 9,2 cm | 12,2 → 11,9 cm | 65,0 → 63,6° | 88,0 → 103,3° |
| Rennend 135° | 14,2 cm → nie | 54,0 cm → 14,7 cm | 22,0 → 14,3 cm | 69,6 → 68,4° | 100,2 → 89,8° |
| Rennend 180° | 25,0 cm → nie | 72,4 cm → 9,5 cm | 68,3 → 14,8 cm | 150,0 → 68,3° | 177,0 → 86,2° |

Die Füße kreuzen sich quer zum Becken noch, wo ein Fuß weit hinten steht und das Becken sich schon weiter gedreht hat,
die Knie nicht. Losgehen, Anhalten, die Wechsel, Gehen und Rennen geradeaus, die Treppen mit den 20 Starts und die
Rampen rechnen bitgleich wie auf `active-ragdoll-95`.

Gebaut in der Steuerung und in der Gangplanung der Bibliothek, `nbMoveSteered` und `nbSteerFeet`, ohne eine Zeile in Demo
oder Benchmark. Eine Figur, die schaut, wohin sie geht, `nbSteer::faceVelocity`, geht quer zu ihrem Blick höchstens 0,3
m/s oder 0,17 ihres Wunschtempos, was mehr ist, und rückwärts höchstens 0,3 m/s, `NB_STEER_SIDE_SPEED`,
`NB_STEER_SIDE_SHARE` und `NB_STEER_BACK_SPEED`: Sie dreht sich zum Ziel und kommt dorthin, während sie hinschaut, und
der Gang plant mit dieser Geschwindigkeit. Ihr Blick dreht sich in der Voreinstellung mit 4 statt 10 rad/s und kommt
mit 25 statt 100 rad/s² darauf, `nbFigureSteering::turnRate` und `turnAcceleration`, eine Kehrtwende in gut 0,9 s. Ein
stehender Fuß dreht sich auf der Mitte seiner Sohle mit, sobald der Blick mehr als 35° von ihm weg ist,
`NB_FOOT_TWIST`, wie Menschen auf einem Fuß drehen. Ein schwingender Fuß landet auf seiner Seite des anderen Fußes, die
Mitten der Sohlen quer zum Blick mindestens 10 cm auseinander, `NB_GAIT_SIDE_GAP`. Ein Fuß hinter der Hüfte, den sein
Bein gar nicht mehr erreicht, hebt ab, wo immer seine Phase ist. Der äußere Fuß einer Drehung landet weit außen und
kann gleich wieder außer Reichweite sein: Ein Fuß, der im ersten Fünftel des Zyklus gelandet ist und schon verloren
geht, bleibt dann stehen, während der andere hinter der Hüfte zuerst abhebt, `NB_GAIT_FRESH`. Hob er sofort wieder ab,
blieb der andere Fuß stehen, bis er 0,84 m hinter der Hüfte war, ein Ausfallschritt von 85°. Beim Umkehren geht das
Ziel eines schwingenden Fußes an dem Punkt vorbei, wo er abhob: Ein Schwung hebt den Fuß jetzt nicht weniger hoch als
bisher in ihm, die Höhe fiel dort für ein Update auf 3,6 cm und sprang zurück auf 6 cm. Und der Abstoß über die Zehen
zählt den Weg der Hüfte über den Fuß höchstens so schnell, als ginge sie mit 3 m/s, `NB_GAIT_PUSH_SPEED`: Beim Umkehren
ging die Hüfte zurück über einen Fuß, während er noch von der Ferse abrollte, und sobald die Ferse unten war, kam der
ganze Abstoß in einem Update. Diese beiden ändern Gehen und Rennen geradeaus nicht. Gemessen mit `SteerKneeTest`,
achtmal umkehren an acht Stellen im Gang, springen die Knie jetzt in 5 statt 14 Updates mehr als 3 cm, höchstens 3,8
statt 4,7 cm, das Becken 0,8 statt 1,9 cm. Die Figur beugt dabei die Knie zu 0,76 statt 0,89 ihrer Beugung beim
Bremsen und Wenden, `NB_GAIT_BEND_SPEED`, weil sie jetzt nur so schnell bremst, wie die Kapsel rückwärts gehen darf, und der Test verlangt
0,7 statt 0,8. Mit der Geschwindigkeit des Spiels statt der der Kapsel beugte sie 0,97, solange die Kehrtwende dauerte,
und das Becken ging bis 30 cm tief. `SteerSpeedTest` gibt der Drehung auf der Stelle die Zeit, in der sie auf ihre
Drehrate kommt und wieder bremst, und der Hash von `SteerDeterminismTest` ist neu, seine Figuren drehen sich.

Was nicht ging, steht unter „Drehen ohne gekreuzte Beine“ in [Optimierungen](Optimierungen.md), dazu schnellere
Drehungen, festere oder freiere Grenzen quer zum Blick, andere Abstände und Drehgrenzen der Füße und eine Grenze, wie
schnell der seitliche Abstand wachsen darf. Mit dieser Grenze landete der äußere Fuß ruhiger, aber die Knie kreuzten
sich wieder.

Wo es noch fehlt: In den 48 Übergängen von Runde 88 sind 14 statt 21 Updates über den Grenzen, einzelne Spitzen sind
aber höher. In einer von acht Drehungen um 90° landet der äußere Fuß mit 5,4 statt höchstens 4,0 cm in einem Update,
weil sein Ziel spät im Schwung mit dem Blick wandert, und in der Kehrtwende springt das Becken 1,65 statt 1,33 cm. In
der Kehrtwende der Messung von Runde 87 springt ein Schienbein 3,5 statt 2,7 cm, über der Grenze von 3 cm, wo ein Fuß
vorn landet, während die Figur schon zurückgeht, und sofort wieder abhebt. Rennend um 90° gehen die Oberschenkel bis 103
statt 88° auseinander, und die Figur neigt sich beim Bremsen vor der Drehung bis 4° zurück, vorher nicht. Offen von Runde
95 bleiben der erste Schritt aus dem Stand, der Rand einer Stufe und das Becken in den Wechseln, gemessen vor der
Umlenkung: Hielt ein Fuß Halt und Zyklus, wenn er im Schwung keinen neuen Halt fand, änderte das rennend auf der Treppe
höchstens einen Millimeter, und mit dem Vorlauf aus dem Tempo beim Landen ging das Becken rennend hinunter mit 4,5 m/s
weiter 17,7 cm auf und ab. Mit 20 Starts über eine ganze Stufe statt acht über 0,7 m wippt das Becken rennend hinunter
mit 3,0 m/s bis 8,3 statt 7,1 cm gegen die Linie der Treppe, auf `active-ragdoll-95` wie hier. Eine gesteuerte Figur
kostet im Update der Figuren auf der VM 9,1 und 9,1 statt 8,8 und 8,4 µs, mit 20 Figuren in zweimal acht Läufen im
Wechsel, gepaart 0,32 und 0,64 µs mehr, im Median 8,6 und 8,3 statt 8,4 und 8,3 µs, einzelne Läufe rauschen bis 14,8
µs, eine stehende 1,2 statt 1,3 µs, sie rechnet denselben Code. Im Benchmark bleiben alle Hashes. Er hat keine
gesteuerte Figur und rechnet denselben Code: Die Bildzeiten der fünf Städte liegen im Wechsel bei 0,84 bis 1,34 von
`active-ragdoll-95`, im Mittel bei 0,98 bis 1,02, im Rauschen der VM.

Über Schutt gehen, ohne zu sitzen, Runde 97, F84, auf Wunsch des Nutzers. Er zeigte mit Bildern aus der Demo, wie sich
die gesteuerte Figur auf dem Schutt der Ziegelwand hinsetzt, ein Knie auf die Spitzen der Brocken hebt und ein Bein
waagrecht auf sie streckt, und wollte das nach dem Drehen ohne gekreuzte Beine in einer eigenen Runde.

Gemessen in einem Labor mit der Ziegelwand der Szene „Spielfigur“, 8 m lang, 2,2 m hoch und 0,3 m dick, mit
Bruchstücken doppelt so groß, wie ihr Material sagt, wie die Demo sie macht, von einer Seite mit den Granaten der Demo
gesprengt, danach 6 s Ruhe. Drei Felder, eines mit 24 Granaten in drei Reihen und zwei mit 12 und 10, auf jedem zwei
Sätze von Bahnen, gehend mit 1,6 und rennend mit 4,8 m/s, längs der Wand, quer in beide Richtungen und schräg, zusammen
222 Läufe. Gezählt wird ab 0,7 s, solange die Figur steht. Eine schlechte Pose hat ein Update, in dem ein Standknie über
100° gebeugt ist, ein Oberschenkel über 90° vom Lot gehoben, ein Bein von der Hüfte zum Knöchel über 75° vom Lot
gestreckt oder beide Knie über 80° gebeugt sind. Auf ebenem Boden kommt nichts davon vor, gehend höchstens 42,5°, 30°
und 32°, rennend 62°, 66° und 50°.

Die Ursachen, gemessen auf `active-ragdoll-96`: `nbFindFoothold` hielt jede Fläche mit zwei Kanten für eine Stufe, und
die Oberseiten der Brocken sind so tief wie Stufen und so hoch über dem Boden. So hielt die Figur den Schutt in 32,9 %
der Updates für eine Treppe und nahm bis sechs Stufen in einem Schritt. Fand die Suche am Ziel eines Fußes keinen Boden,
meist weil ihre Kugel an der steilen Seite eines Brockens hängen blieb, zielte der Fuß mit der Steigung, die der Gang für
den Weg schätzte, bis 1 m in der Luft, und spät im Schwung hielt `NB_GAIT_LEVEL` so ein Ziel fest. Füße landeten auf
Spitzen und auf hohen Brocken, die Suche reicht bis 0,82 m hinauf, und stand ein Fuß einen halben Meter höher als der
andere, ging die Wurzel mit dem tieferen hinunter: Die Figur saß dazwischen oder hob das Knie bis an die Brust. An
steilen Seiten verlor die Kapsel in 5,4 % der Updates ihren Boden, der Gang ging aus, und die Figur ging in die Hocke.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten und vor dem sauberen Bau, auf den 222 Läufen: 1.
Schlechte Posen in höchstens 2,5 % der Updates, auf `active-ragdoll-96` 9,77 %. 2. Ein Standknie über 120° in höchstens
168 Updates, einem Zehntel von Runde 96. 3. Landungen in der Luft, Ferse und Spitze mehr als 15 cm über dem Boden
darunter, höchstens 4,0 %. 4. Landungen auf einer Spitze, beide mehr als 3 cm darüber, höchstens 15 %. 5. Schutt als
Treppe in höchstens 1 % der Updates. 6. Die Kapsel ohne Boden in höchstens 2,7 % der Updates. 7. Alle Unit-Tests geben
dieselben Zahlen aus wie auf `active-ragdoll-96`, nur der Hash der gesteuerten Szene ändert sich, und das nur durch die
Figur an der Flanke der Treppe, und die Posen-Tabelle von Runde 96 und die 20 Starts über eine Stufe rechnen bitgleich.
8. Tests mit gcc und clang grün, mit 1, 2 und 4 Threads bitgleich. 9. Eine gesteuerte Figur auf ebenem Boden kostet
gepaart höchstens 0,3 µs mehr, dort fragt sie nichts zusätzlich ab. 10. Im Benchmark bleiben die Hashes, die Bildzeiten
im Rauschen. Die Grenzen 1 bis 6 liegen etwas über dem, was die beste Variante vor dem sauberen Bau erreichte: 1,98 %,
57 Updates, 3,6 %, 13,6 %, 0,0 % und 1,7 %.

Erfüllt sind alle zehn, für 9 über alle Paare, siehe unten. Über die 222 Läufe, die letzten vier Zeilen das Schlimmste
in einem Update:

| Über Schutt | `active-ragdoll-96` | `active-ragdoll-97` |
| --- | --- | --- |
| Schlechte Posen | 9,77 % der Updates | 1,98 % |
| Standknie über 120° | in 1 683 Updates | in 42 |
| Landungen in der Luft | 7,5 % | 3,7 % |
| Landungen auf einer Spitze | 17,1 % | 13,9 % |
| Schutt als Treppe | 32,9 % der Updates | 0,0 % |
| Kapsel ohne Boden | 5,4 % der Updates | 1,7 % |
| Standknie | 170,6° | 139,5° |
| Oberschenkel vom Lot | 178,8° | 166,9° |
| Bein vom Lot | 178,8° | 141,0° |
| Beide Knie | 168,4° | 122,3° |

Gebaut in der Gangplanung und der Steuerung der Bibliothek, `nbSteerFeet`, `nbFindFoothold` und `nbMoveSteered`, ohne
eine Zeile in Demo oder Benchmark. Abseits von Treppen landet ein schwingender Fuß, wo er Boden höchstens 25 cm höher
oder tiefer als den Knöchel des anderen Fußes findet, `NB_GAIT_FOOT_BAND`: wo der Gang ihn hinsetzt, sonst an der
nächsten solchen Stelle in Schritten von 8 cm entlang seiner Zehen nach hinten und vorn, bis 24 cm, und in den ersten
beiden Schritten auch nach außen, `nbChooseFoothold`. Liegt keine im Band, nimmt er die, die es am wenigsten verfehlt,
eine 8 cm weiter muss es um 2 mm weniger verfehlen, und nie eine höher als eine Stufe über dem Weg der Kapsel. Findet er
keinen Boden, sucht er noch einmal vom Weg der Kapsel aus, ohne die geschätzte Steigung, bis eine Stufe tiefer, und
zielt sonst auf diesen Weg statt in die Luft, und ein Ziel ohne Boden plant er auch spät im Schwung neu. Eine Fläche,
hinter deren beiden Enden der Boden tiefer liegt und die kürzer ist als die Sohle, ist eine Spitze: Der Fuß rückt über
das nähere Ende hinaus. Eine Treppe beginnt nur an einer Fläche mit einer Kante hinauf und einer hinunter, hinter der die
nächste Fläche ebenso tief ist und ihre Kante ebenso hoch, innerhalb eines Drittels, `NB_STAIR_MATCH`. Und findet der
Fühler unter der Kapsel nur Seiten, die zu steil zum Stehen sind, steht sie im Gehen auf dem stehenden Fuß, der dem
Rahmen am nächsten ist, wenn er höchstens eine Stufe höher oder tiefer liegt. Mit acht Regeln eines ersten Baus, 2,54 %
schlechte Posen, wog jede etwas: Ohne das Band waren es 5,03 %, ohne eine Kante hinauf und eine hinunter für eine Stufe
4,01 %, ohne die zweite Suche nach unten 3,67 %, ohne die Grenze einer Stufe 3,12 %, ohne den Halt der Kapsel am Fuß
3,08 %, ohne das Neuplanen spät im Schwung 2,87 % und ohne die Spitzen 2,66 %. Weiter daneben zu suchen, wo kein Boden
ist, brachte nichts, 2,49 % ohne, und kam nicht in den sauberen Bau. Die passende nächste Stufe kam danach dazu, 1,98
statt 2,49 %.

`SteerRubbleTest` lässt die Figur auf vier dieser Bahnen über die gesprengte Wand gehen und rennen und verlangt, was sie
dort jetzt schafft, mit etwas Luft: den Schutt in höchstens 1 % der Updates für eine Treppe gehalten, Boden für die
Kapsel in mindestens 98 %, schlechte Posen in höchstens 3 % und ein Standknie über 120° in höchstens 0,5 %, rennend mit
1, 2 und 4 Threads dieselben Bits. Auf `active-ragdoll-96` hält die Figur dort den Schutt in 1 016 von 1 546 Updates
für eine Treppe, die Kapsel ist in 144 ohne Boden, die Beine sind in 188 in einer schlechten Pose und ein Standknie in 16
über 120° gebeugt, jetzt in 0, 18, 19 und 0. Der Hash von `SteerDeterminismTest` ist neu: Die Figur, die quer an die
Seite der Treppe läuft, setzt ihren Fuß jetzt vor deren Flanke, wo sie 45 cm hoch ist, statt darauf. Alle anderen Tests
geben dieselben Zahlen aus.

Was nicht ging, steht unter „Über Schutt gehen, ohne zu sitzen“ in [Optimierungen](Optimierungen.md): weiter daneben
suchen, wo kein Boden ist, die ganze Sohle getragen verlangen, das Band gegen den geschätzten Boden statt gegen den
anderen Fuß und eine Wahl, die die Höhe einer Stufe und den Weg nur weich gewichtet.

Wo es noch fehlt: Rennt die Figur gegen einen hohen Brocken wie gegen eine Wand, steht die Kapsel schlagartig, und das
Becken sackt bis 9 cm über den Boden. Daher kommt der schlimmste Oberschenkel, 166,9° vom Lot. In 13,9 % der Landungen
steht ein Fuß noch auf einer Spitze. Von den übrigen schlechten Posen sind in 591 Updates beide Knie über 80° gebeugt, in
420 ein Oberschenkel über 90° gehoben, in 368 ein Bein über 75° gestreckt und in 320 ein Standknie über 100° gebeugt, in
gut einem Viertel steht dabei ein Fuß auf einer Spitze. Eine gesteuerte Figur kostet auf ebenem Boden im Update der
Figuren auf der VM gepaart in vier Sätzen von acht Läufen −0,65, +0,30, +0,35 und +0,13 µs mehr, über alle 32 Paare
+0,03 µs, im Median +0,15 µs, gegen `active-ragdoll-96`, gebaut wie diese Runde, in zwei Sätzen −0,32 und +0,27 µs,
im Median +0,14 µs. Die Sätze nach den ersten beiden kamen, weil diese sich im Vorzeichen widersprachen und einer an der
Grenze lag. Auf ebenem Boden findet ein Fuß Boden im Band des anderen, und `nbChooseFoothold` fragt nichts ab. Eine
stehende Figur kostet 1,2 statt 1,5 µs, sie rechnet denselben Code. Im Benchmark bleiben alle Hashes und Zähler, er hat
keine gesteuerte Figur, und der neue Code läuft dort nicht. Gegen den Build von Runde 96 lagen die Bildzeiten der fünf
Städte im Wechsel bei 0,88 bis 1,20, im Mittel bei 0,99 bis 1,10, die Stadt mit Figuren auch in zwölf Paaren mehr, in
beiden Reihenfolgen, im Mittel bei 1,09 und 1,05, und darin der Schritt von Box3D, dessen Code gleich ist, ebenso. Gegen
`active-ragdoll-96`, gebaut wie diese Runde, mit gleichem Compiler, gleichen Optionen und gleich langen Pfaden, liegt die
Stadt mit Figuren in sechs Läufen bei 0,89 bis 1,14, im Mittel bei 0,99: Der Unterschied lag am alten Build, nicht am
Code.

Gehen und Rennen nach Clips des Spiels, Runde 98, F85 Stufe 1, auf Wunsch des Nutzers: Die Figuren sollen Animationen
folgen und bei einem Treffer in den Controller wechseln. So steht es in 27.3 der Roadmap des Spiels: Im normalen Gehen
und Rennen bestimmen Animation und IK die Pose, ein leichter oder mittlerer Treffer kommt als Pose oben auf den
Controller oder als Schritt zur Erholung, ein schwerer macht die Figur zum Ragdoll. Eine stehende Figur folgt ihrem Clip
schon so, eine gesteuerte bewegte die Bibliothek bisher selbst, Becken, Rumpf und Arme nach Regeln über dem Stand-Clip.

Ein Spiel gibt einer gesteuerten Figur jetzt bis zu zwei Clips ihres Gangs, beim Anlegen mit `nbFigureDef::gaitClips`
oder später mit `nbFigure_SetGaitClips`, den langsameren zum Gehen und den schnelleren zum Rennen. Ein solcher Clip,
`nbGaitClipDef`, ist ein Zyklus aus zwei Schritten geradeaus mit gleichem Tempo auf ebenem Boden, mit dem Tempo, bei dem
er entstand, und den Zeiten, zu denen jeder Fuß aufsetzt und abhebt, wie die Animationspipeline eines Spiels sie
markiert. Die Figur spielt ihn im Takt ihrer eigenen Schritte: Die Gangphase der Bibliothek legt die Zeit im Clip fest,
beide Aufsetzer des Clips fallen auf die der Füße, dazwischen läuft die Zeit gleichmäßig, abgetastet dort, wo die Phase
am Ende des Updates steht. Bewegt der Clip die Wurzel, `nbClipDef::moveRoot`, kommt ihr Weg über den Boden heraus, ihre
Höhe, ihre Drehung und ihr Pendeln bleiben. Rumpf, Arme und Kopf kommen aus den Clips, aus dem Stand-Clip eingeblendet,
wie der Gang anläuft, und zwischen Gehen und Rennen mit derselben Feder gemischt, mit der der Gang der Bibliothek
wechselt. Die Wurzel mischt nach dem kleineren aus dieser Feder und dem Tempo, sonst stand das Becken beim Anhalten aus
dem Rennen noch in dessen Hocke, bis 5 cm tiefer als ohne Clips. Wie lange ein Fuß steht, kommt aus den Marken, zwischen
Gehen und Rennen gemischt, und ebenso, wie oft die Figur einen Schritt macht: mit dem Tempo eines Clips so oft wie er,
schneller und langsamer so viel öfter oder seltener, wie der Gang der Bibliothek es täte. Ein einzelner Clip ist zum
Gehen, wenn seine Füße mindestens die Hälfte des Zyklus stehen, sonst zum Rennen, und das andere nimmt die Bibliothek
aus ihrem eigenen Gang. Ohne diese Regel rannte die Figur mit einem Clip zum Gehen allein mit dem Standanteil des
Gehens, das Becken 25 cm tiefer.

Die Füße plant weiter die Bibliothek, auf Treppen, Schutt und am Hang, sie hält sie am Boden und reicht sie mit der IK.
Das Becken geht nur so viel tiefer als im Clip, wie die Beine es brauchen, und ein stehender Fuß hebt ab, bevor sein
Bein das Becken mehr als 4 cm unter die Höhe bräuchte, die ihm der Gang der Bibliothek gäbe, auch wenn der Clip es
tiefer trägt. Von der Höhe des Clips aus gezählt hielt die Figur Füße zu lange, und in scharfen Wendungen kreuzten sich
die Füße im 90. Perzentil um 18,4 statt 12,9 cm. Auf Treppen läuft das Auf und Ab des Clips gegen seine mittlere Höhe
aus, so weit, wie die Figur den Boden als Treppe nimmt, sonst wippte das Becken gehend hinauf mit dem Clip um 4 cm gegen
die Linie der Treppe, ohne Clips unter 1 cm. Treffer wirken wie bisher auf den Körper, wie der Clip ihn bewegt: Ein
leichter lässt den Rumpf über dem Clip zucken, ein harter macht die Figur zum Ragdoll, und steht sie wieder, geht sie
mit ihren Clips weiter. Spielt ein Clip von `nbFigure_PlayClip`, bewegt die Bibliothek den Körper über diesem wie ohne
Clips des Gangs, die Schritte behalten ihren Takt. Kommen oder gehen die Clips mitten im Gang, blendet der Körper über
wie zu einem neuen Clip, und das Schwingen der Arme aus dem Gang der Bibliothek kommt erst mit dem Überblenden zurück.
Vorher schwangen die Arme beim Entfernen der Clips doppelt, und eine Hand sprang 20,5 cm in einem Update.

`nbMakeHumanoidGaitClip` zeichnet den Gang der Bibliothek als solchen Clip auf, für einen Menschen von
`nbMakeHumanoidBones` jeder Größe und jedes Tempo: Ein gesteuerter Mensch geht oder rennt in einer eigenen Welt
geradeaus, bis sein Gang gleichmäßig ist, und die Pose, die er in jedem Update eines Zyklus anstrebt, wird ein
Schlüsselbild zu der Zeit, die sein Gang hat. Die Marken liegen, wo sein Gang die Füße aufsetzt und abhebt. Der
Stand-Clip hält dabei seine erste Pose, sonst liefe sein Atmen durch den Zyklus, und der Clip spränge an seinem Ende,
die Hände bis 1,3 cm. Für einen Menschen von 1,8 m hat der Zyklus mit 1,6 m/s 57 Schlüsselbilder über 0,924 s, die Füße
stehen 0,6 davon, mit 4,8 m/s 41 über 0,654 s und 0,33. So kann ein Spiel seine Clips gegen den Gang der Bibliothek
ausprobieren oder bei ihm beginnen, und die Tests messen mit diesen Clips: Mit ihnen geht und rennt die Figur wie ohne.

Vorher festgelegt nach einem Prototyp, vor dem sauberen Bau, mit den Clips aus `nbMakeHumanoidGaitClip` bei 1,6 und 4,8
m/s: 1. Ohne Clips rechnet alles bitgleich wie auf `active-ragdoll-97`: Alle Unit-Tests, die Posen-Tabelle von Runde 96,
die 120 Starts über Treppen und die 222 Läufe über Schutt geben dieselben Zahlen aus. 2. Mit den Clips liegen die
Knochen beim gleichmäßigen Gehen und Rennen je Gruppe, Becken, Rumpf, Arme, Beine und Füße, im Mittel höchstens 0,5 und
höchstens 1,5 cm neben denen ohne Clips, Rahmen und Gangphase gleich. 3. In den Übergängen, 20 Arten, Losgehen und
Losrennen, Anhalten, Wendungen um 45 bis 180° und Wechsel zwischen Gehen und Rennen, je 32 Momente, ist mit den Clips je
Art das Becken, der Abstand der Füße und der der Knie im Mittel höchstens 2 cm und im 90. Perzentil höchstens 3 cm
schlechter als ohne, das Standknie höchstens 3° und 4°, der Spagat höchstens 3° und 8°. 4. Auf Treppen, mit 1,5, 3,0 und
4,5 m/s hinauf und hinunter, je 20 Starts, verliert mit den Clips kein Fuß seine Stufe, Auflage und Einsinken sind nicht
schlechter, und das Becken wippt in der Welt höchstens 1 cm und springt in einem Update höchstens 0,5 cm mehr als ohne.
5. Über Schutt hat die Figur mit den Clips in höchstens 2,5 % der Updates eine schlechte Pose, landet in höchstens 15 %
auf einer Spitze und in höchstens 4,0 % in der Luft. 6. Von 1 024 Treffern fallen mit und ohne Clips dieselben,
höchstens 2 % weichen ab, und ebenso fangen sich dieselben. 7. Der Clip zeigt sich: Oberarme, im Clip 60° vorgehoben,
heben die Hände beim Gehen um mindestens 15 cm, eine Wurzel 3 cm höher hebt das Becken im Mittel um 1 bis 3 cm, und mit
einem Clip zum Gehen allein rennt die Figur mit 4,8 m/s, ohne dass das Becken unter 0,80 m sinkt. 8. Tests mit gcc und
clang grün, neue für den Rekorder und den Gang nach Clips, mit Clips mit 1, 2 und 4 Threads bitgleich. 9. Eine
gesteuerte Figur ohne Clips kostet gepaart höchstens 0,3 µs mehr als auf `active-ragdoll-97`, mit Clips höchstens 1,5 µs
mehr als ohne. 10. Im Benchmark bleiben die Hashes, die Bildzeiten im Rauschen. Die Grenzen 2 bis 7 liegen über dem, was
der Prototyp erreichte.

Erfüllt sind alle zehn, 9 mit dem endgültigen Bau, vorher nur über alle Paare, siehe unten. Mit den Clips aus
`nbMakeHumanoidGaitClip` gegen ohne, das Becken auf Treppen gegen ihre Linie, das Schlimmste über 20 Starts:

| Mit den Clips aus `nbMakeHumanoidGaitClip` | ohne Clips | mit Clips |
| --- | --- | --- |
| Treppe gehend hinauf mit 1,5 m/s | 0,7 cm | 0,7 cm |
| gehend hinunter mit 1,5 m/s | 6,2 cm | 5,6 cm |
| rennend hinauf mit 3,0 m/s | 7,2 cm | 0,4 cm |
| rennend hinunter mit 3,0 m/s | 8,3 cm | 0,9 cm |
| rennend hinauf mit 4,5 m/s | 7,1 cm | 0,5 cm |
| rennend hinunter mit 4,5 m/s | 7,0 cm | 3,7 cm |
| Schlechte Posen über Schutt | 1,98 % der Updates | 1,71 % |
| Landungen auf einer Spitze | 13,9 % | 13,4 % |
| Landungen in der Luft | 3,7 % | 3,4 % |
| Von 1 024 Treffern gefangen und gefallen | 114 und 142 | dieselben 114 und 142 |

Beim gleichmäßigen Gehen liegen die Knochen mit den Clips im Mittel höchstens 0,10 cm neben denen ohne, die Beine
höchstens 0,59 cm, rennend im Mittel höchstens 0,24 cm und die Arme höchstens 0,63 cm, Rahmen und Gangphase gleich. In
den Übergängen ist mit den Clips je Art höchstens schlechter: das Becken um 0,6 cm im Mittel und 0,4 cm im 90.
Perzentil, das Standknie um 1,0° und 0,9°, der Abstand der Füße um 0,9 und 1,3 cm, der der Knie um 0,8 und 1,6 cm und
der Spagat um 1,9° und 5,1°, dieser rennend in Wendungen um 135°. In anderen Arten sind die Clips besser, gehend in
Wendungen um 135° kreuzen sich die Füße bis 13,8 statt 17,5 cm, rennend in Wendungen um 135° geht das Becken im 90.
Perzentil 12,8 statt 13,9 cm tiefer als im Stand. Auf Treppen verliert kein Fuß seine Stufe, Auflage und Einsinken sind
nicht schlechter, und das Becken springt in der Welt gehend hinunter in einem Update bis 1,93 statt 1,72 cm, gehend
hinauf gleich weit und in allen anderen Fällen weniger als ohne. Oberarme, im Clip 60° vorgehoben, heben die Hände beim
Gehen um 21,4 cm, eine Wurzel 3 cm höher hebt das Becken um 1,34 cm, den Rest nehmen die Beine, und mit einem Clip zum
Gehen allein rennt die Figur mit 4,8 m/s, das Becken nie unter 0,829 m. Die Treffer kommen auf Brust, Kopf, Becken und
eine Hand, von vorn, von hinten und von beiden Seiten, mit 15, 30, 45 und 70 N s, an acht Stellen im Gang, gehend und
rennend: Mit und ohne Clips fängt sich die Figur bei denselben 114 und fällt bei denselben 142, die übrigen zucken und
gehen weiter.

Gebaut in der Bibliothek, `src/figure.c` und der neuen `src/figure_gait.c`, ohne eine Zeile in Demo oder Benchmark.
`nbGaitClipPose` tastet die Clips ab und mischt sie in die Pose, bevor der Gang die Füße plant, `nbSteerFeet` nimmt
Takt, Standanteil und Höhe des Beckens aus ihnen, `nbLowerSteered` senkt das Becken nur, wo die Beine es brauchen, und
sieht dabei mit der Wurzel des Clips voraus, `nbPlaceFeet` dreht und schiebt die Wurzel wie der Clip. Ohne Clips nehmen
alle diese Stellen die Wege von vorher, eine Multiplikation mit 1 ändert kein Bit. Die neuen Tests: `GaitClipRecordTest`
zeichnet die Clips auf und prüft Zyklus, Marken und Standanteil, `GaitClipFollowTest` verlangt mit ihnen jeden Knochen
im Mittel innerhalb von 0,5 cm und höchstens 1,5 cm des Gangs ohne Clips, gemessen gehend 0,05 und 0,59 cm, rennend 0,07
und 0,57 cm, `GaitClipShowTest` die gehobenen Hände, das gehobene Becken und das Rennen mit einem Clip zum Gehen allein,
`GaitClipHitTest` dieselben Zustände nach 30 und 70 N s von hinten auf die Brust, gehend und rennend,
`GaitClipSwitchTest`, dass die Bewegung eines Knochens sich von einem Update zum nächsten um weniger als 5 cm ändert,
während Clips kommen und gehen, gemessen 4,13 cm, wie beim Überblenden zu jedem Clip, sonst um weniger als 2 cm,
gemessen 1,68 cm, und `GaitClipDeterminismTest` den Parcours von `SteerDeterminismTest` mit Clips, mit 1, 2 und 4
Threads dieselben Bits. Alle anderen Tests geben dieselben Zahlen aus wie auf `active-ragdoll-97`, mit gcc und mit
clang.

Was nicht ging, steht unter „Gehen und Rennen nach Clips“ in [Optimierungen](Optimierungen.md): das Becken in Wendungen
und beim Bremsen zusätzlich so tief wie der Gang der Bibliothek, das Becken vor dem Aufsetzen vorhersagend senken wie
ohne Clips, die Hocke des Rennens auch mit Clips und die Felder der Clips am Ende der Figur statt in ihrer Mitte.

Wo es noch fehlt: Die Beine schwingen weiter nach dem Gang der Bibliothek, nicht nach dem Clip, das ist Stufe 2: der Weg
eines schwingenden Fußes und seine Rolle über Ferse und Zehen aus dem Clip, auf die Fußpunkte der Bibliothek verzogen.
Losgehen, Anhalten und Wenden mischt die Bibliothek aus dem Stand-Clip und den beiden Clips des Gangs, eigene Clips
dafür sind Stufe 3. Auf Treppen nimmt das Becken das Auf und Ab des Clips nicht mit, rennend hinauf geht es ruhig
entlang der Linie der Treppe, ruhiger als ohne Clips und wohl ruhiger als ein Mensch. Rennend neigt sich die Brust nach
Treffern bis 45 N s im Mittel 2 bis 4° weiter als ohne Clips, mit 4,8 m/s und 15 N s im Mittel 13,0 statt 9,8°, warum,
ist offen. Ein Clip des Spiels mit anderen Schritten als der Gang der Bibliothek ist noch nicht gemessen, die Tests
haben nur die aufgezeichneten Clips und solche mit gehobenen Armen und gehobener Wurzel. Eine gesteuerte Figur ohne
Clips kostet im Update der Figuren auf der VM mit dem endgültigen Bau in acht Paaren im Wechsel 8,58 µs, auf
`active-ragdoll-97` 9,14 µs, gepaart 0,56 µs weniger, im Median 0,27 µs weniger. Als GCC `nbClipSegment` noch aufrief,
statt es einzubauen, waren es in vier Sätzen von acht Paaren +0,24, +0,41, +0,15 und −0,64 µs mehr, über alle 32 Paare
+0,04 µs. Mit Clips kostet sie in acht Paaren 0,72 µs weniger als ohne, 8,44 statt 9,16 µs, im Median 0,55 µs weniger:
Sie rechnet das Absenken vor dem Aufsetzen und die Pose aus Regeln nicht. Für Figuren ohne Steuerung, 50 Kämpfer und 50
Figuren, die stehen, zucken, umfallen und aufstehen, zählt Valgrind mit einem Thread 0,07 % mehr Instruktionen in der
Bibliothek als auf `active-ragdoll-97`, bei denselben Ergebnissen und gleich vielen Fehlgriffen in den simulierten
Caches, siehe „Clips abtasten ohne Aufruf“ in [Optimierungen](Optimierungen.md). Im Benchmark bleiben alle Hashes und
Zähler. Nur wie viele Schritte Box3D mit Paketen rechnet, schwankt: Warten seine Threads auf der VM lange auf die
letzten Pakete, hält es sie eine Weile zurück, mit denselben Ergebnissen, so zählten mit beiden Bauten einzelne Läufe
weniger. Die Bildzeiten der fünf Szenen mit Städten liegen mit dem endgültigen Bau in acht Paaren im Wechsel bei 0,78
bis 1,26 von `active-ragdoll-97`, im Mittel bei 0,96 bis 1,05 und im Median bei 0,93 bis 1,08, auch in den drei Städten
ohne Figur, deren Code gleich ist, und das Update des Figuren-Benchmarks liegt über alle Phasen im Median bei 1,00: im
Rauschen der VM.

Wenden und Landen ohne Ruck, Runde 99, F84, auf Wunsch des Nutzers. Nach Runde 98 schrieb er „ich dachte wir machen den
controller vor den clips fertig“. Ich hatte seinen Satz, die Figuren sollen im Allgemeinen Animationen folgen und bei
einem Treffer in den Controller wechseln, als Auftrag für die Clips gelesen. Runde 98 bleibt, denn ohne Clips rechnet
sie bitgleich und ändert den Controller nicht, und F85 ruht, bis der Controller fertig ist. Er nahm den Vorschlag an,
mit dieser Runde zu den Sprüngen in Wendungen, Tempowechseln, beim Anhalten und beim ersten Schritt zurückzukehren, auf
`active-ragdoll-98`.

Gemessen in einem Labor mit der Bibliothek, ohne Clips, bei 60 Hz mit 4 Teilschritten und gesteuert wie in der Demo: Die
Tasten setzen das Tempo sofort, und die Figur schaut, wohin sie geht. 15 Szenen, jede an 24 Stellen im Gang begonnen:
Losgehen mit 1,6 m/s, Losrennen mit 3,0 und 4,8 m/s, Anhalten aus 1,6, 3,0 und 4,8 m/s, Wechsel von 1,6 auf 4,8 m/s und
zurück, Wendungen um 90 und 180° gehend mit 1,6 und rennend mit 4,8 m/s und gleichmäßig mit 1,6, 3,0 und 4,8 m/s. Ein
Sprung ist wie bisher die zweite Differenz des Ursprungs eines Knochens von Update zu Update, gezählt das Schlimmste
über die 24 Stellen.

Die Ursachen, gemessen auf `active-ragdoll-98`: Rennt die Figur in eine Kehrtwende, bremst ihre Kapsel, solange sie sich
dreht, denn quer zu ihrem Blick und rückwärts geht sie höchstens 0,3 m/s, und ihr Gang geht ins Gehen über. Gehend
bleibt ein Fuß stehen, solange der andere schwingt, auch ein Fuß weit hinter der Hüfte, den sein Bein selbst über die
Zehen gerollt nicht mehr erreicht. Er zog die Wurzel nach unten, ohne dass sie so weich herunterkam wie vor einer
Landung, 12 cm in zwei Updates, und Becken und Hände sprangen 8,5 cm in einem Update. Und ein schwingender Fuß hält sein
Ziel ab 0,8 seines Schwungs, weich über 0,7 bis 0,9, `NB_GAIT_AIM` und `NB_GAIT_AIM_FADE`. Eilt ein Schwung, bis 2,5-mal
so schnell, weil das Bein den anderen Fuß bald nicht mehr erreicht, sind diese Teile ein oder zwei Updates lang: Das
Ziel lief bis zuletzt mit dem Rahmen, den die Figur beim Landen erwartet, und der Fuß landete mit voller
Geschwindigkeit, gehend um 90° mit 5,4 cm in einem Update, rennend in der Kehrtwende mit 8,1 cm und losrennend mit 4,0
cm.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten auf `active-ragdoll-98` und vor dem sauberen Bau, das
Schlimmste über die 24 Stellen: 1. Rennend in der Kehrtwende springen Becken und Hände in einem Update höchstens 3,0 cm,
auf `active-ragdoll-98` 8,51 und 8,50 cm. 2. Nach einem eiligen Schwung landen die Füße gehend um 90° mit höchstens 5,0
cm in einem Update, rennend in der Kehrtwende mit höchstens 7,5 cm und losrennend aus dem Stand mit 3,0 und 4,8 m/s mit
höchstens 3,7 cm, vorher 5,41, 8,07 und 4,01 cm. 3. In jeder anderen Szene und Klasse, Füße, Schienbeine, Hände, andere
Knochen und Becken, springt nichts mehr als 0,3 cm stärker. 4. Gleichmäßiges Gehen und Rennen, hier mit 1,6, 3,0 und 4,8
m/s und walk10, walk16, walk20, run30 und run48 von Runde 87, gibt dieselben Zahlen. 5. In den 48 Übergängen von Runde
88 höchstens 14 Updates über den Grenzen, vorher 14. 6. In der Posen-Tabelle von Runde 98, 20 Arten je 32 Momente, ist
je Art das Becken, der Abstand der Füße und der der Knie höchstens 1 cm schlechter, Standknie und Spagat höchstens 2°.
7. Treppen: hinunter mit 4,5 m/s über 96 Starts im Abstand von 1 cm nicht mehr Starts mit über 10 cm Wippen als auf
`active-ragdoll-98`, 6, und nicht mehr als 20,7 cm, in den anderen fünf Fällen der 20 Starts je Fall höchstens 0,5 cm
mehr Wippen, kein Fuß verloren und die Auflage nicht kleiner. 8. Über Schutt, 222 Läufe, schlechte Posen in höchstens
2,5 % der Updates, vorher 1,98 %, Landungen auf einer Spitze in höchstens 15 %, in der Luft in höchstens 4,0 %. 9. Von 1
024 Treffern gehen höchstens 2 % anders aus. 10. Mit den aufgezeichneten Clips des Gangs liegen die Knochen gehend und
rennend im Mittel höchstens 0,5 und höchstens 1,5 cm neben dem Gang ohne Clips, wie in Runde 98. 11. Tests mit gcc und
clang grün, ein Hash ändert sich nur, wo eine gesteuerte Figur wendet oder rennt und dabei eilt, mit 1, 2 und 4 Threads
bitgleich. 12. Eine gesteuerte Figur kostet gepaart höchstens 0,3 µs mehr als auf `active-ragdoll-98`. 13. Im Benchmark
bleiben die Hashes, die Bildzeiten im Rauschen. Die Grenzen 1, 2, 5 und 8 liegen über dem, was der Prototyp erreichte:
2,58 und 2,80 cm, 4,81, 7,21 und 3,62 cm, 13 Updates und 2,10 %.

Erfüllt sind zwölf der 13, die zwölfte über alle Paare, siehe unten. Die sechste nur im Mittel und im Schlimmsten je
Art, im 90. Perzentil nicht. Das Schlimmste in einem Update über die 24 Stellen:

| In einem Update | `active-ragdoll-98` | `active-ragdoll-99` |
| --- | --- | --- |
| Kehrtwende rennend, Becken | 8,51 cm | 2,58 cm |
| Kehrtwende rennend, Hände | 8,50 cm | 2,77 cm |
| Kehrtwende rennend, Füße | 8,07 cm | 7,18 cm |
| Kehrtwende rennend, Schienbeine | 7,34 cm | 6,94 cm |
| Wendung um 90° gehend, Füße | 5,41 cm | 4,85 cm |
| Losrennen mit 3,0 und 4,8 m/s, Füße | 4,01 cm | 3,61 cm |
| Losrennen, Schienbeine | 4,15 cm | 4,21 cm |
| Kehrtwende gehend, Becken | 1,65 cm | 1,73 cm |
| Übergänge von Runde 88 über den Grenzen | 14 Updates | 13 |

In den anderen Szenen springt nichts mehr als 0,08 cm stärker: gehend in der Kehrtwende das Becken 1,73 statt 1,65 cm
und die Hände 1,76 statt 1,69 cm, losrennend die Schienbeine 4,21 statt 4,15 cm. Gleichmäßiges Gehen und Rennen, das
Anhalten, die Wechsel zwischen Gehen und Rennen und rennend die Wendung um 90° geben dieselben Zahlen wie vorher, ebenso
walk10 bis run48 von Runde 87. Von dessen Übergängen springt gehend um 90° ein Fuß 3,23 statt 3,07 cm, und das Becken
sinkt dort 4,3 statt 4,0 cm unter das Gehen. In der Posen-Tabelle ist je Art im Mittel das Becken höchstens 0,2 cm
tiefer, der Abstand der Füße 0,4 cm und der der Knie 0,2 cm kleiner, das Standknie 0,1° und der Spagat 0,5° stärker, im
Schlimmsten das Becken 0,6 cm tiefer, das Standknie 1,0° und der Spagat 0,5° stärker, die Abstände gleich. Im 90.
Perzentil kreuzen sich aber gehend in Wendungen um 90° nach beiden Seiten die Füße 2,0 cm mehr, 13,7 statt 11,7 und 17,1
statt 15,1 cm, gegen die sechste Bedingung, und die Knie dort bis 0,9 cm mehr. Das kommt vom gehaltenen Ziel, denn die
Regel für den Fuß hinter der Hüfte wirkt gehend mit 1,6 m/s nicht, und für jede Figur hätte sie es ausgeglichen, siehe
unten. Auf der Treppe hinunter mit 4,5 m/s wippt das Becken über 96 Starts wie vorher bei 6 über 10 cm, höchstens 20,7
cm, bei fünf derselben Starts und statt des Starts 0,88 m bei 0,85 m, siehe unten. In den anderen fünf Fällen wippt es
gleich oder bis 0,1 cm weniger, kein Fuß verliert seine Stufe, und die Auflage bleibt: Gehend hinauf steht wie vorher
bei einem Start ein Fuß nicht auf seiner Stufe und steckt 2,7 statt 2,4 cm in der nächsten. Über Schutt hat die Figur in
2,10 statt 1,98 % der Updates eine schlechte Pose, ein Standknie über 120° in 49 statt 42 von 71 793 Updates, landet in
14,0 statt 13,9 % auf einer Spitze und in 3,6 statt 3,7 % in der Luft. Dort verändert jede Änderung an den Schritten die
Läufe, und die Zahlen springen um bis zu 0,2 Prozentpunkte: Die Regel für den Fuß hinter der Hüfte allein brachte 2,00
%. Von 1 024 Treffern geht keiner anders aus. Mit den aufgezeichneten Clips liegen die Knochen gehend und rennend so nah
am Gang ohne Clips wie in Runde 98.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet`, ohne eine Zeile in Demo oder Benchmark. Gehend bleibt ein Fuß
stehen, solange der andere schwingt, außer die Figur will rennen, ihr Wunschtempo über `NB_GAIT_RUN_FROM` in Einheiten
der Wurzel aus g mal Standhöhe, und das Bein erreicht ihn hinter der Hüfte auch über die Zehen gerollt nicht mehr: Dann
hebt er ab, und für einen Augenblick sind beide Füße in der Luft wie bei einem Menschen, der aus dem Gehen losläuft. Für
jede Figur, auch eine, die gehen will, war die Kehrtwende gleich gut, über Schutt waren es aber 2,15 statt 2,00 %
schlechte Posen, mit dem gehaltenen Ziel 2,20 statt 2,10 %, deshalb gilt die Regel nur, wenn die Figur rennen will.
Gehend in Wendungen um 90° kreuzten sich so aber die Füße im 90. Perzentil nur 0,6 cm mehr und in die andere Richtung
1,4 cm weniger als auf `active-ragdoll-98`, statt je 2,0 cm mehr. Über Schutt springen die Zahlen mit jeder Änderung an
den Schritten um bis zu 0,2 Prozentpunkte, die Wahl war also knapp und ist nachzumessen. Ein schwingender Fuß hält sein
Ziel außerdem nach der Zeit, die sein Schwung noch braucht: über die letzten 0,08 s vor dem Landen mehr und mehr,
`NB_GAIT_AIM_SETTLE`, ab 0,027 s ganz, `NB_GAIT_AIM_SETTLED`, und dann plant er es nicht mehr neu. Beim Menschen der
Bibliothek hält bei einem Schwung, der nicht eilt, der Anteil des Schwungs das Ziel immer schon fester als die Zeit, so
rechnen gleichmäßiges Gehen und Rennen bitgleich wie vorher. Die Hashes von `SteerDeterminismTest` und
`GaitClipDeterminismTest` sind neu, ihr Parcours wendet und rennt mit eiligen Schwüngen. Alle anderen Tests geben
dieselben Zahlen aus wie auf `active-ragdoll-98`, mit gcc und mit clang, und mit 1, 2 und 4 Threads rechnet die Figur
bitgleich.

Was nicht ging, steht unter „Wenden und Landen ohne Ruck“ in [Optimierungen](Optimierungen.md): die Regel für den Fuß
hinter der Hüfte für jede Figur, der Vorlauf aus dem Tempo beim Landen ein viertes Mal, längere Zeiten für das gehaltene
Ziel, das Rollen über die Zehen mit begrenzter Beschleunigung und ein frisch gelandeter Fuß, der eine Weile stehen
bleibt, bevor er als verloren abhebt.

Was die 96 Starts zeigten: Rennend die Treppe hinunter mit 4,5 m/s hängt das Wippen empfindlich am Start. Über die 20
Starts im Abstand von 5 cm, wie in den Runden davor gemessen, wippte das Becken auf `active-ragdoll-98` höchstens 7,0
cm, über 96 Starts im Abstand von 1 cm aber bei 6 Starts über 10 cm, bei 0,01 bis 0,03 m bis 20,7 cm. Mit dieser Runde
liegt einer davon, 0,85 m, auf dem Raster der 20 Starts und wippt 14,2 cm. Erkennt die Figur die Stufen erst im Flug,
springen die Ziele beider Füße um fast einen Meter. Das ist vor dem Vorlauf aus dem Tempo beim Landen zu beheben: Er
brächte beim ersten Schritt aus dem Stand den Fuß auf 2,6 und das Schienbein auf 2,7 cm, ließ dort aber 10 statt 6
Starts über 10 cm wippen.

Wo es noch fehlt: Rennend in der Kehrtwende landen die Füße noch mit 7,2 cm in einem Update, beim Herauslaufen wandern
ihre Ziele bis 15 cm in einem Update, und die Schienbeine springen 6,9 cm. Schienbeine springen beim Abrollen, Abheben
und Aufsetzen in Wendungen 5,2 bis 5,7 cm, beim Wechsel von 1,6 auf 4,8 m/s 4,9 cm, beim Anhalten aus 4,8 m/s 4,7 cm und
losrennend 4,2 cm. Das Becken springt rennend in der Kehrtwende noch 2,58 cm, gehend 1,73 cm und beim Wechsel von 1,6
auf 4,8 m/s 1,65 cm, das Ziel ist unter 1 cm, und gehend in der Kehrtwende hebt ein vorn gelandeter Fuß beim Umkehren
gleich wieder ab. Gehend um 90° kreuzen sich die Füße an einigen Stellen 2 cm mehr als vorher, mit der Regel für den Fuß
hinter der Hüfte für jede Figur nicht. Eine gesteuerte Figur ohne Clips kostet im Update der Figuren auf der VM gepaart
in vier Sätzen von acht Paaren im Wechsel +0,33, −0,22, −0,77 und +0,44 µs mehr als auf `active-ragdoll-98`, über alle
32 Paare −0,06 µs, im Median −0,02 µs, bei 8,7 bis 9,6 µs. Die Sätze nach dem ersten kamen, weil dieser über der Grenze
lag und die Paare um 1,1 µs streuen. Die Figuren dieser Messung wenden alle 2 s vom Gehen ins Rennen und zurück, gerade
dort, wo die neue Regel wirkt. Im Benchmark bleiben alle Hashes und Zähler, er hat keine gesteuerte Figur. Die
Bildzeiten der fünf Szenen mit Städten liegen in acht Paaren im Wechsel bei 0,81 bis 1,11 von `active-ragdoll-98`, im
Mittel bei 0,93 bis 0,99 und im Median bei 0,94 bis 1,00, und das Update des Figuren-Benchmarks liegt über alle Phasen
im Median bei 1,00, je Phase bei 0,94 bis 1,06: im Rauschen der VM.

Seitwärts, schräg und rückwärts gehen und rennen, Runde 100, F84, auf Wunsch des Nutzers. Nach Runde 99 schrieb er „also
verbessere jetzt bitte das laufen und gehen und drehen und seitlich laufen und so weiter“. Gehen, Rennen, Wenden und die
Wechsel dazwischen haben die Runden 87 bis 99 gemessen und verbessert, für eine Figur, die schaut, wohin sie geht. Eine
Figur, die in eine Richtung schaut und in eine andere geht, wie ein Spieler, der beim Gehen zielt,
`nbSteer::faceVelocity` false mit `nbSteer::heading`, war nie entworfen: Die Demo ließ die Figur immer schauen, wohin
sie ging, und der Gang plante seine Schritte in der Richtung des Blicks.

Gemessen in einem Labor mit der Bibliothek, ohne Clips, wo nicht anders gesagt, bei 60 Hz mit 4 Teilschritten und mit
festem Blick nach vorn: gleichmäßig seitwärts mit 1,0, 1,6 und 3,0 m/s nach links und rechts, schräg vorwärts mit 1,6,
3,0 und 4,8 m/s, schräg rückwärts mit 1,6 und 3,0 m/s und rückwärts mit 1,6 und 3,0 m/s, zum Vergleich geradeaus mit 1,6
und 4,8 m/s, und 21 Übergänge, jeder an 24 Stellen im Gang begonnen, 504 Stellen: Losgehen seitwärts, schräg und
rückwärts, Wechsel von links nach rechts, von vorwärts auf seitwärts, von seitwärts auf rückwärts und von rückwärts auf
vorwärts, Anhalten, Wechsel zwischen Gehen und Rennen seitwärts und ein Blick, der sich dreht, während die Figur
seitwärts oder rückwärts geht. Ein Sprung ist wie bisher die zweite Differenz des Ursprungs eines Knochens von Update zu
Update, den Abstand der Füße misst das Labor quer zum Becken, negativ heißt gekreuzt. Antwort heißt, wann die Figur 90 %
des gewünschten Tempos in seiner Richtung erreicht.

Die Ursachen, gemessen auf `active-ragdoll-99`: Schaute die Figur nicht, wohin sie ging, zeigten Becken und Beine weiter
nach vorn, und sie trat zur Seite über ihre eigenen Füße. Mit 1,6 m/s ging das Becken bis 29 cm tiefer als im Stand, mit
3,0 m/s kreuzten sich die Füße um 56 cm, und Füße, Schienbeine, Hände und Becken sprangen bis 146, 131, 32 und 31 mm in
einem Update. Rückwärts rennend blieb ein Fuß vor der Hüfte stehen, bis der andere landete, auch wenn das Bein ihn
längst nicht mehr erreichte: Das Becken ging mit 3,0 m/s 37 cm tief, schräg rückwärts 50 cm, und Schienbeine sprangen
bis 226,5 mm.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten auf `active-ragdoll-99` und vor dem sauberen Bau, der
Stand von `active-ragdoll-99` in eckigen Klammern: 1. Gleichmäßig seitwärts, schräg und rückwärts sind die Füße nie
gekreuzt und quer zum Becken mindestens 9 cm auseinander [−56 cm], das Becken ist höchstens 13 cm tiefer als im Stand
[50 cm], das Standknie höchstens 65° gebeugt [100°], und Füße, Schienbeine, Hände und Becken springen in einem Update
höchstens 30, 45, 12 und 12 mm [146, 226, 167 und 165 mm]. 2. In den Übergängen liegen von den 504 Stellen höchstens
190, 280, 185 und 280 über den Grenzen von Füßen, Schienbeinen, Händen und Becken, 37, 37, 15 und 10 mm in einem Update,
höchstens 60 mit dem Becken über 15 cm tief und höchstens 90 mit gekreuzten Füßen [432, 452, 418, 421, 364 und 167], und
im Mittel über die Szenen liegt das 90. Perzentil der Sprünge bei höchstens 42, 53, 21 und 19 mm [109, 113, 66,7 und
65,8 mm], das der Tiefe des Beckens bei höchstens 13,5 cm [30,0 cm]. 3. Aus dem Stand seitwärts erreicht die Figur
gehend und rennend 90 % des Tempos nach höchstens 0,45 s [0,15 s, mit Beinen nach vorn], beim Wechsel von links nach
rechts nach höchstens 1,05 s. Zum Vergleich eine Figur, die schaut, wohin sie geht: zur Seite nach 0,40 s, in der
Kehrtwende nach 0,92 und 1,02 s. 4. Mit den aufgezeichneten Clips des Gangs sind seitwärts mit 1,6 und 3,0 m/s, schräg
mit 4,8 m/s und rückwärts mit 3,0 m/s die Füße nie gekreuzt und mindestens 9 cm auseinander [−56 cm], und Füße,
Schienbeine, Hände und Becken springen höchstens 30, 40, 12 und 6 mm [84,5, 179, 72,9 und 72,5 mm], rückwärts mit 1,6
m/s ist keine Klasse mehr als 0,3 cm schlechter. 5. Figuren, die schauen, wohin sie gehen, mit und ohne Clips, rechnen
Byte für Byte wie auf `active-ragdoll-99`, alle Hashes der Tests bleiben. 6. Tests mit gcc und clang grün, mit 1, 2 und
4 Threads bitgleich, dazu ein Test, in dem eine Figur seitwärts geht, rennt und rückwärts rennt, ohne die Füße zu
kreuzen und ohne tiefer als 15 cm zu sacken. 7. Eine gesteuerte Figur, die schaut, wohin sie geht, kostet gepaart
höchstens 0,3 µs mehr, die einer seitwärts gehenden wird berichtet. 8. Im Benchmark bleiben die Hashes, die Bildzeiten
im Rauschen. Die Grenzen liegen über dem, was der Prototyp erreichte.

Erfüllt sind sieben der acht, die fünfte nicht ganz: Figuren, die schauen, wohin sie gehen, rechnen mit und ohne Clips
Byte für Byte wie vorher, aber der Hash von `GaitClipDeterminismTest` ist neu. Sein Parcours lässt eine Figur mit
`faceVelocity` false auf der Stelle wenden, während sie noch mit 1,5 m/s geht, und ihre Beine halten jetzt ihre
Richtung, während sich ihr Blick dreht. Die Bedingung war in der Annahme geschrieben, dass kein Test eine Figur so
steuert. Gleichmäßig, das Schlimmste nach den ersten 3 s, von links und rechts das Schlechtere:

| Gleichmäßig | `active-ragdoll-99` | `active-ragdoll-100` |
| --- | --- | --- |
| Seitwärts mit 1,6 m/s, Becken unter dem Stand | 28,7 cm | 8,3 cm |
| Seitwärts mit 1,6 m/s, Füße, Schienbeine, Hände und Becken in einem Update | 85,5, 78,3, 77,3 und 76,8 mm | 15,0, 26,2, 7,1 und 6,4 mm |
| Seitwärts mit 3,0 m/s, Füße quer zum Becken | 56,0 cm gekreuzt | 14,7 cm auseinander |
| Seitwärts mit 3,0 m/s, Becken unter dem Stand | 29,5 cm | 9,1 cm |
| Seitwärts mit 3,0 m/s, in einem Update | 146, 131, 32,4 und 30,5 mm | 23,1, 35,3, 6,1 und 2,8 mm |
| Rückwärts mit 3,0 m/s, Becken unter dem Stand | 37,0 cm | 9,6 cm |
| Rückwärts mit 3,0 m/s, Standknie | 97° | 59° |
| Rückwärts mit 3,0 m/s, in einem Update | 56,3, 177, 71,1 und 69,1 mm | 20,2, 28,8, 5,9 und 3,1 mm |
| Schräg rückwärts mit 3,0 m/s, Becken unter dem Stand | 49,9 cm | 10,1 cm |
| Schräg rückwärts mit 3,0 m/s, in einem Update | 111, 226,5, 166,8 und 164,8 mm | 20,2, 29,0, 5,8 und 2,8 mm |
| Schräg rückwärts mit 1,6 m/s, Becken unter dem Stand | 24,6 cm | 10,7 cm |
| Schräg vorwärts mit 3,0 m/s, Füße quer zum Becken | 28,3 cm gekreuzt | 14,6 cm auseinander |
| Schräg vorwärts mit 4,8 m/s, Füße quer zum Becken | 46,6 cm gekreuzt | 14,1 cm auseinander |
| Rückwärts mit 1,6 m/s, Becken unter dem Stand | 9,8 cm | 7,6 cm |

In den Übergängen, die Stellen über den Grenzen und das 90. Perzentil im Mittel über die 21 Szenen:

| Übergänge, 504 Stellen | `active-ragdoll-99` | `active-ragdoll-100` |
| --- | --- | --- |
| Stellen über den Grenzen: Füße, Schienbeine, Hände und Becken | 432, 452, 418 und 421 | 174, 260, 169 und 238 |
| Stellen mit dem Becken über 15 cm tief und mit gekreuzten Füßen | 364 und 167 | 52 und 80 |
| 90. Perzentil im Mittel: Füße, Schienbeine, Hände und Becken | 109, 113, 66,7 und 65,8 mm | 38,8, 49,5, 19,1 und 17,4 mm |
| 90. Perzentil im Mittel: Becken unter dem Stand | 30,0 cm | 12,6 cm |
| Aus dem Stand seitwärts mit 1,6 m/s, 90 % des Tempos nach | 0,15 s | 0,42 s |
| Wechsel von links nach rechts mit 1,6 und 3,0 m/s, 90 % des Tempos nach | 0,30 und 0,57 s | 0,78 und 1,00 s |

Mit den aufgezeichneten Clips des Gangs:

| Mit Clips | `active-ragdoll-99` | `active-ragdoll-100` |
| --- | --- | --- |
| Seitwärts mit 1,6 m/s, Becken unter dem Stand | 28,4 cm | 3,5 cm |
| Seitwärts mit 1,6 m/s, Füße, Schienbeine, Hände und Becken in einem Update | 68,7, 52,9, 72,9 und 72,5 mm | 15,0, 20,2, 4,3 und 3,7 mm |
| Seitwärts mit 3,0 m/s, Füße quer zum Becken | 56,4 cm gekreuzt | 14,6 cm auseinander |
| Schräg vorwärts mit 4,8 m/s, Füße quer zum Becken | 48,2 cm gekreuzt | 14,0 cm auseinander |
| Rückwärts mit 3,0 m/s, Becken unter dem Stand | 37,7 cm | 10,8 cm |
| Rückwärts mit 3,0 m/s, in einem Update | 56,6, 179, 66,4 und 61,8 mm | 19,6, 24,6, 8,4 und 2,9 mm |
| Rückwärts mit 1,6 m/s, in einem Update | 24,4, 67,3, 11,0 und 11,9 mm | 26,0, 60,8, 12,4 und 13,1 mm |

Schlechter als vorher sind gleichmäßig nur Kleinigkeiten: Schräg vorwärts mit 1,6 m/s springen Hände und Becken in einem
Update 7,2 und 6,6 statt 6,0 und 4,7 mm, und das Becken geht 7,0 statt 6,2 cm tief, rückwärts mit 1,6 m/s springen die
Füße 26,0 statt 24,4 mm. Geradeaus mit 1,6 und 4,8 m/s schaut die Figur, wohin sie geht, und rechnet bitgleich wie
vorher. Die Kapsel kommt langsamer auf Tempo, weil sie nicht schneller geht, als die Beine sie tragen: Aus dem Stand
seitwärts erreicht sie 90 % des Tempos nach 0,42 statt 0,15 s, beim Wechsel von links nach rechts nach 0,78 und 1,00
statt 0,30 und 0,57 s, eine Figur, die schaut, wohin sie geht, zur Seite nach 0,40 s. Auf `active-ragdoll-99` war die
Kapsel schneller dort, aber die Beine folgten ihr nicht. In den Übergängen wird weniges schlechter: Gehend kreuzen sich
die Füße beim Anhalten aus dem Seitwärtsgehen bis 17 cm, beim Wechsel von seitwärts auf rückwärts bis 15 cm, von links
nach rechts bis 14 cm und von vorwärts auf seitwärts bis 9,6 cm, wo `active-ragdoll-99` sie nicht kreuzte, dafür aber
das Becken 28 bis 38 cm tief sacken ließ, jetzt 16 bis 23 cm. Schräg aus dem Stand springen die Füße 49 statt 29 mm in
einem Update, seitwärts aus dem Stand die Schienbeine 60 statt 56 mm, von rückwärts auf vorwärts gehend die Schienbeine
65 statt 49 mm, und rennend im Wechsel von seitwärts auf rückwärts die Füße im Schlimmsten 98 statt 88 mm, im 90.
Perzentil 65 statt 86 mm.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet` und `nbMoveSteered`, ohne eine Zeile in Demo oder Benchmark
außer dem Aufruf. Schaut eine gesteuerte Figur nicht, wohin sie geht, drehen Becken und Beine zur Richtung, in die ihr
Gang geht, gehend bis 70° und rennend bis 90° vom Blick, `NB_LEG_TURN_WALK` und `NB_LEG_TURN_RUN`, unter 0,5 m/s
weniger, wie bei Menschen, die in eine Richtung schauen und in eine andere gehen. Die Wirbelsäule dreht die Brust um
drei Viertel davon zurück, `NB_LEG_TURN_SPINE`, Hals und Kopf den Rest, so schaut der Kopf in die Blickrichtung. Geht
sie mehr als 110° vom Blick weg, gehen die Beine rückwärts, zum Blick hin gedreht, bis sie wieder unter 70° geht,
`NB_LEG_BACK` und `NB_LEG_FORWARD`. Dreht sich nur der Blick, halten die Beine ihre Richtung in der Welt und drehen sich
zu ihrem Ziel so schnell, wie sich der Blick drehen darf, `nbTurnToward` mit `turnRate` und `turnAcceleration` der
Steuerung. Hält die Figur an, behalten sie ihre Richtung, bis ihr Gang langsamer als 0,05 m/s geht, `NB_LEG_TURN_STILL`,
und steht sie, gehen sie mit dem Blick. Schritte, Schwünge, Landungen und der Stand eines Fußes planen in der Richtung
der Beine, und die Vorhersage der Hüfte rechnet ihre Drehung mit. Quer zu den Beinen geht die Kapsel nicht schneller als
0,3 m/s, 0,17 ihres Wunschtempos oder so weit quer, wie die gedrehten Beine noch quer gehen, und gegen sie nicht
schneller als 0,3 m/s, wie eine Figur, die schaut, wohin sie geht: Sie behält die gewünschte Richtung und wird so
schnell, wie die Beine sie tragen. Rückwärts rennend hebt ein Fuß vor der Hüfte, von dem die Figur weggeht, ab, sobald
das Bein ihn nicht mehr erreicht, wie vorwärts rennend ein Fuß hinter der Hüfte, und sein Schwung hebt ihn so sanft an,
wie ein rennender Fuß aufsetzt. Rückwärts gehend ist die Kadenz um 11 % höher, `NB_GAIT_BACK_CADENCE`: Menschen treten
rückwärts bei gleichem Tempo öfter und kürzer, rückwärts rennend auf dem Laufband rund 11 % öfter. Mit Clips des Gangs
drehen Becken und Wirbelsäule mit den Beinen wie ohne. Mit `faceVelocity` true bleibt die Drehung der Beine genau null,
und die Figur rechnet bitgleich wie auf `active-ragdoll-99`.

In der Demo schaut die Spielfigur mit gedrückter Strg-Taste oder mit dem Haken „Blick zur Kamera“ dorthin, wohin die
Kamera schaut, und geht, wohin die Tasten sie schicken. `--faces-camera` setzt den Haken, mit `--walk-side` geht sie den
Parcours dann seitwärts. Vorher festgelegt war die rechte Maustaste, aber die schaltet in der Szene schon den Blick mit
der Maus um. Die Demo setzt dafür nur `heading` und `faceVelocity` von `nbSteer`.

Neu ist `SteerStrafeTest`: Eine Figur geht mit 1,6 m/s seitwärts, rennt mit 3,0 m/s seitwärts und rennt mit 3,0 m/s
rückwärts. Ihre Beine drehen sich 70 und 90° und laufen dann rückwärts, die Füße bleiben quer zum Becken mindestens 9,5
cm auseinander, der Test verlangt 5 cm, das Becken geht höchstens 9,6 cm tiefer als im Stand, verlangt sind 15 cm, und
mit 1, 2 und 4 Threads gibt sie denselben Hash. Der Hash von `GaitClipDeterminismTest` ist neu, siehe oben, alle anderen
Tests geben dieselben Zahlen aus wie auf `active-ragdoll-99`, mit gcc und mit clang.

Was nicht ging, steht unter „Seitwärts, schräg und rückwärts gehen und rennen“ in [Optimierungen](Optimierungen.md):
Beine, die weniger weit drehen, Beine, die mit einer Feder statt mit der Drehrate folgen, eine Kapsel ohne Grenze quer
zu den Beinen, eine schnellere oder langsamere Drehung der Beine, eine höhere Kadenz rückwärts auch rennend oder um mehr
als 11 %, ein Standfuß, der früher mitdreht, die Regel fürs Rückwärtsrennen nach der wirklichen Bewegung und Beine, die
zur gewünschten Richtung statt zur Richtung des Gangs drehen.

Wo es noch fehlt: Gehend kreuzen sich die Füße in Wechseln, siehe oben, und rennend bis 20 cm, auf `active-ragdoll-99`
bis 81 cm. Rennend springen im Wechsel von seitwärts auf rückwärts im 90. Perzentil die Schienbeine 89 mm, Hände und
Becken 62 und 60 mm, und von rückwärts auf vorwärts sackt das Becken bis 31 cm, vorher 64 cm. Aus dem Stand rückwärts
springen die Schienbeine gehend 59 und rennend 57 mm, vorher 64 und 184 mm, und mit Clips rückwärts gehend 61 mm, vorher
67 mm. Die Grenze von 37 mm in einem Update, unter der gleichmäßiges Gehen und Rennen geradeaus liegt, halten die
Wechsel seitwärts und rückwärts noch lange nicht.

Eine gesteuerte Figur, die schaut, wohin sie geht, kostet im Update der Figuren auf der VM gepaart in 16 Paaren im
Wechsel im Median 0,22 µs weniger als auf `active-ragdoll-99`, im Mittel 0,30 µs weniger, bei 8,8 µs, mit Clips des
Gangs in acht Paaren 0,01 µs weniger, bei 8,5 µs. Eine, die seitwärts geht und rennt, kostet 8,8 µs, so viel wie eine,
die schaut, wohin sie geht, und 0,41 µs weniger als auf `active-ragdoll-99`, wo sie über ihre Füße trat. Die Figuren
dieser Messung wechseln alle 2 s vom Gehen ins Rennen in die Gegenrichtung. Im Benchmark bleiben alle Hashes und Zähler,
er hat keine gesteuerte Figur. Die Bildzeiten der fünf Szenen mit Städten liegen in acht Paaren im Wechsel bei 0,83 bis
1,34 von `active-ragdoll-99`, im Mittel bei 0,96 bis 1,08 und im Median bei 0,94 bis 1,04, und das Update des
Figuren-Benchmarks liegt über alle Phasen im Median bei 1,00, je Phase bei 0,86 bis 1,07: im Rauschen der VM.

Wechsel seitwärts und rückwärts ohne gekreuzte Füße, Runde 101, F84. Nach Runde 100 schlug ich als nächste Runde die
Wechsel seitwärts und rückwärts vor, keine gekreuzten Füße beim Anhalten und Umkehren und ruhigere Schienbeine beim
Rennen, und der Nutzer antwortete „ok“. Gemessen im Labor von Runde 100 mit denselben 21 Übergängen, jeder an 24 Stellen
im Gang begonnen, 504 Stellen, den gleichmäßigen Szenen und der Antwort, ohne Clips, wo nicht anders gesagt, bei 60 Hz
mit 4 Teilschritten und mit festem Blick nach vorn.

Die Ursachen, gemessen auf `active-ragdoll-100`: Drehten sich die Beine weit, während ein Fuß am Boden stand, wanderte
dieser Fuß quer zu den Beinen über die Mitte des Beckens auf die Seite des anderen. Beim Anhalten aus dem Seitwärtsgehen
und -rennen kreuzten sich die Füße so bis 17 und 19 cm, beim Wechsel von links nach rechts bis 14 und 20 cm. Ein
schwingender Fuß landete mit der Richtung, die die Beine gerade hatten, während sie sich weiterdrehten. Rennend von
vorwärts auf seitwärts sackte das Becken in einem Update 5,5 cm: Die Füße planten mit der Beschleunigung der Kapsel
erst, als das Tempo, das die gedrehten Beine erlauben, die Schwelle des Rennens überschritt, und dieser Deckel wächst
gegen Ende der Drehung so schnell, dass die Planung in ein bis zwei Updates einsetzte und das Ziel des Fußes 18 cm
sprang. Dazu zwei Ursachen, die diese Runde nicht behebt: Rückwärts landet ein Fuß wie vorwärts auf der Ferse, die Zehen
0,25 rad hoch, und das Becken muss für den Fuß weit hinten tief. Von rückwärts auf vorwärts rennend geht das Tempo durch
null, der Gang wird zum Gehen, der schwingende Fuß landet 0,64 m vor dem Becken, und der stehende hebt nicht ab.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten auf `active-ragdoll-100` und vor dem sauberen Bau, der
Stand von `active-ragdoll-100` in eckigen Klammern: 1. Gleichmäßig seitwärts, schräg und rückwärts sind die Füße nie
gekreuzt, keine Klasse der Sprünge ist mehr als 4 mm schlechter und das Becken nicht mehr als 0,3 cm tiefer. 2. In den
Übergängen liegen von den 504 Stellen höchstens 140, 260, 130 und 205 über den Grenzen von Füßen, Schienbeinen, Händen
und Becken, höchstens 50 mit dem Becken über 15 cm tief und höchstens 28 mit gekreuzten Füßen [174, 260, 169, 238, 52
und 80], und im Mittel über die Szenen liegt das 90. Perzentil der Sprünge bei höchstens 37,5, 49,0, 17,5 und 15,5 mm
[38,8, 49,5, 19,1 und 17,4 mm], das der Tiefe des Beckens bei höchstens 12,3 cm [12,6 cm]. 3. Beim Anhalten aus dem
Seitwärtsgehen und -rennen und beim Wechsel von links nach rechts gehend kreuzen sich die Füße an keiner der 24 Stellen
[17,3, 19,3 und 13,9 cm], beim Wechsel von links nach rechts rennend höchstens 10 cm [19,8 cm] und von vorwärts auf
seitwärts rennend höchstens 12 cm [18,6 cm]. 4. Die Kapsel erreicht 90 % des Tempos im Mittel über die Übergänge nach
höchstens 0,40 s [0,38 s], beim Wechsel von links nach rechts rennend nach höchstens 1,16 s [1,02 s], aus dem Stand
seitwärts wie vorher nach 0,42 und 0,43 s. 5. Mit den aufgezeichneten Clips des Gangs liegen höchstens 35 Stellen mit
gekreuzten Füßen [81] und höchstens 145, 245, 275 und 290 über den Grenzen [175, 267, 291 und 313]. 6. Figuren, die
schauen, wohin sie gehen, rechnen mit und ohne Clips Byte für Byte wie auf `active-ragdoll-100`, und die Hashes der
Tests, die nur solche Figuren steuern, bleiben. 7. Tests mit gcc und clang grün, mit 1, 2 und 4 Threads bitgleich, dazu
ein Test, in dem eine Figur seitwärts geht, anhält und die Richtung wechselt, ohne die Füße zu kreuzen. 8. Eine
gesteuerte Figur, die schaut, wohin sie geht, kostet gepaart höchstens 0,3 µs mehr, die einer seitwärts gehenden wird
berichtet. 9. Im Benchmark bleiben die Hashes, die Bildzeiten im Rauschen. Die Grenzen liegen über dem, was der Prototyp
erreichte.

Erfüllt sind alle neun. Das zweite Ziel der Runde, ruhigere Schienbeine beim Rennen, ist nicht erreicht: In den
Übergängen, in denen die Figur rennt, liegen 135 statt 118 Stellen mit den Schienbeinen über 37 mm, im 90. Perzentil im
Mittel 49,7 statt 50,0 mm. Die größten Ursachen dort sind die Landung rückwärts auf der Ferse und das Umkehren von
rückwärts auf vorwärts, siehe oben, und für beide reicht keine kleine Änderung. In den Übergängen, die Stellen über den
Grenzen und das 90. Perzentil im Mittel über die 21 Szenen:

| Übergänge, 504 Stellen | `active-ragdoll-100` | `active-ragdoll-101` |
| --- | --- | --- |
| Stellen über den Grenzen: Füße, Schienbeine, Hände und Becken | 174, 260, 169 und 238 | 131, 254, 121 und 194 |
| Stellen mit dem Becken über 15 cm tief und mit gekreuzten Füßen | 52 und 80 | 45 und 21 |
| 90. Perzentil im Mittel: Füße, Schienbeine, Hände und Becken | 38,8, 49,5, 19,1 und 17,4 mm | 36,8, 48,0, 16,4 und 14,6 mm |
| 90. Perzentil im Mittel: Becken unter dem Stand | 12,6 cm | 11,9 cm |
| 90 % des Tempos im Mittel über die Übergänge nach | 0,380 s | 0,396 s |
| Wechsel von links nach rechts mit 1,6 und 3,0 m/s, 90 % des Tempos im Schlimmsten nach | 0,80 und 1,02 s | 0,95 und 1,14 s |
| Mit Clips: Stellen mit gekreuzten Füßen | 81 | 28 |
| Mit Clips: Stellen über den Grenzen von Füßen, Schienbeinen, Händen und Becken | 175, 267, 291 und 313 | 138, 231, 260 und 276 |

Wie weit die Füße quer zum Becken auseinander stehen, das Engste über die 24 Stellen, negativ heißt gekreuzt:

| Füße quer zum Becken | `active-ragdoll-100` | `active-ragdoll-101` |
| --- | --- | --- |
| Anhalten aus dem Seitwärtsgehen mit 1,6 m/s | −17,3 cm | +8,0 cm |
| Anhalten aus dem Seitwärtsrennen mit 3,0 m/s | −19,3 cm | +4,1 cm |
| Wechsel von links nach rechts gehend | −13,9 cm | +0,6 cm |
| Wechsel von links nach rechts rennend | −19,8 cm, an 16 Stellen gekreuzt | −8,2 cm, an 3 Stellen |
| Von vorwärts auf seitwärts gehend | −9,6 cm, an 6 Stellen | −0,3 cm, an 1 Stelle |
| Von vorwärts auf seitwärts rennend | −18,6 cm, an 9 Stellen | −10,5 cm, an 6 Stellen |
| Von seitwärts auf rückwärts gehend | −15,1 cm, an 6 Stellen | −12,0 cm, an 4 Stellen |
| Von seitwärts auf rückwärts rennend | −12,0 cm, an 8 Stellen | −5,8 cm, an 6 Stellen |
| Mit Clips: Anhalten gehend und rennend, Wechsel von links nach rechts gehend | −17,3, −19,3 und −13,3 cm | +5,1, +4,0 und +0,8 cm |

Besser wird auch, wie ruhig die Figur in vielen Wechseln ist: Beim Wechsel von links nach rechts springen Hände und
Becken in einem Update höchstens 14,5 und 14,0 statt 30,2 und 29,3 mm rennend und 14,3 und 13,9 statt 26,4 und 26,5 mm
gehend, aus dem Stand seitwärts gehend die Schienbeine 36 statt 60 mm und die Hände 11 statt 23 mm, rennend von vorwärts
auf seitwärts Hände und Becken 40 und 39 statt 54 und 53 mm, und rennend von seitwärts auf rückwärts die Schienbeine im
Schlimmsten 98 statt 149 mm, Hände und Becken 43 und 41 statt 71 und 69 mm.

Schlechter wird: Rennend von vorwärts auf seitwärts springen die Schienbeine im Schlimmsten 80 statt 56 mm, im 90.
Perzentil 58 statt 53 mm, gehend 66 statt 51 mm im Schlimmsten, gehend von seitwärts auf rückwärts die Füße 77 statt 61
mm und die Schienbeine 82 statt 63 mm, das Becken bis 19,1 statt 16,8 cm tief, rennend von seitwärts auf rückwärts die
Schienbeine im 90. Perzentil 93 statt 89 mm, und beim Wechsel vom Gehen ins Rennen seitwärts die Schienbeine an einer
Stelle 84 statt 67 mm, im 90. Perzentil 51 statt 58 mm. Gleichmäßig seitwärts mit 1,6 m/s springen die Füße 18,2 statt
15,0 mm. Die Kapsel kommt in Wechseln später auf Tempo, weil die Beine bis 0,15 s auf die Füße warten und sie quer zu
den Beinen nicht schneller geht, als die Beine sie tragen: im Mittel nach 0,396 statt 0,380 s, beim Wechsel von links
nach rechts im Schlimmsten nach 0,95 und 1,14 statt 0,80 und 1,02 s. Aus dem Stand seitwärts bleibt es bei 0,42 und 0,43
s.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet`, `nbHoldLegs` und `nbMoveSteered`, ohne eine Zeile in Demo oder
Benchmark. Drehen sich die Beine einer Figur, die nicht schaut, wohin sie geht, drehen sie nicht weiter, als jeder Fuß
am Boden quer zu ihnen mindestens 6 cm auf seiner Seite der Mitte bleibt, `NB_LEG_KEEP`, für den Menschen von 1,8 m, und
warten darauf höchstens 0,15 s am Stück, `NB_LEG_HOLD`, dann drehen sie weiter. Ein Fuß am Boden, der sie um mehr als
10° aufhält, hebt ab und tritt, wo seine Phase auch steht, `NB_LEG_BLOCK`, und ein schwingender Fuß landet so weit
gedreht, wie sich die Beine bis zu seiner Landung mit der Drehrate der Steuerung zu ihrem Ziel drehen. Ob die Füße mit
der Beschleunigung der Kapsel planen, `NB_GAIT_PLAN_FROM`, entscheidet das Tempo, das das Programm will, bevor die
gedrehten Beine es deckeln. Mit `faceVelocity` true bleiben die Beine ungedreht, nichts davon greift, und die Figur
rechnet bitgleich wie auf `active-ragdoll-100`.

Neu ist `SteerStrafeChangeTest`: Eine Figur, die nicht schaut, wohin sie geht, hält gehend und rennend aus dem
Seitwärtsgehen an und wechselt gehend von links nach rechts, jeweils von sechs Stellen in ihrem Zyklus aus. Ihre Füße
bleiben quer zum Becken mindestens 8,0, 7,1 und 5,1 cm auseinander, verlangt ist, dass sie sich nicht kreuzen. Neu sind
die Hashes von `SteerStrafeTest`, `SteerDeterminismTest` und `GaitClipDeterminismTest`: In allen dreien steuert der Test
eine Figur, die nicht schaut, wohin sie geht, und in `SteerStrafeTest` stehen die Füße seitwärts gehend 13,1 statt 13,3
cm auseinander, sonst gibt er dieselben Zahlen aus, 14,7 und 9,5 cm Abstand und 8,3, 9,1 und 9,6 cm Tiefe. Alle anderen
Tests geben bis auf gemessene Zeiten dieselben Zahlen und Hashes aus wie auf `active-ragdoll-100`, mit gcc und mit
clang, und die drei neuen Hashes sind mit 1, 2 und 4 Threads und mit beiden Compilern gleich.

Was nicht ging, steht unter „Wechsel seitwärts ohne gekreuzte Füße“ in [Optimierungen](Optimierungen.md): Beine, die
ohne Grenze der Zeit warten, ein Abstand von 3 cm oder keiner, Warten nur beim Anhalten, kürzeres oder längeres Warten,
Beine, die beim Warten langsamer weiterdrehen, nur der aufhaltende Fuß tritt, ohne dass die Beine warten, Beine, die
auch im selben Update nicht weiter drehen als der Platz, ein aufhaltender Fuß, der schneller schwingt, schneller
drehende Beine, Beine, die beim Anhalten warten, bis die Füße stehen, eine Landung mit mehr Drehung, gehend rennen
wollen nach dem gewünschten Tempo und rückwärts flach oder halb auf der Ferse landen.

Wo es noch fehlt: Rennend sind die Schienbeine in Wechseln nicht ruhiger, siehe oben. Rückwärts landet ein Fuß auf der
Ferse, wo Menschen auf den Zehen landen: Aus dem Stand rückwärts springen die Schienbeine weiter 59 und 57 mm, rennend
von seitwärts auf rückwärts im 90. Perzentil 93 mm. Landete er rückwärts flach, gingen in den Übergängen 25 statt 45
Stellen mit dem Becken über 15 cm tief, aber Hände und Becken lagen öfter über der Grenze. Die Landung auf den Zehen
braucht das Abrollen rückwärts gespiegelt. Von rückwärts auf vorwärts rennend sackt das Becken weiter bis 31 cm.
Gekreuzt bleiben die Füße noch von vorwärts auf seitwärts rennend bis 10,5 cm, von seitwärts auf rückwärts gehend bis 12
cm und rennend bis 5,8 cm und beim Wechsel von links nach rechts rennend bis 8,2 cm.

Eine gesteuerte Figur, die schaut, wohin sie geht, kostet im Update der Figuren auf der VM gepaart in acht Paaren im
Wechsel im Median 0,005 µs weniger als auf `active-ragdoll-100`, im Mittel 0,03 µs mehr, bei 8,7 µs. Eine, die seitwärts
geht und rennt, kostet im Median 0,10 µs weniger, bei 8,65 µs. Die Figuren dieser Messung wechseln alle 2 s vom Gehen
ins Rennen in die Gegenrichtung. Im Benchmark bleiben alle Hashes und Zähler, er hat keine gesteuerte Figur. Die
Bildzeiten der fünf Szenen mit Städten liegen in acht Paaren im Wechsel bei 0,79 bis 1,23 von `active-ragdoll-100`, im
Mittel bei 0,97 bis 1,05 und im Median bei 0,98 bis 1,06, die zweite Messung jedes Paares meist langsamer, und das
Update des Figuren-Benchmarks liegt über alle Phasen im Median bei 1,00, je Phase bei 0,91 bis 1,07: im Rauschen der VM.

Rückwärts auf den Zehen und umkehren ohne Absacken, Runde 102, F84. Nach Runde 101 schlug ich als nächste Runde vor,
dass die Figur rückwärts auf den Zehen landet und von rückwärts auf vorwärts rennend umkehrt, ohne dass das Becken
absackt, damit die Schienbeine beim Rennen ruhiger werden, und der Nutzer antwortete „ok“. Gemessen im Labor von Runde
101 mit denselben 21 Übergängen, jeder an 24 Stellen im Gang begonnen, 504 Stellen, den gleichmäßigen Szenen, der
Antwort, den Sprüngen von Zehen und Fersen und den 13 Übergängen von Figuren, die schauen, wohin sie gehen, an je 24
Stellen, ohne Clips, wo nicht anders gesagt, bei 60 Hz mit 4 Teilschritten.

Die Ursachen, gemessen auf `active-ragdoll-101`: Rückwärts landete ein Fuß wie vorwärts auf der Ferse, und ein Fuß vor
der Hüfte, von dem sie sich rückwärts entfernt, rollte nie über die Ferse ab: Das Becken musste tief, damit das Bein ihn
erreicht, stetig rückwärts gehend 7,6 cm unter dem Stehen mit dem Standknie bei 49°, aus dem Stand rückwärts 14 cm. Von
rückwärts auf vorwärts rennend geht das Tempo durch null, der Gang wird kurz zum Gehen, und der schwingende Fuß plante
mit dem Vorlauf des Gehens bei dem Tempo, das die Figur bei seiner Landung hat: Er zielte 0,7 m vor die Hüfte statt etwa
0,26 m, und das Becken ging bis 31 cm tief, damit das Bein hinreicht. Sprang das Ziel eines schwingenden Fußes weit, wie
beim Umkehren, wuchs seine Hubhöhe in einem Update mit der Länge des Schritts, der Knöchel sprang 9 cm hoch und das
Schienbein 106 mm. Und die Sprünge der Füße werden an den Knöcheln gemessen: Ein Fuß, der schnell über die Ferse rollt,
bewegt seine Zehen viel weiter. Rollte er rückwärts nach Bedarf ohne Grenze über die Ferse, sprangen Zehen und Fersen in
den Übergängen an 230 statt 180 Stellen über 37 mm, ohne dass es in den Knochen zu sehen war.

Vorher festgelegt, nach der Suche nach den Ursachen mit Varianten auf `active-ragdoll-101` und vor dem sauberen Bau, der
Stand von `active-ragdoll-101` in eckigen Klammern: 1. Stetig rückwärts gehend mit 1,6 m/s ist das Becken höchstens 2 cm
unter dem Stehen [7,6 cm] und das Standknie höchstens 30° gebeugt [49°], Füße, Hände und Becken springen höchstens 1 mm
mehr, die Schienbeine höchstens 37 mm [26 mm], und rückwärts rennend und schräg rückwärts ist keine Klasse mehr als 4 mm
schlechter. 2. In den Übergängen liegen höchstens 100, 246, 72 und 120 Stellen über den Grenzen von Füßen, Schienbeinen,
Händen und Becken, höchstens 2 mit dem Becken über 15 cm tief und 25 mit gekreuzten Füßen [131, 254, 121, 194, 45 und
21], im Mittel liegt das 90. Perzentil bei höchstens 34,5, 44,5, 13,5 und 11,5 mm [36,8, 48,0, 16,4 und 14,6 mm] und das
Becken höchstens 9,7 cm tief [11,9 cm]. 3. Von rückwärts auf vorwärts rennend geht das Becken an keiner der 24 Stellen
mehr als 11 cm unter das Stehen [31,2 cm], Hände und Becken springen im Schlimmsten höchstens 15 und 12,5 mm [35,9 und
33,6 mm], aus dem Stand rückwärts gehend und rennend geht das Becken höchstens 7 und 14 cm tief [13,6 und 15,1 cm]. 4.
Rennend liegen die Schienbeine an höchstens 130 Stellen über 37 mm [135], im 90. Perzentil im Mittel bei höchstens 45,0
mm [49,7 mm] und im Schlimmsten bei 50 mm [58,2 mm]. 5. Zehen und Fersen springen in den Übergängen an höchstens 175
Stellen über 37 mm [180], bei Figuren, die schauen, wohin sie gehen, an höchstens 63 [64]. 6. Bei diesen Figuren liegen
höchstens 42, 80, 98, 76 und 5 Stellen über den Grenzen von Füßen, Schienbeinen, Händen, Becken und Tiefe [40, 132, 99,
49 und 5], und stetiges Gehen bleibt bitgleich. 7. Mit den aufgezeichneten Clips liegen höchstens 108, 235, 215, 232, 4
und 32 Stellen über den Grenzen [138, 231, 260, 276, 38 und 28]. 8. Die Kapsel erreicht 90 % des Tempos im Mittel über
die Übergänge nach höchstens 0,40 s [0,396 s]. 9. Tests mit gcc und clang grün, mit 1, 2 und 4 Threads bitgleich, dazu
ein Test, in dem eine Figur rückwärts geht und auf den Zehen landet, ohne dass das Becken mehr als 3 cm sackt, und von
rückwärts auf vorwärts rennend umkehrt, ohne dass es mehr als 12 cm sackt. 10. Eine gesteuerte Figur, die schaut, wohin
sie geht, kostet gepaart höchstens 0,3 µs mehr, die einer rückwärts gehenden wird berichtet, und im Benchmark liegen die
Bildzeiten im Rauschen. Die Grenzen liegen über dem, was der Prototyp erreichte.

Im sauberen Bau scheiterte dann `SteerRunStairsTest`: Hinunter mit 4,5 m/s aus 0,0 m wippte das Becken 28,8 cm gegen die
Linie der Treppe. Der Vorlauf mit dem Tempo bei der Landung macht den ersten Schritt aus dem Stand kürzer, und die Figur
kam in einem anderen Takt an die Treppe. Schwach war der Anlauf schon vorher: Über 96 Starts von 0,00 bis 0,95 m wippte
das Becken auf `active-ragdoll-101` bei 6 Starts über 10 cm, bis 20,2 cm, und 0,00 m lag knapp neben einem schlechten
Fenster von 0,01 bis 0,03 m. Mit dem neuen Vorlauf waren es 14 Starts. Die Ursache: Beim Anlauf schwingen beide Füße
zugleich, und beide stellten dasselbe Tempo, mit dem auf einer Treppe ein Fuß seinen Schwung so verlängert oder
verkürzt, dass er auf seiner Stufe landet, wo die Hüfte über sie kommt, `NB_STAIR_RATE`. Der Fuß kurz vor der Landung
bremste es, der andere zielte mit der verlängerten Zeit bis 0,9 m weiter, wählte eine Stufe weiter unten, konnte dann
nicht mehr langsamer als 60 % und landete bis 49 cm vor der Hüfte, und das Becken musste tief, damit das Bein hinreicht.
Vor dem sauberen Bau der Lösung kam als elfte Bedingung dazu: 11. Über 96 Starts hinunter mit 3,0 und 4,5 m/s wippt das
Becken an keinem mehr als 10 cm [8,9 und 20,2 cm], im Mittel höchstens 5,0 und 6,0 cm [5,6 und 6,8 cm], hinauf steht an
höchstens 3 und 0 Starts ein Fuß über einer Kante [2 und 0], ebener Boden bleibt bitgleich zum Bau ohne die Lösung, und
über den Schutt von Runde 97 halten die Grenzen von Runde 97, mit höchstens 0,3 Prozentpunkten mehr schlechten Posen
[2,10 %].

Erfüllt sind alle elf. Die Werte im Vergleich:

| Übergänge, 504 Stellen | `active-ragdoll-101` | `active-ragdoll-102` |
| --- | --- | --- |
| Stellen über den Grenzen: Füße, Schienbeine, Hände und Becken | 131, 254, 121 und 194 | 96, 242, 68 und 116 |
| Stellen mit dem Becken über 15 cm tief und mit gekreuzten Füßen | 45 und 21 | 0 und 23 |
| 90. Perzentil im Mittel: Füße, Schienbeine, Hände und Becken | 36,8, 48,0, 16,4 und 14,6 mm | 34,1, 44,1, 13,0 und 11,0 mm |
| 90. Perzentil im Mittel: Becken unter dem Stand | 11,9 cm | 9,5 cm |
| Rennend: Schienbeine über 37 mm, im 90. Perzentil im Mittel und im Schlimmsten | 135, 49,7 und 58,2 mm | 128, 44,5 und 49,4 mm |
| Zehen und Fersen über 37 mm, Figuren mit festem Blick und solche, die schauen, wohin sie gehen | 180 und 64 | 165 und 61 |
| 90 % des Tempos im Mittel über die Übergänge nach | 0,396 s | 0,397 s |
| Mit Clips: Stellen über den Grenzen von Füßen, Schienbeinen, Händen und Becken | 138, 231, 260 und 276 | 104, 230, 211 und 227 |
| Mit Clips: Becken über 15 cm tief und gekreuzte Füße | 38 und 28 | 2 und 30 |
| Figuren, die schauen, wohin sie gehen, 312 Stellen: Füße, Schienbeine, Hände und Becken über den Grenzen | 40, 132, 99 und 49 | 40, 75, 96 und 74 |

| Rückwärts und auf der Treppe | `active-ragdoll-101` | `active-ragdoll-102` |
| --- | --- | --- |
| Stetig rückwärts gehend mit 1,6 m/s: Becken unter dem Stand und Standknie | 7,6 cm und 49° | 1,2 cm und 26° |
| Von rückwärts auf vorwärts rennend: Becken unter dem Stand, Hände und Becken im Schlimmsten | 31,2 cm, 35,9 und 33,6 mm | 9,6 cm, 13,5 und 11,0 mm |
| Aus dem Stand rückwärts gehend und rennend: Becken unter dem Stand | 13,6 und 15,1 cm | 6,3 und 13,0 cm |
| Von seitwärts auf rückwärts rennend: Schienbeine, Hände und Becken im Schlimmsten | 98, 43 und 41 mm | 61, 23 und 22 mm |
| Treppe hinunter, 96 Starts mit 3,0 und 4,5 m/s: Wippen im Schlimmsten und im Mittel | 8,9 und 20,2 cm, 5,6 und 6,8 cm | 5,6 und 6,2 cm, 4,1 und 5,5 cm |

Besser wird auch, wo die Figur schneller wird: Von vorwärts auf seitwärts rennend springen Füße, Schienbeine, Hände und
Becken im Schlimmsten 42, 59, 22 und 21 statt 65, 80, 40 und 39 mm, das Becken geht 10,0 statt 16,9 cm tief, beim
Wechsel vom Gehen ins Rennen seitwärts springen die Schienbeine 53 statt 84 mm, und bei Figuren, die schauen, wohin sie
gehen, liegen die Schienbeine an 75 statt 132 Stellen über der Grenze, losrennend mit 4,8 m/s springt ein Schienbein 27
statt 42 mm. Rennend hinunter wippt das Becken gegen die Linie der Treppe an keinem von 96 Starts mehr über 10 cm, auf
`active-ragdoll-101` an 6.

Schlechter wird: Stetig rückwärts gehend springen die Schienbeine 36 statt 26 mm in einem Update: Das Bein landet auf
den Zehen fast gestreckt, und wo die Zehen herunterkommen, beugt sich das Knie in drei Updates von 12 auf 21° und
zurück. Aus dem Stand rückwärts gehend springen die Füße 58 statt 48 mm, beim Anhalten aus dem Rückwärtsrennen die
Schienbeine im Schlimmsten 51 statt 38 mm und von seitwärts auf rückwärts gehend 100 statt 82 mm. Gekreuzt sind die Füße
an 23 statt 21 Stellen, von seitwärts auf rückwärts rennend bis 8,3 statt 5,8 cm. Bei Figuren, die schauen, wohin sie
gehen, liegt das Becken an 74 statt 49 Stellen über 10 mm: Losrennend mit 4,8 m/s springt es an allen 24 Stellen 10,5
statt 8,5 mm, und beim Wechsel vom Gehen ins Rennen springen die Schienbeine 60 statt 49 mm. Über den Schutt von Runde
97, 207 Läufe gehend und rennend, hat die Figur in 2,14 statt 2,10 % der Updates eine schlechte Pose und ein Standknie
über 120° in 84 statt 49 von 71 793 Updates, landet aber seltener auf einer Spitze, 13,0 statt 14,0 %. Hinauf mit 3,0
m/s steht über 96 Starts an 3 statt 2 ein Fuß über der Kante der ersten Stufe: Der kürzere erste Schritt aus dem Stand
trifft andere Starts, die Lösung für den Anlauf ändert daran nichts.

Gebaut in der Gangplanung der Bibliothek, `nbSteerFeet`, `nbLowerSteered`, `nbRollToReach` und `nbStairPace`, ohne eine
Zeile in Demo oder Benchmark. 1. Ein Schritt rückwärts, gemerkt beim Abheben, landet auf den Zehen statt auf der Ferse,
so weit gerollt wie vorwärts auf der Ferse, `NB_GAIT_HEEL`, und die Zehen kommen nach der Landung herunter. Im Schwung
geht alles gespiegelt: die Zehen hoch beim Abheben und unten vor der Landung, und über die Ferse rollt der Fuß im
Schwung nicht weiter als beim Abheben, höchstens `NB_FOOT_HEEL_MOST`. Die Hüfte plant die Landung auf den Zehen ein. 2.
Jeder Fuß am Boden rollt über die Kante, hinter der die Hüfte liegt, so weit sein Bein ihn braucht: über die Zehen, wo
die Hüfte vor ihnen liegt, und rückwärts gehend über die Ferse, wo sie hinter ihr liegt, über die Ferse höchstens 0,35
rad und nicht schneller als 6 rad/s, `NB_FOOT_HEEL_MOST` und `NB_FOOT_HEEL_RATE`. Ein Fuß auf der anderen Kante kommt
erst flach, `NB_FOOT_EDGE_FLAT`. Rückwärts hebt ein Fuß hinter der Hüfte in Richtung der Bewegung ab, sobald sein Bein
ihn über die Ferse um 0,2 rad gerollt nicht mehr erreicht, `NB_FOOT_HEEL_LIFT`, rennend um weniger, ganz im Rennen schon
flach, und das Abdrücken über die Zehen kommt, sobald die Hüfte über dem Fuß ist, auch während ein kleiner Rest auf der
Ferse abklingt. 3. Ein schwingender Fuß plant den Vorlauf vor der Hüfte mit dem Tempo, das die Figur bei seiner Landung
hat, statt mit dem jetzigen, `nbGaitLead`, für alle gesteuerten Figuren. 4. Springt das Ziel eines Schwungs weiter weg,
wächst seine Hubhöhe nicht schneller als 1 m/s, `NB_GAIT_LIFT_RISE`. 5. Auf einer Treppe hat jeder schwingende Fuß sein
eigenes Tempo für seine Stufe, bis 40 % schneller oder langsamer, der Takt des Gangs folgt dem Schwung, der zuerst
landet, und ein Fuß behält seine Stufe, solange er sie mit diesem Tempo noch erreicht, `NB_STAIR_RATE`. Auf ebenem Boden
ändert das nichts, die Figur rechnet dort bitgleich wie ohne Punkt 5.

Neu sind zwei Tests. `SteerBackTest`: Eine Figur, die nicht schaut, wohin sie geht, geht rückwärts, und alle 5 Landungen
der gemessenen 2 s sind auf den Zehen, das Becken geht 1,2 cm unter das Stehen, verlangt sind höchstens 3 cm. Rennend
kehrt sie von rückwärts auf vorwärts um, von acht Stellen in ihrem Zyklus aus, und das Becken geht höchstens 9,5 cm
tief, verlangt sind höchstens 12 cm. `SteerStairApproachTest`: Eine Figur rennt mit 4,5 m/s von 48 Starts 2 cm
auseinander die Treppe hinunter, das Becken wippt gegen ihre Linie höchstens 6,0 cm, verlangt sind höchstens 10 cm, und
jeder Fuß steht auf seiner Stufe. `active-ragdoll-101` fällt durch beide, der Bau ohne Punkt 5 durch den zweiten. Die
Läufe von `SteerRunStairsTest` stecken jetzt in `RunStairs`, und er gibt aus, was der neue Gang an der Treppe tut,
hinauf wie vorher im Schlimmsten 7,1 cm Wippen und hinunter 5,9 statt 7,0 cm. Neu sind die Hashes von `SteerStrafeTest`,
`SteerDeterminismTest` und `GaitClipDeterminismTest`, mit 1, 2 und 4 Threads und mit beiden Compilern gleich. Andere
Zahlen geben die Tests aus, in denen eine Figur schneller wird oder auf Kanten und Stufen tritt: `SteerStairsTest`
gehend auf der Treppe 0,5 statt 0,4 cm Wippen hinauf, `SteerRunTest` rennend mit 3,0 m/s das Becken 3,0 statt 2,9 mm und
mit 4,8 m/s den unruhigsten Knochen außer dem Becken 26,2 statt 25,9 mm, `SteerStrafeTest` rückwärts rennend die Füße
quer zum Becken mindestens 9,3 statt 9,5 cm auseinander, `SteerStartStopTest` mit 3,0 und 4,8 m/s ein Fuß 22,9 und 25,9
statt 36,1 mm, das Becken 6,3 und 10,5 statt 6,6 und 9,3 mm und mit 3,0 m/s eine Hand 15,6 statt 15,4 mm, `SteerHitTest`
das Becken nach dem Stoß 5,55 statt 5,70 m/s, und die Figur steht nach 5,43 statt 5,25 s wieder, `SteerRubbleTest` über
den Schutt eine schlechte Pose in 2,0 statt 1,3 % der Updates, ein Standknie über 120° in 1 statt 5,
`GaitClipFollowTest` rennend die Knochen bis 0,62 statt 0,57 cm von denen ohne Clips und `GaitClipShowTest` mit dem Clip
des Gehens allein rennend das Becken mindestens 0,831 statt 0,829 m hoch. Alle anderen Tests geben bis auf gemessene
Zeiten dieselben Zahlen und Hashes aus wie auf `active-ragdoll-101`, mit gcc und mit clang.

Was nicht ging, steht unter „Rückwärts auf den Zehen und umkehren ohne Absacken“ in [Optimierungen](Optimierungen.md):
über die Ferse nach Bedarf ohne Grenze der Geschwindigkeit, kein Rollen über die Ferse, rückwärts nach der Zeit über die
Ferse abrollen, die Zehen nach Bedarf herunterkommen lassen, früher abheben, die Kante wechseln, ohne erst flach zu
kommen, rennend über die Ferse gerollt abheben, die Ferse langsamer zurückrollen, und für den Anlauf auf die Treppe nur
die Stufe behalten, solange der Fuß sie erreicht, ein eigenes Tempo je Fuß, ohne dass der Takt dem ersten Schwung folgt,
und beides ohne das Behalten der Stufe.

Wo es noch fehlt, siehe oben unter „Schlechter wird“: Rückwärts gehend sind die Schienbeine unruhiger als vorher, weil
das Bein auf den Zehen fast gestreckt landet, beim Anhalten aus dem Rückwärtsrennen und von seitwärts auf rückwärts
gehend springen sie im Schlimmsten mehr, und hinauf mit 3,0 m/s steht an 3 von 96 Starts ein Fuß über der Kante der
ersten Stufe.

Eine gesteuerte Figur, die schaut, wohin sie geht, kostet im Update der Figuren auf der VM gepaart in acht Paaren im
Wechsel im Median 0,07 µs weniger als auf `active-ragdoll-101`, im Mittel 0,11 µs weniger, bei 8,9 µs. Eine, die
seitwärts geht und rennt, kostet im Median 0,15 µs weniger, bei 9,1 µs, eine, die vorwärts geht und rückwärts rennt,
0,09 µs mehr, im Mittel 0,04 µs, bei 9,2 µs. Die Figuren dieser Messung wechseln alle 2 s vom Gehen ins Rennen in die
Gegenrichtung. Im Benchmark bleiben alle Hashes und Zähler, er hat keine gesteuerte Figur. Die Bildzeiten der fünf
Szenen mit Städten liegen in acht Paaren im Wechsel bei 0,73 bis 1,21 von `active-ragdoll-101`, im Mittel bei 0,88 bis
1,00 und im Median bei 0,86 bis 1,02. Die Stadt aus 16 Häusern bei ×1 war auf `active-ragdoll-102` in allen acht Paaren
schneller, im Median um 14 %, mit denselben Hashes und Zählern, also mit derselben Arbeit: Das liegt nicht an dieser
Runde, die dort nichts rechnet, sondern an der VM oder an der Lage des Codes. Das Update des Figuren-Benchmarks liegt
über alle Phasen im Median bei 1,00, je Phase bei 0,93 bis 1,01: im Rauschen der VM.

Der nützlichste Fuß, Runde 103, F83, auf Wunsch des Nutzers nach Runde 102, nichts gebaut. Der Nutzer schrieb: „bau den
fuß doch so dass er am nützlichsten ist“. Der Fuß des Menschen der Bibliothek ist ein Quader, 26 cm lang, 9 cm breit und
6 cm hoch, die Sohle 6 cm unter dem Knöchel, die Ferse 4 cm hinter und die Zehen 22 cm vor ihm, 2,5 kg. Die Balance
nimmt ihre Stützfläche aus seinen Ecken, der gesteuerte Gang rollt über Ferse und Zehen ab und setzt ihn auf Treppen und
Schutt mit der ganzen Sohle, und in Runde 102 durfte er nur wenig und langsam über die Ferse rollen, weil sie so nah am
Knöchel liegt. Die Runden 76 bis 82 maßen größere Füße nur für die Balance. Gemessen wurden fünf Füße, nur der Fuß
anders, gleich schwer, Knöchel und Sohle in derselben Höhe, alles andere wie auf `active-ragdoll-102`: die Ferse wie
beim Menschen 6,5 cm hinter dem Knöchel und die Zehen wie heute, 28,5 cm lang; gleich lang wie heute mit dem Knöchel bei
einem Viertel der Sohle, die Ferse 6,5 und die Zehen 19,5 cm; nach den Maßen des Menschen 27 cm lang, 15 % der Größe,
und 10 cm breit, die Ferse 6,5 und die Zehen 20,5 cm; die großen Füße von Runde 76, 30 cm lang und 11 cm breit, die
Ferse 6 und die Zehen 24 cm; und nur breiter, 11 statt 9 cm.

Die Balance auf neuen Tabellen: Treffer auf Becken, Bauch, Rippen, untere Brust, Brust, Kopf, linken Oberarm, linken
Oberschenkel und linkes Schienbein, aus 32 Richtungen mit 20 bis 140 N s in Schritten von 4, 8 928 Treffer je Tabelle,
jeder in einer eigenen Welt, mit jeder Stütze bei 60, 30 und 20 Hz, neun Tabellen. Leicht sind Treffer bis 60 N s. Dazu
der Gang wie in Runde 102: 21 Übergänge an je 24 Stellen, die Übergänge von Figuren, die schauen, wohin sie gehen, an
312 Stellen, die gleichmäßigen Szenen, Zehen und Fersen, die Treppe mit 96 Starts hinauf und hinunter mit 3,0 und 4,5
m/s und der Schutt von Runde 97 mit 207 Läufen.

Ein Fehler der Messung, gefunden vor der Entscheidung: Die neuen Tabellen rechneten zuerst bei jedem Takt mit 4
Teilschritten. Die Bibliothek nimmt in `nbWorld_Advance` so viele, dass es 240 in der Sekunde sind, 8 bei 30 Hz und 12
bei 20 Hz, `nbGetSubStepCount`, und so maßen auch die früheren Runden. Mit 4 Teilschritten bei 20 Hz fiel mit eigener
Kraft und ohne Stütze jeder der 6 624 Treffer ab 52 N s und keiner darunter, mit jedem Fuß und ebenso mit der Bibliothek
von `active-ragdoll-83`, vor allen Runden zum Gehen: Es lag an der Messung, nicht an der Bibliothek. Alle Zahlen hier
sind mit den Teilschritten der Bibliothek gemessen. Mit dem Fuß von heute fallen bei 60 Hz mit eigener Kraft, mit der
Stütze gegen die Welt und ohne Stütze 1 235, 444 und 1 941 Treffer, bei 30 Hz 1 400, 496 und 2 143 und bei 20 Hz 2 367,
648 und 2 966, zusammen 13 640.

Vorher festgelegt, bevor ein anderer Fuß gemessen war, nur die ersten drei Tabellen bei 60 Hz mit dem Fuß von heute
waren bekannt: Ein Fuß ist nützlich, wenn gegen den von heute 1. über die drei Stützen bei 60, bei 30 und bei 20 Hz je
höchstens 1 % mehr Treffer fallen, über alle neun Tabellen mindestens 3 % weniger und in keiner Tabelle ein leichter
Treffer mehr; 2. in den Übergängen in keiner Klasse, Füße, Schienbeine, Hände und Becken, mehr als 5 % mehr Stellen über
der Grenze liegen, höchstens 2 mehr mit dem Becken über 15 cm tief und höchstens 3 mehr mit gekreuzten Füßen; 3. bei
Figuren, die schauen, wohin sie gehen, in keiner Klasse mehr als 5 % mehr Stellen über der Grenze liegen; 4. stetig
rückwärts gehend mit 1,6 m/s die Schienbeine höchstens 37 mm in einem Update springen und das Becken höchstens 2 cm
unter dem Stehen ist; 5. auf der Treppe hinunter an keinem Start das Becken mehr als 10 cm gegen ihre Linie wippt und
hinauf je Tempo höchstens ein Start mehr einen Fuß über einer Kante hat; 6. über den Schutt höchstens 0,3 Prozentpunkte
mehr schlechte Posen und höchstens 1 Prozentpunkt mehr Landungen auf einer Spitze. Unter den nützlichen gewinnt der mit
den wenigsten Stürzen über alle neun Tabellen, ist keiner nützlich, wird keiner gebaut.

Keiner der fünf erfüllt die Regel:

| Fuß | Stürze über die neun Tabellen | Leichte Treffer, die in einer Tabelle mehr fallen | Nicht erfüllt |
| --- | --- | --- | --- |
| heute, 26 × 9 cm, Ferse 4 cm | 13 640 | – | – |
| Ferse wie beim Menschen, 28,5 × 9 cm, Ferse 6,5 cm | 13 410, 1,7 % weniger, bei 20 Hz 1,5 % mehr | ohne Stütze bei 60 Hz 5 statt 3 und bei 20 Hz 81 statt 57, mit eigener Kraft bei 20 Hz 33 statt 32 | 1, 2, 4 und 5 |
| Knöchel bei einem Viertel der Sohle, 26 × 9 cm, Ferse 6,5 cm | 13 180, 3,4 % weniger | mit eigener Kraft bei 30 Hz 2 statt 1 und bei 20 Hz 35 statt 32 | 1, 2 und 5 |
| Maße des Menschen, 27 × 10 cm, Ferse 6,5 cm | 12 882, 5,6 % weniger | mit eigener Kraft bei 60 Hz einer statt keinem und bei 20 Hz 34 statt 32, ohne Stütze bei 60 Hz 4 statt 3 | 1, 2 und 5 |
| groß, 30 × 11 cm, Ferse 6 cm | 12 879, 5,6 % weniger | ohne Stütze bei 20 Hz 59 statt 57 | 1, 2, 3, 4 und 5 |
| breiter, 26 × 11 cm, Ferse 4 cm | 13 042, 4,4 % weniger | ohne Stütze bei 30 Hz 13 statt 11 und bei 20 Hz 66 statt 57 | 1 |

| Gang, Treppe und Schutt | heute | Ferse wie beim Menschen | Knöchel bei einem Viertel | Maße des Menschen | groß |
| --- | --- | --- | --- | --- | --- |
| Übergänge, 504 Stellen: Füße, Schienbeine, Hände und Becken über der Grenze | 96, 242, 68 und 116 | 102, 214, 72 und 125 | 97, 194, 71 und 122 | 95, 198, 66 und 123 | 101, 218, 72 und 125 |
| Übergänge: gekreuzte Füße und Becken über 15 cm tief | 23 und 0 | 25 und 0 | 25 und 0 | 25 und 0 | 23 und 1 |
| Rennend: Schienbeine über 37 mm | 128 | 119 | 90 | 116 | 119 |
| Zehen und Fersen über 37 mm, Übergänge und Figuren, die schauen, wohin sie gehen | 165 und 61 | 155 und 71 | 150 und 61 | 150 und 60 | 155 und 74 |
| Figuren, die schauen, wohin sie gehen, 312 Stellen: Füße, Schienbeine, Hände und Becken über der Grenze | 40, 75, 96 und 74 | 40, 77, 97 und 72 | 38, 75, 63 und 45 | 38, 77, 65 und 45 | 42, 82, 94 und 51 |
| Stetig rückwärts gehend: Schienbeine und Becken unter dem Stehen | 36,1 mm und 1,2 cm | 37,2 mm und 1,2 cm | 34,8 mm und 1,6 cm | 34,5 mm und 1,3 cm | 38,1 mm und 0,7 cm |
| Treppe hinunter mit 4,5 m/s: Starts über 10 cm Wippen, im Schlimmsten | keiner, 6,2 cm | keiner, 6,5 cm | 5, 21,3 cm | 5, 20,7 cm | keiner, 6,3 cm |
| Treppe hinauf mit 3,0 und 4,5 m/s: Starts mit einem Fuß über einer Kante | 3 und 0 | 96 und 96 | 0 und 0 | 1 und 0 | 96 und 96 |
| Schutt: schlechte Posen und Landungen auf einer Spitze | 2,14 und 13,0 % | 2,32 und 13,7 % | 1,70 und 12,4 % | 1,98 und 12,5 % | 2,35 und 14,0 % |

Mit den breiteren Füßen rechnet die gesteuerte Figur in allen Übergängen, Szenen und auf der Treppe bitgleich wie heute
und über den Schutt mit denselben Zahlen.

Was die Füße zeigen: Die Ferse 6,5 cm hinter dem Knöchel beruhigt den Gang. Gleich lang wie heute und nach den Maßen des
Menschen springen die Schienbeine in den Übergängen an 194 und 198 statt 242 Stellen über der Grenze, rennend an 90 und
116 statt 128, bei Figuren, die schauen, wohin sie gehen, die Hände an 63 und 65 statt 96 Stellen und das Becken an 45
statt 74, und über den Schutt hat die Figur seltener eine schlechte Pose. Der Gang ist aber auf den Fuß von heute
abgestimmt: Das Becken springt in den Übergängen an 122 und 123 Stellen über 10 mm, erlaubt waren 121, und rennend die
Treppe hinunter mit 4,5 m/s wippt es an 5 von 96 Starts über 10 cm. Längere Füße passen nicht auf die 30 cm tiefen
Stufen: Mit 28,5 und 30 cm steht an jedem Start hinauf und an fast jedem hinunter ein Fuß über einer Kante. Breitere
Füße fangen mehr: 11 cm breit fallen 4,4 % weniger Treffer, mit eigener Kraft bei 20 Hz von den leichten 11 statt 32,
und 11 cm breit und 30 cm lang 5,6 % weniger.

Wie viel davon Zufall ist, zeigen die neun Tabellen mit dem Fuß von heute und dem Treffpunkt 2 mm höher: Zusammen fallen
13 621 statt 13 640, in einer Tabelle wechseln aber bis 444 Treffer ihren Ausgang, und von den leichten fallen mit
eigener Kraft bei 20 Hz 22 statt 32 und ohne Stütze 52 statt 57. Die Summen trennen die Füße also sicher, eine Handvoll
leichter Treffer in einer Tabelle bei 20 Hz nicht. Über alle neun Tabellen zusammen fallen von den leichten Treffern mit
den großen Füßen 75, gleich lang mit der Ferse 6,5 cm und nach den Maßen des Menschen je 89, mit den breiteren Füßen 93
und mit der Ferse 6,5 cm bei 28,5 cm Länge 127 statt 104, mit dem Treffpunkt 2 mm höher 87. Mehr als Zufall ist es bei
den breiteren Füßen ohne Stütze bei 20 Hz: 54 leichte Treffer fallen neu, 17 davon in den ersten 2 s, mit dem Treffpunkt
2 mm höher sind es 24, einer davon in den ersten 2 s.

Siehe „Der nützlichste Fuß“ und die Zeilen danach in [Optimierungen](Optimierungen.md). Vorgeschlagen ist als Runde 104
der Fuß nach den Maßen des Menschen mit einem Gang, der zu ihm passt: finden, warum mit ihm das Becken rennend die
Treppe hinunter an 5 Starts über 10 cm wippt und in den Übergängen öfter springt, das in der Bibliothek beheben und dann
nach einer vorher festgelegten Regel messen, die die leichten Treffer über alle Tabellen zusammen zählt und den Zufall
an Tabellen mit verschobenem Treffpunkt misst. Die breiteren Füße bleiben ein Vorschlag: Sie kosten am Gang nichts,
lassen aber ohne Stütze bei 20 Hz leichte Treffer nach wenigen Schritten fallen.

| Nr. | Schritt | Gewinn | Voraussetzung | Stand |
| ---: | --- | --- | --- | --- |
| F1 | **Federn mit Grenze in Box3D**: Dreh- und Kugelgelenke ziehen mit begrenztem Drehmoment, nach 27.15 P2 der Roadmap des Spiels, bitgleich ohne Grenze | Voraussetzung der aktiven Ragdolls | – | ✅ auf `active-ragdoll-1`: `maxSpringTorque`, siehe „Federn mit Grenze“ in [`extern/README.md`](../extern/README.md) |
| F2 | **Physikskelett eines Menschen**: 18 Knochen mit Formen, Massen und Gelenken in ihren Grenzen, nach 27.15 P3 | – | F1 | ✅ auf `active-ragdoll-1`: `nbMakeHumanoidBones` |
| F3 | **Stand und Zucken**: Die Knochen folgen kinematisch dem Clip, ein schwacher Treffer lässt den Rumpf zucken, nach 26.7 M2a und 28.2 | – | F2 | ✅ auf `active-ragdoll-1` |
| F4 | **Aktiver Ragdoll**: Sturz durch Treffer, Explosion, Trümmer, Last und Boden, der wegbricht, mit den Geschwindigkeiten der Knochen und dem Treffer einmal, Kraft nach Geschwindigkeit, Abstützen und Einrollen, nach 26.7 M2b, 26.13, 28.1 und 28.4 | – | F3 | ✅ auf `active-ragdoll-1` |
| F5 | **Aufstehen**: Boden, Last und Platz prüfen, Clip vom Bauch oder vom Rücken aus der Haltung, in der die Figur lag, Abbruch bei Treffern, nach 26.7 M2c und 26.15 | – | F4 | ✅ auf `active-ragdoll-1` |
| F6 | **Demo und Benchmark**: Szene „Figuren“ mit „Zustand“ und „Zielhaltung“, `nebenan_benchmark figures` mit 1, 10 und 50 Figuren, nach 26.8 und 27.14 | Werkzeug | F5 | ✅ auf `active-ragdoll-1` |
| F7 | **In der Demo ansehen**: auf dem Ryzen die Szene „Figuren“ mit Gewehr, Granate und Kanone, mit „Zustand“ und „Zielhaltung“ | entscheidet, was an Haltungen und Clips nachzubessern ist | F6 | ✅ vom Nutzer angesehen, nach `active-ragdoll-27` passt es erstmal, am 1. November 2026. Jede neue Runde sieht er weiter in der Demo an |
| F8 | **Pakete trotz Gelenken**: Gelenke in die Pakete und Gebiete des Lösers legen, wie es `ragdoll-7` für die Pakete tat, bitgleich für Szenen ohne Gelenke | mit wachen Ragdolls wieder ganze Inseln pro Thread und Gebiete, für Figuren und Trümmer | F6 | ✅ auf `active-ragdoll-2`: Pakete rechnen ihre Gelenke selbst, bitgleich mit den Farben, und große Inseln behalten ihre Gebiete, die Gelenke eines Ragdolls im Schutt rechnet der aufrufende Thread vor jedem Durchgang, siehe „Gelenke in Paketen und Gebieten“ in [`extern/README.md`](../extern/README.md). In der Stadt mit Figuren 15,5 % weniger Zeit im Löser mit vier Threads |
| F9 | **Ausgleichsschritt**: Ein mittlerer Treffer lässt die Figur einen Schritt machen, statt sie nur zucken zu lassen, nach 26.14 und 28.3 | – | F5 | ✅ auf `active-ragdoll-15`: Ein Treffer unter `knockdownImpulse` gibt dem Körper den Impuls durch seine Masse als Geschwindigkeit. Liegt der Punkt, über dem er wie ein Pendel von seiner Standhöhe zur Ruhe käme, außerhalb der Sohlen, um den Schwerpunkt auf 60 % verkleinert (`nbFigureDef::stepZone`), geht der Körper dorthin, und die Füße treten nach, einer nach dem anderen, der stehende hält, wo er steht. Ohne Boden am Ziel, mit etwas Festem im Weg oder für mehr als 45 cm (`stepReach`) fällt die Figur. Der Mensch der Bibliothek tritt nach hinten ab 15 N s, zur Seite ab 21 N s und nach vorn ab 31 N s, siehe „Ausgleichsschritt“ in [Optimierungen.md](Optimierungen.md) |
| F10 | **Füße im Stand auf dem Boden**: IK für die Füße auf unebenem Boden und auf Schutt, nach 26.5 und 28.3 | – | F5 | ✅ auf `active-ragdoll-16`: Unter der Mitte jeder Sohle sucht eine Kugel von 4 cm Radius den Boden, jedes achte Update im Wechsel mit den anderen Figuren und sobald sich die Sohle 2 cm bewegt hat. Der Fuß folgt seiner Höhe und kippt mit seiner Neigung bis 0,4 rad, die Zwei-Knochen-IK setzt das Bein, das Becken geht mit dem tieferen Fuß. Einen Fuß, den der Clip ab 3 cm hebt, lässt die Bibliothek allmählich los, ab 10 cm ganz. Auf ebenem Boden behält der Clip die Beine. 50 stehende Figuren kosten im Update 0,091 statt 0,067 ms, siehe „Füße auf dem Boden“ in [Optimierungen.md](Optimierungen.md) |
| F11 | **Platz zum Aufstehen suchen**: Eine Figur ohne Platz rückt zur Seite oder dreht sich, statt zu warten, etwa mit dem Kopf an einer Wand | – | F5 | ✅ auf `active-ragdoll-17`: Ein Mensch der Bibliothek ohne Platz sucht 30 und 60 cm zurück entlang seiner Wirbelsäule, 30 und 60 cm zu jeder Seite, dann um eine Vierteldrehung und eine halbe um sein Becken gedreht, nach Boden, Platz und einem Weg für seinen liegenden Körper, und rückt mit 0,5 m/s oder dreht sich mit 2 rad/s dorthin, bevor er aufsteht. Ohne eine solche Stelle wartet er. Siehe „Platz zum Aufstehen suchen“ in [Optimierungen.md](Optimierungen.md) |
| F12 | **Gelenke in den Gebieten**: Die Gelenke eines Ragdolls im Schutt in das Gebiet eines seiner Körper legen und gegen dessen Kopien mit einem Anteil der Masse rechnen wie die Kontakte, statt sie vor jedem Durchgang auf dem aufrufenden Thread allein zu rechnen. Dafür müssen die neun Arten von Gelenken beim Vorbereiten die Masse ihrer Körper skalieren und auf Kopien zeigen können, bitgleich mit dem Faktor 1 | bis 0,024 ms pro Ragdoll im Schutt und Schritt auf der VM mit vier Threads, 0,48 ms mit 20 in einem Haufen, siehe „Gelenke in Paketen und Gebieten“ in [`extern/README.md`](../extern/README.md) | F8 | ✅ auf `active-ragdoll-18`: Jedes Gelenk einer Insel mit Gebieten liegt im Gebiet seines ersten Körpers in der Insel, seine Körper bekommen dort eine Kopie mit ihrem Anteil der Masse, wenn sie auch in anderen Gebieten Bedingungen haben, und jedes Gebiet bereitet seine Gelenke vor und rechnet sie in jedem Durchgang vor seinen Kontakten. Außerhalb der Gebiete und mit dem Faktor 1 bitgleich. Mit 20 Ragdolls auf einer Platte über 4 513 Würfeln, neu im Benchmark, kosten die Bedingungen mit vier Threads 2,82 statt 3,14 ms, gewonnen sind 0,28 der 0,47 ms, die der aufrufende Thread allein rechnete, denn das vollste Gebiet hält 102 der 340 Gelenke. Siehe „Gelenke in den Gebieten“ in [Optimierungen.md](Optimierungen.md) |
| F13 | **F8 auf dem Ryzen messen**: die Stadt mit Figuren in der Demo mit und ohne Gelenke in den Paketen unter denselben Beschuss nehmen, im Wechsel in einer Sitzung, mit dem fertigen Paket der CI | zeigt, was F8 mit 8 Threads und einer echten Grafikkarte bringt, auf der VM 15,5 % weniger Zeit im Löser mit vier Threads | F8 | ✅ gebaut auf `active-ragdoll-3`: Szene „Stadt mit Figuren“ und „Mit und ohne F8 messen“, `--compare-joints`, mit dem Schalter `b3WorldDef::enableJointPacks`, siehe „Gelenke in Paketen und Gebieten“ in [`extern/README.md`](../extern/README.md). Auf dem Ryzen mit 8 Threads sind mit F8 der Löser 9,9 %, der Box3D-Schritt 5,8 % und ein Bild 4,8 % schneller, die langsamsten 1 % der Bilder aber langsamer, 1 % Low 219 statt 243 FPS, siehe F14 |
| F14 | **Die langsamsten Schritte mit F8**: herausfinden, was die langsamsten 1 % der Schritte mit Gelenken in den Paketen und Gebieten langsamer macht als ohne, und es beheben, bitgleich | auf dem Ryzen in der Stadt mit Figuren 1 % Low 219 statt 243 FPS mit F8, das längste Bild 6,3 statt 5,8 ms; im Benchmark auf der VM mit vier Threads 99 % der Frames bis 40,2 statt 35,1 ms, bei 95 % gleich | F13 | ✅ auf `active-ragdoll-4`: Zum größten Teil liegt es an der Welt, die mit F8 anders endet. Unter acht Folgen von Granaten waren die 99 % der Schritte mit F8 in fünf langsamer und in drei schneller, und die Bedingungen des Lösers kosten in den langsamsten 1 % mit und ohne F8 gleich viel. Sicher mehr kostet nur die Vorbereitung, 0,45 statt 0,03 ms pro Schritt in den langsamsten 1 % auf der VM: Mit F8 bekommen die Schutthaufen auch mit wachen Ragdolls Gebiete, und eine Insel bekommt sie gerade in den langsamen Schritten, mit 1,3 bis 4,3 ms für einen großen ersten Schnitt. Der Schnitt lädt jetzt voraus, bitgleich, ein großer kostet ein Drittel weniger, gepaart in denselben Welten in den langsamsten 1 % 0,33 statt 0,42 ms pro Schritt, siehe „Die langsamsten Schritte mit F8“ in [Optimierungen.md](Optimierungen.md). Auf dem Ryzen mit „Mit und ohne F8 messen“ nachzumessen |
| F15 | **Erste Schnitte auf mehreren Threads**: Das Verschieben der Kontakte einer Insel in die Arrays ihrer Gebiete auf die Worker verteilen, jede Farbe auf einem Thread in der Reihenfolge der Kontakte, die Zähler Gebiet für Gebiet, bitgleich | was der Schnitt nach F14 in den langsamsten Schritten noch kostet, auf der VM mit vier Threads rund 0,3 ms pro Schritt, ein großer Schnitt 0,9 bis 1,6 ms auf dem aufrufenden Thread | F14 | 🔜 niedrig, erst wenn der Ryzen zeigt, dass die langsamsten Bilder mit F8 weiter zurückliegen |
| F16 | **Knoten gegen statischen Schutt über die Gebiete verteilen**: Die Kontakte eines Knotens mit Partnern, die sich nicht bewegen, liegen alle im Gebiet des Knotens, in den Farben verteilt der Löser sie auf bis zu acht Gruppen. In 100 von 599 Schnitten der Stadt mit Figuren hielt so ein Gebiet mehr als die Hälfte der Kontakte, meist auf Inseln aus 3 bis 20 Körpern. Sie mit Kopien des Knotens über die Gebiete verteilen wie in den Farben | solche Inseln auf mehreren Threads statt auf einem, mit 8 Threads mehr als mit 4; ändert Ergebnisse | F8 | 🔜 niedrig, erst messen, was sie auf dem Ryzen kosten |
| F17 | **Großer Schutt im ständigen Wechsel**: Ein Teil aus Schutt mit 1 354 Formen wechselt in der Stadt mit Figuren alle ein bis drei Schritte zwischen statisch und dynamisch, meist über das Auftauen von Schutt in einer Box (`nbThawRubbleInBoxes`), und bekommt als dynamisches jedes Mal Gebiete. Herausfinden, warum es so schnell wieder aufwacht, und es ruhen lassen, ohne dass etwas in der Luft hängen bleibt | jeder Wechsel eines Teils mit mindestens 100 Formen kostet im Update im Mittel 1,4 ms auf dem aufrufenden Thread, bis 85 ms pro Lauf, mit und ohne F8; 34 von 175 lagen in einem der langsamsten Bilder oder direkt daneben, wo das Update 7 bis 12 ms statt 2 bis 2,7 ms kostet | – | ✅ auf `active-ragdoll-5`: Das Teil ist ein Teil eines Gebäudes von 49 m³, auf vier Stücken Schutt erstarrt. Neben ihm rutscht ein kleines Stück mit 0,3 m/s, auf dem kleiner Schutt immer wieder erstarrt, und alle 5 cm weckte es allen Schutt in seiner Box über seinem Schwerpunkt, auch das Teil. Das bewegte sich einen Millimeter, das Budget ließ es gleich wieder erstarren. Jetzt wacht ein großes Teil eines Gebäudes durch ein bewegtes Stück nur auf, wenn es auf ihm erstarrt ist. In acht Folgen von Granaten wachen Teile so 38 statt 116 Mal auf, mit 8 359 statt 55 758 Formen, und bei der Probe am Ende fällt kein Teil, wenn es allein wieder zum Leben kommt. Die Zeiten liegen im Rauschen der VM, das Update der langsamsten Bilder kostet vor allem das Budget, siehe F18 und „Große Hausteile wachen nur durch ihren Halter auf“ in [Optimierungen.md](Optimierungen.md) |
| F18 | **Das Budget in den langsamsten Bildern**: In den 12 langsamsten Bildern eines Laufs der Stadt mit Figuren kostet das Update 9,5 bis 10,6 statt im Mittel 2,4 bis 2,7 ms, davon das Budget 3,3 bis 4,9 statt 0,7 ms und darin die Zustandswechsel 3,1 bis 3,8 ms: Hunderte Stücke erstarren auf einmal, bis zu 256 pro Update. Herausfinden, was ein Zustandswechsel dort kostet, und ihn billiger machen oder verteilen, ohne dass mehr Trümmer wach bleiben, als das Budget erlaubt | bis zu 3 bis 4 ms in den langsamsten Bildern, in allen Szenen über dem Budget | – | ✅ auf `active-ragdoll-6`: In diesen Bildern lässt das Budget rund 190 Stücke mit je 18 Kontakten erstarren und liegt trotzdem im Mittel 1 300 Trümmer über seiner Grenze, denn frische und fliegende Trümmer dürfen nicht erstarren. Verteilen ginge also nur mit mehr wachen Trümmern, als das Budget erlaubt. Ein Typwechsel kostete dort in Box3D 9,0 µs: rund 5,2 µs das Löschen der Kontakte, 1,8 µs das Nachführen der Kontakte des Körpers, der im Satz der wachen Körper an den Platz des erstarrten rückt, und 1,6 µs die Formen. Box3D führt diese Kontakte jetzt am Ende der Klammer nach, acht Listen zugleich, und lädt beim Löschen drei Kontakte voraus, bitgleich. In denselben Welten kostet ein Typwechsel durch das Budget so 7,3 statt 9,0 µs, ein Auftauen 3,8 statt 4,7 µs, und die Zustandswechsel kosten in den langsamsten Bildern 3,44 statt 3,82 ms, im Mittel aller Bilder 0,59 statt 0,66 ms. Gepaart gegen `active-ragdoll-5` kostet das Update in den Städten des Benchmarks 3 bis 9 % weniger. Mehr steckt in den Blicken des aufrufenden Threads und in den Kontakten zu bewegten Stücken, F19 und F20, siehe „Typwechsel im Schub“ in [Optimierungen.md](Optimierungen.md) |
| F19 | **Blicke, die gelten, wenn ihre Nachbarn erstarren**: Das Budget sieht sich die Stücke auf den Workern an, bevor eines erstarrt. Ist ein bewegtes Stück, das ein Blick las, inzwischen erstarrt, sieht der aufrufende Thread das Stück noch einmal an. In den 12 langsamsten Bildern eines Laufs der Stadt mit Figuren sind das 149 seiner 217 Blicke, je rund 3 µs. Weil das Erstarren die Kontakte löscht, sieht er dabei nicht mehr, was gerade erstarrt ist, und ein Stück, das nur darauf liegt, erstarrt erst mit einer späteren Rangliste. Erst für alle Stücke entscheiden, mit der Welt vor dem ersten Erstarren, dann die Typen wechseln, mit einem und mit vielen Threads gleich | rund 0,5 ms in den langsamsten Bildern auf der VM, und das Budget findet dort mehr, was erstarren darf: Es ließ 188 von 239 verlangten Stücken erstarren | F18 | ✅ auf `active-ragdoll-7`: Das Budget wählt alle Stücke in der Welt vor dem ersten Erstarren und lässt sie danach in der gewählten Reihenfolge erstarren, mit den Blicken der Worker. Der aufrufende Thread sieht ein Stück nur noch an, wenn kein Worker es tat, und mit einem und mit vier Threads wählt das Budget dieselben. In den 12 langsamsten Bildern eines Laufs der Stadt mit Figuren sieht er so 60 bis 71 statt 211 bis 229 Stücke selbst an, und das Budget kostet dort 0,2 bis 0,5 ms weniger, in allen Bildern mit Budget rund 10 % weniger. So erstarrten aber mehr große Stücke dort, wo sie hängen bleiben: In der Probe am Ende, in der jedes große Stück Schutt allein wieder zum Leben kommt, fielen in acht Folgen 13 statt 9 mehr als 5 cm. Sie lagen im Gleichgewicht nur über bewegten Stücken, die sie an der Seite berührten oder die das Budget im selben Update gewählt hatte, oder rutschten noch mit 0,5 bis 0,9 m/s. Jetzt erstarrt ein großes Stück im Budget nur, wo es auch in der Ruhe erstarren würde, im Gleichgewicht über dem, was ohne es hält, und nur langsamer als 0,25 m/s. Über 16 Folgen fallen so 9 statt 24 große Stücke mehr als 5 cm, zusammen 1,40 statt 3,48 m. Gepaart gegen `active-ragdoll-6` kostet das Update in den Städten des Benchmarks bis zu 3 % weniger, in der Stadt mit Figuren des Figuren-Benchmarks 7,6 %, ein Bild liegt im Rauschen der VM, siehe „Blicke, die bis zum Ende gelten“ in [Optimierungen.md](Optimierungen.md). Was noch fällt, ist F21 |
| F20 | **Kontakte zu bewegten Stücken behalten**: Von den 18 Kontakten eines Stücks, das durch das Budget erstarrt, gehen rund 7,7 zu bewegten Stücken. Box3D löscht sie mit dem Typwechsel und legt sie im nächsten Schritt neu an, ohne die Stöße und Trennachsen von vorher. Sie stattdessen zu Kontakten mit einem statischen Körper machen und in Insel, Farbe und Gebiet umhängen | das Löschen und neue Anlegen von rund 1 400 Kontakten in den langsamsten Bildern, auf der VM zusammen etwa 1 ms im Update und im Schritt danach | F18 | ✅ auf `active-ragdoll-9`: Ein Körper, der erstarrt, behält seine Kontakte mit wachen beweglichen Körpern, im Mittel 1,9 berührende und 5,4 weitere, mit ihren Stößen und Trennachsen. Wer wieder beweglich wird, löscht weiter alle Kontakte, sonst stoßen ihn die Stöße an, die einen Haufen auf ihm hielten. In acht Folgen kostet ein Bild 1,9 % und der Schritt von Box3D 2,2 % weniger, ein Typwechsel 3 % mehr, in den langsamsten Bildern der Schritt danach 1,1 bis 1,7 ms weniger und das Update 0,6 ms mehr. Im gepaarten Benchmark dauern die 99 % der Bilder in der Stadt mit Figuren 37 bis 39 statt 48 ms, das Update kostet außer unter dem Trommelfeuer 3 bis 7 % mehr. Auf 48 neuen Folgen fallen 17 statt 20 große Stücke mehr als 5 cm. Siehe „Kontakte beim Erstarren behalten“ in [Optimierungen.md](Optimierungen.md) |
| F21 | **Große Stücke, die eine Seite hält**: In der Probe am Ende fallen auf `active-ragdoll-7` über 16 Folgen noch 9 große Stücke mehr als 5 cm, wie auf `active-ragdoll-6` meist so: Ein großes Stück erstarrt im Gleichgewicht über Schutt, den es zum Teil nur an der Seite berührt, oft ein Teil eines Gebäudes. Wacht dieser Schutt später auf und gleitet weg, weckt er das Stück nicht, denn es lag nicht auf ihm und liegt nicht höher als sein Schwerpunkt. Ein großes Stück könnte sich die Stützen, die es für sein Gleichgewicht braucht, wie seine Träger merken | weniger große Stücke, die in der Luft hängen bleiben | F19 | ✅ auf `active-ragdoll-8`: Nachverfolgt war bei 5 der 9 Stücke Schutt an ihrer Seite weggerückt, den sie für ihr Gleichgewicht brauchten. Jetzt merkt sich ein großes Stück diesen Schutt mit seinen Trägern, als Stütze an der Seite: Rückt er weg, wacht es auf, in der Suche nach dem, was Schutt trägt, zählt er aber nicht. Die übrigen fielen, obwohl alles an seinem Platz lag, die meisten waren in der Ruhe nur dazwischen verkeilt erstarrt, ohne auf etwas Festem zu liegen. So erstarrt ein großes Stück jetzt auch in der Ruhe nicht mehr, wie schon im Budget, es bleibt beweglich und schläft. Über 16 Folgen fallen so 2 statt 9 große Stücke mehr als 5 cm, zusammen 0,20 statt 1,40 m, und keins, weil eine Stütze wegrückte. Über acht Folgen kostet ein Bild 10,33 statt 10,51 ms. Gepaart gegen `active-ragdoll-7` kostet in den Städten des Benchmarks ohne Figuren ein Bild bis zu 9 % und das Update bis zu 7 % weniger. In der Stadt mit Figuren stürzt dort ein sechstes Stockwerk ein, und die 99 % der Bilder dauern 45 bis 48 statt 36 ms, über die acht Folgen 29,25 statt 29,77 ms. Siehe „Große Stücke, die eine Seite hält“ in [Optimierungen.md](Optimierungen.md) |
| F22 | **Schutt, den ein bewegtes Stück trägt, ohne ihn zu halten**: Liegt ein kleines Stück beim Erstarren auch auf etwas Festem, hält ein bewegtes Stück, auf dem es außerdem liegt, es nicht (`nbFreezeOnCarriers`). Bricht das Feste später weg, trägt nur noch das bewegte, und rückt das weg, bleibt der Schutt in der Luft. Der `StoreyResidueTest` fand das mit einer Variante von F20, die Probe am Ende auf 48 Folgen zweimal mit `active-ragdoll-8` und mit F20. Bricht ein fester Träger weg, könnten die bewegten Träger den Schutt halten, oder er kommt zur Prüfung zurück | kein Schutt ohne Halt | – | ✅ auf `active-ragdoll-10`: Nachverfolgt fehlte das Halten auf drei Wegen. Die Freigabe begann jedes Update vorn und erreichte in Einstürzen ihre Grenze, bevor hintere Stücke an die Reihe kamen, ein Stück vergaß beim Erstarren, was es hielt, und nach einem Einschlag, der ein Bruchstück zerbrach, zeigte der Eintrag des Schutts darauf ins Leere. Jetzt geht die Freigabe reihum, ein Stück weckt vor dem Erstarren, was auf ihm erstarrt ist, und der Eintrag geht an das kleinere Bruchstück am Punkt der Berührung. Auf 48 Folgen liegt so 244 statt 268 Mal Schutt in der Luft, keiner ohne Halt statt 2, und keiner mehr, weil das Halten fehlte. Über acht Folgen kostet ein Bild 10,25 statt 10,28 ms, und es tauen 2,7 % mehr Stücke auf. Der erste Bau weckte vor dem Erstarren allen Schutt über dem Schwerpunkt und ließ die Box eines Halters wachsen, das brachte 19 % mehr Auftauen und kostete 4 % pro Bild. Was bleibt, ist F23. Siehe „Schutt folgt seinem Träger“ in [Optimierungen.md](Optimierungen.md) |
| F23 | **Schutt, den das Budget an steilen Flächen einfriert**: Im ersten Durchgang des Budgets erstarrt ein kleines Stück, sobald es etwas Festes berührt, auf einer Fläche bis 84° steil und gleich wie schnell. Am Ende von 48 Folgen der Stadt mit Figuren lagen so auf `active-ragdoll-10` 230 von 244 Stücken Schutt in der Luft nur an der Seite eines anderen Stücks oder Teils, 11 an einem bewegten Stück, und 3 erstarrten in der Bewegung, mit einer Berührung aus dem Schritt davor. Die naheliegenden Regeln aus „Nichts erstarrt schneller als 1 m/s“ kosteten bei ×1 10 bis 17 % pro Bild und ließen mehr Stücke wenige Zentimeter über ihrer Auflage stehen. Ein kleines Stück könnte im ersten Durchgang nur auf flacheren Flächen erstarren, oder erst, wenn die Berührung nach dem Schritt noch besteht | kein Schutt in der Luft, auf `active-ragdoll-10` rund 5 Stücke pro Lauf | – | ✅ auf `active-ragdoll-11`: Ein kleines Stück, das sich mit 0,05 m/s oder schneller bewegt, erstarrt an einer Fläche ab 60° nicht mehr, es rutscht oder fällt weiter, bis es liegt. Mit 60° blieb 3 Mal Schutt ohne Halt, auf einem bewegten Stück, das noch Schutt von früher hielt: Jetzt gibt ein solches Stück den frei, bevor etwas auf ihm erstarrt, und hält dann von dort, wo es liegt. Auf 96 Folgen liegt so 219 statt 523 Mal Schutt in der Luft, keiner ohne Halt, und an einer steilen Fläche in der Bewegung erstarrt waren davon nur noch 12 statt 298. Über acht Folgen kostet ein Bild 3,7 % mehr, dort stürzen aber 40 statt 37 Stockwerke ein, und wo gleich viele einstürzen, sind es 0,5 %. Mit 45° erstarrten 32 % mehr kleine Stücke auf bewegten, und es tauten 12 % mehr auf: verworfen. Was bleibt, ist F24. Siehe „Schutt rutscht an steilen Flächen weiter“ in [Optimierungen.md](Optimierungen.md) |
| F24 | **Schutt, den das Budget in der Bewegung neben seiner Auflage einfriert**: Im ersten Durchgang des Budgets erstarrt ein kleines Stück auf einer Fläche flacher als 60° gleich wie schnell, auch wenn sein Schwerpunkt neben den Punkten liegt, auf denen es aufliegt. Am Ende von 96 Folgen der Stadt mit Figuren waren auf `active-ragdoll-11` 108 von 219 Stücken Schutt in der Luft so erstarrt, mit 0,05 bis 68 m/s, und 84 davon fallen, wenn sie allein wieder zum Leben kommen. Bei 98 lag der Schwerpunkt neben den Auflagepunkten. Ein kleines Stück, das sich bewegt, könnte nur erstarren, wenn sein Schwerpunkt über seinen Auflagepunkten liegt, wie es für große schon gilt (`nbSupport::balanced`). Was das an Erstarren kostet, muss die Probe zeigen | kein Schutt in der Luft, auf `active-ragdoll-11` rund 1 Stück pro Lauf | F23 | ✅ auf `active-ragdoll-12`: Neben seiner Auflage allein ist kein Grund zu warten: Das Budget friert pro Lauf 21 300 kleine Stücke so in der Bewegung ein, und nur etwa eines bleibt in der Luft. Die Regel nach dem Gleichgewicht allein brachte ein Drittel mehr Auftauen: verworfen. Jetzt erstarrt ein solches Stück nur, wenn ein Strahl unter seinem Schwerpunkt innerhalb seines Radius und 30 cm auf etwas trifft, das betrifft 1 320 Stücke pro Lauf. Auf 96 Folgen fallen so 56 statt 115 kleine Stücke aus der Luft, Schutt in der Luft liegt 183 statt 219 Mal, keiner ohne Halt, und es tauen 3,6 % mehr Stücke auf. Über acht Folgen kostet ein Bild 1,0 % weniger, mit 35 statt 40 eingestürzten Stockwerken, und wo gleich viele einstürzen, 3,4 % weniger. Was bleibt, ist F25. Siehe „Schutt fällt über die Kante“ in [Optimierungen.md](Optimierungen.md) |
| F25 | **Schutt, der nach F24 noch fällt**: Am Ende von 96 Folgen der Stadt mit Figuren fallen auf `active-ragdoll-12` 56 kleine Stücke Schutt, unter denen 30 cm weit nichts liegt, wenn sie allein wieder zum Leben kommen, in kleinen Gruppen: 15 lagen beim Erstarren mit dem Schwerpunkt über ihren Auflagepunkten, 13 waren langsamer als 0,05 m/s und lagen daneben, 10 lagen daneben, aber der Strahl fand beim Erstarren etwas unter ihrem Schwerpunkt, innerhalb ihres Radius und 30 cm. Jede Gruppe wie bei F22 nachverfolgen, bevor eine Regel kommt | kein Schutt, der fällt, auf `active-ragdoll-12` rund 0,6 Stücke pro Lauf | F24 | ✅ auf `active-ragdoll-13`: 18 der 56 lagen auf etwas, das weiterrückte, meist auf einem bewegten Stück, während sie auch auf etwas Festem lagen, und ein solches Stück halten bewegte seit `kollaps-4` nicht. Jetzt halten sie ein kleines Stück, das sich mit 0,05 m/s oder schneller bewegt, wenn sein Schwerpunkt neben den Punkten liegt, auf denen es fest liegt, und 30 cm darunter nichts Festes ist. Und unter einem Stück zählt für F24 nur noch Festes, bewegte Stücke rücken weiter. Auf 96 Folgen fallen so 29 statt 56 kleine Stücke, und es tauen 5,9 % mehr Stücke auf. Jedes solche Stück zu halten kostete 54 % mehr Auftauen, auch ruhende zu halten 10,3 %, ohne dass weniger fielen, und ließ man die Stücke warten, statt sie zu halten, erstarrten sie, sobald ihr Träger weg war, noch bevor sie kippten: verworfen. Über acht Folgen kostet ein Bild 2,2 % mehr, wo gleich viele Stockwerke einstürzen 1,0 %. Was bleibt, ist F26. Siehe „Schutt hält sich an bewegten Stücken“ in [Optimierungen.md](Optimierungen.md) |
| F26 | **Schutt, der langsam neben seiner Auflage erstarrt**: Am Ende von 96 Folgen der Stadt mit Figuren fallen auf `active-ragdoll-13` noch 29 kleine Stücke Schutt, wenn sie allein wieder zum Leben kommen. 15 davon fror das Budget langsamer als 0,05 m/s ein, und 4 erstarrten in der Ruhe, meist an Flächen steiler als 58° und mit dem Schwerpunkt 3 bis 22 cm neben ihrer Auflage. Die Regel von F24 und das Halten von F25 gelten nur für Stücke in Bewegung. Wie bei F22 nachverfolgen, was sie beim Erstarren hielt, bevor eine Regel kommt | kein Schutt, der fällt, auf `active-ragdoll-13` rund 0,3 Stücke pro Lauf | F25 | ⛔ verworfen auf `active-ragdoll-14`: Nachverfolgt lag keines der 19 auf etwas, sie klemmten an Flächen steiler als 60°, meist von oben gedrückt, 13 von bewegten Stücken, 6 zwischen festen Teilen. Solche Stücke erstarren pro Lauf rund 1 300 Mal, fast alle bleiben liegen. Hielten die bewegten Nachbarn sie, tauten 9,5 bis 26 % mehr Stücke auf, und es fielen nicht weniger. Was bleibt, sind rund 0,3 kleine Stücke pro Lauf, die nur ein Nachbar hält. Siehe „Eingeklemmter Schutt hält sich an bewegten Stücken“ in [Optimierungen.md](Optimierungen.md) |
| F27 | **Gelenke beim Schnitt der Gebiete mitwiegen**: Der Schnitt einer großen Insel in ihre Gebiete und die Frage, wann er neu schneidet, zählen nur Kontakte. Seit F12 rechnet jedes Gebiet auch seine Gelenke, ein Gelenk kostet etwa so viel wie zweieinhalb Kontakte. Liegen viele Ragdolls in wenigen Gebieten, warten die anderen Threads in jedem Durchgang auf diese. Gelenke im Gewicht der Körper beim Schnitt mitzählen und beim Urteil, ob die Gebiete ungleich sind, mit den Gelenken pro Gebiet aus dem letzten Schritt | was vom Gewinn von F12 fehlt, mit 20 Ragdolls auf der Platte und vier Threads bis 0,19 ms pro Schritt, mit 8 Threads auf dem Ryzen, wo jedes Gebiet einen Thread hat, mehr | F12 | 🔜 niedrig, erst wenn viele Ragdolls in einem Haufen liegen |
| F28 | **Ragdoll durch die Wand**: Ein Ragdoll, der gegen ein Zerstörbares fliegt, bricht nach seiner Energie und dem Material durch oder prallt ab, nach 26.16, 26.19 C, 28.5 und 30.9 der Roadmap des Spiels: Holz und Beton in verschiedenen Dicken, frontal und seitlich, mehrere Kontakte in einem Schritt, kein doppelter Stoß, keine vergessene Kollision, durchgebrochene Kante, Trümmer auf der liegenden Figur, Aufstehen oder Abbruch. Eine Figur, die gegen eine Wand flog, kam nie hindurch, wie schwach die Wand auch war | die Abnahme aus 26.16 | F4 | ✅ auf `active-ragdoll-19`: Ihre Knochen trafen als einzelne kleine Einschläge mit ihren eigenen Massen, nach einem Bruch bekam nur der treffende Knochen seine Geschwindigkeit zurück, und Stücke, die an einer Verbindung hängen blieben, hielten sie auf wie die Wand. Dünne Bretter brachen gar nicht. Jetzt sind alle Kontakte der Knochen einer Figur mit einem Teil eines Zerstörbaren in einem Schritt ein Einschlag und ein Kollisionsereignis (`nbEvents::collisions`), mit der Arbeit ihrer Stöße als Energie und einem Schaden über alle Kontakte hinaus, und jeder Kontakt drückt das Stück unter sich heraus, wenn dessen Verbindungen zusammen weniger halten. Wo die Stücke unter den Kontakten weggebrochen sind, bekommt jeder Knochen bis zu 60 % seiner Geschwindigkeit zur Wand von vor dem Schritt zurück, zusammen nie mehr als 60 % des Schwungs der Figur. Mit 1 000 N s aus 2 m Abstand geworfen, bricht ein Mensch der Bibliothek in 4 von 8 Würfen rückwärts durch Bretter von 5 cm und in 6 von 8 seitlich durch Bretter von 3 cm, nie durch Bretter von 20 cm oder Beton von 20 cm, die bis zu 62 und 44 Stücke verlieren, und ein Tritt von 150 N s bricht nichts. Ohne Figuren bitgleich wie `active-ragdoll-18`. Siehe „Ragdoll durch die Wand“ in [Optimierungen.md](Optimierungen.md) |
| F29 | **Treffer wie auf `ragdoll-11`**: Ein harter Treffer machte eine stehende Figur sofort zum Ragdoll, 0,4 s ohne Kraft in den Gelenken, sie klappte an der Hüfte zusammen und lag nach einer halben Sekunde, wie auch immer sie getroffen wurde, und eingerollt auf dem Rücken warf sie ihre Beine hoch. Auf dem Branch `ragdoll-11` taumelte dieselbe Figur mit echten Schritten, behielt ihre Kraft und fiel nur, wo kein Schritt reichte. Das in die Bibliothek bringen, nach 26.13 bis 26.15 und 28.3 der Roadmap des Spiels | Figuren, die nach einem Treffer taumeln und sich fangen, auf Wunsch des Nutzers | F9, F10 | ✅ auf `active-ragdoll-20`: Neuer Zustand `nb_figureBalancing` mit der Balance von `ragdoll-11` in `src/figure_balance.c`: Die Knochen werden dynamisch, die Gelenke ziehen mit voller Kraft zur Haltung des Clips, 800 N m pro rad bis 150 N m, ein Gelenk gegen einen statischen Anker hält das Becken mit höchstens 250 N m aufrecht, und ein Fuß tritt dorthin, wo der Capture Point bei seiner Landung sein wird. Wo kein Schritt reicht, fällt die Figur als Ragdoll, ruhig auf beiden Füßen steht sie nach 0,5 s wieder animiert. Auf die obere Brust getroffen fängt sich der Mensch der Bibliothek von vorn bis 70 N s, von hinten bis 100 N s und von beiden Seiten bis 60 N s, darüber fällt er gestreckt wie auf `ragdoll-11`. `stunTime` ist null, das Einrollen mild wie dort. Ohne Figuren bitgleich wie `active-ragdoll-19`, eine Figur, die sich fängt, kostet im Update rund 4,4 µs und Box3D rund 0,06 ms pro Schritt mit einem Thread. Siehe „Treffer wie auf ragdoll-11“ in [Optimierungen.md](Optimierungen.md) |
| F30 | **Aufwachen im Stand**: Ein Ragdoll, der auf seinen Füßen ruht, das Becken höher als 60 cm über dem Boden, findet keinen Boden unter dem Becken und wartet für immer (`nb_waitGround`). Im Benchmark mit 50 Figuren hängt seit `active-ragdoll-20` am Ende eine so an ihrem Arm, der in einem Riss ihrer Mauer steckt, das Becken 94 cm hoch. Wie WakeUp in Overgrowth das Stehen vom Fallen unterscheidet, soll sie stehend aufwachen, nach 26.15 der Roadmap des Spiels | keine Figur, die für immer auf ihren Füßen an einer Wand hängt | F5, F29 | ✅ auf `active-ragdoll-21`: Bevor ein Mensch der Bibliothek prüft, ob er aufstehen kann, prüft er, ob er steht: beide Füße auf Boden, auf dem er stehen könnte, das Becken mindestens 60 % der Standhöhe darüber, die Wirbelsäule höchstens 1 rad geneigt, nichts, was auf ihm lastet, und Platz für Rumpf und Kopf in der Standhaltung zwischen seinen Knöcheln. Dann steht er dort, wie eine Figur nach dem Abfangen, der Stand-Clip blendet in 0,6 s ein, die Beine mit, und die Füße treten unter den Körper. Ein neuer Test hängt eine Figur an ihrem Unterarm in einen engen Tunnel aus vier festen Blöcken: Sie wacht nach 1,47 s stehend auf, kein Knochen schneller als 2,9 cm pro Schritt. Alle übrigen Tests enden bitgleich. Im Benchmark wacht die hängende Figur der 50 stehend auf, am Ende stehen 49 statt 48, alle übrigen Szenen enden bitgleich wie auf `active-ragdoll-20`. Siehe „Aufwachen im Stand“ in [Optimierungen.md](Optimierungen.md) |
| F31 | **Treffer je Körperpartie**: Ein Treffer unter `knockdownImpulse` dreht den ganzen Rumpf um das Becken weg, Kopf und Arme folgen ihm starr. Nach 26.4 und 26.5 der Roadmap des Spiels soll ein leichter Treffer Brust, Schulter, Kopf und Arme je nach Trefferlage additiv auslenken, mit Gewicht und Dämpfung je Körperpartie, die Füße im Stand, wie die Mikro-Reaktionen von Kickback (Rumpf beugen, Kopf schnellt zurück, Drehung um die Hochachse) | lesbare leichte Treffer im Nahkampf, ohne die Figur dynamisch zu machen | F3 | ✅ auf `active-ragdoll-22`: Hals, Kopf und Arme des Menschen der Bibliothek haben eigene gedämpfte Federn (`nbBoneDef::flinch`, `flinchHertz`, `flinchDamping`). Ein Treffer auf eine Kette mit Federn dreht sie wie einen Körper um ihre Gelenke, mal `flinch`, der Rumpf dreht sich nur mit dem, was ihr oberstes Gelenk weitergibt, und was einen hängenden Arm gegen den Körper drückt, nimmt der Körper. Die übrigen Glieder hängen nach, der Kopf in seiner Drehung, die Arme als Pendel, ohne in den Körper zu rutschen. Ein Gesichtstreffer von 20 N s reißt den Kopf 0,40 rad gegen die Brust zurück, ein Treffer von hinten auf den Unterarm schwingt die Hand 23 cm nach vorn, und Brusttreffer drehen den Rumpf wie bisher. Gut 2 µs pro schwingender Figur im Update, Szenen ohne zuckende Figuren bitgleich wie `active-ragdoll-21`. Siehe „Treffer je Körperpartie“ in [Optimierungen.md](Optimierungen.md) |
| F32 | **Schmerz und lockerer Oberkörper beim Abfangen**: Eine Figur, die sich fängt, zog alle Gelenke mit voller Kraft. Nach der Idee von BIMOS soll ein Treffer das getroffene Gelenk schwächen und weniger auch seine Nachbarn, und die Kraft kommt mit der Zeit zurück. BIMOS nimmt dafür 0,025 pro N s, 45 % für die Nachbarn, 15 % Restkraft und 0,9 pro Sekunde, nur zum Vergleich: Wie stark, wie weit und wie schnell, misst die Bibliothek an ihren eigenen Figuren. Nach der Idee von Kickback bleiben beim Stolpern nur Beine und Becken fest, Rumpf, Kopf und Arme lockerer | Kopf und Arme fliegen beim Treffer sichtbar mit, die Beine fangen weiter | F29 | 🔜 hoch für den Schmerz. Die Arme hängen seit `active-ragdoll-23` mit 60 % der Kraft, wo sie nicht rudern, siehe F35. Mit 30 % fing die Figur nicht mehr und stand später ruhig. Bestätigt in der Durchsicht vom 2. November 2026 von godot-active-ragdoll und bevy-ragdoll, mit Einzelheiten: Die Schwächung läuft den Baum entlang, zu den Kindern weiter als zu den Eltern, um so weiter, je stärker der Treffer, mit einer Untergrenze je Körperteil, die Wirbelsäule behält Spannung, ein Treffer gegen ein Bein lässt auch das andere Knie nachgeben, und die Kraft kommt nach einer Haltezeit zurück, je Körperteil zu seiner Zeit. Das gilt auch für die Antriebe folgender Kämpfer, siehe F64 |
| F33 | **Mindestträgheit kleiner Knochen**: Bei breitem Stand zittert eine Hand am Oberschenkel dauernd mit 0,26 bis 0,32 m/s, gemessen in F29. BIMOS hebt die Trägheit jedes Knochens auf mindestens seine Masse mal (3 cm)², weil kleine Trägheiten unter starken Antrieben zittern | ruhige Hände und Füße beim Abfangen und am Boden | F29 | 🔜 mittel, klein und messbar. Eine Armature von 0,5 oder 2 g m² am Handgelenk änderte auf `active-ragdoll-29` das Zittern der Hände beim Abfangen nicht, siehe F48 |
| F34 | **Aufrichten auch an der Brust**: Das Gelenk gegen die Welt hält nur das Becken aufrecht. BIMOS legt die Hälfte seines aufrichtenden Drehmoments auf die Brust, damit ein schwerer Oberkörper nicht über das Becken klappt | vielleicht höhere Grenzen des Abfangens, heute von vorn 70 N s | F29 | 🔜 mittel, mit der Abtastung aus F29 messen |
| F35 | **Arme beim Stolpern und zum Boden**: Beim Abfangen bleiben die Arme in der Haltung des Clips. Nach der Idee von Kickback sollen sie beim Stolpern rudern, und bei einem Sturz nach vorn oder zur Seite greift der führende Arm zum Boden, nach den Schutz-Layers aus 26.13 der Roadmap des Spiels. Auf Wunsch des Nutzers sollen die Arme freier schwingen und wirklich beim Gleichgewicht helfen, wie die Füße | sichtbare Selbsterhaltung wie in Euphoria | F29, F32 | ✅ das Rudern auf `active-ragdoll-23`: Von 109 Stürzen bei 184 Brusttreffern fielen 100 zwei Updates nach dem Treffer, weil kein einzelner Schritt den Capture Point erreichte, bevor Schritte oder Arme etwas tun konnten. Jetzt dreht der Motor jeder Schulter ihren Oberarm in Fallrichtung, wenn der geplante Schritt mehr als 10 cm zu kurz kommt, nach der Idee von Hof (2007), nie in den Körper, und das Gegenmoment bremst über die Füße den Schwerpunkt. Sonst hängen die Arme mit 60 % der Kraft. Die Balance gibt erst auf, wenn der beste Schritt den Capture Point mehr als 1,1 m draußen ließe, sie tritt bis zu viermal in Folge, und ein Fuß, der nach einem Ausfallschritt in der Luft hängt, tritt ab. Bei 1 488 Treffern aus acht Richtungen fängt sich die Figur 1 206- statt 736-mal, die Arme tragen 121 davon, auf die obere Brust von vorn bis 110 statt 70 N s, von hinten bis 140 statt 100 und von der Seite bis 80 statt 60 N s, gleich schnell ruhig. Siehe „Rudernde Arme“ in [Optimierungen.md](Optimierungen.md). ✅ Auf `active-ragdoll-24` schwingen die Arme, statt zu rucken, auf Wunsch des Nutzers mit Rampen und einem langsamen Sinus: geplant auf einer Rampe hinaus, je weiter, je kürzer der Schritt kommt, mit gebeugtem Ellbogen, halten mit langsamen Ruderschlägen, solange die Figur tritt, und auf einer Rampe zurück, und die Balance tritt bis zu fünfmal. Gegen die Brust drehen die Arme im Median 6,0 statt 17,7 rad/s, die Figur fängt 1 211 statt 1 206 Treffer, alle 736 von `active-ragdoll-22`, und bis 80 N s fällt von 672 keiner mehr, siehe „Schwingende Arme“. 🔜 mittel bleibt der Griff zum Boden beim Sturz. Nach vorn stützen sich die Arme schon ab, `nb_clipBrace`, zur Seite und nach hinten rollt sich die Figur nur ein, `nb_clipCurl`. Der Griff zum Boden beim Sturz zur Seite ist in Runde 83 begonnen. ⛔ Runde 83: Greift der Arm der Fallseite zum Boden, sobald die Figur fällt, und neigt sich der Kopf weg, schlägt bei Stürzen zur Seite der Kopf nur noch halb so oft zuerst auf, die Hand kommt aber nur 13 statt 20 Punkte öfter zuerst auf. Der Arm hebt sich erst, wenn die Balance aufgibt, im Median 0,28 s vor dem Aufschlag, nichts gebaut, siehe Runde 83 |
| F36 | **Verletzungen je Körperbereich**: Nach 30.6 der Roadmap des Spiels verschlechtern Bein-, Arm- und Rumpftreffer Stand, Deckung und Recovery. BIMOS lässt einem verkrüppelten Glied 15 % seiner Kraft, ein verkrüppeltes Bein trägt nicht mehr, und mit zwei solchen Beinen steht die Figur nicht mehr auf. Kickback lässt verletzte Glieder hängen. Die Bibliothek bräuchte die Kraft je Knochen zur Laufzeit und eine Balance, die ein schwaches Bein kennt | Verletzungen, die man sieht und im Kampf spürt | F29 | 🔜 erst, wenn das Spiel sein Schadensmodell festlegt |
| F37 | **Muskeln nach der bewegten Trägheit**: Jedes Gelenk zieht heute mit 800 N m pro rad, mal einem Faktor je Knochen, der von Hand gesetzt ist: dreifach in den Segmenten des Rumpfs und 0,3 an Hals und Kopf, weil ein Hals mit voller Steifigkeit den Kopf schüttelte. Box3D rechnet Feder und Dämpfung gegen die Trägheit der zwei Körper eines Gelenks, deshalb bekommen die Beine von Hand dreifache Dämpfung, ihre Gelenke bewegen weit mehr. BIMOS rechnet beides aus der Trägheit von allem, was ein Gelenk bewegt, k = Kraft × I × ω², mit einer Frequenz je Gruppe (Rumpf 22, Kopf 16, Arm 18, Hand 12, Bein 30 und Fuß 24 rad/s), und begrenzt das Drehmoment bei einem Fehler von 40° bis 60°, über den hinaus ein Stoß gewinnt | Gelenke, die alle gleich schnell und gleich gedämpft antworten, ohne Faktoren von Hand, auch an Rigs eines Spiels mit anderen Massen | F29 | 🔜 mittel, ändert jede Figur mit dynamischen Knochen, die Grenzen des Abfangens und das Zittern aus F33 dabei neu messen |
| F38 | **Zeitfilter vor dem Sturz**: Eine Figur, die sich fängt, fällt im selben Schritt, in dem ihr Becken mehr als 0,6 rad kippt oder ihr Schwerpunkt unter 75 % seiner Höhe im Stand sinkt, und nach 0,3 s ohne Fuß am Boden. BIMOS wartet: die Brust mehr als 55° geneigt für 0,12 s, das Becken unter 55 % der Standhöhe für 0,35 s, kein Fuß am Boden für 0,75 s | ein kurzer Ausschlag, den das Aufrichten zurückholt, endet nicht mehr im Sturz, vielleicht höhere Grenzen des Abfangens | F29 | 🔜 mittel, mit der Abtastung aus F29 messen |
| F39 | **Treffer gegen die Beine**: Ein Tritt gegen das Schienbein unter `knockdownImpulse` lässt den Rumpf zucken und die Figur, wo nötig, einen Schritt machen, wie ein Treffer an der Brust, das Bein selbst wird nicht weggezogen. BIMOS zählt Treffer an den Beinen 1,4-fach gegen seine Schwelle zum Sturz, für Feger, und misst seine Schwellen an der Masse der Figur, 16 N s zum Taumeln und 60 N s zum Sturz pro 70 kg, die Bibliothek nimmt 50 N s für jede Masse. Sweeps sind Angriffe der Kämpfer und des Giants in der Roadmap des Spiels | ein Feger holt die Figur von den Füßen | F9, F29 | 🔜 mittel, zuerst messen, was ein Feger heute bewirkt |
| F40 | **Liegen bleiben zur Laufzeit und geführter Tod**: Ob eine Figur aufsteht, legt `canGetUp` beim Anlegen fest, ein Spiel kann eine Figur später nicht bewusstlos oder tot liegen lassen, ohne sie neu anzulegen. Kickback schaltet eine Figur dafür dauerhaft zum Ragdoll und lässt sie auf Wunsch geführt sterben: Der Sterbe-Clip läuft, die Gelenke ziehen ihm mit halber Kraft nach, die in 0,5 s auf null sinkt, so formt der Clip den Sturz, und am Ende liegt ein schlaffer Ragdoll. Nach 26.13 der Roadmap des Spiels haben bewusstlose Ragdolls keine Gelenkkraft, Verletzte winden sich mit r_writhe | K.O. und Tod im Kampf | F5 | 🔜 hoch für das Liegenbleiben, klein; der geführte Tod mittel. Für den geführten Tod aus der Durchsicht vom 2. November 2026: godot-active-ragdoll lässt die Muskeln gruppenweise nach, zuerst die Beine, dann Rumpf und Kopf, zuletzt die Arme, so knickt der Körper in den Knien ein und sackt zusammen, und die Arme werden zuletzt schlaff |
| F41 | **Ermüdung und Treffer-Serien**: Heute zählt jeder Treffer allein gegen `knockdownImpulse`, und eine Figur, die sich gefangen hat, steht danach mit voller Kraft. Bei Kickback setzt jeder Treffer Ermüdung an, die die Kraft um bis zur Hälfte senkt und mit 0,05 pro Sekunde in etwa 20 s abklingt, schnelle Treffer hintereinander steigern sich über einen Multiplikator, und Dauerfeuer führt deterministisch vom Zucken über das Taumeln zum Sturz | Combos und Dauerfeuer bringen eine Figur verlässlich zu Fall | F32 | 🔜 niedrig, mit dem Kampfsystem des Spiels klären, was davon die Bibliothek übernimmt. Auch bevy-ragdoll schwächt bei Treffern kurz hintereinander stärker, Durchsicht vom 2. November 2026 |
| F42 | **Vorahnung eines Treffers**: Kickback lässt eine Figur sich gegen einen Angriff stemmen, bevor er trifft (`anticipate_threat`), wenn ein Gegner ausholt oder Kugeln vorbeifliegen. Nach 26.4 der Roadmap des Spiels reagiert ein geblockter Schlag anders als ein ungedeckter Treffer. Die Bibliothek bräuchte dafür einen Aufruf, mit dem sich eine Figur zur Laufzeit gegen eine Richtung stemmt und Treffer von dort besser fängt | Deckung und Block, die man in der Physik spürt | F29 | 🔜 niedrig, erst wenn das Spiel Block und Deckung festlegt |
| F43 | **Obergrenze gleichzeitig dynamischer Figuren**: Kickback lässt höchstens 5 aktive Ragdolls zugleich zu, ein weiterer Treffer lässt die Figur nur taumeln, ein gewollter Sturz und ein Tod gehen vor. Nach 26.17 der Roadmap des Spiels soll es eine Höchstzahl voll simulierter Ragdolls nach Priorität geben, nach Nähe zum Spieler, Sicht, aktiver Kollision und Bedeutung fürs Spiel, ohne Kontakte zu Trümmern bloß wegen der Entfernung abzuschalten, und erst nach Messungen mit 1, 10 und 50 Figuren und Einsturzszenen | feste Kosten auch in Massenszenen | F6, F29 | 🔜 niedrig, erst wenn Messungen zeigen, dass viele gleichzeitig dynamische Figuren das Bild sprengen; keine Kollision wird dabei abgeschaltet. bevy-ragdoll nimmt bei voller Grenze deterministisch die älteste dynamische Figur heraus und meldet das dem Spiel. Bei uns ginge das für folgende Kämpfer, die dabei kinematisch stehen bleiben, seit Runde 43 kostet jede rund 0,065 ms pro Schritt, nie für einen fallenden Ragdoll, der in der Luft erstarren würde |
| F44 | **Schwingenden Fuß zum Ziel ziehen**: BIMOS zieht einen Fuß, der einen Schritt macht, zusätzlich zu den Gelenken direkt zu seinem Ziel, mit 14 rad/s und höchstens 80 m/s², fest im Schwung und leicht, solange er steht, weil seine Gelenke allein den Fuß nicht schnell genug nach vorn brachten und die Zehen am Boden hängen blieben | Schritte, die rechtzeitig ankommen | F29 | 🔜 niedrig, nur wenn eine Messung zeigt, dass die Schritte der Bibliothek zu spät ankommen |
| F45 | **Angenähertes Muskel-Skelett-Modell**: Jedes Gelenk ist heute eine Feder mit Dämpfer und Drehmomentgrenze, die sofort ihre volle Kraft gibt, in jeder Lage und bei jeder Geschwindigkeit. Darum ruckten die rudernden Arme, und auf `active-ragdoll-24` bekamen die Arme eigene Rampen. Auf Wunsch des Nutzers soll ein angenähertes Muskel-Skelett-Modell auf der Ebene der Gelenke dazukommen, nach der Idee von Hill (1938) und von Jiang et al. (2019), die Gelenkmomente mit Grenzen nach Winkel und Geschwindigkeit statt einzelner Muskeln rechnen: eine Aktivierung, die mit der Zeit anspannt und loslässt, ein Drehmoment, das mit der Geschwindigkeit in Zugrichtung sinkt und beim Bremsen steigt, eines nach dem Winkel, passive Momente der Bänder nahe den Grenzen, später Sehnen als nachgiebige Feder in Reihe und eine Reaktionszeit der Balance. Die Bibliothek setzt Steifigkeit und Grenze jedes Gelenks in jedem Update aus diesem Zustand, die Federn mit Grenze hat Box3D schon (F1). Übernommen werden nur die Ideen, die Werte werden an den eigenen Figuren gemessen und abgestimmt | Bewegungen, die an keinem Gelenk rucken, ohne Sonderregeln wie die Rampen der Arme, glaubwürdige Kraft beim Abfangen und Aufstehen, später Ermüdung (F41) | F29, F35, mit F37 zusammen abstimmen | ✅ Stufe 1 auf `active-ragdoll-25`, als Schalter `nbFigureMotors::muscles`, standardmäßig aus: Aktivierung mit 30 ms hinauf und 80 ms hinab, Kraft nach der Kurve von Hill mit Bremskraft 1,5 an Rumpf, Hals und Kopf (25 rad/s), Arme und Beine ohne. An 4 464 Treffern fängt die Figur mit Muskeln 3 644-mal statt 3 627-mal, etwa jeder 30. Treffer geht anders aus, in beide Richtungen, 80 N s von vorn werfen sie in der Szene der Tests um, und paarweise ruckt der Rumpf im Median gleich, bei Treffern ab 100 N s auf den Bauch ein Fünftel weniger, auf die Brust ein Sechstel mehr. Die Rampen der Arme bleiben: Muskeln allein glätten keinen Zug zu einem fernen Ziel, das Rudern von `active-ragdoll-23` mit Muskeln drehte die Arme mit 14 bis 18 rad/s. Gelernt: Die Kurve wirkt über die Feder des Gelenks und nimmt ihr bei Tempo auch Dämpfung und Halt, an Armen und Beinen schadet das. Siehe „Muskeln“ in [Optimierungen](Optimierungen.md). 🔜 niedrig für Stufe 2 und 3: erst, wenn der Nutzer die Muskeln in der Demo will, dann aktive Kraft und Dämpfung trennen, Kraft nach Winkel und Bänder, Reaktionszeit und Sehnen |
| F46 | **Treffer entlang des Skeletts**: Ein harter Treffer verteilte sich wie in Overgrowth nach der geraden Linie vom Treffpunkt zum Schwerpunkt jedes Knochens. Ein Unterarm, der neben der Hüfte hängt, bekam so von einem Treffer auf ihn nur ein Fünftel und das Becken daneben fast ebenso viel, ein Tritt gegen ein Schienbein schob das andere Bein mit einem Fünftel des Stoßes mit. Auf Wunsch des Nutzers soll der Weg vom Treffpunkt durch die Gelenke zählen, und als Versuch der Anteil des getroffenen Knochens am Treffpunkt wirken, mit seinem Drehmoment | Treffer an Armen und Beinen bewegen das getroffene Glied statt des Rumpfs daneben, ein Tritt gegen ein Bein lässt das andere stehen | F29, F31 | ✅ auf `active-ragdoll-26`, ohne Schalter: Von 10 N s auf den Unterarm nimmt er die Hälfte statt ein Fünftel und das Becken 4 %, von einem Tritt gegen das Schienbein bleiben drei Viertel im Schienbein und 2 % im anderen Bein, Brust und Kopf nehmen fast so viel wie vorher. An je 256 Treffern mit 40 bis 130 N s aus acht Richtungen dreht ein Treffer auf den Unterarm die Schulter im Median um 1,59 statt 0,69 rad, ein Tritt gegen das Schienbein wirft die Figur 61-mal statt 106-mal um, sehr harte Treffer ab 88 N s auf den Arm reißen sie öfter mit, ein Treffer auf den Kopf 165-mal statt 154-mal. An 4 464 Treffern auf Brust und Bauch fängt sie sich 3 623-mal statt 3 627-mal, mit denselben Grenzen, von 168 leichten Treffern fällt keiner mehr, aber ihr Rumpf dreht sich bei einem Treffer auf die Brust paarweise um 19 % schneller, weil der getroffene Knochen mehr nimmt. Das Drehmoment am Treffpunkt ist verworfen, mit ihm fing sie sich nur 3 388-mal. Siehe „Treffer entlang des Skeletts“ in [Optimierungen](Optimierungen.md). Offen bleiben leichte Treffer gegen die Beine und Feger, F39 |
| F47 | **Gelenke wie beim Menschen**: Ein Kugelgelenk begrenzte den Schwung seines Knochens mit einem runden Kegel bis 90°, in jede Richtung gleich weit. Die Schulter ließ den Arm so 86° nach vorn schwingen und ebenso weit zurück und zur Seite, die Hüfte beugte das Bein mit ihrer Drehgrenze wie ein Scharnier. Auf Wunsch des Nutzers sollen die Schultern Kugelgelenke wie beim Menschen sein, nach vorn, hinten, zur Seite und dazwischen, und alle Gelenke sollen sich so weit und in die Richtungen bewegen wie bei einem Menschen, mit ihren echten Grenzen | Arme, die über den Kopf schwingen, aber nur wenig zurück, Beine, die weit nach vorn und wenig zurück gehen, Haltungen wie die eines Menschen | F1, F35 | ✅ auf `active-ragdoll-27`, ohne Schalter: Box3D hat einen Kegel mit eigener Grenze in jede der vier Richtungen bis 0,95 π, dazwischen eine Ellipse im Tangens eines Viertels des Schwungs, um 16 Achsen angedreht liegt ein Stab höchstens 0,13° jenseits. Der Mensch der Bibliothek hat die Normalwerte der Neutral-Null-Methode, Schulter 160° nach vorn und zur Seite und 40° zurück, Hüfte 135° nach vorn und 20° zurück, nur das Sprunggelenk wie unter dem Gewicht des Körpers 40° hinauf und 50° hinab. 90 N s von hinten auf den Unterarm schwingen den Arm 162° hoch statt 89°, von vorn 51° zurück statt 90°. An 4 464 Treffern auf Brust und Bauch fängt sich die Figur 3 617-mal statt 3 623-mal, mit denselben Grenzen, steht im Mittel 0,1 s früher ruhig und braucht nur 88-mal statt 128-mal über 4 s. Sehr harte Treffer ab 106 N s auf den Unterarm werfen sie 60-mal statt 22-mal von 80 um, die Schulter dreht sich dabei im Median 2,15 statt 1,59 rad, und von 168 leichten Treffern fällt einer. Ein Schritt kostet so viel wie vorher. Verworfen: die Normale der Ellipse als Achse, das Sprunggelenk nach Lehrbuch und Hals und Kopf mit den alten Grenzen, siehe „Gelenke wie beim Menschen“ in [Optimierungen](Optimierungen.md). Offen bleiben die Drehung um die Längsachse, F50, und ein Hals, der den Kopf mit voller Kraft hält, F48 |
| F48 | **Gelenke mit Armature**: Ein Gelenk bekommt eine zusätzliche Drehträgheit auf die Relativdrehung seiner beiden Knochen, im Löser von Box3D, nach der Idee von MuJoCo, die auch der Fork VPhysics-Box3D von DMS-in-Gmod (GPL-3, nur die Idee) für Box3D umsetzt: Innerhalb eines Teilschritts hält sie die relative Drehgeschwindigkeit weich fest. Heute zieht der Hals mit 0,3 der Steifigkeit, weil seine zwei Federn den leichten Hals schneller drehten als die halbe Zahl der Teilschritte pro Sekunde und den Kopf schüttelten, und eine Hand zittert am Oberschenkel, F33. Seit F47 bewegen sich Hals und Kopf so weit wie bei einem Menschen, halten ihn aber nur mit diesen schwachen Federn | ein Hals, der den Kopf hält wie ein Mensch, ruhige Hände, vielleicht ohne F33 | F1, F47 | ✅ auf `active-ragdoll-29`. Armature heißt hier, wie bei MuJoCo, eine Zusatzträgheit im Gelenk, nicht das Skelett wie in Blender. Box3D gibt Dreh- und Kugelgelenken mit `armature` ein Schwungrad, das an die Drehung von Körper B gegen Körper A gekoppelt ist und über die Schritte weiterdreht, Gelenke ohne rechnen bitgleich wie vorher. Die Bibliothek setzt es mit `nbBoneDef::armature` und stellt die Feder auf die schwerere Drehung ein. Hals und Kopf des Menschen haben je 1 g m², so viel wie der Hals um seine Längsachse. Mit voller Steifigkeit hält der Hals den Kopf dann ruhig, die Figur fängt sich damit aber an 4 464 Treffern auf Brust und Bauch nur 3 574-mal statt 3 620-mal. Deshalb bleibt der Hals bei 0,3 der Steifigkeit, mit der Armature: Der Kopf kehrt beim Abfangen in 8 statt 11 % der Schritte um, die Figur fängt sich 3 623-mal, an den Gliedern 16-mal öfter von 1 280 und von der Seite bis 90 statt 80 N s, braucht aber 71- statt 61-mal länger als 4 s. Die zitternde Hand, F33, beruhigt eine Armature am Handgelenk nicht. Siehe „Gelenke mit Armature“ in [Optimierungen](Optimierungen.md). ✅ Auf `active-ragdoll-59` dämpfen Hals und Kopf achtmal so stark, solange die Figur sich fängt, `nbBoneDef::catchDamping`, eine Idee der Branch `ragdoll-sol`: Nach 60 N s auf die Brust dreht sich der Kopf gegen den Hals mit höchstens 2,8 statt 5,3 rad/s, und die Drehung ändert sich von Schritt zu Schritt um 0,17 statt 0,43 rad/s. Mit der Stütze gegen die Welt fallen dafür 28 harte Treffer mehr, siehe Runde 59 |
| F49 | **Szenen aus dem Alltag als Tests**: DMS-in-Gmod vergleicht jede Szene Bit für Bit mit einem Referenzprogramm: Stoß, Schuss, Schrotflinte, Explosion, Abfangen im Sturz, Treppe, Bordstein, niedrige Mauer, Dachkante, knapp verfehlt, betrunken, Aufstehen. Die Tests der Bibliothek prüfen Figuren auf ebenem Boden, an Wänden, auf Schutt, an Stufe und Hang. Eine Treppe hinunter, ein Bordstein, eine niedrige Mauer, gegen die eine Figur fällt, und eine Dachkante fehlen | Fehler an Kanten fallen auf, bevor ein Spiel sie findet | F10, F29 | 🔜 niedrig |
| F50 | **Drehung um die Längsachse**: Ein Treffer an der Schulter soll den Oberkörper auch um seine Längsachse drehen. Gemessen auf `active-ragdoll-26`: Ein leichter Treffer dreht den Rumpf um eine Achse aus Hebel mal Stoß um das Becken, mit gleichem Winkel in jede Richtung, so dreht er bei 45 N s vorn an der rechten Schulter die Schulterlinie um 7° und neigt die Brust um 20°, ein Drittel. Ein harter Treffer auf die Brust neben der Mitte dreht die Schulterlinie in der ersten Zehntelsekunde um 3 bis 5°, weil der Anteil des getroffenen Knochens seit F46 ohne Drehmoment wirkt, einer auf den Oberarm um 21 bis 25°, weil der Arm die Schulter mitzieht. Ein Oberkörper dreht sich um seine Längsachse viel leichter, als er um die Hüfte kippt. Idee: Beim leichten Treffer Drehung und Neigung trennen, jede nach der Trägheit des Oberkörpers um ihre Achse, beim harten nur den Anteil des Drehmoments um die Längsachse an den Rumpf geben, nicht das ganze, das F46 verwarf | Schultertreffer, die man sieht | F31, F46 | ✅ auf `active-ragdoll-28`, ohne Schalter: Der Oberkörper des Menschen, 36 kg, hat um die Achse die Wirbelsäule hinauf eine Drehträgheit von 0,8 kg m², um die Achsen quer dazu 4,9 und 5,4. Ein leichter Treffer dreht ihn um diese Achse 2,5-mal so weit, wie er ihn kippt, die Wurzel des Verhältnisses, und so viel schneller zurück, wie bei Muskeln, die um jede Achse gleich steif sind, geteilt nach den Drehbereichen der Wirbelsäule bis 40°: 30 N s vor der rechten Schulter drehen die Schulterlinie um 12° statt 5°. Ein harter Treffer auf den Rumpf gibt dem Oberkörper als einem Körper das Moment um die Längsachse, das die Verteilung entlang des Skeletts auslässt: 60 N s vor der Schulter drehen sie in einer Zehntelsekunde um 43° statt 3°. An 4 464 Treffern auf Brust und Bauch fängt sich die Figur 3 620-mal statt 3 617-mal, steht früher ruhig und stolpert nur 61- statt 88-mal länger als 4 s, von 168 leichten Treffern fallen aber 5 statt 1, alle von der Seite quer über die Brust. Verworfen: das ganze Trägheitsverhältnis beim leichten Treffer, das halbe Moment beim harten und das Moment auch bei Treffern auf den Oberarm, siehe „Drehung um die Längsachse“ in [Optimierungen](Optimierungen.md). Offen bleibt, dass sich die Balance nach der Richtung des Beckens richtet und eine gedrehte Figur zu breit stehen kann, siehe F59 |
| F51 | **Unter Schutt freikommen**: Lastet Schutt auf einem Ragdoll, wartet er nach 26.15 der Roadmap des Spiels, bis er fort ist, notfalls für immer, und meldet `nb_waitDebris`. In der Szene der Tests fällt ein Block von 154 kg auf eine Figur, die hinter einer Wand aus Brettern aufsteht. In 4 der 11 von 32 Bruchmustern, in denen sie durchbricht, bleibt er auf ihr liegen, dreimal halb auf dem Boden auf einem Arm oder Bein, einmal ganz auf dem Bauch, auf `active-ragdoll-26` in 6 von 13. Ein Mensch zöge das Glied heraus oder schöbe den Block von sich. Idee: Trägt der Boden den größten Teil des Schutts, zieht die Figur das Glied mit der Kraft ihrer Muskeln gegen die Reibung heraus, liegt leichter Schutt auf ihr, schiebt sie ihn von sich, und dann steht sie auf, sonst wartet sie weiter | Figuren, die nicht unter einem Block für immer liegen bleiben, wo ein Mensch frei käme | F11, F30, F45 | 🔜 niedrig |
| F52 | **Inspektor**: Die Demo zeigt bei einer gewählten Figur Capture Point, Stützfläche, Schrittziele, Gelenke an ihrer Grenze, die Kraft der Muskeln, die Verteilung eines Treffers und den Verlauf der Zustände, dazu die Wiedergabe eines einzelnen Kampfes Schritt für Schritt. Die Daten liefert die Bibliothek mit einer eigenen Zeichenschnittstelle wie die von Box3D, so sieht ein Spiel dasselbe, und die Demo zeichnet nur | jede Runde lässt sich in der Demo auf dem Ryzen prüfen und abstimmen, nicht nur ihr Ergebnis | F6, F7 | 🔜 auf Wunsch des Nutzers nach der besseren Balance, F59 bis F61 |
| F53 | **Eigene Rigs des Spiels**: Heute machen nur die 19 Knochen des Menschen der Bibliothek Ausfallschritte, fangen sich und rudern, andere Rigs fallen bei einem harten Treffer gleich. Die Figuren aus der Rig-Pipeline des Spiels, Kapitel 19, brauchen eine Zuordnung, welcher Knochen Becken, Wirbelsäule, Hals, Kopf, Schlüsselbein, Arm, Hand, Bein, Fuß und Zehen ist, und Archetypen für leichte und schwere Kämpfer und den Giant aus einem Profil, mit F37 statt Faktoren von Hand | alles, was die Bibliothek kann, für die Figuren des Spiels | F37 | 🔜 auf Wunsch des Nutzers nach F52 |
| F54 | **Griffe und Würfe**: Eine Hand greift einen Körperteil des Gegners über ein Gelenk mit Kraftgrenze, zieht, drückt oder wirft, nach 30.7 mit einer kurzlebigen Anbindung von Hand und Arm statt einer geskripteten Animation für zwei. Der Griff reißt bei zu viel Kraft und, nach 30.7, bei fehlender Reichweite, einem Hindernis, fehlender Balance oder einem Konter | Grappling und Würfe wie in Overgrowth, Clinch und Takedowns | F53 | 🔜 hoch. Am 4. November 2026 vom Nutzer als Teil des Spiels bestätigt, nach Folgen ohne Fäden von F64, siehe „Schieben, Festhalten und Abstützen gehören dazu“ nach Runde 54 |
| F55 | **Gegen die Wand**: Ein Tritt treibt die Figur gegen eine Wand, wie es 30.5 verlangt: Sie prallt an, stützt sich mit den Händen ab oder rutscht daran herunter, und die Wand bekommt nach 30.7 genau einen Einschlag | Wandtreffer wie in Sifu, Kämpfe, die die Umgebung nutzen | F28, F53 | 🔜 hoch. Wie eine stehende Figur sich selbst an Gegenständen abstützt, ist seit dem 4. November 2026 F80 |
| F56 | **Treffer in der Bewegung**: Ein Treffer im Lauf oder beim Wall-Run und der Bail vom Skateboard gehen ohne Sprung in Haltung und Geschwindigkeit in den Ragdoll über, nach 26.19 B, auch auf einer Plattform, die sich bewegt, nach 28.5 (D) | Kampf und Stürze aus der Bewegung, wie sie das Spiel braucht | F53 | 🔜 mittel |
| F57 | **Flug, Landung und Abrollen**: Eine Figur, die mit einem Stockwerk fällt oder durch die Luft fliegt, bereitet nach 27.10 die Landung rechtzeitig mit Knien und Fußzielen vor und landet je nach Geschwindigkeit, Aufprallwinkel und Untergrund stehend, stolpernd, abrollend oder als Ragdoll | Stürze aus Einstürzen und Sprüngen, die man sieht | F53 | 🔜 mittel |
| F58 | **Figuren speichern und zurückholen**: Heute geht das nur für Häuser, `nbSaveDestructibles`. Für Replays und Divergenztests im PvP über das Netz, 30.8, muss jede Figur mit Balance, Muskeln und Federn bitgleich zurückkommen | Replays, Divergenztests und Netzcode | F53 | 🔜 mittel, vor PvP über das Netz |
| F59 | **Balance einer gedrehten Figur**: Seit F50 dreht ein Treffer quer über die Vorderseite der Brust die ganze Figur auf ihren Füßen weg, bei 62 N s von rechts um 22 bis 28°. Die Balance richtet sich nach der Richtung des Beckens, so setzt die Figur den zweiten Fuß zurück statt zur Seite und steht zu breit: Von 168 leichten Treffern in der Szene der Tests fallen 5 statt 1. Plan, Stufe 1 der besseren Balance: Ausrichtung und Seitenabstand des Fußes nimmt die Balance vom Standfuß und von der Richtung, in die der Stoß die Figur schiebt, nicht vom Becken, und das Becken dreht sich zu den Füßen zurück | weniger Stürze nach leichten Treffern von der Seite | F9, F50 | ✅ auf `active-ragdoll-34`, auf Wunsch des Nutzers: Jeder Fuß zeigt, wohin die Figur auf ihm stünde, die Figur zeigt nach ihrem Standfuß, solange der andere schwingt, sonst nach ihren Füßen zusammen. Ein Schritt landet gedreht wie der Standfuß und hält seinen Abstand zur Seite von ihm, ein Fuß, der steht, bleibt gedreht, wie er steht, und die Beine drehen das Becken über die Füße zurück. Nach 168 leichten Treffern sind die Füße im Mittel um 7,9 statt 13,3° gedreht, höchstens um 36 statt 67°, nach 62 N s von rechts um 16 statt 25°. Die Stürze hatten einen anderen Grund: Nach einem langen ersten Schritt schwebt der hintere Fuß knapp über dem Boden und verlor seine Kontakte, die Balance ließ ihn dann nach außen treten. Jetzt steht ein Fuß, der nicht schwingt, auch bis 3 cm über dem Boden. Von 168 leichten Treffern fällt keiner statt vier, von 50 bis 89 N s auf Brust und Bauch 14 statt 42, mit 1,24 statt 2,38 Schritten. Die Balance nach den Füßen allein ließ dort 62 fallen. Siehe „Balance nach den Füßen“ und „Fuß knapp über dem Boden steht“ in [Optimierungen](Optimierungen.md) |
| F60 | **Dauer jedes Schritts nach dem Stoß**: Stufe 2 der besseren Balance. Heute dauert jeder Schritt 0,3 s, ein harter Stoß bekommt denselben langsamen Schritt wie ein leichter, und solange der Fuß schwingt, läuft der Capture Point mit dem Exponential der Pendelfrequenz mal der Dauer davon. Die Balance probiert für jeden Schritt einige Dauern zwischen etwa 0,2 und 0,4 s und nimmt die, nach der der Capture Point bei der Landung am nächsten an der Stütze liegt, wie es die Anpassung der Schrittzeit von Khadiv 2016 und 2020 und Griffin 2017 am Atlas tut, deterministisch und mit festen Kandidaten. Messen an den 4 464 Treffern auf Brust und Bauch, den Grenzen und den leichten Treffern | härtere Treffer gefangen, weniger Stolperschritte, kürzere Fänge | F9, F59 | ⛔ auf `active-ragdoll-34` gemessen: Die Balance probierte für jeden Schritt 0,3, 0,25 und 0,2 s und nahm einen schnelleren, wenn er den Capture Point bei der Landung um mehr als 1 cm näher an die Stütze brachte. Das Schwungbein folgt seinem Ziel aber etwa 0,1 s zu spät, ein Schritt von 0,2 s nach hinten brachte den Fuß 15 statt 57 cm weit. Von 168 leichten Treffern fielen 22 statt keinem, von 50 bis 89 N s 299 statt 14. Mit einer Landung, die auf den Fuß wartet, fielen mehr, mit einer Grenze von 1,5 m/s für das Tempo des Fußes von 90 bis 139 N s 33 mehr. Siehe „Schrittzeit nach dem Stoß“ in [Optimierungen](Optimierungen.md). 🔜 erst mit einem Schwungbein, das seinem Ziel schneller folgt |
| F61 | **Balance aus eigener Kraft**: Stufe 3 der besseren Balance. Heute hält eine begrenzte Kraft gegen die Welt das Becken aufrecht, `uprightTorque`, und ohne sie fällt der Mensch der Bibliothek schon bei 50 N s. Stattdessen hält die Figur Rumpf und Schwerpunkt mit Drehmomenten in ihren eigenen Gelenken: Die Hüfte des Standbeins richtet den Oberkörper auf und stemmt sich dabei gegen den Boden, das Standbein trägt den Schwerpunkt über virtuelle Kräfte durch seine Gelenke, die Arme helfen wie heute, nach SIMBICON, Yin 2007, und Coros 2010. Die Kraft von außen bleibt als Option. Größerer Umbau, eigene Branch | eine Balance, die physikalisch stimmt und den Körper sichtbar arbeiten lässt, für ein Spiel, das es rein physikalisch will | F59, F60 | ✅ Stufe 1 auf `active-ragdoll-39`, auf Wunsch des Nutzers über mehrere Runden, in jeder ein Baustein, jeder gegen die Stütze gegen die Welt gemessen, die Standard bleibt, bis die eigene Balance gleich gut fängt: Der Capture Point rechnet die Drehung des Körpers mit, für jede Figur, und die Stütze kann das Becken statt gegen die Welt gegen die Oberschenkel der tragenden Beine halten, `uprightAgainstWorld`. Mit ihr fallen von 168 leichten Treffern 87 statt 110, mit der Stütze gegen die Welt keiner. Die übrigen fallen vor allem durch Sinken, 50, und Kippen, 24. Siehe „Balance aus eigener Kraft, Stufe 1“ in [Optimierungen](Optimierungen.md). ✅ Stufe 2 auf `active-ragdoll-40`, gegen das Sinken: Die Beine am Boden tragen den Körper über ihnen mit einem Schub zu seinem Schwerpunkt, so stark, wie sie sein Gewicht tragen, von dort, wo der Boden den Fuß drückt, nach den virtuellen Kräften von Coros 2010, für jede Figur. Mit der Stütze durch die Beine fallen von den leichten Treffern 43 statt 87, mit der Stütze gegen die Welt weiter keiner, von 90 bis 139 N s 577 statt 628. Die übrigen 43 fallen durch Sinken, 30, und Kippen, 11. 20 der 30 sinken nach einem Treffer von der Seite: Das Becken kippt über dem Standbein, die Stütze drückt das Bein nach außen, der Fuß rutscht weg. Siehe „Balance aus eigener Kraft, Stufe 2“ in [Optimierungen](Optimierungen.md). ✅ Stufe 3 auf `active-ragdoll-41`, gegen das Kippen: Die Wirbelsäule beugt den Oberkörper mit der Neigung des Beckens weiter, um das Doppelte der Neigung und höchstens 0,5 rad, nach der Hüftstrategie von Horak und Nashner 1986, für jede Figur. Das Drehmoment, das den Oberkörper weiterdreht, dreht das Becken zurück. Mit der Stütze durch die Beine fallen von den leichten Treffern 17 statt 43, mit der Stütze gegen die Welt weiter keiner, von 90 bis 139 N s 385 statt 577. Den Oberkörper gegen die Drehung aufrecht zu halten, wie hier geplant, ließ fast doppelt so viele fallen, und Arme, die nach dem Drehimpuls schwingen, änderten nichts. Die übrigen 17 fallen durch Sinken, 12, davon 9 nach Treffern von der Seite ab 72 N s, durch Kippen, 4, und einer nach dem fünften Schritt. Siehe „Balance aus eigener Kraft, Stufe 3“ in [Optimierungen](Optimierungen.md). Mit den etwas kürzeren Armen von Runde 44 fallen mit der Stütze durch die Beine von den leichten Treffern 25 statt 17, von 50 bis 89 N s 389 statt 385, von 90 bis 139 N s 1 449 statt 1 456 und an den Gliedern 479 statt 465, mit der Stütze gegen die Welt fast genau wie vorher. ✅ Ein Baustein von Stufe 4 auf `active-ragdoll-45`: Die Sohlen greifen wie Gummi, 0,9 statt 0,6, über eine eigene Reibung je Knochen, und die Stütze durch ein Bein lässt nach, solange sein Fuß rutscht. Mit der Stütze durch die Beine fallen von den leichten Treffern 9 statt 25, von 50 bis 89 N s 240 statt 389, von 90 bis 139 N s 1 453 statt 1 449 und an den Gliedern 405 statt 479, mit der gegen die Welt weiter keiner, keiner, 362 statt 385 und 122 statt 135. Von den 9 kippen 7 nach Treffern von vorn oder schräg von vorn nach hinten: Der erste Schritt zurück kommt zu kurz, weil die Stütze durch das Standbein die Hüfte mit nach hinten schiebt, und der zweite fängt nicht, siehe F62. Siehe „Sohlen, die greifen“ in [Optimierungen](Optimierungen.md). 🔜 Stufe 4: Die Stütze durch die Beine wird Standard, sobald sie an den leichten Treffern, an Brust und Bauch und an den Gliedern so gut fängt wie die gegen die Welt. Für den PvP-Modus des Kampfspiels empfohlen nur als Option, die Stütze gegen die Welt bleibt dort Standard, siehe „Euphoria und Overgrowth im Vergleich“ unter Runde 40. ✅ Ein weiterer Baustein von Stufe 4 auf `active-ragdoll-49`, auf Wunsch des Nutzers, der die eigene Kraft für den Kern der Umsetzung hält: Die Stütze durch ein Bein hält so stark wie das Gelenk der Hüfte, und ohne die Stütze gegen die Welt stemmt die Figur ihre Knie, tritt nach einem deutlichen Stoß sofort und macht bis zu zehn Schritte. Mit der Stütze durch die Beine fallen von den leichten Treffern keiner statt 9, von 50 bis 89 N s 50 statt 240, von 90 bis 139 N s 1 063 statt 1 453 und an den Gliedern 265 statt 405, die Stütze gegen die Welt rechnet bitgleich, siehe Runde 49. Im Vergleich aller Ansätze, Runde 48, fallen mit der Stütze durch die Beine von 5 264 Treffern 2 098 statt 455 mit der gegen die Welt, und 120 statt 14 Ergebnisse kippen, wenn der Treffpunkt 1 mm wandert. Beine und Welt zusammen fangen so viel wie die Welt allein, mit 8 % weniger Hilfe der Welt. Weiterverfolgt wird die Stütze gegen die Welt, siehe F75. ✅ Ein weiterer Baustein von Stufe 4 auf `active-ragdoll-50`: Ohne die Stütze gegen die Welt schwingt jeder Fuß mit dem Tempo weiter, das er schon hat. Mit der Stütze durch die Beine fallen von den leichten Treffern weiter keiner, von 50 bis 89 N s 34 statt 50, von 90 bis 139 N s 1 030 statt 1 063 und an den Gliedern 260 statt 265, die Stütze gegen die Welt rechnet bitgleich. Bei fast allen Stürzen, die bleiben, rutscht der Standfuß weg, während das Standbein das Becken aufrichtet, siehe Runde 50. ✅ Ein weiterer Baustein von Stufe 4 auf `active-ragdoll-51`: Ohne die Stütze gegen die Welt zieht die Stütze durch ein Bein nicht stärker, als sein Fuß auf dem Boden hält. Mit der Stütze durch die Beine fallen von den leichten Treffern weiter keiner, von 50 bis 89 N s 28 statt 34, von 90 bis 139 N s 1 000 statt 1 030 und an den Gliedern 257 statt 260, über vier Lagen des Treffpunkts zusammen 119 statt 122, 3 999 statt 4 108 und 1 020 statt 1 045, die Stütze gegen die Welt rechnet bitgleich. Von den Stürzen, die bleiben, sinken 8 statt 17, 13 kippen mit dem Stoß, siehe Runde 51. ✅ Ein weiterer Baustein von Stufe 4 auf `active-ragdoll-52`: Ohne die Stütze gegen die Welt bremst die Stütze durch ein Bein das Kippen des Beckens, auf Anregung des Nutzers, und der schwingende Fuß zieht die Zehen an. Mit der Stütze durch die Beine fallen von den leichten Treffern weiter keiner, über vier Lagen des Treffpunkts zusammen von 50 bis 89 N s 73 statt 119, von 90 bis 139 N s 3 919 statt 3 999 und an den Gliedern 1 036 statt 1 020, die Stütze gegen die Welt rechnet bitgleich. Von den Stürzen, die bleiben, kippen 4 statt 13, siehe Runde 52. ✅ Ein weiterer Baustein auf `active-ragdoll-57`: Die Sohlen greifen mit 1,1 statt 0,9, zusammen mit weniger Gegendrehung bei Schritten zurück, siehe F79 und Runde 57. ✅ Ein weiterer Baustein auf `active-ragdoll-58`: Spät im Schritt senkt das Standbein das Becken so tief, wie es mit beiden Füßen an ihren Zielen aufrecht steht, nicht bei Schritten nach vorn und nur, solange eine Stütze das Becken aufrecht hält. Mit der Stütze durch die Beine fallen von allen 4 × 5 264 Treffern 3 865 statt 3 946, von den leichten weiter keiner, mit der Stütze gegen die Welt 1 505 statt 1 487, und ab dem vierten Schritt rutscht der Standfuß nach den mittleren Treffern bei 201 statt 439 Schritten, siehe Runde 58. ✅ Ein weiterer Baustein auf `active-ragdoll-59`, Ideen der Branch `ragdoll-sol`: Der erste Schritt mit eigener Kraft geht zur Seite des Falls, und nach dem Fangen bremst der Boden die Restbewegung. Mit der Stütze durch die Beine fallen, zusammen mit dem ruhigeren Kopf von F48, von allen 4 × 5 264 Treffern 3 752 statt 3 865, von den mittleren 18 statt 40, von den leichten weiter keiner, und nach den mittleren steht die Figur nach 2,03 statt 2,24 s still, siehe Runde 59. ✅ Ein weiterer Baustein auf `active-ragdoll-61`: Solange eine Stütze das Becken aufrecht hält, bleibt der Standfuß, wo er zuletzt 0,7 des Gewichts trug, und das Standbein zieht ihn dorthin zurück, `NB_PLANT_LOAD`. Mit der Stütze durch die Beine fallen von allen 4 × 5 264 Treffern 3 484 statt 3 733, von den mittleren 20 statt 27, von den leichten weiter keiner, mit der Stütze gegen die Welt 1 282 statt 1 523, und ab dem zweiten Schritt rutscht der Standfuß nach den mittleren Treffern bei 761 statt 1 032 Schritten mehr als 5 cm, siehe Runde 61. ✅ Ein weiterer Baustein auf `active-ragdoll-62`: Spät im Schritt greift das Schwungbein unter den Boden, solange eine Stütze das Becken aufrecht hält ab 0,6 der Schrittdauer bis 5 cm tief, ohne Stütze ab 0,9 bis 3 cm, `NB_REACH_DEPTH`. Mit der Stütze durch die Beine fallen von allen 4 × 5 264 Treffern 3 043 statt 3 484, von den mittleren 1 statt 20, von den leichten weiter keiner, mit der Stütze gegen die Welt 1 051 statt 1 282, ohne Stütze 4 954 statt 5 165, und im ersten Schritt zur Seite weg bewegt sich der Standfuß nach den mittleren Treffern bei 0,9 statt 16,5 % der Schritte mehr als 5 cm, siehe Runde 62. ⛔ Runde 63: Das Hochfedern nach dem Aufsetzen zu bremsen nimmt es nur zum Teil und lässt mit eigener Kraft mehr Treffer fallen, nichts gebaut, siehe Runde 63 ⛔ Runde 64: Ohne Stütze rutscht der Standfuß ab dem zweiten Schritt pro Treffer seltener als vor Runde 62, nur anteilig öfter, weil Schritte wegfallen, in denen er selten rutscht, und keine Form nimmt die übrigen Rutscher um 10 %, nichts gebaut, siehe Runde 64 📊 Runde 83: Bei 60 Hz fängt nur die Stütze gegen die Welt 1 632 von 17 856 Treffern, 1 629 davon mit 90 N s und mehr, deren Drehimpuls ein Fuß nicht halten kann. Mittlere Treffer fallen mit eigener Kraft bei 60 Hz einer von 1 920, bei 30 Hz 6 und bei 20 Hz 174, siehe Runde 83 |
| F62 | **Mehrere Schritte planen**: Stufe 4 der besseren Balance. Die Balance wählt heute nur den nächsten Schritt. Sie plant den ersten so, dass der zweite fangen kann, nach der Capturability von Koolen 2012, oder mit einer kleinen modellprädiktiven Regelung auf dem Pendelmodell nach Wieber 2006, mit festen Iterationen bitgleich | weniger lange Stolperfolgen nach sehr harten Treffern | F60 | ⛔ Runde 46: als Varianten gebaut und an allen Treffern gemessen, nicht übernommen. Ab dem zweiten Schritt sagt das Pendel den Capture Point bei der Landung im Mittel 13 bis 18 cm falsch voraus, vor allem seitlich, und auf zwei Schritte geplant fallen mit der Stütze durch die Beine von 50 bis 89 N s 312 statt 240. Eine Planung über mehrere Schritte braucht zuerst eine Vorhersage, die die Drehung des Körpers, die Stütze und die Beine kennt. Als Option für den Nutzer: eine spätere Aufgabe bei harten Treffern, mit der Stütze gegen die Welt 22 Treffer mehr gefangen, die übrigen fallen nach einem Schritt statt sofort, siehe Runde 46. ✅ Diese Option auf `active-ragdoll-47`, nach der Roadmap des Spiels, 26.4: Ein starker Treffer lässt die Figur taumeln und dann fallen, erst ein extremer sofort, siehe Runde 47 |
| F63 | **Mehr Stücke im Rumpf**: Über dem Becken hat der Mensch der Bibliothek drei Stücke, Bauch und Rippen mit je 5 kg und die Brust mit 12 kg, jedes Gelenk beugt bis 25° nach vorn, zusammen 75° vor, 30° zurück, 34° zur Seite und 40° gedreht, wie die Rigs von Mixamo. Ein Mensch beugt sich vor allem in der Lendenwirbelsäule und dreht sich in der Brustwirbelsäule. Fünf Stücke, zwei für die Lende und drei für die Brust, wie die fünf Wirbel des Mannequins von Unreal 5, biegen den Rücken runder, lassen einen Treffer am Bauch dort einknicken und legen die Beugung in den Bauch. Leichtere Stücke zwischen steifen Federn zittern wie der Hals, dafür ist die Armature aus F48 die Voraussetzung. Zwei Knochen mehr kosten etwa ein Zehntel mehr je Figur. Clips, Balance, Zucken, die Drehung aus F50, die Tests und die Nummern der Knochen hängen heute an drei Wirbeln: Die Zahl der Stücke wird in der Bibliothek variabel, der erste Teil von F53, und gemessen gegen drei Stücke an Biegung, Treffern, Fangen und Kosten. Eigene Branch | ein Rumpf, der sich biegt wie ein Mensch, und Rigs mit bis zu fünf Wirbeln | F48, F50 | ✅ auf `active-ragdoll-37`, mit vier statt fünf Stücken über dem Becken nach Wunsch des Nutzers, einem für jeden Ring der Vorlage: Bauch, Rippen, untere und obere Brust mit 5, 5, 5 und 7 kg. Die vier Gelenke sind je viermal so steif wie die anderen, Bauch und Rippen beugen 50 der 75° nach vorn, die Teile der Brust drehen 25 der 40°, und das Zucken eines leichten Treffers teilen sie nach diesen Bereichen, nach 20 N s von hinten beugen Bauch und Rippen 6,4 von 9,2°. Die beiden Teile der Brust nehmen einen Treffer als ein Brustkorb, jeder für sich ließ die Figur von 50 bis 89 N s 42-mal statt 15-mal fallen, so 19-mal, von 90 bis 139 N s 910 statt 885. Die Nummern der Knochen und `NB_SPINE_COUNT` tragen die Zahl der Stücke, ein Spiel wählt sie noch nicht selbst, das bleibt bei F53. Siehe „Rumpf aus vier Stücken“ in [Optimierungen](Optimierungen.md) |
| F64 | **Aktiv-dynamisch bei Kontakt**: Berühren sich zwei Kämpfer oder trifft ein Glied während einer Aktion etwas Festes, werden die Beteiligten dynamisch, solange der Kontakt dauert: Ihre Motoren folgen der Zielpose aus den Kampfclips mit begrenzter Kraft und so genau, dass ein Schlag präzise bleibt, ihre Balance hält sie, und die Kräfte gehen in beide Richtungen. Ein geblockter Schlag gibt in Schulter und Ellbogen nach, statt durch die Deckung zu gehen. Ohne Kontakt und ruhig werden sie wieder kinematisch. Heute wird eine Figur nur nach einem harten Treffer dynamisch, und zwei stehende Figuren berühren sich nicht, weil Box3D kinematische Körper nicht miteinander kollidieren lässt. Der Zustand, in dem sich eine Figur fängt, ist schon dieser Baustein, er beginnt nur nach harten Treffern und endet, sobald sie ruhig ist | Kampf mit echtem Widerstand, Blocken, Drücken und Schieben ohne Skript, nach 30.5 und 30.7 der Roadmap des Spiels | F61 | ✅ Stufe 1 auf `active-ragdoll-30`, auf Wunsch des Nutzers: Drückt ein Körper einen Knochen eines stehenden Menschen ein paar Schritte lang mit mindestens `pushForce` (40 N) seitlich, fängt er sich, statt eine Wand zu bleiben, und bleibt dynamisch, solange es mit der Hälfte drückt. Mit 80 N gegen die Brust lehnt er sich 8 cm zurück, mit 300 N tritt er 3 Schritte weg, mit 150 N gegen die Stirn dreht sich sein Kopf um 34°. Die Demo hat dazu den Stock. Siehe „Drücken weckt die Figur“ in [Optimierungen](Optimierungen.md). Seit `active-ragdoll-31` gibt ein Arm einem Drücken aus jeder Richtung nach: Ein Stock, der mit 80 N langsam in den Ellbogen drückt, schiebt die runden Enden von Ober- und Unterarm schräg nach oben und unten, und bis `active-ragdoll-30` blieb die Figur dafür eine Wand, jetzt lehnt sie sich 12 cm zur Seite. Siehe „Ein Arm gibt nach“ in [Optimierungen](Optimierungen.md). ✅ Stufe 2 auf `active-ragdoll-43`: Mit `engageRadius` folgt eine stehende Figur ihrem Clip mit dynamischen Knochen, solange eine andere näher ist, die Gelenke drehen mit dem Clip, und jeder Knochen wird so stark zum Clip gezogen, wie der Clip ihn bewegt. Ein Schlag trifft Deckung oder Körper mit der Kraft dahinter, im Idle bleibt jeder Knochen 2,6 mm am Clip, die Faust eines Schlags 4,1 cm. Siehe „Kämpfer machen die Bewegung selbst“ unter Runde 43 und in [Optimierungen](Optimierungen.md). 🔜 der Rest: F65, eine einzelne Figur, die immer folgt, und Schritte statt rutschender Füße, wenn das Spiel einen folgenden Kämpfer bewegt. Aus der Durchsicht vom 2. November 2026 dazu: Antriebe, die mit dem Abstand vom Clip schwächer ziehen, statt bei 20 cm hart loszulassen, wie bei bevy-ragdoll, ein Treffer, der die Antriebe am getroffenen Glied eine Weile schwächt, siehe F32, die Drift für die Fortbewegung des Spiels, F71, und die Vorsteuerung aus inverser Dynamik, F73. Früher stand hier: nach F61 der Rest: Kämpfer, die sich berühren, schon vor dem Kontakt dynamisch, und Kampfclips, denen die Motoren genau genug folgen. Referenz dafür ist Blade & Sorcery, siehe „Blade & Sorcery als vierte Referenz“ unter Runde 40, den Absatz zur Dokumentation seines SDK danach und die Tabelle, wer wann zur Animation gezogen wird. Der Weg danach: Kämpfer in Kontakt oder in einem Radius um einen Gegner werden dynamisch, und jeder Knochen wird mit einer begrenzten, gedämpften Feder zu seiner Lage im Kampfclip in der Welt gezogen, Position und Drehung, zusätzlich zu den Gelenkmotoren, deterministisch in der Bibliothek. Ein Stoß über der Grenze lenkt den Knochen aus, die Animation zieht ihn zurück, und ein Schlag bleibt präzise, solange nichts im Weg ist. Kippt die Figur dabei, lässt sie die Federn los wie Destabilized bei Blade & Sorcery und fängt sich mit Schritten wie heute. Ob die Grenzen der Gelenke von Box3D, die das tragen könnten, halten, was ihre Zahl sagt, vorher prüfen, siehe F70. Seit dem 4. November 2026 ist die nächste Stufe Folgen ohne Fäden: Nur die Gelenke folgen den Winkeln des Clips, Füße und Balance tragen den Körper, damit Schieben, Festhalten und Abstützen nicht von den Fäden verfälscht werden, nach F61, siehe „Schieben, Festhalten und Abstützen gehören dazu“ nach Runde 54 |
| F65 | **Glieder steuern**: Das Spiel setzt ein Ziel für Hand oder Fuß, aus einem Kampfclip oder in der Entwicklung aus der Eingabe, die Bibliothek rechnet die Gelenkwinkel der Kette, und die Motoren ziehen mit begrenzter Kraft dorthin. Ein Kontakt hält das Glied auf: Drückt ein Fuß langsam gegen einen Kopf, dreht sich der Kopf, der Hals gibt die Kraft an den Rumpf weiter, der Körper wird weggeschoben oder fängt sich mit Schritten, und das eigene Bein spürt die Gegenkraft. Ein Tritt wirkt dann nach Geschwindigkeit und Masse hinter dem Fuß, Trefferpunkt, Steifigkeit und Balance, ein harter anders als ein leichter, mit fließenden Übergängen statt der Schwellen zwischen Zucken, Abfangen und Fallen. Die Demo bekommt dazu ein Werkzeug der Entwicklung, ein Glied mit der Maus zu führen, nur über diese Schnittstelle | Drücken, Treten und Schieben nach der wirkenden Kraft, Grundlage für Griffe F54 und die Wand F55 | F64 | 🔜 vorgeschlagen mit F64. Seit `active-ragdoll-43` trifft ein Schlag zwischen Kämpfern mit `engageRadius` schon mit der Kraft, die der Clip hinter ihm hat, und der Getroffene spürt den Impuls des Kontakts, siehe „Kämpfer machen die Bewegung selbst“ unter Runde 43. F65 bringt dazu Ziele für Hand und Fuß statt nur Clips und fließende Übergänge statt der Schwellen zwischen Zucken, Abfangen und Fallen. Soll eine Hand ihr Ziel auch über die Wirbelsäule erreichen, käme FABRIK in Frage, wie OpenMotion es nutzt, Durchsicht vom 2. November 2026 |
| F66 | **Anspannung und Entspannung**: Ein Mensch spannt die Muskeln um ein Gelenk gegeneinander an, Beuger und Strecker zugleich, und macht es steif, ohne es zu bewegen. Wer einen Stoß kommen sieht und den Hals vorher anspannt, dessen Kopf wird weniger beschleunigt, gemessen von Eckner u. a. (2014) an Sportlern. Heute zieht jedes Gelenk einer Figur mit derselben Steifigkeit und Grenze, ob sie gewappnet ist oder überrascht. Das Spiel soll die Spannung je Bereich setzen können, Hals, Rumpf, Arme und Beine, aus Deckung, Block, Angriff oder Überraschung, und die Bibliothek hebt oder senkt damit Steifigkeit, Dämpfung und Drehmomentgrenze der Gelenke. Mit den Muskeln aus F45 braucht das Anspannen Zeit: Wer zu spät reagiert, ist nicht gewappnet. Eine bewusstlose Figur ist ganz locker. Das Stemmen gegen eine Richtung aus F42 wird ein Fall davon, und mit F41 kann Spannung Ausdauer kosten | Derselbe Treffer wirkt nach der Haltung des Getroffenen: Ein gewappneter Hals hält, ein überraschter gibt nach, eine angespannte Deckung hält einen leichten Schlag und weicht einem harten | F45, F64 | 🔜 vorgeschlagen mit F64 |
| F67 | **Kräftigere Figur**: Der Nutzer schickte am 1. November 2026 zwei Bilder einer kräftigeren grünen Gliederpuppe, mit breiteren Schultern, dickeren Armen und Beinen und mehr Brust, für später. Die Bibliothek bekäme dazu Maße für die Breite von Gliedern und Rumpf, die Massen danach und Gelenke, die stark genug sind, und die Demo zeichnet sie. Zu prüfen ist, wie sie sich fängt, wie bei „Körper wie die Vorlage“ | Kämpfer verschiedener Statur, wie das Spiel sie braucht | F53 | 🔜 später, auf Wunsch des Nutzers |
| F68 | **Balance für den Körper der Vorlage**: Seit `active-ragdoll-36` hat der Mensch der Bibliothek längere Arme und breitere Hüften und steht wie die Vorlage. Er fängt Treffer auf den Rumpf schlechter: Von 90 bis 139 N s fallen 885 statt 676 von 2 064, von hinten fängt er sich nur bis 103 statt 127 N s, von der Seite bis 88 statt 94, und nach leichten Treffern stehen die Füße bis 43 statt 32° gedreht. Kein Teil des Körpers erklärt das allein. Idee: Reichweite und Dauer der Schritte, die Kraft der Arme zwischen den Schwüngen und ihren Schwung auf den neuen Körper abstimmen, gemessen an den 5 432 Treffern. Arme mit 0,8 statt 0,6 der Kraft zwischen den Schwüngen ließen von 90 bis 139 N s nur 819 fallen | wieder so viele Treffer gefangen wie mit dem alten Körper, im Aussehen der Vorlage | F59 | ✅ auf `active-ragdoll-38`: Die Höhe im Stand, unter deren drei Viertel die Figur fällt, kommt aus der ersten Haltung ihres Clips statt aus den Drehpunkten der Knochen in Ruhe, der Fuß hebt beim Schritt 12 statt 8 cm, und die Arme schwingen bis an die Grenze der Schulter statt 0,3 rad davor. Von 50 bis 89 N s, von 90 bis 139 N s und auf die Glieder fallen 2, 665 und 214 statt 19, 910 und 290, auf `active-ragdoll-35` 11, 676 und 326, von hinten fängt sie sich wieder bis 130 N s. Siehe „Balance für den Körper der Vorlage“ in [Optimierungen](Optimierungen.md) |
| F69 | **Nicht über einer Kante liegen bleiben**: Eine Figur, die in eine Wand aus Brettern fliegt, kann über der unteren Kante des Lochs zur Ruhe kommen, den Rumpf auf der einen Seite, die Beine auf der anderen, und ohne Boden unter dem Becken wartet sie dort für immer, `nb_waitGround`: auf `active-ragdoll-36` in 4 von 96 Bruchmustern, mit dem Rumpf aus vier Stücken von `active-ragdoll-37`, der sich besser über eine Kante legt, in 10. Auf `active-ragdoll-39` liegt auch im Benchmark mit 10 Figuren eine so über ihrer Mauer, das Becken 0,8 m hoch, und macht dort jeden Schritt, während die Deckung bricht, rund viermal so teuer. Auf `active-ragdoll-40` liegt dort keine mehr, dafür eine im Benchmark mit 50 Figuren, das Becken 0,89 m hoch, und auf `active-ragdoll-45` wieder, das Becken 0,9 m hoch. Auf `active-ragdoll-47` liegt dort keine mehr, weil die Figuren nach dem Taumeln anders fallen. Dasselbe droht über einem Zaun, einem Geländer oder einem Auto. Idee: Eine Figur, die ohne Boden unter dem Becken ruht, rutscht herunter, zur Seite, auf der ihr Gewicht hängt, mit ihren eigenen Gliedern wie ein Mensch, der sich herunterschiebt, deterministisch, und steht dann am Boden auf. Gemessen an den Bruchmustern der Wand | keine Figur bleibt für immer über einer Kante liegen | F11 | 🔜 vorgeschlagen als Runde 48, nach Runde 47, ⏸ am 3. November 2026 zurückgestellt: Dem Nutzer ist die Balance wichtiger, siehe Runde 48 |
| F70 | **Die Stütze so stark wie dokumentiert**: Das Parallelgelenk von Box3D, das das Becken gegen die Welt oder gegen die Oberschenkel aufrecht hält, begrenzt seinen Impuls auf `h · maxTorque`, wendet davon aber nur die Hälfte als Drehmoment an, weil seine Achsen aus der Ableitung des Quaternions den Faktor 0,5 tragen. Die 250 N m von `nbFigureMotors::uprightTorque` wirken so als 125 N m, mit beiden Stützen. Gemessen auf `active-ragdoll-40` mit 500 N m, also den dokumentierten 250: Mit der Stütze gegen die Welt fallen von 50 bis 89 N s keiner statt 3, von 90 bis 139 N s 297 statt 577 und an den Gliedern 101 statt 171, mit der Stütze durch die Beine mehr, 57, 729, 1 579 und 604 statt 43, 683, 1 550 und 571. Zwei Wege: Die Grenze in Box3D wird verdoppelt und der Standardwert auf 125 N m gesetzt, dann rechnet alles bitgleich, und die Zahl stimmt. Oder die Stütze wird wirklich doppelt so stark, dann fängt sie mehr harte Treffer, und ob ein Treffer umwirft, hängt noch mehr von ihr ab als von den Schritten | eine Stütze, die hält, was ihre Zahl sagt | F61 | ✅ auf `active-ragdoll-42` der erste Weg, bitgleich: Die Grenze im Parallelgelenk ist doppelt so hoch, der Standard von `uprightTorque` 125 N m. Kugelgelenk, Scharnier und Motorgelenk halten ihre Grenzen schon genau. Siehe „Die Stütze hält, was ihre Zahl sagt“ in [Optimierungen](Optimierungen.md) |
| F71 | **Drift für die Fortbewegung des Spiels**: Wird ein folgender Kämpfer vom Clip weggedrückt, ziehen ihn seine Antriebe zurück, bis er bei 20 cm loslässt, und das Spiel erfährt davon erst, wenn er sich fängt. Wie bei godot-active-ragdoll meldet die Bibliothek, wie weit der Schwerpunkt des Körpers waagerecht neben dem des Clips liegt und wie schnell er sich entfernt, und ein Spiel, das seine Figur mit `nbFigure_Move` bewegt, schiebt sie dorthin nach, statt dass die Antriebe den Körper gegen den Druck zurückziehen. Ein Block, der einen Kämpfer zurückdrückt, schiebt so auch seine Fortbewegung | Kämpfer geben einem Druck mit ihrer Fortbewegung nach, statt zu rutschen oder loszulassen | F64 Stufe 2 | 🔜 vorgeschlagen am 2. November 2026, klein: eine Angabe in `nbFigureInfo`, die Bibliothek bewegt die Figur nicht selbst |
| F72 | **Nur der Oberkörper dynamisch**: Ein leichter Treffer dreht Rumpf, Kopf und Arme heute mit berechneten Federn, ohne Physik, F31. Wie bei godot-active-ragdoll, bevy-ragdoll und OpenMotion folgen nach einem leichten Treffer, oder wenn das Spiel es will, nur Rumpf, Kopf und Arme ihrem Clip dynamisch, mit den Antrieben aus Runde 43, während Becken und Beine kinematisch weiterlaufen und die Balance nicht gebraucht wird. Ein Arm träfe dann wirklich, was neben ihm steht, auch bei einer Figur, die läuft und keinen Gegner hat, und es kostet weniger als eine ganze folgende Figur | Physikalische Reaktionen des Oberkörpers ohne Balance und ohne Gegner | F64 Stufe 2 | 🔜 vorgeschlagen am 2. November 2026, mittel. Vorher messen, wie das kinematische Becken die Kräfte des Oberkörpers trägt und was der Wechsel zurück kostet |
| F73 | **Vorsteuerung aus inverser Dynamik**: Die Gelenke folgender Kämpfer ziehen mit voller Kraft und kennen seit Runde 43 die Drehgeschwindigkeit des Clips, die Antriebe ziehen so stark, wie der Clip den Knochen beschleunigt. Nach der inversen Dynamik, wie Simbody und OpenSim sie rechnen, ergibt sich aus dem Clip für jedes Gelenk das Drehmoment, das seine Bewegung samt allem, was es trägt, braucht, rekursiv von Händen und Füßen zum Becken. Bekommen die Gelenke dieses Drehmoment vorab mit, können Federn und Antriebe weicher sein, ohne dass ein Schlag nachläuft: Ein Arm, der etwas trifft, gibt nach, und zwei Kämpfer stemmen sich nicht mit voller Kraft gegeneinander | Weiche, natürliche Reaktionen bei genauer Bewegung, kleinere Kräfte im Clinch | F64 Stufe 2 | 🔜 vorgeschlagen am 2. November 2026, mittel bis groß: Box3D bräuchte ein vorgegebenes Drehmoment im Kugelgelenk und im Scharnier, bei null bitgleich |
| F74 | **Zahlen der Balance mit einem Optimierer abstimmen**: Die Zahlen der Balance sind von Hand gemessen und gewählt, Variante für Variante an den Tabellen der Treffer. Mit einem Optimierer wie CMA-ES, den Simbody mitbringt und den Arbeiten zu SIMBICON nutzen, ließen sich mehrere Zahlen zugleich gegen dieselben Tabellen abstimmen, deterministisch auf der VM, mit einem zurückgehaltenen Teil der Treffer zum Prüfen, damit sie nicht nur auf die gemessenen passen | Mehr gefangene Treffer, ohne jede Zahl einzeln zu suchen | F61 | 🔜 vorgeschlagen am 2. November 2026, niedrig, nach Wunsch des Nutzers |
| F75 | **Folgetreffer werfen eine taumelnde Figur um**: Die Stütze gegen die Welt hilft gleich stark, ob die Figur ruhig steht oder schon taumelt. Nach 60 N s auf die obere Brust und einem zweiten Treffer aus derselben Richtung 0,1 bis 0,3 s später fällt sie von je 72 nach 20 bis 70 N s zusammen einmal, nach 80 N s 7-mal, durch die Beine nach 50 N s schon 46-mal. Ein Treffer auf eine Figur, die sich schon fängt, soll ihre Stütze schwächen, nach seiner Stärke, so dass Folgetreffer sie umwerfen, wie 30.9 der Roadmap des Spiels es verlangt, „Kick bei gutem Stand führt zu Stabilisierung, bei schlechter Stütze zu Sturz“, und wie die Struktur in 30.5. Als Variante gemessen: Halbiert jeder solche Treffer die Stütze, fallen nach zweiten Treffern von 60, 70 und 80 N s 8, 12 und 28 von 72, und Treffer auf eine stehende Figur enden bitgleich wie vorher. Dazu eine Zahl in `nbFigureMotors` und ein Test mit zwei Treffern | Kombos wirken, ein Spieler sieht, warum die Figur fällt, Treffer auf eine stehende Figur bleiben, wie sie sind | F29, Runde 47 | 🔜 vorgeschlagen als Runde 49, nach dem Vergleich der Ansätze in Runde 48, ⏸ am 3. November 2026 zurückgestellt: Dem Nutzer ist die eigene Kraft wichtiger, siehe Runde 49 |
| F76 | **Clip als Absicht, Physik als Ausführung**: Jede Bewegung einer Figur kommt aus ihrer eigenen Kraft. Die Gelenke folgen den Drehungen des Clips mit begrenzter Kraft, Lage und Drehung des Beckens erreicht der Körper selbst über Füße und Gelenke, Balance und Flug verändern die Ziele nach dem Zustand des Körpers, und Treffer wirken auf die dynamischen Knochen. Kein Faden zum Clip und keine Stütze gegen die Welt. In vier Schritten, in dieser Reihenfolge: 1. Die eigene Kraft fängt so gut wie die Stütze gegen die Welt, F61. 2. Folgen ohne Fäden als Option, gemessen an Stehen, Schlagen, Blocken und Treffern, gegen eine Faust, die dem Clip nachläuft, hilft F73. 3. Laufen aus eigener Kraft, wenn das Spiel die Figur bewegt. 4. Absprung, Flug und Landung: Im Flug stellt die Figur Arme und Beine nach ihrem Drehimpuls und passt die Zeit des Clips an. Größerer Umbau über mehrere Runden, jede auf ihrer Branch | Treffer stören jede Bewegung, auch einen Schlag oder einen Sprung, wie die Physik es will, und der Körper arbeitet sichtbar, nach dem Wunsch des Nutzers aus Runde 43, dass die Animation nur die Keyframes gibt | F61, F64, F65, F73 | Zielbild vom 3. November 2026, siehe „Clip als Absicht, Physik als Ausführung“ nach Runde 49. Die Akrobatik des Spielers bleibt nach Kapitel 2 der Roadmap des Spiels kinematisch, bis ein harter Treffer sie physikalisch macht, und fern vom Geschehen bleibt eine Figur kinematisch, nach 26.17. ⛔ Als Ganzes am selben Tag vom Nutzer verworfen: Eine Figur, die in jeder Bewegung physikalisch ist, will das Spiel nicht. Es bleiben zwei Optionen, die ein Spiel je Aktion wählt, kinematisch mit Umschalten wie heute oder die physikalische Clip-Verfolgung von F77, siehe „Zwei Optionen statt immer physikalisch“ nach Runde 49. Schritt 3, Laufen aus eigener Kraft, entfällt damit, die Schritte 1, 2 und 4 gehen in F77 auf |
| F77 | **Physikalische Clip-Verfolgung für einzelne Aktionen**: Ein Spiel lässt bestimmte Aktionen, etwa Angriffe im Nahkampf oder einen Salto, mit dynamischen Knochen ausführen. Die Gelenke ziehen mit begrenzter Kraft zu den Winkeln des Clips, nicht die Knochen zu ihrer Lage in der Welt, und Box3D rechnet Kontakte und Kräfte. Streift ein Fuß eine Kiste, geben Fuß, Knie und Hüfte nach, und der Clip läuft weiter. Ein starker Treffer findet die Physik schon laufend vor, und die Balance übernimmt. Normale Fortbewegung bleibt kinematisch mit Umschalten. In drei Schritten: 1. Die eigene Kraft fängt so gut wie die Stütze gegen die Welt, F61. 2. Folgen ohne die Fäden von `nbBeginFollowing` als Option, gemessen an Stehen, Schlagen, Blocken, Treffern und Kontakten mit Gegenständen, gegen eine Faust, die dem Clip nachläuft, hilft F73. 3. Absprung, Flug und Landung für Sprünge und Saltos: Der Absprung muss physikalisch reichen, im Flug stellt die Figur Arme und Beine nach ihrem Drehimpuls und passt die Zeit des Clips an | Bei kleinen Kollisionen setzt eine Figur ihre Aktion fort, und nur die betroffenen Körperteile geben nach, ohne Umschalten | F61, F64, F73 | Vom 3. November 2026, siehe „Zwei Optionen statt immer physikalisch“ nach Runde 49. Runde 50 arbeitet an Schritt 1, der eigenen Kraft, die auch nach dem Umschalten trägt. ⏸ Am selben Tag zurückgestellt: Mit kinematischer Animation und einem guten Umschalten sieht die Figur praktisch genauso aus, siehe „Kinematisch mit Umschalten reicht“ nach Runde 49 und F78. Folgen ohne Fäden, Schritt 2, bleibt für die folgenden Kämpfer von F64 sinnvoll, deren Fäden eine Kollision übersteuern. Am 4. November 2026 kehrt Schritt 2, Folgen ohne Fäden, als nächste Stufe von F64 zurück, für den Nahkampf, weil Schieben, Festhalten und Abstützen zum Spiel gehören. Der Rest von F77 bleibt zurückgestellt |
| F78 | **Umschalten bei Kollisionen mit Gegenständen**: Eine kinematische Figur spürt keine Kontaktkräfte. Box3D paart kinematische Körper nicht mit festen, und was ihre Knochen beiseiteschieben, zählt heute nicht. Die Bibliothek erkennt, wenn ein Knochen einer kinematischen Figur auf einen Körper oder auf feste Geometrie trifft, und macht sie in diesem Schritt dynamisch, mit den Lagen und Geschwindigkeiten ihrer Knochen. Wann ein Kontakt zählt, entscheidet eine Grenze nach dem Impuls, den der Knochen hineinträgt, deterministisch. Danach ziehen ihre Gelenke weiter zu den Winkeln des laufenden Clips, nicht die Knochen an ihre Lage in der Welt: Ein Salto läuft als Absicht weiter, der Fuß wird an der Kiste gebremst, das Knie gibt nach, und die Kiste fliegt weg. In der Luft folgt die Figur dem Clip, bis sie landet, dann fängt sie sich oder fällt | Treffer und Kollisionen mit Gegenständen wirken physikalisch, ohne dass eine Figur vorher dynamisch sein muss, zu den Kosten einer kinematischen Figur | F61, F64 | 🔜 vom 3. November 2026, auf Wunsch des Nutzers, der beim Hybrid bleibt, siehe „Kinematisch mit Umschalten reicht“ nach Runde 49. Am 4. November 2026 Teil von Schritt 3 der Reihenfolge, der sauberen Übergänge zwischen kinematischer Animation und dynamischer Steuerung, nach Folgen ohne Fäden, siehe „Schieben, Festhalten und Abstützen gehören dazu“ nach Runde 54. Auf der Branch `ragdoll-sol` prüft Sol vor jedem Schritt mit Sweeps vom Knochen zu seinem Ziel, ob ein animierter Knochen feste Geometrie mit mindestens 3 N s trifft, und lässt die Figur dann sich fangen, eine erste Stufe dieser Idee, in Runde 59 nicht gemessen |
| F79 | **Drehung im Körper**: Der Nutzer sieht keine Drehung im Körper. Ein Treffer dreht den Oberkörper seit F50 um seine Längsachse, mitten auf die Brust aber nur um 3 bis 5°. Beim Fangen dreht die Balance das Becken zu den Füßen zurück, F59, die IK der Beine hält es in der Richtung der Füße, und die Wirbelsäule beugt sich nur nach vorn und zur Seite. So drehen Schultern und Becken bei den Schritten nicht gegeneinander wie bei einem Menschen, und ein Stolpern dreht den Körper nicht mit. Plan: Das Becken dreht mit dem Schwungbein, Oberkörper und Arme drehen dagegen, und eine Drehung aus dem Treffer wird nicht sofort zu den Füßen zurückgedreht, die Füße drehen sich mit | Figuren, die lebendiger stolpern und sich fangen | F50, F59 | ✅ auf `active-ragdoll-53`, auf Wunsch des Nutzers, eingetragen in Runde 52: Das Becken dreht zum vorderen Fuß und mit dem Schwungbein, höchstens 0,25 rad, der Oberkörper 2,5-mal so weit dagegen, so bleibt die Drehung des ganzen Körpers, und im Stand klingt sie in 0,3 s ab. Im Schritt drehen Schultern und Hüften im Mittel bis 24 statt 3° gegeneinander, mit eigener Kraft bis 29 statt 5°. Von allen 4 × 5 264 Treffern fallen mit eigener Kraft 4 573 statt 5 028, mit der Stütze gegen die Welt 1 503 statt 1 814 und ohne Stütze 5 973 statt 7 457, von den leichten keiner, keiner und 4 statt 11, von den mittleren mit eigener Kraft aber 126 statt 73, siehe Runde 53. Auf `active-ragdoll-54` dreht der Oberkörper dreimal statt 2,5-mal so weit zurück: Mit eigener Kraft fallen von den mittleren 104 und von allen 4 461, mit der Stütze gegen die Welt 1 486 und ohne Stütze 5 767, und Schultern und Hüften drehen im Schritt im Mittel bis 28°, mit eigener Kraft bis 33°, siehe Runde 54. Auf `active-ragdoll-55` dreht das Becken, während ein Fuß schwingt, glatt über die Zeit des Schritts statt der Linie zum schwingenden Knöchel nach: Mit eigener Kraft fallen von den mittleren 72 statt 104 und von allen 4 353, ohne Stütze 5 611, mit der Stütze gegen die Welt aber 1 532 statt 1 486, siehe Runde 55. Auf `active-ragdoll-56` nimmt das Becken bei einem Schritt nur seinen Anteil an der Drehung, keinen bei einem Schritt gerade zurück, und der Oberkörper dreht wie bisher um die ganze zurück: Mit eigener Kraft fallen von allen 4 156, von den mittleren aber 81 statt 72, mit der Stütze gegen die Welt 1 502 und ohne Stütze 5 446, und Schultern und Hüften drehen im Schritt wie vorher, siehe Runde 56. Auf `active-ragdoll-57` dreht der Oberkörper bei einem Schritt gerade zurück nur um 0,85 der Drehung zurück, und die Sohlen greifen mit 1,1 statt 0,9: Mit eigener Kraft fallen von allen 3 946, von den mittleren 48, mit der Stütze gegen die Welt 1 487 und ohne Stütze 5 389, von den leichten mit allen drei Stützen keiner, bei Schritten rückwärts rutscht der Standfuß im dritten Schritt bei 29 statt 49 % der Schritte, und Schultern und Hüften drehen im Schritt mit eigener Kraft bis 31,5°, siehe Runde 57. ⛔ Arme, die gegen die Beine schwingen, und Füße, die sich mit dem Körper drehen: Je ein leichter Treffer fällt, und gefangen wird nicht mehr. Eine Drehung aus dem Treffer dreht die Balance weiter zu den Füßen zurück |
| F80 | **Abstützen an Gegenständen**: Eine Hand, die eine Wand, ein Geländer, einen Tisch oder einen Gegner berührt, wird zur Stütze. Sie drückt mit begrenzter Kraft gegen die Fläche, der Arm gibt die Kraft an den Rumpf, und die Balance zählt den Kontakt der Hand zur Stützfläche, so dass der Capture Point dort liegen darf, wo Füße und Hand zusammen halten. Lehnt sich eine Figur an, wird sie nicht weggeschoben, und taumelt sie gegen eine Wand, fängt sie sich dort, statt nur mit Schritten. Die Hand drückt nicht stärker entlang der Fläche, als ihre Reibung hält, wie die Füße | Abstützen beim Schieben, am Geländer, an der Wand und am Gegner, nach dem Wunsch des Nutzers vom 4. November 2026 | F61, F64, F65 | 🔜 vom 4. November 2026, siehe „Schieben, Festhalten und Abstützen gehören dazu“ nach Runde 54. Anders als F55, wo ein Tritt die Figur gegen die Wand treibt, stützt sie sich hier selbst ab, und anders als F35, wo der führende Arm im Sturz zum Boden greift, hält die Hand eine stehende Figur |
| F81 | **Balance bei jedem Takt**: Die Balance rechnet einmal pro Schritt der Welt, bei `stepRate` 30 halb so oft wie bei 60 Hz, und hält die Ziele der Gelenke dazwischen doppelt so lange fest. Gemessen auf Runde 58, auf den neun Tabellen auf Brust und Bauch, wie der Treffpunkt liegt, je 496 Treffer: Mit 30 Hz fallen mit eigener Kraft 1 739 statt 752 bei 60 Hz, mit der Stütze gegen die Welt 567 statt 266. 442 der 1 739 kommen acht Sekunden lang nicht zur Ruhe: In einem nachverfolgten Fall mit der Stütze gegen die Welt pendelt der Körper im Stand mit etwa 4 Hz, die mittlere Geschwindigkeit der Knochen bleibt um die Grenze der Ruhe von 0,15 m/s, und die halbe Sekunde Ruhe beginnt immer wieder neu. Die Verschiebung des Beckens gegen die Geschwindigkeit des Schwerpunkts im Stand wirkt bei 30 Hz ein ganzes Update später, und der Ausgang eines einzelnen Treffers kippt schon bei kleinen Änderungen | Figuren, die bei 30 Hz so gut fangen wie bei 60 Hz, für Spiele, die die Physik langsamer rechnen, um Zeit zu sparen | F61 | ✅ ein Teil auf `active-ragdoll-59`: Bei 30 Hz und langsamer tritt die Figur nach einem Update mit dem Capture Point außerhalb, eine Idee der Branch `ragdoll-sol`, allein mit der Stütze gegen die Welt 418 statt 567. Mit den anderen Ideen von Runde 59 fallen mit eigener Kraft 1 465, 253 davon ohne Ruhe, und mit der Stütze gegen die Welt 409. Das Becken nach der Zeit statt nach Updates abzusenken und die Hüftbremse zu glätten half bei 30 Hz nicht, siehe „Absenken nach Zeit statt nach Updates“ und „Hüftbremse im Stand geglättet“ in [Optimierungen](Optimierungen.md). 🔜 Der Rest: Regeln, deren Verzögerung bei jedem Takt gleich wirkt, gemessen bei 30, 60 und 120 Hz ✅ Ein weiterer Teil auf `active-ragdoll-65`: Unter 60 Hz wirken die Dämpfung der Höhe im Stand und, mit der Stütze gegen die Welt, das Schieben des Beckens gegen die Bewegung pro Update so viel wie bei 60 Hz, ohne Stütze schiebt es sich stärker, `NB_TUNED_STEP`. Bei 30 Hz fallen über vier Lagen des Treffpunkts mit eigener Kraft 3 781 statt 3 935, mit der Stütze gegen die Welt 908 statt 1 472 und ohne Stütze 5 672 statt 5 861, weil sie keine Ruhe fanden nur noch 18, keiner und 45 statt 174, 512 und 268, von den leichten keiner, keiner und 4 statt 2, 3 und 9, bei 60 Hz und schneller bitgleich, siehe Runde 65. 🔜 Der Rest: harte Treffer, an denen die Figur bei 30 Hz nach weniger Schritten scheitert, als sie bei 60 Hz zum Fangen braucht, mit eigener Kraft am mittleren Treffpunkt 368, und bei 20 Hz die leichten, von denen ohne Stütze 57 von 168 fallen ✅ Ein weiterer Teil auf `active-ragdoll-66`: Solange eine Stütze das Becken aufrecht hält, nimmt die Balance unter 60 Hz die Ziele eines Schritts um das Doppelte der Zeit voraus, die ein Update länger als 1/60 s dauert, `NB_STEP_LEAD`. Bei 30 Hz fallen über vier Lagen mit eigener Kraft 2 993 statt 3 781 und mit der Stütze gegen die Welt 699 statt 908, bei 60 Hz und schneller bitgleich, siehe Runde 66. 🔜 Der Rest: ohne Stütze bei 30 Hz, wo jeder Vorlauf mehr leichte Treffer fallen ließ, und bei 20 Hz ⛔ Runde 68: Ohne Stütze die Ziele nur bei Schritten nach vorn und hinten vorauszunehmen, nimmt bei 30 Hz kaum Stürze und lässt 8 bis 12 statt 4 leichte Treffer fallen, nun vor allem von vorn. Der Gewinn des Vorlaufs ohne Stütze kommt von den Schritten zur Seite, nur dort vorausgenommen fallen 4 552 statt 5 585, und von dort auch die leichten Treffer, die er fallen lässt, nichts gebaut, siehe Runde 68 ⛔ Runde 69: Mit Vorlauf ohne Stütze landet der Nachziehschritt 3 bis 5 cm neben dem Standfuß statt 16 cm, und im nächsten Schritt kippt die Figur über den Schwungfuß, der nicht abhebt. Ohne den Vorlauf im ersten Update eines Schritts fallen weiter 8 leichte Treffer, ein Vorlauf, der ausklingt, verliert den Gewinn, nichts gebaut, siehe Runde 69 ⛔ Runde 70: Der Nachziehschritt schießt über sein Ziel hinaus, und ein Fuß landet an oder auf dem anderen. Hält das Ziel den Schwungfuß vom Standfuß fern, fallen mit dem Vorlauf ohne Stütze bei 30 Hz 4 621 statt 5 585 und einer statt 4 der leichten Treffer, mit der Stütze gegen die Welt aber 710 statt 699 und bei 40 Hz ohne Stütze 6 statt 4 leichte, nichts gebaut, siehe Runde 70 ✅ Ein weiterer Teil auf `active-ragdoll-71`: Ohne Stütze hält die Balance den Schwungfuß vom Standfuß fern, `NB_KEEP_TIME`, und nimmt unter 60 Hz die Ziele eines Schritts voraus wie mit Stütze. Ohne Stütze fallen bei 30 Hz über vier Lagen 4 551 statt 5 585 und von den leichten Treffern 3 statt 4, am mittleren Treffpunkt bei 20 Hz 1 803 statt 2 671 und 21 statt 56 leichte, bei 40 Hz 951 statt 1 135 und keiner statt 4, mit eigener Kraft und der Stütze gegen die Welt bitgleich, siehe Runde 71. 🔜 Der Rest: Auf denselben Tabellen fallen bei 30 Hz mit eigener Kraft 2 958 statt 2 359 bei 60 Hz, ohne Stütze 4 551 statt 3 751, von den mittleren 256 statt 7, und bei 20 Hz ohne Stütze 21 leichte Treffer, die meisten von vorn ⛔ Runde 72: Ohne Stütze bleibt die Figur bei 60 Hz nach einer Landung stehen, wo sie bei 30 Hz mit dem Capture Point weiter draußen sofort weitertritt und die Doppelstütze verliert. Nach der Landung unter 60 Hz bis 6 oder 9 cm stehen zu bleiben, nimmt bei 30 Hz höchstens 1,4 % der Stürze, mit 9 cm fallen bei 40 Hz 3 leichte Treffer, nichts gebaut, siehe Runde 72 ⛔ Runde 73: Den ersten Fangschritt schon beim Treffer zu beginnen, lässt bei 60 Hz mit der Stütze gegen die Welt 482 statt 1 051 und mit eigener Kraft 2 662 statt 3 032 fallen, ohne Stütze aber 5 005 statt 4 864, an den Gliedern mit jeder Stütze mehr, und bei 20 und 30 Hz mehr als vorher, nichts gebaut, siehe Runde 73 ⛔ Runde 74: Den ersten Schritt beim Rumpftreffer samt seiner Haltung zu beginnen, lässt bei 60 Hz 7 479 statt 8 947 fallen, mit der Stütze gegen die Welt 498 statt 1 051, bei 30 Hz 7 447 statt 8 208, aber mehr mittlere und leichte Treffer, und bei 20 Hz mit der Stütze gegen die Welt 526 statt 217, nichts gebaut, siehe Runde 74 ⛔ Runde 75: Den Schwungfuß beim Treffer stehen zu lassen, hilft bei 20 Hz nicht, oder, mit dem Schritt wie vorher, nur mit mehr leichten Treffern, die fallen. Diese geraten in einen Spagat, den es auch ohne den Schritt beim Treffer gibt: Bei 20 Hz fallen mit eigener Kraft nur solche leichten Treffer, deren Knöchel bei einer Landung über 1 m seitlich auseinander standen, nichts gebaut, siehe Runde 75 |
| F82 | **Rumpf gibt nach**: Auf die Rückmeldung des Nutzers an Sol auf der Branch `ragdoll-sol`: Der Körper wirkt steif, Verwringen, Beugen und Mitkippen fehlen. Seit F79 verdreht der Rumpf sich nach 60 N s auf die Brust bis 40° gegen das Becken, beugt sich aber nur 15 bis 17°. Auf `ragdoll-sol` gibt die Wirbelsäule nach einem Treffer 0,15 s mit 40 % ihrer Kraft nach, kommt in 0,35 s weich zurück, und der Treffer gibt dem Oberkörper seinen ganzen Drehimpuls, nicht nur um die Längsachse wie in F50 | ein Körper, der sichtbar mit dem Treffer geht | F50, F79 | Gemessen in Runde 59: So wie auf `ragdoll-sol` beugt sich der Rumpf mit eigener Kraft 22 bis 40°, mit der Stütze gegen die Welt 28 bis 54°, aber mit eigener Kraft fallen 5 991 statt 3 865, mit der Stütze gegen die Welt 2 711 statt 1 505, und leichte Treffer fallen. Der ganze Drehimpuls wirft den Rumpf über das Becken, wie schon in F46. Nur nachgeben, und nur bei 50 bis 90 N s, beugt den Rumpf 21 bis 23°, mit der Stütze gegen die Welt bis 32°, und fängt mit eigener Kraft mehr, 3 848, ohne Stütze kommt aber ein leichter Treffer acht Sekunden nicht zur Ruhe. Siehe „Rumpf gibt nach wie auf ragdoll-sol“ in [Optimierungen](Optimierungen.md). ✅ auf `active-ragdoll-60`: Nach einem Treffer ab der Hälfte von `knockdownImpulse` auf die Wirbelsäule behalten ihre vier Gelenke 0,15 s lang 0,3 ihrer Steifigkeit und nehmen sie in 0,35 s weich wieder auf, mit derselben Kraft, `nbBoneDef::hitStiffness`, so weit, wie der Treffer von vorn oder hinten kommt. Nach 60 N s von vorn und von hinten beugt sich der Rumpf mit der Stütze gegen die Welt bis 25 und 31 statt 19 und 19°, nach 80 N s bis 23 und 35 statt 17 und 21°, und von allen 4 × 5 264 Treffern fallen mit eigener Kraft 3 733 statt 3 752, mit der Stütze gegen die Welt 1 523 statt 1 532, ohne Stütze 5 165 statt 5 258, von den leichten keiner, siehe Runde 60. 🔜 Mitkippen zur Seite: Von der Seite gibt die Wirbelsäule nicht nach, denn weicher fing die Figur dort ohne Stütze leichte Treffer nicht mehr. Dafür bräuchte sie Schritte zur Seite, die einen weiter gekippten Oberkörper fangen ✅ Ein weiterer Teil auf `active-ragdoll-67`: Ohne die Stütze gegen die Welt bleibt die Wirbelsäule nach harten Treffern kürzer weich, ab dem Doppelten von `knockdownImpulse` 0,075 statt 0,15 s, eine Idee der Branch `ragdoll-sol` in halber Stärke, `NB_SOFT_HARD_CUT`. Von allen 4 × 5 264 Treffern fallen mit eigener Kraft 3 032 statt 3 043 und ohne Stütze 4 868 statt 4 954, mit der Stütze gegen die Welt bitgleich, siehe Runde 67 |
| F83 | **Körperform für die Balance**: Auf Wunsch des Nutzers am 6. November 2026: die Form des Menschen der Bibliothek, mit der er bei derselben Größe und Masse die meisten Treffer fängt. Ideen aus anderen Projekten, ohne ihre Zahlen: flache Kastenfüße, die vor das Fußgelenk reichen, wie bei DeepMimic (MIT), die Hüftgelenke innen am Becken und das Fußgelenk weiter vorn über der Sohle wie beim Menschen von MuJoCo (Apache 2.0), Kapseln für alles wie bei Jolt (MIT) und godot-active-ragdoll (MIT), dort mit kleinen und leichten Füßen oder ganz ohne. Keines der Projekte begründet die Form seiner Füße oder die Verteilung der Masse mit der Balance | eine Figur, die bei jedem Takt mehr Treffer fängt | F68 | ⛔ Runde 76: Füße 30 statt 26 cm lang und 11 statt 9 cm breit lassen bei 60 Hz 8 044 statt 8 947 fallen und bei 30 Hz 7 069 statt 8 208, aber ohne Stütze bei 60 Hz 24 statt 7 mittlere Treffer und von den leichten bei 20 Hz mit der Stütze gegen die Welt 2 und bei 40 Hz ohne Stütze einen, die vorher standen. Schmale Hüften, das Fußgelenk weiter vorn und mehr Masse in den Beinen helfen weniger oder schaden, nichts gebaut, siehe Runde 76. 🔜 Die Balance hält die Füße um `footGap` auseinander, nach dem Abstand der Hüften, und ohne Stütze auf `NB_KEEP_LENGTH` vom Standfuß fern, der Länge der Füße von heute: beides aus Hüften und Sohlen ableiten und die großen Füße erneut messen ⛔ Runde 76, Sols Körper von `ragdoll-sol`: Längere Beine, ein kürzerer Rumpf und Sohlen, 12 cm breit und 11 cm hinter dem Knöchel, lassen bei 60 Hz 7 027 statt 8 947 fallen und bei 30 Hz 6 677 statt 8 208, aber mit eigener Kraft bei 30 Hz 164 statt 20 mittlere und bei 20 Hz 14 statt 4 leichte Treffer, die nicht mehr zur Ruhe kommen. Der Schwerpunkt steht dann im Stand über 64 statt 39 % der Sohle von der Ferse aus, nichts gebaut, siehe Runde 76. 🔜 Dazu auch den Stand der Füße unter dem Becken aus der Sohle ableiten, `NB_STANCE_BACK`, und Sols Füße mit dem Schwerpunkt über verschiedenen Stellen der Sohle messen ⛔ Runde 77: Die Maße der Balance aus dem Körper, `footGap` aus Hüften und Sohlen, die Länge zum Fernhalten aus der Sohle und der Stand aus der Mitte der Sohle, bitgleich für den Menschen der Bibliothek, helfen den großen Füßen nicht, bei 60 Hz fallen 8 211 statt 8 044. Mit Sols Füßen bringt der Schwerpunkt weit vorn über der Sohle bei 60 Hz den Gewinn und kostet bei 30 Hz mittlere Treffer, und mit der Stütze gegen die Welt fallen bei 20 Hz 343 bis 367 statt 217, nichts gebaut, siehe Runde 77. 🔜 Die drei leichten Treffer bei 20 und 40 Hz und die mittleren ohne Stütze bei 60 Hz verfolgen, an denen die großen Füße scheitern ⛔ Runde 78: Ein Schritt, sobald der Capture Point im Stand 0,15 s außerhalb der Stütze liegt, lässt bei 60 Hz mit den großen Füßen ohne Stütze 13 statt 24 mittlere Treffer fallen und bei 40 Hz keinen leichten, kostet aber mit eigener Kraft bei 30 Hz mittlere und bei 20 Hz leichte Treffer, weil er aus dem breiten Stand in den Spagat führt. Ein Fuß, der bis 6 cm über dem Boden noch steht, fängt mit der Stütze gegen die Welt den Spagat bei 20 Hz und kostet ohne Stütze viel, nichts gebaut, siehe Runde 78. 🔜 Den Schritt nach der Zeit nur ohne Stütze und 6 cm nur mit der Stütze gegen die Welt an neuen Treffern prüfen, danach ein Schritt aus dem breiten Stand heraus ⛔ Runde 79: Den Schritt nach der Zeit nur ohne Stütze und den Boden bis 6 cm nur mit der Stütze gegen die Welt, vorher festgelegt und an neuen Treffern geprüft: Beide Körper lassen bei 25, 30, 50 und 60 Hz weniger fallen, der Mensch der Bibliothek aber bei 25 Hz ohne Stütze einen leichten Treffer mehr, der aus dem Stand in den Spagat tritt und nicht zur Ruhe kommt, nichts gebaut, siehe Runde 79. 🔜 Der Schritt aus dem breiten Stand heraus: erst das Gewicht über den Fuß nahe dem Capture Point, dann den anderen Fuß heranholen, danach diese Formen erneut ⛔ Runde 80: Ein breiter Stand ist das gewöhnliche Ende eines Fangs, mit der Stütze gegen die Welt muss das Becken dabei im Median 8 cm tiefer. Das Gewicht über den nahen Fuß zu verlagern und den anderen heranzuholen, lässt mit eigener Kraft bei 30 Hz 38 und 39 statt 20 mittlere Treffer und die großen Füße bei 60 Hz mehr fallen, nichts gebaut, siehe Runde 80. 🔜 Allein den Boden bis 6 cm mit der Stütze gegen die Welt an Treffern prüfen, die noch keine Runde gesehen hat, danach ruht F83 ✅ Runde 81 auf `active-ragdoll-81`: Allein der Boden bis 6 cm mit der Stütze gegen die Welt, vorher festgelegt und an Treffern geprüft, die noch keine Runde gesehen hatte. Mit der Stütze gegen die Welt fallen dort 1 327 statt 1 340, auf den Tabellen bei 60, 30, 20 und 40 Hz 1 047, 696, 202 und 234 statt 1 051, 699, 217 und 243, mit eigener Kraft und ohne Stütze bleibt alles bitgleich, siehe Runde 81. F83 ruht, die großen Füße bleiben ein Vorschlag: Ohne Stütze fehlen ihnen bei 60 Hz die mittleren Treffer und bei 40 Hz ein leichter ✅ Runde 82 untersucht: In der Stadt mit Figuren kostet der Boden bis 6 cm keine Fänge. Gepaart wiederholt ab dem Update, in dem er greift, fängt sich die Figur in 7 von 266 Fängen nur mit ihm und in 3 nur ohne ihn, die 22 fehlenden Fänge im Stand sind Zufall der Stadt, siehe Runde 82 ⛔ Runde 103, auf Wunsch des Nutzers: der nützlichste Fuß für Balance, Gang, Treppe und Schutt, fünf Füße auf neun neuen Tabellen bei 60, 30 und 20 Hz und mit dem Gang von Runde 102. Nach den Maßen des Menschen, 27 cm lang, 10 cm breit und mit der Ferse 6,5 statt 4 cm hinter dem Knöchel, fallen 5,6 % weniger Treffer, und die Schienbeine springen in den Übergängen an 198 statt 242 Stellen über der Grenze, aber das Becken an 123 statt 116, und rennend die Treppe hinunter wippt es an 5 von 96 Starts über 10 cm. Längere Füße passen nicht auf die Stufen, nur breitere ändern am Gang nichts und lassen 4,4 % weniger fallen, ohne Stütze bei 20 Hz aber mehr leichte. Jeder Fuß lässt in einer Tabelle mehr leichte Treffer fallen, nichts gebaut, siehe Runde 103. 🔜 Der Fuß nach den Maßen des Menschen mit einem Gang, der zu ihm passt, gemessen mit einer Regel, die die leichten Treffer über alle Tabellen zusammen zählt |
| F84 | **Gesteuerte Figur**: Ein Spiel steuert eine Figur, und die Bibliothek bewegt sie auf ihren eigenen Füßen, nach Kapitel 2 und 25.1 der Roadmap des Spiels: Das Spiel gibt nur die Geschwindigkeit, die sie will, und wohin sie schaut. Sie bewegt sich als Kapsel mit den Abfragen von Box3D für Figuren, gleitet an Wänden entlang, steigt Stufen, bleibt an zu steilem Grund stehen, fällt ohne Boden und landet, und ihre Füße treten in einem Gang aus der Länge der Beine nach, gehend und rennend. Treffer wirken wie auf jede stehende Figur, und nach einem Sturz steuert sie weiter | eine Spielfigur für das Spiel, mit Gehen, Rennen, Treppen und Stürzen aus derselben Bibliothek wie die Treffer | F9, F10 | ✅ Stufe 1 auf `active-ragdoll-84`, auf Wunsch des Nutzers: `nbFigureDef::steered`, `nbFigureSteering`, `nbFigure_Steer` und `nbSteer`, der Gang mit Abrollen über die Zehen und einer Hüfte, die über dem Standbein schwingt, alle sieben vorher festgelegten Bedingungen erfüllt, Figuren ohne `steered` bitgleich, und die Szene „Spielfigur“ in der Demo, siehe Runde 84. ✅ Ein Teil von Stufe 2 auf `active-ragdoll-85`: Die Abfragen einer Figur lassen ihre eigenen Knochen weg, bevor Box3D sie rechnet, `b3QueryFilter::excludeGroup`, bitgleich, eine gesteuerte Figur kostet 7,7 statt 22,1 µs. Am Hang und auf Treppen landet ein Fuß bergauf weiter vorn und bergab näher unter dem Körper, die Schritte werden kürzer, die Feder unter der Kapsel folgt der Ebene des Bodens, und die Hüfte geht vor dem Landen so tief, wie der wirkliche Landepunkt braucht. Gehend landet ein Fuß auf der Ferse. Alle sieben vorher festgelegten Bedingungen erfüllt, die Hüfte springt die Rampe hinunter 0,3 statt 18,6 cm in einem Update, siehe Runde 85. ✅ Treppen Stufe für Stufe auf `active-ragdoll-86`: Ein Fuß landet mit der ganzen Sohle auf einer Stufe, `nbFindFoothold`, jeder Schritt nimmt gleich viele Stufen, und die Hüfte folgt der Linie der Treppe, `nbStairGround`. Fünf der sechs vorher festgelegten Bedingungen erfüllt, die dritte bis auf 19 × 25 cm hinunter, wo das Becken um 7,3 statt höchstens 6 cm auf und ab geht, siehe Runde 86. ✅ Gehen in der Ebene auf `active-ragdoll-87`: Die Figur sieht voraus, wie lange ihr Bein einen Fuß am Boden noch erreicht, `nbTimeToLose`, und ein Fuß, den es bald nicht mehr erreicht, hebt vorher ab, oder der schwingende eilt, der Blick dreht mit `nbFigureSteering::turnAcceleration`, die Füße zielen mit geglätteter Geschwindigkeit, gestrecktere Knie, schmalere Schritte, das Becken dreht und pendelt, die Brust dreht gegenläufig, die Ellbogen schwingen mit, und der Fuß schwingt flach. Acht der zwölf vorher festgelegten Bedingungen erfüllt, in Wendungen springt das Becken bis 0,9 statt 6,8 cm in einem Update und sinkt 4,7 statt 30,6 cm, siehe Runde 87. ✅ Übergänge an jeder Stelle im Gang auf `active-ragdoll-88`: Ein schwingender Fuß plant seinen Weg und seine Drehung in jedem Update neu, `nbSwingPath`, von zwei Füßen am Boden hebt der zuerst ab, den sein Bein zuerst verliert, ein schwingendes Bein streckt sich nie ganz, `NB_LEG_SWING_SLACK`, und der Takt des Gangs hält Schritt mit den Füßen, `NB_GAIT_KEEP`. Über acht Stellen im Gang springen Füße bis 4,6 statt 6,6 cm in einem Update, Schienbeine bis 6,3 statt 9,6 cm und das Becken bis 1,9 statt 4,7 cm, es sinkt bis 6,1 statt 12,8 cm, und nach jedem Übergang ist der Gang wieder im Takt. Fünf der sieben vorher festgelegten Bedingungen erfüllt, siehe Runde 88. ✅ Hub, Rolle und Hüfte in den Übergängen auf `active-ragdoll-89`: Ein Fuß hebt sich so hoch, wie die Eile beim Abheben sagt, ein Fuß, der wieder neben die Hüfte kommt, rollt mit 0,06 s flach, `NB_FOOT_UNROLL`, das Ziel eines schwingenden Fußes läuft um 0,8 des Schwungs weich aus, `NB_GAIT_AIM_FADE`, und bremst oder wendet die Kapsel, beugt die Figur gehend die Knie, `NB_GAIT_BEND_SPEED`. In 23 statt 76 Updates über den Grenzen, Schienbeine bis 4,3 statt 6,3 cm, das Becken bis 1,3 statt 1,9 cm in einem Update, es sinkt bis 6,6 statt 6,1 cm, geradeaus wie vorher. Sieben der acht vorher festgelegten Bedingungen erfüllt, siehe Runde 89. ✅ Der Körper fällt, die Beine fangen ihn, auf `active-ragdoll-90`, auf Wunsch des Nutzers: Der Oberkörper neigt sich mit der Beschleunigung des Körpers, `NB_GAIT_LEAN_LAG`, losrennend 15° weiter vor als im Lauf, bremsend zurück und in Wendungen nach innen, und rennend dreht die Brust mit den Schultern 12° zu jeder Seite gegen die Beine, `NB_RUN_CHEST_TURN`. 13 der 16 vorher festgelegten Bedingungen erfüllt, zwei weitere für Brust und Schultern, die Übergänge, das Gehen und Rennen geradeaus und die Treppen wie vorher, siehe Runde 90. ✅ Rennen ohne Ruck auf `active-ragdoll-91`, auf Wunsch des Nutzers: Rennend geht die Wurzel vor jeder Landung gleichmäßig so tief, wie das landende Bein es bis dahin braucht, `NB_RUN_DESCEND`, der Fuß setzt mit 70 % seines Tempos auf, `NB_RUN_LAND_SOFT`, ein Fuß hinter der Hüfte rollt schon so weit über die Zehen, wie sein Bein gleich braucht, `NB_FOOT_ROLL_AHEAD`, und das Becken dreht rennend 5° gegen die Brust, `NB_RUN_PELVIS_TURN`. Geradeaus rennend springt das Becken in einem Update höchstens 3,5 statt 18,4 mm, Füße und andere Knochen weniger als vorher, die Übergänge und das Gehen bleiben, und auf Treppen und Rampen springt das Becken weniger. 12 der 14 vorher festgelegten Bedingungen erfüllt, die dritte und die sechste nur mit 3,0 m/s, weil das Becken mit 4,8 m/s nur 4 bis 5 statt 6 bis 9 cm auf und ab geht, siehe Runde 91. ✅ Losrennen und Anhalten ohne Ruck auf `active-ragdoll-92`, auf Wunsch des Nutzers: Die Beugung der Ellbogen, das Wippen und die Höhe des Hubs gehen mit der Feder von `NB_RUN_TURN_LAG` in die neue Gangart über, die Hocke des Rennens und der Abstieg des Gehens kommen mit ihr, und ein schwingender Fuß behält die Form seines Hubs bis in die zweite Hälfte des Schwungs, `nbLiftShare`. Beim Losrennen und Anhalten springen die Hände in einem Update höchstens 3,1 statt 14,1 cm und das Becken 2,9 statt 4,7 cm, Gehen, Rennen, die Übergänge und die Treppen bleiben. 10 der 12 vorher festgelegten Bedingungen erfüllt, siehe Runde 92. ✅ Erster Schritt und Abheben ohne Ruck auf `active-ragdoll-93`, auf Wunsch des Nutzers: Ein Fuß rollt über die Zehen bis 1 rad, `NB_FOOT_ROLL_MOST`, und hebt weiter bei 0,7 rad ab, losrennend plant ein schwingender Fuß mit der Beschleunigung der Kapsel, `NB_GAIT_PLAN_FROM`, der Abstoß kommt nicht schneller als im Vorschwung, ein Schwung behält seinen Anteil am Zyklus, `nbSwingTime`, und rennend hält sich ein Fuß, der früh abhebt, an den Takt. Über 24 Stellen im Gang springen die Hände in einem Update höchstens 2,9 statt 3,9 cm, ein Fuß 4,0 statt 6,4 cm und das Becken 2,1 statt 3,6 cm, und rennend wechseln die Füße gleichmäßig, mit 4,8 m/s 19 und 20 Updates statt bis 17 und 22. 11 der 13 vorher festgelegten Bedingungen erfüllt, siehe Runde 93. ✅ Wechsel ganz ohne Ruck auf `active-ragdoll-94`, auf Wunsch des Nutzers: Die Form des Hubs und der Abstieg des Gehens gehen mit der Feder von `NB_RUN_TURN_LAG` in die neue Gangart, `nbLiftShare`, ein Fuß hinter der Hüfte rollt voraus, sobald der Gang rennt, die Figur sucht den Boden am Ziel eines Fußes so viel weiter oben, wie sie tiefer suchen darf, und auf einer Treppe geht ein Fuß bis zwei Stufen weiter zu der, die ein Schritt will, `NB_STAIR_SHIFT`. Über 24 Stellen im Gang springt das Becken beim Anhalten höchstens 1,15 statt 2,1 cm und ein Schienbein beim Wechsel von 1,6 auf 4,8 m/s 4,9 statt 5,3 cm, und rennend auf der Treppe steht bei 32 verschiedenen Starts jeder Fuß auf seiner Stufe, hinunter mit 4,5 m/s mit drei Stufen in jedem Schritt und höchstens 14,3 statt 20,7 cm Wippen gegen die Linie der Treppe. 11 der 14 vorher festgelegten Bedingungen erfüllt, siehe Runde 94. ✅ Treppe hinunter ruhig auf `active-ragdoll-95`, auf Wunsch des Nutzers: Auf einer Treppe folgt die Wurzel dem Boden zwischen den Fußpunkten höchstens 0,6 m/s schneller oder langsamer, als dieser Boden selbst steigt oder fällt, statt als die Treppe, so geht sie nicht schon hinunter, während die Kapsel noch auf dem Podest läuft, und spät im Schwung hält ein Fuß seine Stufe. Rennend geht das Becken auf der Treppe bei acht Starts über 0,7 m höchstens 7,1 cm gegen ihre Linie auf und ab, hinunter vorher bis 18,1 cm, bei 20 Starts über eine ganze Stufe hinunter mit 3,0 m/s bis 8,3 cm. 12 der 14 vorher festgelegten Bedingungen erfüllt, siehe Runde 95. ✅ Drehen ohne gekreuzte Beine auf `active-ragdoll-96`, auf Wunsch des Nutzers: Schaut die Figur, wohin sie geht, geht sie quer zu ihrem Blick höchstens 0,3 m/s oder 0,17 ihres Wunschtempos und rückwärts höchstens 0,3 m/s, ihr Blick dreht mit 4 statt 10 rad/s, ein stehender Fuß dreht sich auf seiner Sohle mit, ein schwingender landet auf seiner Seite des anderen, und ein Fuß, den sein Bein nicht mehr erreicht, hebt ab. In Drehungen um 45 bis 180° kreuzen sich die Knie nie, vorher bis 25 cm, das Becken geht gehend höchstens 10,9 und rennend 14,8 cm tiefer als im Stand, vorher bis 27,8 und 68,3 cm, und die Oberschenkel gehen höchstens 66,5 und 103,3° auseinander, vorher bis 100 und 177°. Alle zehn vorher festgelegten Bedingungen erfüllt, siehe Runde 96. ✅ Über Schutt gehen, ohne zu sitzen auf `active-ragdoll-97`, auf Wunsch des Nutzers: Abseits von Treppen landet ein Fuß möglichst höchstens 25 cm höher oder tiefer als der andere, `NB_GAIT_FOOT_BAND`, nie höher als eine Stufe über dem Weg der Kapsel, ohne Boden am Ziel zielt er auf den Weg der Kapsel statt in die Luft, auf Spitzen von Brocken stellt er sich nicht, eine Treppe beginnt nur, wo die nächste Stufe passt, `NB_STAIR_MATCH`, und zwischen steilen Seiten hält sich die Kapsel am stehenden Fuß. Über die Trümmer einer gesprengten Ziegelwand, 222 Läufe gehend und rennend, hat die Figur in 1,98 statt 9,77 % der Updates eine schlechte Pose, ein Standknie über 120° in 42 statt 1 683 Updates, und hält den Schutt nie für eine Treppe, vorher in 32,9 % der Updates. Alle zehn vorher festgelegten Bedingungen erfüllt, siehe Runde 97. ✅ Wenden und Landen ohne Ruck auf `active-ragdoll-99`, auf Wunsch des Nutzers: Will die Figur rennen, hebt gehend ein Fuß hinter der Hüfte ab, den ihr Bein nicht mehr erreicht, auch wenn der andere noch schwingt, und eilt ein Schwung, hält der Fuß sein Ziel über die letzten 0,08 s vor dem Landen, `NB_GAIT_AIM_SETTLE`. Rennend in der Kehrtwende springen Becken und Hände in einem Update höchstens 2,58 und 2,77 statt 8,51 und 8,50 cm, gehend um 90° landet ein Fuß mit 4,85 statt 5,41 cm und losrennend mit 3,61 statt 4,01 cm. Zwölf der 13 vorher festgelegten Bedingungen erfüllt, die sechste nicht im 90. Perzentil, wo sich gehend in Wendungen um 90° die Füße 2 cm mehr kreuzen, siehe Runde 99. ✅ Seitwärts, schräg und rückwärts auf `active-ragdoll-100`, auf Wunsch des Nutzers: Schaut die Figur nicht, wohin sie geht, `nbSteer::faceVelocity` false, drehen Becken und Beine zur Laufrichtung, gehend bis 70° und rennend bis 90° vom Blick, Brust und Kopf bleiben beim Blick, über 110° geht sie rückwärts, rennend mit Flugphase, und quer zu den Beinen und gegen sie geht die Kapsel nicht schneller, als die Beine sie tragen. Seitwärts mit 1,6 m/s geht das Becken 8,3 statt 28,7 cm tiefer als im Stand, mit 3,0 m/s stehen die Füße 14,7 cm auseinander statt 56 cm gekreuzt, rückwärts rennend geht das Becken 9,6 statt 37 cm tief, und in 21 Übergängen an je 24 Stellen liegen 174 statt 432 Stellen mit den Füßen über der Grenze. Sieben der acht vorher festgelegten Bedingungen erfüllt, die fünfte nicht ganz, siehe Runde 100. ✅ Wechsel seitwärts ohne gekreuzte Füße auf `active-ragdoll-101`, vorgeschlagen nach Runde 100: Die Beine drehen nicht weiter, als die Füße am Boden quer zu ihnen auf ihrer Seite bleiben, und warten darauf höchstens 0,15 s, ein Fuß, der sie aufhält, tritt, ein schwingender landet so gedreht, wie die Beine bis dahin sind, und die Füße planen mit dem Tempo, das das Programm will. Beim Anhalten aus dem Seitwärtsgehen und -rennen und beim Wechsel von links nach rechts gehend kreuzen sich die Füße an keiner von 24 Stellen mehr, vorher bis 17, 19 und 14 cm, in 21 Übergängen an 21 statt 80 Stellen. Alle neun vorher festgelegten Bedingungen erfüllt, die Schienbeine rennend aber nicht ruhiger, siehe Runde 101. ✅ Rückwärts auf den Zehen und umkehren ohne Absacken auf `active-ragdoll-102`, vorgeschlagen nach Runde 101: Rückwärts landet ein Fuß auf den Zehen, ein Fuß vor der Hüfte rollt über die Ferse ab, so weit sein Bein ihn braucht, bis 0,35 rad und nicht schneller als 6 rad/s, `NB_FOOT_HEEL_MOST`, ein schwingender Fuß plant seinen Vorlauf mit dem Tempo bei seiner Landung, sein Hub wächst nicht sprunghaft, `NB_GAIT_LIFT_RISE`, und auf einer Treppe hat jeder schwingende Fuß sein eigenes Tempo, `NB_STAIR_RATE`. Stetig rückwärts gehend geht das Becken 1,2 statt 7,6 cm unter das Stehen, von rückwärts auf vorwärts rennend höchstens 9,6 statt 31,2 cm, in 21 Übergängen liegen 0 statt 45 Stellen mit dem Becken über 15 cm tief, losrennend springt ein Schienbein höchstens 3,6 statt 4,2 cm, und rennend die Treppe hinunter wippt das Becken an keinem von 96 Starts über 10 cm, vorher bis 20,2 cm. Alle elf vorher festgelegten Bedingungen erfüllt, siehe Runde 102. 🔜 Rest von Stufe 2: zuerst seitwärts und rückwärts: rückwärts gehend wieder ruhigere Schienbeine, heute 36 mm in einem Update, auf `active-ragdoll-101` 26 mm, weil das Bein auf den Zehen fast gestreckt landet, aus dem Stand rückwärts Füße und Schienbeine unter 3,7 cm, heute bis 5,8 und 5,6 cm, beim Anhalten aus dem Rückwärtsrennen und von seitwärts auf rückwärts weniger Sprung in den Schienbeinen, heute bis 5,1 cm, gehend bis 10,0 cm und rennend bis 6,1 cm, und Wechsel ohne gekreuzte Füße auch von vorwärts auf seitwärts rennend und von seitwärts auf rückwärts, heute bis 11,1 und 8,3 cm. Der Anlauf auf eine Treppe hinauf, wo mit 3,0 m/s bei 3 von 96 Starts ein Fuß über der Kante der ersten Stufe steht. Danach kein Schienbein über 3,7 cm, heute bis 6,0 cm beim Wechsel von 1,6 auf 4,8 m/s, 4,2 cm beim Aufsetzen nach dem Anhalten und 5,2 bis 6,9 cm in Wendungen, das Becken in allen Wechseln unter 1 cm, heute bis 2,6 cm rennend in der Kehrtwende, 1,73 cm gehend, 1,32 cm beim Wechsel von 1,6 auf 4,8 m/s und 1,05 cm losrennend mit 4,8 m/s, mit 4,8 m/s mehr Wippen, die Hüfte als Pendel im Schritt, Wendungen um 90 und 180°, in denen kein Fuß mehr als 3,7 cm in einem Update springt, heute gehend um 90° bis 4,85 cm und rennend in der Kehrtwende bis 7,3 cm, wo die Ziele beim Herauslaufen bis 15 cm in einem Update wandern, ohne dass ein vorn gelandeter Fuß beim Umkehren sofort wieder abhebt und ohne dass sich die Füße gehend um 90° mehr kreuzen als auf `active-ragdoll-98`, dabei nicht tiefer als 6 cm, das Knie beim Aufsetzen, Rennen gegen einen hohen Brocken, ohne dass das Becken wie heute bis 9 cm über den Boden sackt, wenn die Kapsel schlagartig steht, weniger Landungen auf Spitzen von Schutt, heute 13 %, Gehen und Rennen mit den Clips des Spiels statt dem Schritt der Bibliothek, Kapsel, Anpassung an den Boden und aktiver Ragdoll bleiben, ist F85, Stufe 1 auf `active-ragdoll-98`, Anhalten auf einer Treppe, ohne dass ein Fuß dabei über einer Kante landet, und nach 25.1 der Roadmap des Spiels Parcours mit 1 000 gleichen Eingabefolgen, gemessen an Verhaken, Eindringen, Bodenverlust, Fehltritten und Hashes, auf zerstörten Treppen, Schutt und Boden, der wegbricht, mit bewegten Plattformen und mit dynamischen Körpern, die die Figur schiebt. Springen, Landen und Klettern spielt das Spiel weiter mit eigenen Clips |
| F85 | **Gehen und Rennen nach Clips**: Auf Wunsch des Nutzers am 8. November 2026: Die Figuren folgen Animationen und wechseln bei einem Treffer in den Controller, nach 27.3 der Roadmap des Spiels, wo im normalen Gehen und Rennen Animation und IK die Pose bestimmen. Eine gesteuerte Figur spielt die Clips des Spiels für Gehen und Rennen, je einen Zyklus mit den Zeiten, zu denen jeder Fuß aufsetzt und abhebt, im Takt ihrer Kapsel, und die Bibliothek plant weiter, wo die Füße landen, auf Treppen, Schutt und am Hang, und hält sie dort mit der IK. Treffer wirken wie bisher. Stufe 1: Rumpf, Arme, Kopf und Becken aus den Clips, zwischen Gehen und Rennen gemischt, Takt und Standanteil aus ihren Marken, das Becken nur so viel tiefer, wie die Beine es brauchen. Stufe 2: der Weg eines schwingenden Fußes und seine Rolle über Ferse und Zehen aus dem Clip, auf die Fußpunkte der Bibliothek verzogen. Stufe 3: eigene Clips für Losgehen, Anhalten und Wenden | Gehen und Rennen, wie die Animationen des Spiels es zeigen, mit Boden, Treppen, Schutt und Treffern aus der Bibliothek | F84 | ✅ Stufe 1 auf `active-ragdoll-98`: `nbGaitClipDef`, `nbFigureDef::gaitClips` und `nbFigure_SetGaitClips`, dazu `nbMakeHumanoidGaitClip`, das den Gang der Bibliothek als solchen Clip aufzeichnet. Mit diesen Clips geht und rennt die Figur wie ohne, die Knochen im Mittel höchstens 0,24 cm daneben, in den Übergängen je Art im Mittel höchstens 0,9 cm und 1,9° schlechter, über Schutt mit 1,71 statt 1,98 % schlechten Posen, auf Treppen ruhiger, und von 1 024 Treffern fangen sich und fallen dieselben. Oberarme, im Clip 60° vorgehoben, heben die Hände beim Gehen um 21,4 cm. Ohne Clips bitgleich, alle zehn vorher festgelegten Bedingungen erfüllt, siehe Runde 98. 🔜 ruht auf Wunsch des Nutzers, bis der Controller fertig ist, F84. Dann Stufe 2 und 3, das Auf und Ab des Clips auf Treppen gegen ihre Linie, die Brust, die sich rennend nach Treffern mit Clips 2 bis 4° weiter neigt, und Clips eines Spiels, deren Schritte anders sind als die der Bibliothek |

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
- Die Listen der berührenden Kontakte billiger pflegen: Auf dem Ryzen kostet die Pflege mit 8 Threads 0,013 ms pro
  Schritt, so viel, wie das Update mit `kollaps-15` spart, mit einem Thread 0,003 ms, siehe Schritt 49. 0,004 ms davon
  gehen ins Anlegen der Kontakte, wo Box3D für jeden neuen Kontakt seinen Eintrag schreibt, auch für die vielen, die nie
  berühren, der Rest in die Zustandswechsel, die Typwechsel und das Erstarren. Schriebe Box3D den Eintrag erst, wenn ein
  Kontakt zu berühren beginnt, entfiele das beim Anlegen, aber die Nummer, die ein Kontakt beim Anlegen bekommt, müsste
  dann anderswo stehen.
- Nur als Versuch, ändert das Verhalten überall: „Physik mit 30 Hz und 8 Teilschritten“.

## Verworfen

Mit Grund in [`Optimierungen.md`](Optimierungen.md): ein eigenes Box3D, Einschläge gesammelt rechnen, 16 Threads mit
SMT, AVX-512 auf Zen 4, ein Jacobi-Löser statt Inseln und Farben, Physik auf der GPU, kleine Trümmer als Partikel, mehr
Farben im Graphen, den Überlauf nachträglich einfärben, die Kosinusse der Kanten in der Hülle wie upstream, kompakte
Paare für nicht berührende Kontakte, einen Vorfilter über die Boxen vor dem Hüllentest, mehr Ladevorgänge gleichzeitig
in der Paarsuche, den Überlauf in unabhängigen Gruppen, gesammelte Typwechsel, weniger Teilschritte für langsame Inseln,
einen anderen Rand der Boxen und die neuen Kontakte parallel anzulegen, die erste Stufe des Umbaus der Buchhaltung von
Box3D.
