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
beide auf dem Ryzen nachgemessen, und den großen Paketen im Team auf `kollaps-9`, auf dem Ryzen nachgemessen.
Begründungen und Messwerte stehen in [`Optimierungen.md`](Optimierungen.md), hier nur das Fazit und die Reihenfolge.
Nach jeder Runde wird der Stand hier nachgetragen.

`ragdoll-1` bis `ragdoll-6` sind wie `kollaps-1` keine Runden der Physik: Sie bauen die Figuren auf Box3D nach Kapitel
26 bis 28 und 31 bis 33 der Roadmap des Spiels, siehe „Figuren auf Box3D“, „Balance“, „Clips und Keyframe-Editor“ und
„Kosten der Figuren“ unten. `ragdoll-6` misst, wohin der Löser bei Figuren rechnet, mit einer Ergänzung des Profils in
Box3D, und ändert sonst nichts, siehe „Wohin der Löser bei Figuren rechnet“ unten. `ragdoll-5` prüft die Kontakte
zwischen den Knochen einer Figur und ändert an der Bibliothek nichts, der Benchmark bekommt nur einen Schalter dafür. `ragdoll-4` misst, was Figuren kosten, und ändert an der Physik nichts: Gepaart gegen
`ragdoll-3` auf dem 7800X3D, je zwei Läufe im Wechsel mit 2 Threads, enden alle Szenen des Benchmarks mit denselben
Zahlen, und ein Frame kostet −1,6 bis −0,3 %, bei einer Streuung von Lauf zu Lauf bis 5 %. Beim Messen fiel auf, dass
im Build mit MinGW die Physik auf ihrem eigenen Thread das Update um ein Vielfaches langsamer machte, siehe
„Thread-Nummer ohne Thread-lokale Variable unter Windows“ in [`Optimierungen.md`](Optimierungen.md). `ragdoll-3` ändert
an der Physik ebenfalls nichts, Clips und Aufnahmen laufen nur für Figuren, die sie spielen:
Gepaart gegen `ragdoll-2` auf dem 7800X3D, je zwei Läufe im Wechsel mit 2 Threads, enden alle Szenen des Benchmarks mit
denselben Zahlen an Bruchstücken, Schutt, Körpern und Kontakten, und ein Frame kostet −0,5 bis 0 %, bei einer Streuung
von Lauf zu Lauf bis 3 %. Nur der Höchststand des Speichers in der gestreamten Stadt, je Bild abgelesen, während im
Hintergrund Häuser laden, lag in einem der Läufe 1 MB höher. `ragdoll-2` ändert an der
Physik nichts, die Balance läuft nur für Figuren, die sie einschalten: Gepaart gegen `ragdoll-1` auf dem 7800X3D, je zwei
Läufe im Wechsel mit 2 Threads, enden alle Szenen des Benchmarks mit denselben Zahlen, und ein Frame kostet −1,5 bis
+3 %, bei einer Streuung von Lauf zu Lauf bis 5 %. Der Benchmark misst dasselbe wie vorher: Nebenan benutzte bis dahin
keine Gelenke, und die Ergänzung in Box3D ändert nur Gelenke, deren Feder eine Drehmomentgrenze hat. Gepaart gegen
`kollaps-10` auf dem 7800X3D, je zwei Läufe im Wechsel mit 2 Threads, enden alle Szenen des Benchmarks mit denselben
Zahlen an Bruchstücken, Schutt, Körpern, Kontakten und Etagen, und ein Frame kostet 0,1 bis 1 % mehr, bei einer Streuung
von Lauf zu Lauf bis 3 %.

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

## Figuren auf Box3D (`ragdoll-1`)

Gebaut am 24. Oktober 2026 nach Kapitel 27, 32 und 33 der Roadmap des Spiels, siehe [`Referenz`](Referenz/README.md),
die Schritte 1 bis 4 aus Kapitel 33.4. Was gebaut ist und wie es sich verhält, steht im README unter „Figuren“, „Benutzung“
und „Grenzen“, die Abnahme in `test/test_articulation.c`, die Demo-Szene „Figuren“ zeigt es: drei Humanoide vor einer
Ziegelwand, einer steht, einer spielt den Clip, einer fällt passiv, das Gewehr trifft sie, und die Pose, zu der die Gelenke
ziehen, liegt als dünnes dunkles Skelett über den Knochen.

Zwei Dinge hat der Bau gezeigt, die in der Roadmap des Spiels nicht stehen. Erstens gibt Box3D die Steifigkeit einer
Gelenkfeder in Hertz gegen die Trägheit ihrer beiden Körper an: Dieselben Hertz machen am schweren Oberschenkel eine
starke Feder und am leichten Fuß eine schwache. Mit 15 Hz in allen Gelenken und 3 Hz für die Hilfe, die das Becken
aufrecht hält, kippte der Humanoid schon nach 15 N s auf die Brust, sein Knöchel brachte bei 0,19 rad Abweichung rund
19 N m auf, und der Ellbogen hing 0,25 rad unter seinem Ziel. Das Motorprofil nennt die Steifigkeit deshalb in N m je
Radian, und die Bibliothek rechnet sie je Gelenk in Hertz um. Mit 800 N m je Radian steht die Figur 20 N s, und der
Ellbogen trifft sein Ziel. Zweitens dehnen sich die Gelenke bei harten Aufschlägen: Schlägt ein Knochen mit einigen
Metern pro Sekunde auf, dehnt sich sein Gelenk um 4 bis 6 cm, bis der Löser es in den folgenden Schritten zurückzieht,
gemessen an Knöchel und Hals. Mit einer Dämpfung der Gelenke von 5 statt 2 geben die Grenzen von Knie und Ellbogen dabei
um rund 0,1 statt bis zu 0,37 rad nach. Box3D begrenzt die Gelenke auf ein Viertel der Teilschritte, 60 Hz, steifer geht
es ohne mehr Teilschritte nicht. In einer Matrix aus Selbstkollision, Dämpfung der Gelenke und Dämpfung der Körper
schwankte die Dehnung zwischen 19 und 61 mm, ohne Richtung.

