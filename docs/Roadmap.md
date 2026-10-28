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
auf Wunsch des Nutzers gebaut, und den dauerhaften Gebieten auf `kollaps-32`, gebaut und auf dem Ryzen nachzumessen.
Begründungen und Messwerte stehen in [`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge.
Nach jeder Runde wird der Stand hier nachgetragen.

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
  Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-6`“. Auf `kollaps-11` kostet er 2,22 ms, hochgerechnet bleiben
  51 % auf einem Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-11`“. Auf `kollaps-12` kostet er 2,27 ms, im
  Rahmen der Schwankungen so viel wie davor, hochgerechnet bleiben 50 % auf einem Thread, siehe „Skalierung auf dem
  Ryzen mit `kollaps-12`“. Auf `kollaps-27` kostet er bei ×2 2,25 und bei ×4 1,64 ms, hochgerechnet bleiben bei ×2 49 %
  auf einem Thread, siehe „Skalierung auf dem Ryzen mit `kollaps-27`“.
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
  einem Thread 0,2 % mehr und mit 4 Threads 12,2 % weniger, siehe Schritt 68. Ob sie bleiben, entscheidet der Ryzen,
  siehe Schritt 71.
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
  Anstiegs zurück, siehe Schritt 67. Auf Wunsch des Nutzers ist es seit `kollaps-31` trotzdem gebaut, mit einem festen
  Abschnitt der Blöcke für jeden Thread, in dem die großen Stücke zuerst drankommen: Beim Abriss auf der VM bei ×4
  kosten die Prüfungen mit 4 Threads 6 % weniger und mit einem Thread 2 % mehr, im Benchmark unter Trommelfeuer 1 bis
  3 % mehr, siehe Schritt 69.
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
| 68 | **Dauerhafte Gebiete**: auf einer eigenen Branch die Gebiete der großen Inseln wie die Farben im Graphen über die Schritte führen, jeder Körper fest in einem, jeder Kontakt mit Gebiet und Farbe, sobald er zu berühren beginnt, neu geschnitten nur selten, die Körper an den Grenzen mit geteilter Masse wie im Prototyp von Schritt 62, ändert die Ergebnisse | mit 8 Threads ein Teil der 0,255 ms pro Schritt mit Team, die die Threads an den Sperren zwischen den Stufen verlieren, ohne den Aufbau in jedem Schritt, an dem der Prototyp scheiterte | 67 und die Entscheidung des Nutzers | 🔜 auf `kollaps-32` gebaut: Die Ergebnisse ändern sich, bleiben aber mit jeder Zahl von Threads, in beiden Breiten und auf Windows, Linux und macOS gleich, die Tests grün, nichts hängt in der Luft. Beim Abriss bei ×2 kostet der Löser auf der VM gepaart mit einem Thread 0,2 % mehr, erlaubt sind 5 %, und mit 4 Threads 12,2 % weniger. Im Benchmark unter Trommelfeuer kostet er mit 1 bis 4 Workern bis zu 9 % mehr, mit so wenigen Threads lohnen Gebiete auf kleineren Inseln nicht. Auf dem Ryzen nachzumessen, siehe 71 |
| 69 | **Prüfungen des Updates auf eigenen Zeilen**: was die Prüfungen von jedem bewegten Stück schreiben, auf eigenen Cache-Zeilen am Ende des Akteurs, alle Allokationen auf 64 Bytes, die Arenen der Worker und die Aufgaben der Prüfungen auf eigenen Zeilen, und mit Workern für jeden Thread ein fester Abschnitt der Blöcke, den er von Update zu Update behält, darin die Blöcke mit großen Stücken zuerst, bitgleich, auf Wunsch des Nutzers trotz „Nichts davon“ in 67 | auf dem Ryzen hochgerechnet rund 0,011 ms pro Schritt bei ×4 mit 8 Threads | 67 und der Wunsch des Nutzers | ✅ auf `kollaps-31`: Die Welt endet bitgleich, die Tests bleiben grün. Beim Abriss auf der VM bei ×4 kosten die Prüfungen mit 2 und 4 Threads 2,8 und 6,2 % weniger, ein Stück in den Blöcken mit 4 Threads 11,6 %, mit einem Thread 1,6 % mehr. Im Benchmark unter Trommelfeuer bei ×4 kosten sie 1,0 bis 3,3 % mehr. Ein Akteur ist 448 statt 384 Bytes groß, die Stadt braucht 1,5 % mehr Speicher. Auf dem Ryzen nachzumessen, siehe 70 |
| 70 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-30` und `kollaps-31` direkt nacheinander beim Abriss bei ×4 und ×2 | zeigt, was 69 mit 8 Threads bringt, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob es bleibt | 69 | 🔜 |
| 71 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-31` und `kollaps-32` direkt nacheinander beim Abriss bei ×2 und ×4, gern mit 70 in einem Zug | zeigt, ob die dauerhaften Gebiete mit 8 Threads mindestens 0,10 ms pro Schritt bringen, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, ob sie bleiben | 68 | 🔜 |
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