| Nr. | Schritt | Stand |
| ---: | --- | --- |
| F1 | **Bestand prüfen** (33.4 Schritt 1, 32.7 P0): Box3D hat Dreh-, Kugel- (Kegel und Twist), Schweiß-, Motor-, Parallel-, Filter-, Abstands-, Schiebe- und Radgelenke, Warmstart, Federn mit Zielwinkel oder Zielrotation, Geschwindigkeitsmotoren mit Drehmomentgrenze und Aufnahmen mit Wiedergabe. Es fehlte die Drehmomentgrenze der Federn, Nebenan benutzte keine Gelenke, RagdollBridge und Pose-Graph gab es nicht | ✅ |
| F2 | **Federn mit Drehmomentgrenze** in Box3D, `maxSpringTorque` und das Drehmoment der Feder zum Ablesen | ✅ siehe „Federn der Gelenke mit Drehmomentgrenze“ in [`extern/README.md`](../extern/README.md) |
| F3 | **Pendel, Drei-Link-Arm, passive Ragdoll** (27.4, Stufen 1 bis 3 aus 27.14) | ✅ Tests: Anker des Pendels 0,5 mm, Grenzen gehalten; der Arm erreicht seine Winkel auf 0,03 rad; die Ragdoll aus 0,5 m dehnt ihre Gelenke beim Aufprall bis 4 cm, die Grenzen geben bis 0,07 rad nach, sie kommt zur Ruhe |
| F4 | **Figuren in der Bibliothek**: Rig, Articulation, Motorprofil in N m je Radian, Clips aus Keyframes im Update gesampelt, Treffer genau einmal, Zielpose und Motorlast zum Zeichnen, Humanoid aus 15 Knochen | ✅ `include/nebenan/types.h`, `include/nebenan/nebenan.h`, `src/articulation.c` |
| F5 | **Standpose, Clip, Treffer** (33.4 Schritte 2 bis 4) | ✅ Tests: steht 5 s mit 0,004 rad Abweichung; der Drei-Keyframe-Clip läuft ohne Sprung, Fehler bis 0,04 rad, der Ellbogen erreicht 1,00 rad; 20 N s auf die Brust: steht, Gelenke 4 mm gedehnt; 120 N s: der Rumpf gibt 0,18 rad nach, die Figur fällt |
| F6 | **Wand**: eine passive Figur mit 20 m/s bricht eine Wand über die Kollisionsschäden der Welt (26.16, 27.11) | ✅ Test |
| F7 | **Determinismus**: zwei Figuren mit Clip und Treffer mit 1 und 4 Threads bitgleich | ✅ Test |
| F8 | **Balance und Stützschritt** (33.4 Schritt 5, 27.9, 28.3): Schwerpunkt, Stützpolygon, Schrittziel, Knockdown und Recovery mit Übergabe an die Steuerung ohne Sprung (28.4) | ✅ auf `ragdoll-2`, siehe „Balance“ unten |
| F9 | **Gelenkdehnung beim Aufprall**: 4 bis 6 cm für wenige Schritte. Bitgleich zu beheben nur mit mehr Teilschritten für die Welt oder einer Projektion der Gelenke nach dem Schritt in Box3D; vorher messen, was ein Spiel davon sieht | 🔜 |
| F10 | **Kosten messen** (27.14): eine Stufe des Profils für die Figuren, Szenen mit 1, 10 und 50 Figuren im Benchmark und in der Demo, p50 bis p99 wie bei den Trümmern | ✅ auf `ragdoll-4`, siehe „Kosten der Figuren“ unten |
| F11 | **Keyframe-Format und Video-Editor** (32.5, 32.6, 33.4 Schritt 6): erst nach F8 | ✅ auf `ragdoll-3`, siehe „Clips und Keyframe-Editor“ unten |

## Balance (`ragdoll-2`)

Gebaut am 24. Oktober 2026 nach Kapitel 26.13 bis 26.15, 27.9 und 28.3 bis 28.5 der Roadmap des Spiels, Schritt 5 aus
Kapitel 33.4. Was gebaut ist und wie es sich verhält, steht im README unter „Balance“, „Benutzung“, „Parameter“ und
„Grenzen“, die Abnahme in `test/test_balance.c`. In der Demo-Szene „Figuren“ halten die beiden stehenden Figuren ihre
Balance, der Capture Point und das Ziel eines Schritts liegen als Marker auf dem Boden.

| Nr. | Schritt | Stand |
| ---: | --- | --- |
| B1 | **Messen** (27.9, 26.14): Schwerpunkt, Fußkontakte auf tragfähigem Boden, Stützfläche als Hülle der Sohlen, Capture Point und sein Abstand zur Stützfläche | ✅ |
| B2 | **Stand**: Beine per IK zu den Füßen, Becken als gedämpfte Feder über der Mitte der Füße, bei weitem Stand tiefer, übriger Körper in der Pose des Programms | ✅ Test: 3 s ohne Schritt, Schwerpunkt wandert 12 mm |
| B3 | **Ausgleichsschritt** (28.3, 26.14): Ziel aus dem vorhergesagten Capture Point, Reichweite, Boden, Platz und Weg geprüft, Ziel folgt in den ersten 60 % des Schritts, neue Stützfläche nach der Landung geprüft, bis zu drei Schritte | ✅ Tests: 60 N s von vorn, 50 N s von hinten, 60 N s von der Seite werfen die Figur ohne Balance um, mit Balance steht sie nach zwei Schritten |
| B4 | **Links und rechts** (28.5 A): gespiegelte Stöße, gespiegelter erster Schritt | ✅ Test: 65 N s, Ziel auf 1 cm gespiegelt, gleiche Zahl Schritte |
| B5 | **Blockierter Schritt** (28.5 B): kein Schritt in eine Wand, kein Fuß durch sie | ✅ Test: Wand in 32 cm, kein Fuß berührt sie, über eine kniehohe Mauer fällt die Figur |
| B6 | **Boden weg** (26.14): kein Schritt ins Leere, die Figur fällt | ✅ Test: fällt nach 0,2 s |
| B7 | **Fallen mit Schutzhaltung** (26.13): Hilfe für das Becken lässt los, weiche Motoren, Arme voraus oder eingerollt | ✅ |
| B8 | **Aufstehen mit Sicherheitsprüfung** (26.15, 28.4, 28.5 E): Ruhe, Boden, Platz, kein Schutt; Kraft an der Brust als Spielhilfe; Übergabe erst, wenn der Schwerpunkt über den Füßen ruht | ✅ Tests: nach vorn und hinten umgeworfen steht die Figur nach 5,7 und 6,2 s wieder und bleibt stehen; unter einer Platte wartet sie, ohne sie zu berühren |
| B9 | **Abbruch** (26.15): ein Treffer oder Schutt bricht das Aufstehen ab | ✅ Test: Treffer sofort, ein fallender Block nach 0,18 s |
| B10 | **Ohne Sprung** (28.4): jede neue Art von Pose blendet ein | ✅ Test: kein Knochen mehr als 9,3 cm in einem Schritt |
| B11 | **Determinismus**: 1 und 4 Threads bitgleich, der Hash für alle Plattformen festgehalten, nur Rechenarten mit gleichen Bits überall | ✅ Test |
| B12 | **Aufstehclips** statt der Kraft an der Brust (26.15, 33.4 Schritt 6), Kriechen und Rollen, wenn kein Platz ist | 🔜 der Editor dafür steht seit `ragdoll-3`, die Clips fehlen |
| B13 | **Schrägen und Stufen**: Füße auf die Neigung des Bodens drehen, Stufen testen | 🔜 |
| B14 | **Kosten** der Balance je Figur messen, mit F10 | ✅ auf `ragdoll-4`: 0,8 bis 0,9 µs je Figur und Schritt in der ruhigen Stadt, 1,1 bis 2,0 µs unter Beschuss |

Fünf Dinge hat die Abstimmung gezeigt. Erstens schwingt der Fuß, den die IK vom aufrechten Becken aus führt, am Ziel
vorbei, sobald ein Stoß das Becken kippt, und vom Becken aus, wie es ist, schaukelt sich der Schwung über das Rollen des
Beckens selbst auf. Halb dazwischen trifft er am besten: Von 16 Stößen fing die Balance so alle ab, ganz vom Becken aus
14, ganz vom aufrechten 13. Zweitens hob ein fast gestrecktes Standbein die Figur in einen Hüpfer, sobald das Becken
höher stand als gewollt, und im weiten Stand zogen Beine, die ein zu hohes Becken nicht erreichten, die Füße vom Boden.
Die Beine halten das Becken deshalb tiefer, während es steigt, und nie mehr als 2 cm unter dem, wo es ist, und im weiten
Stand so tief, dass beide Füße unten bleiben: Hüpfer gab es danach in 104 Stößen keinen mehr, vorher in 16. Drittens
landete ein Fuß, der unterwegs am Boden schleifte, zu früh und hob wieder ab, und die Figur fiel bei 40 N s, während sie
35 und 45 N s abfing. Seit ein Fuß nur langsam landet, fängt sie in jeder Richtung bis zu einer festen Stärke ab und
fällt darüber, ohne Ausreißer dazwischen. Viertens dreht die Hilfe für das Becken einen liegenden Körper nicht auf: Ihre
Feder rechnet Box3D gegen die Trägheit des leichten Beckens, sie brachte gemessen 150 bis 250 N m statt der erlaubten
500 N m. Die Kraft hebt deshalb die Brust, nicht das Becken, und der Körper richtet sich an ihr hängend auf. Fünftens
hielten Hüftmotoren am Anschlag das Becken gekippt, solange die IK belastete Füße unter die Hüfte ziehen wollte, sie
bleiben deshalb stehen, wo sie stehen.

Verworfen, mit Grund:

- **Steifere Motoren für die Schritte**: Mit 1600 statt 800 N m je Radian fing die Balance 77 statt 79 von 104 Stößen
  ab, mit einer Grenze von 250 statt 150 N m dazu 66, mit 2400 N m je Radian und 300 N m 58. Der Fuß verfehlt sein Ziel
  nicht aus Schwäche der Motoren.
- **Das Becken im Schritt absenken**, damit das Bein sein fernes Ziel erreicht: 39 und 36 statt 79 von 104 Stößen, die
  Figur sank zu tief und fiel.
- **Schwungbahn schneller als der Schritt**, der Fuß über dem Ziel nach 75 % der Zeit: 6 von 16 Stößen statt 14, er
  überschwang das Ziel um bis zu 30 cm. Ebenso ein Becken, das um die Verzögerung der Motoren vorausgerechnet wird, 7 von
  16, und eine Höchstgeschwindigkeit für den Fuß, 7 von 16.
- **Aufstehen über Knien und Aufsitzen**: Auf Kniehöhe gehalten, drehte die Hilfe für das Becken den liegenden Körper
  nicht auf, die Figur blieb liegen, bis die Zeit ablief. Aufstehen geht deshalb über die volle Höhe.

## Clips und Keyframe-Editor (`ragdoll-3`)

Gebaut am 24. Oktober 2026 nach Kapitel 31.2, 32.5 bis 32.8 und 33.2 der Roadmap des Spiels, Schritt 6 aus Kapitel
33.4. Was gebaut ist und wie es sich verhält, steht im README unter „Clips als Dateien“, „Szenen“, „Benutzung“,
„Parameter“ und „Grenzen“, die Abnahme in `test/test_pose_clip.c`. Die Demo-Szene „Keyframes“ ist der Editor.

| Nr. | Schritt | Stand |
| ---: | --- | --- |
| K1 | **Format** (32.6): Skelett, Name, Bildrate, Knochennamen, Keyframes mit Status, Herkunft, Bild des Videos, Wurzel, Kontakten, Drehungen und Sicherheit, Ereignisse, getrennt nach Hand, berechnet und simuliert, als JSON mit Version | ✅ Test: ein Clip aus 3 Keyframes und 2 Ereignissen kommt aus der Datei bitgenau zurück, acht kaputte Dateien melden Zeile und Grund |
| K2 | **Freigeben und Sperren** (32.5, 32.8 Punkt 4): Vorschläge, Aufnahmen und Korrekturen an Nachbarn ändern freigegebene Keyframes nicht, Korrekturen verbessern die Vorschläge in der Nähe | ✅ Test: von drei Vorschlägen zwei übernommen, einer vom freigegebenen Keyframe abgelehnt; eine Korrektur von 0,8 rad dreht den Vorschlag 15 Frames weiter um 0,051 rad |
| K3 | **Prüfung** (31.2, 32.5, 33.2 C): Knochen, Normierung, Gelenkgrenzen, Unsicherheit, Boden, Sprünge, rutschende Kontakte über den ganzen Kontakt, gleich für Hand, Schätzer und Assistent | ✅ Test: jede Art einmal, ein Fuß, der langsam rutscht, alle 3 cm gefunden |
| K4 | **Keypoints** (32.5): COCO 17 und MediaPipe 33 zu einer Pose des Humanoiden, unpassende Knochenlängen gemeldet, Keypoint-Dateien als Vorschläge | ✅ Test: dieselbe Pose auf fünf Stellen zurück, ein Unterarm 35 % zu lang gefunden, eine um 10 % kleinere Person ohne Fund |
| K5 | **Spielen und Aufnehmen** (33.2 D und E, 33.5): ein Clip aus der Datei auf einer Figur, die Aufnahme Frame für Frame in den Schritten mit Kontakten und den Wechseln der Balance, die Abweichung vom Clip | ✅ Tests: bitgleich wie `nbClipDef`, kein Knochen mehr als 3,2 cm in einem Schritt; 49 Frames in 97 Schritten, mit 1 und 4 Threads und den Schritten auf eigenem Thread derselbe Text, 0,039 rad hinter dem Clip, der Sturz bei Frame 15 |
| K6 | **Editor** (32.7 P4, 33.2 E): Referenzbilder, Keyframes anlegen, freigeben und entsperren, Pose und Kontakte mit Reglern, drei Ansichten, Vorschläge laden, Prüfung, Simulation mit Motoren und Balance, Backen und Speichern | ✅ Demo-Szene „Keyframes“ |
| K7 | **Gizmos und Ansichten nebeneinander**: Gelenke im Bild ziehen statt mit Reglern, zwei Blickwinkel zugleich für die Prüfung aus 32.5 | 🔜 |
| K8 | **2D-Keypoints**: Tiefe aus mehreren Ansichten oder von einem Schätzer, der 3D liefert | 🔜 |
| K9 | **Assistent im Editor** (32.7 P4): Vorschläge anfragen statt als Datei laden | 🔜 nach K7 |
| K10 | **Ereignisse und Kontakte als Intervalle** im Editor setzen (33.2 C), Phasen mit eigenem Timing (33.2 B) | 🔜 |
| K11 | **Binärformat** für Clips im Spiel (32.6), das JSON bleibt für den Editor | 🔜 wenn Clips groß werden |

Drei Dinge hat der Bau gezeigt. Erstens nahm der Editor zuerst in jedem gezeichneten Bild auf: Der gebackene Clip hing
damit an der Bildrate und an der Last des Rechners, und mit der Physik auf ihrem eigenen Thread hätte das Menü den Clip
gelesen, während der Schritt in ihn schreibt. Die Aufnahme läuft deshalb in der Bibliothek, im Update nach dem Schritt,
jeder Frame auf dem Schritt, der ihm am nächsten liegt, und das Programm liest sie zwischen den Schritten. Zweitens fand
die Prüfung rutschende Füße nur von Keyframe zu Keyframe: Ein Fuß, der in einer Aufnahme je Frame 1 cm rutscht, blieb
immer unter den 3 cm und fiel nie auf. Sie misst deshalb von der Stelle, an der der Kontakt begann. Drittens sind die
Winkelfunktionen von Box3D Näherungen: Eine Drehung aus Achse und Winkel lag im Keypoint-Import 3,3 mrad daneben. Wo eine
Pose genau zurückkommen soll, baut die Bibliothek sie aus Achsen statt aus Winkeln, und Knie und Ellbogen kommen auf
fünf Stellen genau zurück.

Verworfen, mit Grund:

- **Aufnehmen im Programm**, je gezeichnetem Bild: hängt an der Bildrate und liest den Clip, während der Schritt ihn
  schreibt, siehe oben.
- **Ein Teil der Korrektur per NLerp** für die Nachbarn: NLerp gibt nicht den Teil des Winkels, den das Gewicht verlangt,
  je größer die Korrektur, desto weniger. Die Bibliothek nimmt den Teil des Winkels um dieselbe Achse.

## Kosten der Figuren (`ragdoll-4`)

Gebaut und gemessen am 24. Oktober 2026 nach Kapitel 27.14, Stufe 6 der Messmatrix, und 32.8 der Roadmap des Spiels.
Das Profil der Bibliothek hat eine Stufe für die Figuren im Update mit ihren Teilen: Clips samt Aufnahmen, Balance und
die Ziele der Gelenke (`nb_profileFigures`). `nbWorld_GetFigureStats` sagt, was die Figuren einer Welt tun: wie viele
stehen, austreten, fallen, liegen und aufstehen, wie oft sie gestürzt und aufgestanden sind, wie oft Aufstehen abbrach,
wie viele Knochen wach sind und wie viele Motoren an ihrer Grenze arbeiten. Der Benchmark hat die Stadt aus 16 Häusern
mit doppelter Bruchstückgröße mit 0, 1, 10 und 50 Figuren in den Straßen, ruhig und unter einer Granate alle 1/12 s
(`nebenan_benchmark figures`). Die Figuren haben Balance und spielen einen Clip, und jede, die steht, bekommt alle 6 s
einen Stoß, abwechselnd einen, den sie mit Schritten abfängt, und einen, der sie umwirft. In der Demo stehen in der Stadt
auf Wunsch 1, 10 oder 50 Figuren, das Menü zeigt, was sie tun und kosten, und „Parallel gegen nacheinander messen“ misst
sie mit. Die Abnahme steht in `test/test_balance.c`.

Gemessen auf dem Ryzen 7 7800X3D mit MinGW und 2 Threads, Mittel aus zwei Läufen, Zeiten in ms pro Schritt von 1/60 s,
die Figuren im Update in µs je Figur:

| Stadt | Figuren | Bild p50 | p95 | p99 | p99,9 | Max | Box3D | Update | Figuren je Figur |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| ruhig | 1 | 0,03 | 0,03 | 0,04 | 0,06 | 0,11 | 0,03 | 0,002 | 1,3 µs |
| ruhig | 10 | 0,24 | 0,26 | 0,28 | 0,32 | 0,34 | 0,21 | 0,013 | 1,2 µs |
| ruhig | 50 | 0,96 | 1,05 | 1,07 | 1,14 | 1,17 | 0,87 | 0,073 | 1,4 µs |
| unter Beschuss | 0 | 1,49 | 2,49 | 3,12 | 3,66 | 6,01 | 1,35 | 0,129 | – |
| unter Beschuss | 1 | 1,45 | 2,70 | 3,45 | 4,31 | 4,93 | 1,38 | 0,130 | 2,7 µs |
| unter Beschuss | 10 | 1,67 | 2,79 | 3,32 | 4,75 | 6,31 | 1,51 | 0,149 | 1,8 µs |
| unter Beschuss | 50 | 2,44 | 3,59 | 4,26 | 6,44 | 6,58 | 2,22 | 0,214 | 1,6 µs |

Die Figuren kosten vor allem in Box3D. In der ruhigen Stadt brauchen 50 Figuren 0,87 ms im Schritt, 17 µs je Figur, fast
alles im Löser, für 750 Gelenke mit der Hilfe, die das Becken aufrecht hält, und rund 850 Kontakte. Im Update der
Bibliothek kosten sie 1,2 bis 1,4 µs je Figur, davon rund zwei Drittel die Balance, ein Fünftel die Ziele der Gelenke
und ein Achtel die Clips. Unter Beschuss sind es 1,6 bis 2,7 µs, denn öfter fallen, liegen und stehen Figuren auf, und
die Balance liest dann die Kontakte aller Knochen und prüft den Platz zum Aufstehen. Mit 50 Figuren steigt das mittlere
Bild unter Beschuss von 1,5 auf 2,4 ms und das 99. Perzentil von 3,1 auf 4,3 ms. Mit 1 und 10 Figuren gehört nur ein
Teil des Unterschieds zu ihnen: Sie stehen den Trümmern im Weg, und die Zerstörung verläuft anders, am Ende liegen
0,8 bis 1,9 % mehr Bruchstücke. Je Figur kommen rund 45 KB in Nebenan und Box3D dazu. Was die Figuren tun, ist von Lauf zu
Lauf bitgleich: In der ruhigen Stadt stürzen die 50 in den 20 s 58-mal, stehen 29-mal wieder auf, und zweimal bricht
Aufstehen ab, unter Beschuss sind es 60, 28 und 6, die Trümmer brechen das Aufstehen öfter ab. Zwischen zwei Sitzungen
schwankte der Schritt der ruhigen Stadt mit 50 Figuren um ein Drittel, zwischen den beiden gepaarten Läufen um 0,1 %.

In der Demo, mit MSVC und 8 Threads auf der RTX 4070 SUPER, gemessen mit „Parallel gegen nacheinander messen“ beim Abriss
der Stadt mit 50 Figuren in den Straßen: Die Figuren kosten im Update 0,13 ms je Schritt, 2,5 µs je Figur, davon 0,075 ms
die Balance, 0,038 ms die Ziele der Gelenke und 0,013 ms die Clips. Ein Bild kostet parallel 4,2 und nacheinander 4,6 ms,
parallel ist 8,6 % schneller, ohne Figuren sind es 3,6 und 3,9 ms und 5,9 %. Ganz den Figuren gehört der Unterschied nicht,
mit ihnen kommt der Abriss in 4 000 Bildern mit 862 statt 711 Granaten ein Haus weiter. Am Ende stehen 22 der 50, 25
liegen, 30-mal sind sie gestürzt. In einer ersten Messung mit Figuren kostete ein paralleler Lauf 10,5 ms pro Bild bei
derselben Zeit der Simulation, die Zeit ging also außerhalb der Physik verloren, und die Wiederholung lief glatt.

Beim Messen in der Demo fiel auf, dass im Build mit MinGW mit der Physik auf ihrem eigenen Thread das Update um ein
Vielfaches langsamer war, auch ohne Figuren: beim Abriss der Stadt 7,8 statt 0,37 ms je Schritt, 50 Figuren 2,27 statt
0,07 ms. Jeder Zugriff aus dem Schritt-Thread fragte nach der Nummer des eigenen Threads, und die liegt bei MinGW in einer
emulierten Thread-lokalen Variable. Unter Windows nimmt die Nummer jetzt die Kennung des Threads, siehe
„Thread-Nummer ohne Thread-lokale Variable unter Windows“ in [`Optimierungen.md`](Optimierungen.md). Mit MSVC, wie
`build.bat` und die CI bauen, war es nie so.

| Nr. | Schritt | Stand |
| ---: | --- | --- |
| C1 | **Profil**: eine Stufe für die Figuren im Update, mit Clips, Balance und Zielen der Gelenke, je Figur gemessen, ohne die Reihenfolge der Figuren zu ändern | ✅ |
| C2 | **Was die Figuren tun** (32.8): Zustände der Balance, Stürze, Aufstehen und Abbrüche, wache Knochen, Motoren an der Grenze, Gelenke | ✅ Test: drei Figuren gezählt, ein Sturz, ein Abbruch durch einen Treffer und ein Aufstehen |
| C3 | **Benchmark** (27.14 Stufe 6): die Stadt mit 0, 1, 10 und 50 Figuren, ruhig und unter Beschuss, p50 bis p99,9, Box3D, Speicher | ✅ |
| C4 | **Demo**: die Stadt mit 1, 10 oder 50 Figuren, im Menü und im Bericht der Messung | ✅ |
| C5 | **Thread-Nummer unter Windows** ohne Thread-lokale Variable, für MinGW | ✅ |
| C6 | **Kontakte der Figuren** (27.13 A): 17 Kontaktpaare je stehender Figur, welche davon zwischen ihren eigenen Knochen liegen und ob ein Filter für ungeeignete Paare den Löser entlastet | ⛔ geprüft auf `ragdoll-5`: Die Hälfte der Paare liegt zwischen den Knochen einer Figur, sie kosten aber nur 4 % ihrer Zeit in Box3D, siehe „Kontakte der Figuren“ unten |
| C7 | **Aktivierungsbudget und LOD** (27.13 B): ferne Figuren ohne Balance oder ruhend, wenn Box3D zu viel kostet | 🔜 erst bei Bedarf |
| C8 | **Wohin der Löser bei Figuren rechnet**: rund 11 µs je Figur und Schritt im Löser, für 15 Gelenke mit Federn, Grenzen und der Hilfe für das Becken und für die Kontakte mit dem Boden. Erst getrennt messen, etwa mit Figuren ohne Grenzen oder ohne Boden, dann entscheiden | ✅ auf `ragdoll-6`: 83 % in den Gelenken, 5 % in allen Kontakten, und ein waches Gelenk nimmt der ganzen Welt die Pakete, siehe „Wohin der Löser bei Figuren rechnet“ unten |
| C9 | **Gelenke in den Paketen**: Box3D rechnet Gelenke nur nach Farben mit einer Sperre je Stufe, und ein einziges waches Gelenk schaltet die Pakete für alle Inseln ab. Die Gelenke einer Insel in ihrem Paket rechnen, Farbe für Farbe vor ihren Kontakten wie mit Sperren, damit die Ergebnisse bitgleich bleiben | 🔜 hoch: 19 bis 25 % des Schritts der Trümmer in jeder Welt mit Figuren, dazu die Figuren selbst ohne Sperren |
| C10 | **Gelenke billiger**: rund 1 µs Rechenzeit je Gelenk und Schritt, skalar, während die Kontakte breit rechnen. Erst messen, welche Art Gelenk was kostet, Kugel mit Kegel, Verdrehung und Feder, Scharnier, die Hilfe für das Becken | 🔜 nach C9 |

## Kontakte der Figuren (`ragdoll-5`)

Geprüft am 24. Oktober 2026 nach Kapitel 27.13 A der Roadmap des Spiels: Filter für ungeeignete Paare, nachdem die
tatsächlichen Kosten der Kontakte gemessen sind. Die Knochen einer Figur, die kein Gelenk verbindet, kollidieren
miteinander (`nbRigDef::selfCollision`), Eltern und Kinder nicht. In der Ruhepose überlappt kein solches Paar: Am
nächsten liegen Unterarme und Becken und Hände und Oberschenkel, mit 2 cm Luft, mehr als der spekulative Abstand von
Box3D. Box3D legt für sie trotzdem Kontaktpaare an, sobald sich ihre Hüllquader samt Rand überlappen, und prüft jedes
Paar in jedem Schritt in der Kollision. `nebenan_benchmark figures 2 noself` rechnet die Städte mit Figuren ohne
Kollisionen zwischen den Knochen einer Figur. Gemessen auf dem Ryzen 7 7800X3D mit MinGW und 2 Threads, je zwei Läufe im
Wechsel, die ruhige Stadt mit 50 Figuren:

| 50 Figuren, ruhige Stadt | Kontaktpaare höchstens | Box3D-Schritt | Kollision | Löser |
| --- | ---: | ---: | ---: | ---: |
| mit Kollisionen zwischen den Knochen | 851 | 0,615 ms | 0,040 ms | 0,552 ms |
| ohne | 423 | 0,588 ms | 0,023 ms | 0,544 ms |

Die Paare zwischen den Knochen einer Figur sind die Hälfte aller Kontaktpaare, eine einzelne Figur hat im Fallen und
Liegen bis zu 13 davon. Sie kosten aber nur 0,027 ms für 50 Figuren, 0,5 µs je Figur und Schritt oder 4 % ihrer Zeit in
Box3D, fast alles in der Kollision, denn die meisten berühren sich nie, und der Löser rechnet an ihnen kaum. Ein Filter
nur für ungeeignete Paare brächte weniger als das. Er bräuchte eine Filterfunktion von Nebenan an der Box3D-Welt des
Programms, die Box3D nur einmal je Welt kennt, oder Filtergelenke, die selbst im Löser mitlaufen, und er änderte die
Bewegung: Ohne die Kollisionen enden die Figuren der ruhigen Stadt anders, mit 3 statt 2 abgebrochenen Versuchen
aufzustehen und 16 statt 10 Motoren an der Grenze, und Glieder gehen durcheinander durch. Die Balance müsste neu
abgestimmt werden. Die Zeit der Figuren in Box3D geht in den Löser, rund 11 µs je Figur und Schritt, siehe C8.

Verworfen, mit Grund:

- **Ungeeignete Paare filtern** (27.13 A): höchstens 4 % der Zeit der Figuren in Box3D, ein Filter nur für einen Teil der
  Paare weniger, dafür eine andere Bewegung und eine neue Abstimmung der Balance, siehe oben.

## Wohin der Löser bei Figuren rechnet (`ragdoll-6`)

Gemessen am 25. Oktober 2026, C8 oben. Der Löser von Box3D misst jetzt in jedem achten Schritt, wie viel Rechenzeit seine
Threads für Gelenke, Kontakte und das Integrieren der Körper brauchen, ohne das Warten an den Sperren, siehe „Wohin der
Löser rechnet“ in [`extern/README.md`](../extern/README.md), und `nbWorld_GetProfile` gibt das weiter
(`nbProfile::solverJoints`). Der Benchmark der Figuren zeigt es für jede Stadt, dazu eine Stadt ohne Figuren mit einem
einzigen Pendel weit weg. Gemessen auf dem Ryzen 7 7800X3D mit MinGW, je zwei Läufe, Rechenzeit in ms je gemessenem
Schritt über alle Threads:

| Stadt | Threads | Gelenke | Kontakte | Körper | Bedingungen, Uhrzeit |
| --- | ---: | ---: | ---: | ---: | ---: |
| ruhig, 50 Figuren | 2 | 0,760 | 0,043 | 0,115 | 0,481 |
| ruhig, 50 Figuren | 8 | 0,803 | 0,049 | 0,120 | 0,264 |
| unter Beschuss, ohne Figuren | 2 | – | 0,507 | 0,112 | 0,321 |
| unter Beschuss, ein Pendel | 2 | 0,003 | 0,542 | 0,152 | 0,408 |
| unter Beschuss, 50 Figuren | 2 | 1,179 | 0,685 | 0,360 | 1,188 |

Bei den Figuren rechnet der Löser fast nur an ihren Gelenken: 83 % seiner Rechenzeit, 12 % das Integrieren der Körper
und 5 % alle Kontakte, mit dem Boden und zwischen den Knochen. Das sind rund 15 µs je Figur und Schritt für ihre 14
Gelenke und die Hilfe, die das Becken aufrecht hält, rund 1 µs je Gelenk über die vier Teilschritte. Die Gelenke rechnet
Box3D skalar, die Kontakte breit mit SIMD. Die Kontakte mit dem Boden kosten fast nichts.

Teurer für eine zerstörbare Welt ist ein Nebeneffekt: Box3D rechnet Gelenke nur nach Farben mit einer Sperre je Stufe,
und ein einziges waches Gelenk schaltet die Pakete aus „Ganze Inseln pro Thread“ für die ganze Welt ab. In der Stadt unter
Beschuss ohne Figuren kostet ein Pendel weit weg den Box3D-Schritt mit 2 Threads 25 % mehr, 0,97 statt 1,21 ms, den Löser
24 % und das mittlere Bild 1,08 statt 1,40 ms. Mit 8 Threads sind es 19 % im Schritt, 0,38 statt 0,45 ms, 41 % im Löser
und 1,20 statt 1,29 ms im 99. Perzentil. Mit 2 Threads wird auch die Kollision 27 % langsamer, mit 8 nicht, warum, ist
noch offen. Mit 8 Threads rechnen die Pakete übrigens mehr Rechenzeit als die Farben, 0,84 gegen 0,58 ms, sind aber in der
Uhrzeit schneller, 0,13 gegen 0,21 ms, denn ohne Sperren wartet niemand. Jede Welt mit Figuren verliert so ein Fünftel bis
ein Viertel des Schritts ihrer Trümmer, mehr als die Figuren selbst kosten, solange es wenige sind. Das ist C9.

Die Messung kostet nichts Messbares: Gepaart gegen `ragdoll-5` auf dem 7800X3D, je zwei Läufe im Wechsel mit 2 Threads,
enden alle Szenen des Benchmarks mit denselben Zahlen, und ein Frame kostet 0 bis +0,9 %, bei einer Streuung von Lauf zu
Lauf bis 4 %. Die erste Fassung las in den Paketen die Uhr an jedem ihrer vielen kleinen Blöcke: Mit 8 Threads
rechneten die Pakete der Stadt ohne Figuren dann 0,90 statt 0,84 ms, und die Uhr machte sie in der Messung langsamer, als
sie sind. Jetzt misst sie ein Paket im Ganzen.

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
  siehe „Übrige Pakete im Team anders verteilen“. Seit `kollaps-8` zählt der Löser die Stufen des Teams: Auf dem Ryzen
  wartet es mit 8 Threads ein Viertel seiner Zeit nach der letzten Stufe, auf das Paket einer zweiten großen Insel, das
  ein Thread allein rechnet, siehe Schritt 34 und 35. Seit `kollaps-9` rechnet das Team ein solches Paket mit, sobald es
  mehr als das Doppelte des Anteils eines Threads hält: Auf dem Ryzen kosten die Bedingungen mit 8 Threads damit 19 %
  weniger und ein Bild 6 %, siehe Schritt 36.
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
| 29 | **Überlauf an Knoten auf mehreren Threads**: die Bündel eines Knotens in einer festen Zahl von Gruppen zugleich auf mehreren Threads rechnen, jede gegen eigene Kopien des Knotens, danach der Mittelwert, wie „Knoten im Überlauf mit geteilter Masse“ über die Spuren hinaus, ändert die Ergebnisse | mit den Einstürzen bei ×2 bis zu 0,3 ms pro Schritt, rund 10 % eines Bilds mit 8 Threads | 25 | ✅ auf `kollaps-7`: Liegen an den Knoten großer Inseln mindestens 256 Kontakte, rechnen die Threads ihre Bündel in acht Gruppen gegen eigene Kopien. Beim Abriss auf der VM bei ×2 kostet der Überlauf auf dem aufrufenden Thread mit 2 und 4 Threads 19 und 42 % weniger, die Bedingungen 9 und 11 %, mit einem Thread pro Kontakt 17 % mehr. Der Abriss verläuft anders, in 4 000 Bildern fallen 6 statt 4 Häuser. Schwere Teile liegen wie vorher und kommen öfter zur Ruhe. Auf dem Ryzen mit 8 Threads am Überlauf 63 % weniger, siehe 30 |
| 30 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-7` beim Abriss bei ×2 | zeigt, was 29 mit 8 Threads bringt | 29 | ✅ am 23. Oktober: Mit 8 Threads rechnet der aufrufende Thread am Überlauf 0,16 statt 0,43 ms pro Schritt, die Bedingungen kosten 19 % weniger, ein Bild dauert 2,80 statt 2,96 ms, obwohl der Abriss weiter ist, und mit 8 Threads bleiben 54 statt 60 % auf einem Thread. Mit einem Thread kostet ein Kontakt an Knoten 30 % mehr |
| 31 | **Wartezeit des Teams auf eine Stufe zählen**: wie sich mit 8 Threads Arbeit und Warten auf die Stufen der Farben verteilen | Werkzeug, entscheidet, wie es im Löser weitergeht: Jeder Worker wartet im Team 47 % der Zeit, und die Bedingungen haben 0,55 ms seriell | 30 | ✅ auf `kollaps-8`: In jedem achten Schritt mit Team zählt Box3D pro Farbe und für Aufbau, Integration, Knoten und Speichern, wie lange die Stufen offen sind, wie die Threads die Zeit darin verbringen und was der aufrufende Thread zwischen ihnen allein rechnet, bitgleich. Beim Abriss auf der VM bei ×2 mit 4 Threads rechnen die Threads in den offenen Stufen 65 bis 66 % an Blöcken und 18 bis 19 % an den übrigen Paketen, 12 bis 13 % warten sie nach ihrem letzten Block und 3 % davor. Dort tritt keine der Regeln unter „Was als Nächstes in Frage kommt“ ein, auf dem Ryzen nachzumessen, siehe 34. Auf dem Ryzen tritt nur „Allein“ ein |
| 34 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-8` beim Abriss bei ×2 | zeigt, wo das Team mit 8 Threads wartet, und entscheidet nach den Regeln unter „Was als Nächstes in Frage kommt“, wie es im Löser weitergeht | 31 | ✅ am 24. Oktober: Mit 8 Threads tritt nur die Regel „Allein“ ein. Der aufrufende Thread rechnet 35 % der Zeit des Teams allein, 26 % davon nach der letzten Stufe. Dort wartet er, auf der VM gezählt, auf das Paket einer zweiten großen Insel, das ein Worker allein rechnet, siehe 35 |
| 35 | **Große Pakete im Team**: Das Team rechnet nicht nur das größte Paket in seinen Stufen, sondern jedes, das mehr als den Anteil eines Threads hält, bitgleich | geschätzt mit 8 Threads rund 15 % der Zeit des Teams, 3 bis 4 % eines Bilds, mit 4 Threads nichts | 34 | ✅ auf `kollaps-9`: Mit dem größten Paket rechnet das Team jedes weitere, das mehr als das Doppelte des Anteils eines Threads hält, bitgleich. Mit dem einfachen Anteil kostete das Team auf der VM mit 4 Threads in den Schritten mit zweitem Paket 5,6 % mehr, denn ein Thread im Team rechnet nur etwas mehr als halb so schnell wie allein. Mit 4 Threads hält eine zweite Insel nie das Doppelte, auf der VM ändert sich nichts. Mit 8 Threads nimmt das Team beim Abriss nach den Inseln auf der VM in rund 40 % seiner Schritte ein zweites Paket mit, auf dem Ryzen nachzumessen, siehe 36. Dort kosten die Bedingungen mit 8 Threads 19 % weniger und ein Bild 6 % |
| 36 | **Auf dem Ryzen nachmessen**: „Skalierung messen“ mit `kollaps-9` beim Abriss bei ×2 | zeigt, was 35 mit 8 Threads bringt und ob die Schwelle passt | 35 | ✅ am 24. Oktober, `kollaps-8` und `kollaps-9` direkt nacheinander: Mit 8 Threads kostet das Team 0,724 statt 0,894 ms pro gemessenem Schritt, nach der letzten Stufe bleiben 0,001 statt 0,229 ms, die Bedingungen kosten 19 % weniger, ein Schritt 8 % und ein Bild 2,62 statt 2,78 ms, 6 % weniger. Mit 1, 2 und 4 Threads gleich. Von den Regeln tritt keine mehr ein, weiter mit 32 |
| 32 | **Aufbau des Lösers auf den Workern**: die Teile des Aufbaus, die nicht voneinander abhängen, parallel, bitgleich | höchstens 0,1 ms pro Schritt, rund 4 % eines Bilds mit 8 Threads | 30 | 🔜 als Nächstes, erst messen, welche Teile es sind |
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
