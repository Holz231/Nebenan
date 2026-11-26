---
name: figure-round
description: How a round of work on the steered figure of Nebenan is done (active ragdoll - standing, walking, running, turning, jumps of feet, shins, pelvis and hands). Use it whenever a round active-ragdoll-N starts or continues, whenever the gait, the feet, the legs or the root of a figure are changed in src/figure.c, whenever something is measured with tools/measure, when a rule in docs/Regeln is written or checked, and right after a context reset in the middle of a round. It holds the order of the steps, the pattern of the pre-registered rule, the map of the figure's code and the mistakes already made once.
---

# Eine Runde an der Figur

Eine Runde macht die gesteuerte Figur an einer Stelle ruhiger oder richtiger und weist nach, dass nichts anderes
schlechter wird. Sie endet mit einer eigenen Branch `active-ragdoll-N`, grüner CI, einem Windows-Zip und einem Bericht.
Die nächste Runde beginnt erst, wenn der Nutzer sie freigibt.

Der Stand steht in `docs/Roadmap.md` (letzter Abschnitt, F84 mit dem 🔜-Rest) und in `docs/Optimierungen.md` (offene
🔜-Zeilen, verworfene ⛔-Zeilen). Dieser Skill hält die Methode fest, nicht die Zahlen. Die Werkzeuge beschreibt
`tools/measure/README.md`, die Karte des Codes steht in [references/figur.md](references/figur.md).

## Vier Fragen, an denen keine Runde vorbeikommt

Die Runden 148 bis 150 galten der kleinsten Klasse von Sprüngen, und der Hebel, der ihr Ziel nahm, war in Runde 149
schon gemessen und an der falschen Zahl verworfen. Jede Lehre dazu stand unten schon. Eine Liste von Lehren wird
überlesen: Darum stehen die vier als Fragen an den Stellen des Ablaufs, an denen entschieden wird. Jede braucht eine
Antwort mit Zahl, im Bericht an den Nutzer, bevor es weitergeht.

1. **Vor jedem Vorschlag und jeder Wahl: Wie groß ist das Thema neben den anderen?** `classes150.py` zählt über die acht
   Wechsel im Rennen, was das Bein tut, wo sein Schienbein am härtesten springt, die größte Klasse zuerst. Die Zahl des
   vorgeschlagenen Themas steht im Vorschlag neben denen der anderen. Wer das kleinste vorschlägt, sagt es.
2. **Vor dem Verwerfen einer Variante: Nimmt sie das Ziel der Runde, und greift sie überhaupt?** Jede Variante bekommt
   zwei Zahlen, die an den Stellen der Ursache und die der Tabelle ohne die Starts (`table150.py`). Was das Ziel nimmt
   und woanders kostet, wird einmal zusammen mit dem Hebel gebaut, der dort hilft. Was nichts ändert, greift vielleicht
   nicht: das Gewicht oder die Bedingung an der Stelle ausgeben.
3. **Vor dem Festschreiben der Regel: Nimmt der Prototyp das Ziel der Runde?** Nimmt er es nur halb oder gar nicht,
   steht das mit Zahl beim Nutzer, bevor die Regel geschrieben wird, und die Runde heißt nach dem, was er wirklich
   nimmt.
4. **Nach der Prüfung: Ist die Szene der Runde besser geworden?** Die frischen Sätze je Szene ansehen (`table150.py` mit
   `SETS` und `FACING`) und im Bericht sagen, was mit dem Ziel der Runde geschah, auch wenn alle Bedingungen erfüllt
   sind.

Zu allen vier gehört: Starts aus dem Stand zählen einmal je Satz. Ihre 24 Stellen sind gleich, und in Runde 149 waren
die Hälfte des gemeldeten Gewinns und alles, was „kippte“, dieser Block.

## Ablauf

1. **Lesen.** Roadmap und Optimierungen. Was dort mit ⛔ steht, ist schon versucht: nicht noch einmal, außer der Grund
   ist weggefallen. Dann die Rangliste, Frage 1.
2. **Arbeitsplatz.** Ein Arbeitsbaum auf der neuen Branch vom letzten Stand, ein zweiter als Versuchsbaum (detached,
   bekommt die Schalter), ein dritter als sauberer alter Stand für Kosten und Benchmark. Die cmake-Bauten des alten
   Stands behalten, sie sind der Vergleich. Der Hauptcheckout bleibt unberührt.
3. **Ursache verfolgen, bevor etwas gebaut wird.** Die härtesten Stellen aus den Ergebnissen der letzten Prüfung holen,
   eine davon mit `lab101` nachfahren und Update für Update ansehen (`jumps119.py`, `lift121.py`, eine kleine Tabelle in
   Python). Erst wenn im Lauf zu sehen ist, welche Größe springt und welche Zeile sie setzt, steht die Ursache. Eine
   Vermutung ist keine: In Runde 121 und 122 waren je zwei naheliegende Vermutungen falsch. `leg150.py` zeigt ein Bein
   Update für Update mit der Länge von Hüfte zu Knöchel.
4. **Varianten mit Schaltern.** Im Versuchsbaum hinter `#ifdef NB_X_…`, eingebaut von einem kleinen Python-Skript im
   Scratchpad, gebaut mit `build.sh name "-DNB_X_…"`. Zuerst an der verfolgten Stelle, dann auf den Sätzen der
   Entwicklung (`sweeps.sh`, `facing119.py`, `fixed121.py`). Jede verworfene Variante mit ihrer Zahl notieren, sie kommt
   in die Optimierungen. Die ganze Tabelle fährt `devtable.sh`, `table150.py` vergleicht darauf zwei Stände je Szene und
   ohne die Starts. Vor dem Verwerfen Frage 2.
5. **Stetigen Gang prüfen.** Läufe stetig gehend, rennend (3,0, 4,5, 4,8 m/s), seitwärts und rückwärts mit `cmp` gegen
   den alten Stand. Ein Eingriff für Wechsel soll den stetigen Gang bitgleich lassen, sonst ist er größer als gedacht.
6. **Toleranzen und Zielgröße.** `tolerance<N>.py` auf dem alten Stand. Die Zielgröße so wählen, dass der Gewinn auf den
   Entwicklungssätzen deutlich über der Streuung zwischen ähnlichen Sätzen liegt, siehe Lehren. Jede Toleranz bekommt
   eine Untergrenze, Zählungen 2 Stellen, härteste Sprünge und Summen 2 mm: Eine Toleranz von null macht aus der letzten
   Stelle eine verfehlte Bedingung, siehe Runde 152.
7. **Frische Geschwindigkeiten.** `tools/measure/speeds.py v …` sagt, ob ein Wert je gelaufen ist, und rechnet die
   Spreizung über einen Zyklus aus der Kadenzformel, ohne einen Lauf. Neue Sätze in `common.sh`.
8. **Regel festschreiben.** `docs/Regeln/RULE<N>.txt`, `tol<N>.json`, `check<N>.sh`, `eval<N>.py`, als eigener Commit
   ohne die Bibliothek, und pushen, bevor irgendein Lauf mit den frischen Geschwindigkeiten existiert. Der Push gibt der
   Regel einen Zeitstempel außerhalb der Maschine. Davor Frage 3. Und davor die Befehle zählen: `cost_count.sh` mit dem
   Prototyp als neuem Stand, damit ein Preis an den Kosten vor dem Festschreiben auf dem Tisch liegt und nicht nach der
   Prüfung, siehe Runde 151.
9. **Sauberer Bau** in `src/`, mit Kommentar, bitgleich zum Prototyp: dieselben JSONs auf mindestens zwei Sätzen.
10. **Unabhängige Prüfung.** `check<N>.sh alt neu`, dann `eval<N>.py alt neu`. Das Ergebnis gilt, wie es kommt. Danach
    Frage 4, mit Antwort im Bericht.
11. **Tests.** `tests.sh` zeigt geänderte Hashes. Ein neuer Test bewacht die Ursache: Seine Zahl auf dem neuen und auf
    dem alten Stand messen und beide in den Kommentar schreiben. Zeigt er kaum einen Unterschied, kommt das Ereignis
    in seinen Momenten nicht vor: mit `lab101` die Momente absuchen. Dann cmake mit gcc und clang, alle Tests, und der
    Benchmark der Figuren mit 1, 2 und 4 Threads.
12. **Kosten, Benchmark, Sanitizer.** `cost_pairs.sh`, gleich danach `cost_count.sh`, dann die Paare mit clang,
    `bench_pairs.sh` zweimal (r und s), `sanitize.sh`. Dabei läuft nichts anderes auf der Maschine.
13. **Doku.** Abschnitt in der Roadmap (Ursache, was gebaut ist, Versuchsbaum, Entwicklung, Regel, Tabelle der
    Bedingungen, nach Szenen, Tests, Kosten, Benchmark, zur Regel, verworfen, offen), Eintrag in F84 und neuer 🔜-Text,
    Zeilen in den Optimierungen (✅, je verworfener Variante ⛔, der Rest als 🔜, die alte offene Zeile abschließen),
    ein Satz im README mit der neuen Zahl der Tests, die Zeile in `tools/measure/README.md`. Nichts löschen.
14. **Abschluss.** Skill `library-first`, `git diff --stat`, Commit, Push, CI abwarten, Zip der Windows-Demo laden und
    seine Prüfsumme mit der von GitHub vergleichen, Bericht mit „Kurz:“ und einem Vorschlag für die nächste Runde. Der
    Vorschlag nennt die Rangliste, Frage 1.

## Die Regel

- **A Gewinn:** Die Zielgröße ist auf den frischen Geschwindigkeiten um mehr als ihre Toleranz besser.
- **B und C Schutz**, mit festem Blick und mit Blick in Laufrichtung: Jede Größe höchstens alt plus Toleranz. Stellen
  über den Grenzen, Größe der Sprünge (Mittel und oberes Zehntel), härtester Sprung, Becken tief, gekreuzte Füße,
  Antwort der Kapsel, Zehen und Fersen, Flüge.
- **D Sonst:** Treffertabelle Byte für Byte gleich, stetige Läufe, Tests und Sanitizer, Kosten, Benchmark, vorwärts
  rennend.
- **Toleranz** ist die größte Streuung zwischen Sätzen, deren Geschwindigkeiten höchstens 5 % auseinanderliegen, auf
  dem alten Stand gemessen. Zählungen und Summen über zwei Sätze bekommen das Doppelte.
- **Kosten** sind die Befehle, die `nbUpdateFigures` ausführt, höchstens 0,5 % mehr. Zeiten stehen daneben.
- **Benchmark** ab Runde 125: Endzustände gleich und nicht mehr Befehle in Szenen, die der Eingriff nicht berührt.
  Zeiten stehen daneben.
- Was wiederholt werden darf, steht vorher in der Regel. Eine Messung, die nicht passt, wird nie wiederholt, bis sie
  passt. Verfehlt der saubere Bau eine Bedingung, wird nicht gebaut, oder es steht offen in der Roadmap, warum doch,
  und der Nutzer entscheidet.

Grenzen der Messgrößen: Sprung ist die zweite Differenz des Ursprungs eines Knochens je Update. Füße, Schienbeine,
Zehen und Fersen 37 mm, Hände 15 mm, Becken 10 mm, Becken tief ab 15 cm unter dem Stand, Flug ab 0,2 s.

## Lehren

- **Zeit messen streut mehr als kleine Grenzen.** Zwei Binaries desselben Quelltexts liegen je nach Compiler und Modus
  bis 0,5 µs auseinander. Kosten deshalb an Befehlen messen (Runde 120).
- **Auch der Benchmark streut mehr als seine Grenze.** Zwei Binaries liegen in den Bildzeiten im Median 6 bis 9 %
  auseinander, auch in Szenen, an deren Code sich nichts ändert (Runde 124). Den Benchmark deshalb an Endzuständen und
  Befehlen prüfen, die Zeiten daneben festhalten. Kosten, die daher kommen, dass die Figur anderes tut, nach Funktionen
  aufteilen (`callgrind_annotate` auf `out/cost/cg`): Sie sind der Eingriff selbst, nicht sein Preis.
- **Seltene harte Sprünge zeigen sich nicht in Zählungen.** Die Summe dessen, was über die Grenze hinaus springt,
  wiegt Größe mit und streut wenig (Runde 122).
- **Blöcke von 24 gleichen Stellen.** In den Szenen aus dem Stand sind alle 24 Stellen gleich und kippen gemeinsam
  über eine Grenze. Eine Zählung, die um 24 springt, ist ein Wert knapp neben der Grenze, kein breiter Schaden.
- **Rückwärts ist anders.** Rückwärts gehende Beine landen auf den Zehen und rollen sofort. Was für vorwärts gehende
  Beine gilt, erst getrennt prüfen (`side`, `stepBack`).
- **Felder, die zwischen den Updates jemand anders setzt.** `footRoll` setzt auch der Gang, das zuletzt gegebene
  Rollen ist `footRolled`. `swingAlong` ist in `nbSteerFeet` schon weitergezählt, wenn der Schwung gerechnet wird. Vor
  einem „Wert des letzten Updates“ alle Zuweisungen des Felds suchen.
- **Nicht aufhören lassen, sondern weiterlaufen lassen.** Wer eine Größe auf einmal begrenzt, erzeugt den Sprung an
  anderer Stelle: der Fuß, der auf einmal nicht mehr steigt (Runde 121). Die Änderung des letzten Updates fortsetzen.
- **Eine Konstante ohne Ordnung ist keine.** Ändert ein Wert das Ergebnis mal so, mal so, kommt er nicht hinein.
- **Was die Wurzel hergibt, muss das Bein holen und umgekehrt.** Rollen, Senken der Wurzel und Strecken des Beins
  decken dieselbe Not. Wer eines bremst, sieht nach, was die anderen dafür tun (Becken tief, Becken über 10 mm).
- **Macht eine Korrektur etwas anderes häufiger, ist oft das andere der Fehler.** Die Fassungen für das Becken in
  Runde 123 ließen den Lauf dreimal so oft in einen Galopp fallen. Der Galopp war ein eigener, alter Fehler im Takt.
  Erst ihn verfolgen, dann die Runde neu schneiden: den kleinen sicheren Teil bauen, den Rest als Ursache festhalten.
- **Ein Ziel kann eine Eigenschaft sein.** Tritt ein Fehler selten, aber in jedem Satz auf, heißt die Bedingung „keine
  Stelle, und vorher mindestens eine“ statt „um mehr als die Toleranz besser“ (Runde 123). Vorher zählen, ob der alte
  Stand ihn wirklich in jedem Satz hat.
- **Den Ort suchen, an dem die Entscheidung fällt.** Ein Fuß, der falsch landet, wird nicht am Boden repariert,
  sondern im Schwung, der die Landung vorbereitet (Runde 124 nach vier verworfenen Fassungen in Runde 123). An jeder
  Landung messen, was den guten vom schlechten Fall trennt (`land124.py`), bevor eine Schwelle gewählt wird.
- **Vorwärts heißt zweierlei.** `stepBack` sagt, wohin ein Schwung ging, `legBack`, wohin die Beine jetzt gehen.
  Wechselt die Figur dazwischen die Richtung, gilt für denselben Fuß das Gegenteil: Ein Eingriff für vorwärts prüft
  beide.
- **Eine Lage in ihre Richtungen zerlegen, bevor gewählt wird.** Die Schultern des Modells lagen innen, höher und
  hinten: Nur hinten kostete Stürze, innen sparte welche (Runde 126). Jede Richtung einzeln auf der Tabelle der
  Entwicklung messen, bevor Lagen zur Wahl stehen, nicht erst nach der verfehlten Prüfung.
- **Am Körper zuerst die Treffertabelle, nach Richtungen.** Eine gesteuerte Figur ist kinematisch: Was an Armen und
  Rumpf hängt, ändert ihren Gang nicht, wohl aber jede Figur, die sich fängt. `falls126.py` teilt die Stürze danach,
  wohin der Treffer schiebt. Mehr Stürze in einer Richtung und weniger in der anderen: Der Schwerpunkt ist gewandert.
- **Die Entwicklung unterschätzt die Prüfung.** 1,2 % mehr Stürze auf der Tabelle der Entwicklung wurden 2,13 % in der
  Prüfung, am meisten bei 30 Hz (Runde 126). Liegt die Entwicklung über der halben Grenze, ist die Grenze nicht sicher,
  und das steht vor der Wahl beim Nutzer.
- **Die Körper folgen der Pose des Updates davor.** Eine Zeile von `lab101` hält die Körper nach dem Schritt und die
  Felder der Figur nach dem Update: Ein Sprung im Update des Abhebens gehört zur letzten Pose am Boden (Runde 128). An
  zwei Größen prüfen, die zusammengehören, etwa Rollen und Höhe der Ferse, welche Zeile zu welcher Pose gehört.
- **Erst zählen, welcher Moment das härteste macht.** In den Kehrtwenden steckten vier Ursachen in einer Zahl.
  `turn128.py` teilt jede Stelle danach, was das Bein tut, wo sein Schienbein springt. Die Zielgröße einer Runde sind
  die Stellen ihrer Ursache: Die Summe aller streute so viel, wie der Eingriff nahm (Runde 128).
- **Eine Vermutung, die zum Lauf passt, ist noch nicht die Zeile.** Dass der Schwung aus dem Stand anläuft, passte zur
  Spur und war falsch. Ein Ausdruck hinter einem Schalter im Versuchsbaum, der je Update zeigt, was eine Größe will und
  bekommt, entscheidet es in einem Lauf (Runde 128).
- **Die Tests des Repositorys laufen mit dem Prototyp, bevor die Regel festgeschrieben wird.** In Runde 129 fielen drei
  Treppen-Tests erst nach der unabhängigen Prüfung auf: Die Messszenen sind eben, die Bremse hielt auf der Treppe den
  Fuß zurück. `tests.sh` mit dem Versuchsbaum als `NB_SRC` gehört vor Schritt 8, und was ein Eingriff auf Treppen, am
  Hang und unter Clips tut, steht dann in der Regel statt in einem Nachtrag.
- **Der Gewinn der Entwicklung hängt an ihren Geschwindigkeiten.** Auf dem schnellen Satz der Entwicklung lagen 24
  Stellen der Zielgröße von Runde 129 nur 0,6 mm über der Grenze, und die härtesten Landungen waren dort härter als auf
  den frischen Sätzen: 376 mm Gewinn dort, 135 mm in der Prüfung, bei 133 mm Toleranz. Vor dem Festschreiben nachsehen,
  wie viel der Zielgröße knapp über der Grenze liegt, und die Entwicklung auf mehr als zwei Sätzen messen.
- **Stetig gleich heißt ab der fünften Sekunde gleich.** Ein stetiger Lauf beginnt mit Losgehen oder Losrennen, und das
  ist ein Wechsel. Wo ein Eingriff für Wechsel den stetigen Gang nicht ändern soll, die Zeilen ab 5 s vergleichen, und
  die Stärke des Eingriffs so wählen, dass er im stetigen Gang nie greift: Der stetige Gang gibt die Grenze vor (Runde
  129).
- **Die Grenze zum stetigen Gang wird gesucht, nicht geschätzt.** Soll ein Eingriff den stetigen Gang nicht anfassen,
  seine Stärke mit `steady130.py` über das ganze Tempo suchen, 231 Läufe und mit `FINE=1` das schnelle Rennen in
  Schritten von 0,01 m/s, und erst dann einen Schritt über die Grenze gehen. In Runde 130 stand die Stärke auf 17
  Läufen, die unabhängige Prüfung fand einen stetigen Lauf, der sich änderte, und die Runde brauchte eine zweite Regel.
  Die Geschwindigkeiten der Prüfung vorher festlegen und aus der Suche heraushalten.
- **Eine alte Stärke nicht nebenbei nachziehen.** Die Suche von Runde 130 fand auch an der Bremse aus Runde 129 zwei
  stetige Läufe, die sich ändern. Sie schwächer zu stellen hätte einen guten Teil ihres Gewinns gekostet: erst den Preis
  messen, dann den Befund mit dem Preis dem Nutzer vorlegen, nicht in die laufende Runde mischen.
- **Ein neues Skript einmal trocken laufen lassen.** `check130b.sh` war nicht ausführbar, die Prüfung startete nicht,
  und der Abschlusslauf lief ohne sie los. Neue Skripte mit `chmod +x` anlegen und den Rückgabewert der Prüfung
  abwarten, bevor Kosten und Benchmark starten.
- **Die Zielgröße muss den Eingriff zeigen können.** Ändert ein Eingriff nur wenige Stellen, erst messen, wie stark die
  geplante Zielgröße zwischen ähnlichen Sätzen streut, und das mit dem Gewinn der Entwicklung vergleichen, bevor die
  Regel steht. In Runde 131 streute die Summe der Schienbeine um 51 mm bei 14 bis 34 mm Gewinn an 1 bis 2 von 24
  Stellen. Gezählt wurde stattdessen das Ereignis selbst, `hops131.py`: Es streut um 3 und fiel von 23 auf 0.
- **Dreht ein Eingriff eine Entscheidung um, früh die ganze Tabelle fahren.** Den eben gelandeten Fuß stehen zu lassen
  nahm den Schienbeinen Sprünge und gab sie dem Becken, in Szenen, die nicht die der Runde waren. Die Tabelle mit Blick
  in Laufrichtung zeigte es, eine verfolgte Stelle daraus gab die Schwelle. Greift ein Schalter gar nicht, die
  Entscheidung an der Stelle mit allen ihren Größen ausgeben lassen, statt zu raten.
- **Den Probelauf der Prüfung auf alten Sätzen fahren.** Eine Kopie von `check` und `eval` auf Sätze der Entwicklung
  umgeschrieben und einmal ganz durchlaufen lassen, bevor die Regel festgeschrieben wird: In Runde 131 hatte die
  Auswertung beim Umschreiben alle Schutzbedingungen verloren, und der Probelauf fand es. Frische Geschwindigkeiten des
  Gehens gibt es zwischen 1,05 und 1,68 m/s auf den Zentimeter nicht mehr, halbe Zentimeter nehmen.
- **Eine alte Schutzregel erst mit einem Schalter auf der ganzen Tabelle lockern.** Vor Regel, Toleranzen und frischen
  Geschwindigkeiten: die Stelle ausgeben lassen, die Variante bauen, die Szene der Runde und alle Szenen mit festem
  Blick fahren, mit den gekreuzten Füßen. Runde 132 wollte die Lücke zwischen den Füßen lockern und war so in einer
  Stunde entschieden: schlechter auf jeder Größe, nichts gebaut, keine frische Geschwindigkeit verbraucht. Eine Runde
  darf mit „gemessen, nicht gebaut“ enden, die Zahlen kommen in die Liste.
- **Vor der Wahl einer Runde die Klassen über alle Sätze zählen.** `where129.py` sagt je Stelle, was das Bein tut, wo
  sein Schienbein am härtesten springt. Über 19 Sätze gezählt war ein abhebendes Bein an 418 von 1 209 Stellen das
  härteste, mehr als die Hälfte der Summe. Runde 129 hatte dasselbe an einer Stelle gesehen und „niedrig“ genannt: Eine
  verfolgte Stelle sagt, was geschieht, nicht, wie oft.
- **Schadet die ganze Stärke an anderer Stelle, erst die Stärke abtasten.** Der sanfte Anlauf beim Bremsen nahm mit
  ganzer Stärke 174 von 732 mm und machte den Seitenwechsel schlechter, mit 0,25 bis 0,5 davon 220 mm und keine Szene
  schlechter. Das Plateau zeigt, wo die Zahl liegen darf; gewählt wird die Mitte zwischen den Werten, die nicht mehr
  halten.
- **Den guten Fall daneben messen.** Derselbe Fuß hebt im stetigen Rückwärtsrennen ohne Ruck ab. Eine Tabelle beider,
  wie weit das Bein beim Abheben gestreckt ist und wie schnell es davor und danach länger wird, zeigte den Unterschied
  in einem Lauf: 0,94 und weiter länger gegen 0,98 und kürzer (Runde 134). Wo ein Wechsel hart ist und der stetige Gang
  nicht, zuerst dieser Vergleich.
- **Alle Ereignisse vermessen, bevor eine Variante gebaut wird.** Ein Ausdruck im Versuchsbaum an der Stelle der
  Entscheidung, hier welche Bedingung den Fuß hebt, und ein Skript, das ihn mit den Zeilen von `lab101` zusammenführt,
  über alle Stellen von vier Szenen auf fünf Sätzen: 3 340 Abheber, zwei Klassen mit verschiedener Ursache. Eine
  verfolgte Stelle hätte nur eine gezeigt. Aus dem Skript wurde `lifts134.py`.
- **Varianten im Stapel.** Mehrere Schalter in einem Lauf bauen und vier Varianten zugleich messen: 23 Varianten in fünf
  Stapeln, eine Wartezeit je Stapel. Den Versuchsbaum erst wieder ändern, wenn die Bauten eines Stapels fertig sind.
- **Vor der Regel rechnen, wie oft sie den Gewinn zeigen würde.** Aus den Sätzen der Entwicklung alle Paare aus einem
  schnellen und einem langsamen Satz bilden und je Paar den Gewinn des Prototyps gegen die Toleranz halten: Die Summe
  über 37 mm hätten 72 von 90 Paaren gezeigt, die Zahl der Stellen 25. Was den Gewinn nicht in fast allen Paaren zeigt,
  wird Schutz, und die Zielgröße läuft auf mehr Paaren mit entsprechend größerer Toleranz, die frischen Sätze im Mittel
  so schnell wie die der Entwicklung.
- **Die Toleranz aus dem Probelauf lesen, nicht aus der Datei.** `tol<N>.json` hält für Zählungen mit Blick in
  Laufrichtung die Toleranz eines Satzes, `eval<N>.py` verdoppelt sie für zwei. In Runde 134 sah eine Bedingung des
  Gewinns mit 5 Stellen sicher aus und war mit 10 zu knapp.
- **Die Raster der Suche verbrauchen Geschwindigkeiten.** `steady130.py` läuft mit `FINE=1` jede Geschwindigkeit von
  4,30 bis 4,80 m/s in Schritten von 0,01 außer denen in `FRESH`. Werden danach weitere frische Sätze gebraucht, liegen
  sie auf halben Zentimetern oder unter 4,30 m/s.
- **Wo ein Merkmal umschlägt, beide Zweige für denselben Fuß vergleichen.** Der Sprung von Runde 135 kam in dem Update,
  in dem die Beine von vorwärts auf rückwärts wechselten (`legBack`): Derselbe Fuß fiel von dem Zweig mit dem Blick
  voraus in einen ohne, und sein Rollen ging zurück. Ein Ausdruck, der je Update zeigt, woraus das Rollen besteht, fand
  es in einem Lauf, und `flips135.py` zählt jeden Fuß an einem solchen Umschlag.
- **Den Test auf das Fenster der Ursache legen.** Die erste Fassung des Tests von Runde 135 nahm die erste halbe Sekunde
  nach dem Wechsel, und das Härteste war alt und neu fast gleich, 54,6 und 53,3 mm: Spätere Abheber, die der Eingriff
  nicht berührt, überdeckten es. Die Momente mit `lab101` abgesucht, lag das Ereignis bei 0,18 bis 0,2 s; mit der ersten
  Viertelsekunde trennt der Test 31,9 von 54,6 mm.
- **Vor einer Eigenschaft als Ziel alle Auswahlen zählen.** „Keiner neu, alt mindestens einer“ hält nur, wenn der alte
  Stand auf den frischen Sätzen sicher einen hat. In Runde 135 hatte er ihn in jeder der 10 080 Auswahlen von drei
  schnellen und drei langsamen Sätzen der Entwicklung, im Median 8, und der Prototyp auf keinem der 19 Sätze, das
  härteste 32 mm.
- **Die Toleranzen liegen vor dem Probelauf neben den Regeln.** `eval<N>.py` liest `docs/Regeln/tol<N>.json`, nicht
  `out/tol<N>.json`. Der Probelauf von Runde 135 fand das; die Datei wird kopiert, sobald `tolerance<N>.py` gelaufen
  ist.
- **Wo die Summe den Gewinn nicht zeigt, das Merkmal der Ursache suchen.** In Runde 136 nahm der Eingriff je Satzpaar im
  Median 13 mm bei 12,6 mm Streuung zwischen ähnlichen Sätzen. Die Drehung des schwingenden Fußes zur Ferse je Update
  trennte dagegen scharf: der Prototyp höchstens 0,157 rad, der alte Stand bis 0,215 rad. Sie wurde die Zielgröße, die
  Summe der Schienbeine nur ihre Richtung.
- **Das härteste Beispiel trägt das Merkmal nicht immer.** Der verfolgte Schwung von Runde 136 drehte seinen Fuß nur
  0,124 rad je Update, dafür drei Updates nacheinander, und über mehrere Updates drehen gewöhnliche Schwünge genauso
  weit. Darum steht neben einem Merkmal je Update eine Bedingung an den Schienbeinen derselben Schwünge.
- **Jede Zahl im Regeltext gegen das „allowed“ des Probelaufs halten.** In Runde 136 nannte der Regeltext für zwei Sätze
  mit Blick in Laufrichtung die Toleranz eines Satzes, 5 Stellen; `eval<N>.py` rechnet das Doppelte, 10, wie in Runde
  135. Die Prüfung erfüllte beide Lesarten, aber die Unstimmigkeit steht offen in der Roadmap. Vor dem Festschreiben
  jede Grenze des Texts mit der Ausgabe des Probelaufs vergleichen.
- **Zwei Größen eines Schwungs, die aus verschiedenen Quellen kommen, gegeneinander halten.** In Runde 137 kam die Höhe
  eines Schwungs von der Feder des Gangs, seine Form vom Rennanteil des Gangs beim Abheben. Solange beide gleich sind,
  im stetigen Gang, passt das; fällt der Gang beim Bremsen schneller als die Feder, bekam ein gehender Schwung die Höhe
  des Rennens. Wo eine Größe aus der Feder und eine andere aus dem Gang kommt, die Fälle suchen, in denen beide
  auseinanderlaufen.
- **Eine Korrektur für das Bremsen nur beim Bremsen.** Die Höhe immer nach dem kleineren Rennanteil nahm in Runde 137
  die Abheber, aber wer beim Wiederbeschleunigen abhob, hob zu niedrig für die Landung im Rennen: 647 statt 229 mm an
  diesen Landungen. Begrenzt auf den Rückstand der Figur hinter ihrem Gang (`slower`) blieb davon nichts. Das Maß
  `braking` von Runde 134 taugte dafür nicht, es ist mit dem Rennanteil multipliziert und dort schon null.
- **Neue Skripte sofort ausführbar machen.** `proto137.sh` entstand mit `sed` aus dem Skript der Runde davor und war
  nicht ausführbar; der Hintergrundlauf scheiterte still, und eine Viertelstunde war verloren. Nach jedem `sed > datei`
  folgt `chmod +x` und ein Blick auf `ls -l`.
- **Ein Eingriff kann den Ruck verlagern statt nehmen.** In Runde 138 nahm der ganze Zug den Sprung des Schienbeins fast
  ganz, aber der Fuß sprang dafür selbst, wenn er mit dieser Geschwindigkeit abhob: mit festem Blick Füße an 12 statt 6
  Stellen über 37 mm. Wer an den Schienbeinen misst, sieht dann nur den Gewinn. Bei jedem Eingriff, der eine
  Geschwindigkeit setzt, die Tabelle mit festem Blick samt Füßen ansehen, bevor die Stärke steht.
- **Jede Bedingung eines Eingriffs einzeln wegnehmen und messen.** Der Zug von Runde 138 hat vier: rückwärts gehende
  Beine, gegen die Richtung gesteuert, die Linie zur Hüfte und die Grenze der Bremse. Jede einzeln weggenommen ergab
  eine eigene Zahl (Seitenwechsel 1 201 statt 24 mm, Anhalten 90 statt 79 mm, 184 statt 128 mm), und eine fünfte, das
  Bremsen, fiel weg, weil es ohne sie besser wurde. So steht jede Bedingung mit ihrem Grund in der Regel.
- **Gleichnamige Größen im Versuchsbaum prüfen.** Eine Fassung von Runde 138 griff nie, weil sie `stretch` nahm, und das
  war an dieser Stelle eine andere Größe als die Streckung des Beins, die erst weiter unten berechnet wird. Ein
  Schalter, der Stelle für Stelle gleich misst wie alt, ist verdächtig: zuerst nachsehen, ob er überhaupt greift.
- **Einen verlagerten Ruck an der Stelle erkennen, nicht an der Klasse.** In Runde 139 wurden die späten Landungen mit
  einem längeren Schwung scheinbar ruhig, 29 statt 113 mm, aber derselbe Ruck kam ein Update früher, als der Fuß noch
  als schwingend galt, und `where129.py` zählte ihn als Schwung, 234 statt 2 mm. Wo eine Klasse fällt und eine
  benachbarte steigt, die Stelle selbst Update für Update ansehen.
- **Eine Grenze, die sich aus dem Update davor fortschreibt, braucht ein Ende.** `NB_X_LG` in Runde 139 nahm die Rate,
  mit der das Bein im Update davor kürzer wurde, und zog jedes Update mehr ab: Das Bein wurde immer schneller kürzer,
  bis der Fuß landete, und Schienbeine sprangen über 1 m. Vor dem Messen prüfen, wo eine solche Grenze aufhört.
- **Ein seltenes Ereignis braucht frische Sätze, auf denen es vorkommt.** Der Fuß, der in Runde 140 sofort wieder abhob,
  kam nur in einem Band von 4,365 bis 4,415 m/s und an einer Stelle des Zyklus vor; auf sechs gewöhnlichen frischen
  Sätzen hätte die alte Fassung ihn wahrscheinlich nicht gezeigt, und die Regel hätte keinen Gewinn sehen können. Vor
  dem Festschreiben nachsehen, wo das Ereignis vorkommt, die frischen Sätze dorthin legen und dichter messen,
  `hops140.py` mit 96 statt 24 Punkten, und in die Regel schreiben, dass alt mindestens ein Fall da sein muss.
- **Ein Test misst nur das Fenster seines Ereignisses.** Der erste `SteerWaitedFootTest` maß das härteste Schienbein
  über die ganzen 3 s nach dem Wechsel und fiel mit dem Prototyp an einer Landung aus Runde 139, 46,4 mm. Das Fenster
  von 0,3 bis 0,55 s trennt das Wiederabheben, 53,0 statt 39,0 mm, von dem, was der Eingriff nicht anfasst.
- **Lange Hintergrundläufe mit langem Zeitlimit starten.** Ein Hintergrundlauf ohne eigenes Zeitlimit endet nach 30
  Minuten; in Runde 140 brach er so mitten im Messen der Toleranzen ab. Läufe, die länger dauern können, mit dem größten
  Zeitlimit starten und ihr Ende prüfen, bevor ihre Ausgabe zählt.
- **Eine Bedingung am gewollten Ziel messen, nicht am schon begrenzten.** In Runde 141 sollten die Beine nur schneller
  drehen, wo sie weit drehen müssen. Am Ziel gemessen, das `nbHoldLegs` schon für die Füße begrenzt hatte, griff die
  Bedingung zu selten, 1,20 bis 1,25 statt 1,29 s; am gewollten Ziel, `legAim`, 1,17 s. Wer eine Schwelle an eine
  Größe hängt, prüft vorher, ob eine andere Stelle diese Größe schon gestutzt hat.
- **Bei schnellerem Drehen Rate und Beschleunigung getrennt messen.** Doppelte Drehrate und doppelte
  Winkelbeschleunigung der Beine kreuzten in Runde 141 die Füße um 2,5 cm; die doppelte Rate allein nicht, und kein Hash
  änderte sich. Bei 180° ist die Beschleunigung die eigentliche Grenze, die Rate fast nicht.
- **In einer kopierten Auswertung neue Größen eigen benennen.** `eval141.py` entstand aus `eval140.py`; die neue
  Bedingung A1 nannte ihre Werte `sa, sb`, und genau so hießen weiter unten die Werte der Schwünge für A9. Der Probelauf
  brach daran ab. Neue Größen bekommen Namen, die es in der Datei noch nicht gibt, und der Probelauf läuft immer ganz
  durch, bevor die Regel steht.
- **Zehen und Fersen schon beim Vergleich der Varianten mitmessen.** In Runde 142 sahen die Werkzeuge der acht Wechsel
  nur Schienbeine, und die Variante sah auf 19 Sätzen gut aus; erst der Probelauf mit allen Bedingungen zeigte Zehen und
  Fersen bis 91 mm. Varianten vergleicht `sweeps.sh` ohne `ONLY`, mit Zehen und Fersen, bevor eine Variante zum Prototyp
  wird.
- **Erst prüfen, ob ein Schalter greift.** In Runde 142 stand ein Faktor auf der falschen Seite der Ungleichung, und
  vier Varianten liefen Stelle für Stelle wie der alte Stand, bis ein Spurlauf zeigte, dass die Bedingung nie wahr
  wurde. Dieselben Ergebnisse an allen Stellen heißen zuerst: Der Eingriff greift nicht.
- **Ein Prototyp, der im Probelauf eine Bedingung verfehlt, wird nicht festgeschrieben.** Dann wird weiter verfolgt oder
  nicht gebaut, wie in Runde 142; die Regel wird nicht an den Prototyp angepasst.
- **Eine Drehung des Fußes Stufe für Stufe verfolgen.** In Runde 143 änderten sich weder das Rollen, das der Schwung dem
  Fuß gibt, noch die Neigung seines Ziels, noch die Drehung aus `nbPlaceFeet`; erst `nbLowerSteered` rollte den
  schwingenden Fuß. Wer eine Drehung sucht, gibt sie nach jeder Stufe aus, die sie ändern kann, bis zu `nbReachAnkle`,
  und rät nicht an der ersten.
- **Die gezielte Größe als Gewinn, die Szene als Schutz.** In Runde 143 nahm der Eingriff die Abheber von Runde 138 auf
  allen Sätzen, aber auf einzelnen langsamen Sätzen sprang später beim Abdruck ein stehendes Schienbein härter. Die
  Summe der Szene war deshalb auf den Sätzen des Probelaufs schlechter, auf den 19 Sätzen besser. Vor dem Festschreiben
  die 19 Sätze nach schnell und langsam getrennt ansehen; was ein anderer Mechanismus später tut, gehört in den Schutz,
  nicht in den Gewinn.
- **Wenn ein Eingriff einen anderen freilegt, zuerst nach dem Überdeckten suchen.** In Runde 144 kostete das Rollen in
  der Luft, das Runde 143 begrenzte, nicht selbst; es hatte nur überdeckt, dass ein Fuß mit dem falschen Rollen abhob.
  Die Ursache lag eine Stufe davor. Wer eine Nebenwirkung zurückholt, verfolgt zuerst, was die alte Stelle zufällig
  ausglich.
- **Wo der Gewinn je Satz streut, mehr frische Sätze prüfen.** In Runde 144 waren Zehen und Fersen auf 13 von 19 Sätzen
  besser, auf 6 schlechter. Statt einer Bedingung auf zwei Sätzen prüfte die Regel zwölf frische Sätze mit einem
  eigenen, schnellen Werkzeug (`walk144.py`); das kostet Minuten und macht die Prüfung belastbar.
- **Felder des Prototyps ohne Schalter in den Header.** In Runde 145 stand ein neues Feld nur unter dem Versuchsschalter
  in `figure.h`; die Tests lesen den Header ohne den Schalter, lasen die Figur mit verschobenen Feldern, und
  `SteerBackTest` zählte keine einzige Landung. Ein Test, der mit dem Prototyp scheitert, zuerst daraufhin prüfen, ob er
  dieselbe Struktur sieht wie die Bibliothek.
- **Ein Zustand, der eine Regel festhält, braucht einen Anfang, der nicht flackert.** In Runde 145 hielt die erste
  Fassung die doppelte Drehrate fest, sobald das Ziel einmal weit wegzeigte; zu Beginn eines Wechsels zeigt es das für
  ein oder zwei Updates, und eine ganze Vierteldrehung lief doppelt so schnell. Erst der Bezug auf den Anfang der
  Drehung (wohin die Beine zu Beginn zeigten) machte daraus eine Kehrtwende.
- **Im Probelauf keine frische Geschwindigkeit, auch nicht für die stetigen Läufe.** In Runde 145 hat die Kopie der
  Prüfung für den Probelauf die Sätze der Entwicklung bekommen, aber die stetigen Läufe von D2 liefen weiter mit den
  frischen Geschwindigkeiten der Regel. Beim Ableiten der Kopie jede Geschwindigkeit ersetzen und mit `speeds.py`
  prüfen, dass der Probelauf nur schon gelaufene nimmt.
- **Eine schnellere Variante zuerst gegen die Tests prüfen, die bewachen, was sie aufweicht.** In Runde 146 war die
  Variante, bei der die Beine mit dem Landeplatz des austretenden Fußes rechnen, die schnellste, und die Tabellen sahen
  gut aus; erst die Tests zeigten, dass die Füße quer zum Becken kreuzen. Lockert eine Variante eine Grenze wie
  `nbHoldLegs`, die Tests dieser Grenze vor der ganzen Tabelle laufen lassen.
- **Nach jedem neuen Feld im Versuchsbaum die Prototypen neu bauen, bevor die Tests laufen.** In Runde 147 kamen Felder
  für eine verworfene Variante in `figure.h` dazu, nachdem der Prototyp gebaut war; die Tests lasen ihn mit dem neuen
  Header, und zehn Tests scheiterten. Ein Test, der den Prototyp liest, braucht denselben Header, mit dem der Prototyp
  gebaut wurde.
- **Eine Größe, die sich gewollt ändert, braucht eine eigene Bedingung statt der Bitgleichheit.** In Runde 147 ändert
  die schmale Spur den stetigen Lauf vorwärts. Die Regel prüft ihn deshalb mit einem eigenen Werkzeug (`track147.py`):
  Spur im Bereich des Menschen, Füße nicht zu nah, keine härteren Sprünge; bitgleich bleiben nur die Läufe, die sich
  nicht ändern sollen.
- **Wenn eine Grenze in beide Richtungen schlechter wird, sitzt die Ursache woanders.** In Runde 148 machte
  `NB_FOOT_ROLL_GAIN` kleiner wie größer das stehende Schienbein härter, ebenso ein längerer Vorausblick. Der Sprung lag
  am fast gestreckten Knie, das jeden Ruck des Knöchels verstärkt; erst der Blick auf alle Körper des Beins
  (Oberschenkel, Schienbein, Fuß) zeigte das.
- **Eine Größe vor dem Bericht auf ihre Rechnung prüfen.** In Runde 148 sah es aus, als rutschte der hintere Fuß im
  stetigen Rennen bis 14 mm je Update; `gait100.py` misst aber die Mitte zwischen Ferse und Zehen, und die wandert, wenn
  der Fuß über die Zehen rollt. Kein Rutschen, nur Rollen.
- **Die verfolgte Stelle zeigt den Mechanismus, nicht, ob ein Hebel wirkt.** In Runde 149 war das Knie an der verfolgten
  Stelle mit einer kürzeren Reichweite im entscheidenden Update weiter fast gestreckt, und die Variante sah verloren
  aus; auf 25 Sätzen und der ganzen Tabelle nahm sie aber rund 500 mm. Eine Variante, die an der Stelle nicht hilft,
  trotzdem auf der Szene messen, bevor sie verworfen wird.
- **Starts aus dem Stand kippen als Block.** Verschiebt ein Eingriff die erste Landung aus dem Stand um ein Update,
  kippen alle 24 Stellen aller Sätze zugleich, in Runde 149 von 272 auf 2 665 mm. Springt eine Szene aus dem Stand
  zwischen benachbarten Stärken hin und her, trifft der Hebel den Start selbst: ihn dort herausnehmen, statt eine Stärke
  zu wählen, die zufällig passt.
- **Das Ziel der Runde bis zum Schluss mitmessen.** In Runde 149 nahm das Rollen, das beim Antritt auch gehend
  vorausschaut, das Ziel fast ganz, das stehende Schienbein 2 statt 31 mm, und wurde an der Summe der ganzen Tabelle
  verworfen; Runde 150 baute dann einen breiten Hebel, der das Ziel auf den frischen Sätzen gar nicht nahm. Jede
  Variante an den Stellen der Ursache und an der Tabelle getrennt bewerten, und nach der Prüfung nach Szenen nachsehen,
  ob die Szene der Runde besser wurde.
- **Zwei Hebel zusammen messen, wenn einer den Preis des anderen decken kann.** Der Vorausblick des Rollens kostete
  allein gehend ins Rennen 10 mm; zusammen mit der kürzeren Reichweite, die dort 47 mm nimmt, blieb davon nichts (Runde
  150). Was einzeln knapp verworfen ist, vor dem Festschreiben einmal zusammen mit dem Prototyp bauen.
- **Vor einer Variante an einem Vorausblick oder Gewicht nachsehen, ob es an der Stelle überhaupt wirkt.** Der längere
  Vorausblick des Rollens in Runde 148 konnte am verfolgten Sprung nichts ändern: Der Gang ging dort noch, und gehend
  ist das Gewicht des Vorausblicks null. Er änderte nur den stetigen Lauf, und die Runde schloss daraus, die Zahlen des
  Rollens seien schon die besten.
- **Bei einem Sprung am Schienbein die Länge des Beins je Update ausgeben.** Ein Bein, das je Update 18 bis 20 mm länger
  wird und in einem Update an seiner Reichweite anhält, ist der Sprung, 2-mal verstärkt am fast gestreckten Knie (Runde
  150). Winkel und Rollen allein zeigten in Runde 148 nur die Folge.
- **Starts aus dem Stand einmal zählen.** Alle 24 Stellen eines Starts sind gleich: Losrennend rückwärts mit 37,6 mm
  waren 456 der 910 Stellen der Tabelle über 37 mm und 272 der 1 532 mm. `starts150.py` fährt jeden Start je Satz
  einmal; in Summen über die Tabelle die Starts getrennt ausweisen. Nachgerechnet mit `table150.py`: In Runde 149 waren
  246 der gemeldeten 496 mm Gewinn dieser Block und alles, was mit Stärke und Form „kippte“, ebenfalls; ohne ihn nahm
  jede Stärke von 0,015 bis 0,03 dasselbe. In Runde 150 war der ganze gemeldete Gewinn am Becken der Block.
- **Ein neuer Zustand der Figur wird beim Losgehen zurückgesetzt.** `nbSteerFeet` kehrt früh zurück, solange der Gang
  aus ist: Was dort mit einer Zeitkonstante folgt, bleibt über den Stand oder einen Flug stehen. Kommt so eine Zeile
  nach der Prüfung dazu, die Ausgaben der Prüfung Byte für Byte mit dem geprüften Bau vergleichen und es in der Roadmap
  sagen.
- **Hashes vergleichen, nicht ganze Zeilen.** Die Zeilen des Benchmarks der Figuren tragen auch den Speicher, der mit
  der Zahl der Threads wächst: `grep -o "hash 0x[0-9a-f]*"`, sonst meldet der Vergleich einen Unterschied, der keiner
  ist.
- **Die Klassen vor jedem Vorschlag zählen, nicht nur vor der Wahl.** Die Lehre von Runde 132 gilt auch für den
  Vorschlag am Ende einer Runde: Über die acht Wechsel im Rennen auf zwölf Sätzen war das stehende Schienbein mit 14 von
  621 mm die kleinste Klasse, und die Runden 148 bis 150 galten ihr. Zum Vorschlag gehört die Zahl seiner Klasse neben
  denen der anderen.
- **Eine Stärke über ihre ganze Spanne fahren und neben dem Ziel die Szene zählen.** In Runde 151 nahm schon der Anteil
  selbst das Ziel fast, das stehende Schienbein 45 statt 253 mm, und die geglättete Variante aus der Durchsicht ganz, 30
  mm. Die 12 Abheber derselben Szene blieben bei beiden; erst ab dem 1,5-Fachen gehen sie auf 2, und bis zum 3-Fachen
  ändert sich nichts mehr. Gebaut ist die Mitte des Plateaus. Die erste Variante, die das Ziel nimmt, ist nicht die
  beste: `where129.py` nach dem, was das Bein tut, trennt sie.
- **Ein Nebengewinn der Entwicklung ist kein Ziel.** Die Abheber, auf den Sätzen der Entwicklung 2 statt 12 Stellen,
  zeigten sich auf den frischen Sätzen von Runde 151 nicht: Dort hob in der Szene vorher wie nachher ein Fuß hart ab.
  Das Ziel selbst hielt, 11 statt 134 mm, weil die Regel vorher ausgewürfelt hatte, wie oft es sich auf anderen Sätzen
  zeigt (in jeder von 200 000 Auswahlen). Bedingung wird nur, was so hält; der Rest steht daneben und wird im Bericht
  als nicht bestätigt genannt.
- **Alle Hashes des Testprotokolls vergleichen, bevor die Regel etwas über sie sagt.** Die Regel von Runde 151 schrieb,
  mit dem Prototyp ändere sich kein Hash; geprüft waren die drei festgeschriebenen. Der laufende Hash, den
  `SteerRubbleTest` nur ausgibt, änderte sich. `diff <(grep -o "hash 0x[0-9a-f]*" alt.log) <(grep -o "hash 0x[0-9a-f]*"
  neu.log)` zeigt beides.
- **Die Befehle des Prototyps zählen, bevor die Regel festgeschrieben wird.** In Runde 151 fiel erst nach der Prüfung
  auf, dass der Bau die Kostengrenze verfehlt, 0,51 %, 0,76 % und 0,79 % statt höchstens 0,5 %: Der Vorausblick des
  Rollens kostet, wo immer er läuft, bis zu 44 Lagen des Fußes je Fuß und Update, und die Runde hat ihn in mehr Updates
  eingeschaltet. `cost_count.sh` mit dem Prototyp als neuem Stand dauert Minuten und hätte den Preis vor dem
  Festschreiben gezeigt, wie die Preise von Runde 150 im Probelauf; so musste der Nutzer nach der Prüfung entscheiden.
  Ein Hebel, der eine vorhandene Rechnung öfter laufen lässt, kostet, auch wenn er nur eine Zeile ist.
- **Jede Toleranz einer Summe in Millimetern bekommt eine Untergrenze von 2 mm.** In Runde 152 war eine Schutzbedingung
  um 0,25 mm verfehlt: derselbe eine Fuß sprang 41,04 statt 40,79 mm, und die Toleranz seiner Summe über 37 mm war 0 mm,
  weil auf den Sätzen, an denen sie gemessen wird, kein solcher Fuß vorkam. Zählungen und härteste Sprünge hatten eine
  Untergrenze, Summen nicht. `tolerance<N>.py` setzt sie ab der nächsten Regel für jede Summe, vor dem Festschreiben;
  eine festgeschriebene Regel wird nicht nachgebessert, der Nutzer entscheidet.
- **Bei einem harten Aufsetzen die Höhe des Knöchels und die Rollung des Fußes je Update ausgeben.** Ein Fuß dreht um
  seine Zehen: Dreht er in der Luft flach, fällt sein Knöchel zusätzlich, auf H33 um 31,9 statt rund 20 mm in einem
  Update, und das Bein trifft den Boden noch länger werdend (Runde 152). Runde 139 hatte dieselben Landungen an der Zeit
  und an der Form des Schwungs gesucht und nichts gefunden, das Sinn macht; die Spalten `rollL`, `rollR` und die Höhe
  des Fußes standen im Protokoll. Der Vergleich mit einer Landung des stetigen Laufs zeigt, was anders ist.
- **Was ohne Verzögerung an `gaitRun` hängt, ist verdächtig.** Der Gang wechselt in vier Updates vom Gehen ins Rennen.
  Alles, was eine Größe direkt mit `1 - run` skaliert, springt dort mit: die Drehung der Zehen für die Landung (Runde
  152), vorher die Neigung des Schwungs (Runde 136) und die Höhe (Runde 137). Vor dem Verfolgen einer Stelle, an der
  `gaitRun` gerade wechselt, im Code nach solchen Größen suchen.
- **Die Zielgröße allein hätte die falsche Variante gewählt, zum zweiten Mal.** Die Drehung ab der Hälfte des Schwungs
  festzuhalten gab die wenigsten späten Landungen, 2 statt 119 mm, und die schlechteste Szene, 595 statt 295 mm (Runde
  152). Jede Variante bekommt die Zählung der Szene nach dem, was das Bein tut, bevor ihre Tabelle läuft: Das kostet
  zwei Minuten, die Tabelle eine halbe Stunde.
- **Vorwärts und rückwärts einzeln messen.** Derselbe Eingriff für alle Füße nahm mit festem Blick mehr, 763 statt 851
  mm, und kostete mit Blick in Laufrichtung 243 mm: Vorwärts landet der Fuß auf der Ferse, rückwärts auf den Zehen, und
  nur die Zehen verkürzen das Bein beim Aufsetzen (Runde 152). Was im Code für beide Richtungen mit einem Vorzeichen
  gerechnet wird, ist nicht dasselbe Verhalten.
- **Fällt ein Test der Vorrunde mit dem Prototyp, gehört das vor dem Festschreiben in die Regel.** In Runde 152 maß
  `SteerFasterRollTest` 27,2 statt 20,6 mm an einer Grenze von 26 mm. Die Regel nennt den Preis mit der neuen Grenze und
  schützt das Ziel der Vorrunde mit einer eigenen Bedingung an seiner eigenen Messgröße (A13, `stand151.py` auf den
  frischen Sätzen), statt nur am einen Test.
- **`speeds.py` muss alles lesen, was je gelaufen ist.** Es kannte die stetigen Läufe der Regeln nicht und hätte die von
  Runde 151 als frisch gemeldet. Seit Runde 152 liest es auch `docs/Regeln` und die `TRACK`-Zeilen der Prüfskripte. Wer
  eine neue Art von Lauf in eine Prüfung nimmt, trägt sie dort nach.
- **Vor einer neuen Architektur messen, was die Bibliothek heute tut.** Der Nutzer fragte nach einem Gang aus Muskeln
  und Physik. Runde 153 hat zuerst gemessen, was bei Treffern und Berührungen im Gang passiert, und fand das Folgen mit
  dynamischen Knochen schon gebaut (`engageRadius`) und zwei Fehler, die niemand gesucht hatte: Arme, die beim Zucken 18
  bis 53 cm springen, und zwei Figuren, die bei der Berührung beide umfallen. Beides liegt vor jeder Entscheidung über
  die Architektur.
- **Ein Messwert, der unglaublich aussieht, wird Update für Update verfolgt, bevor er berichtet oder verworfen wird.**
  Ein Sprung von 533 mm in einem Update sah nach einem Fehler im Werkzeug aus; eine Spur von Rahmen, Becken, Kopf und
  Hand zeigte, dass die Hand wirklich springt (Runde 153). Und ein Sturz ohne Treffer und ohne Stoß hat einen Auslöser
  im Code: Die Aufrufer von `nbFall` durchgehen und den Verdacht mit einem Schalter im Versuchsbaum beweisen.
- **Regeln für die Welt gelten auch für andere Figuren, ob gewollt oder nicht.** „Boden, der sich schnell bewegt, trägt
  nicht“ ist für wegrutschende Platten gebaut und traf den schwingenden Fuß der anderen Figur. Wo eine Abfrage Körper
  der Welt trifft, prüfen, was sie mit den Knochen einer anderen Figur tut: `nbFigureOfBody`.
- **Eine Schwelle im Messprogramm hängt an der Größe, um die es geht.** Runde 154 zählte als fast still, was unter einem
  Zehntel des eigenen Tempos liegt. Hinter einer Figur mit 0,47 m/s ging die mit 5,05 m/s gesteuerte genau mit 0,47 m/s
  mit, und das zählte als Stehen: eine Bedingung dem Buchstaben nach verfehlt, bei richtigem Verhalten. Der Probelauf
  lief nur auf Sätzen, deren Tempi nah beieinanderlagen. Vor dem Festschreiben jede Schwelle an den Rändern des frischen
  Satzes durchrechnen, hier schnellste erste gegen langsamste zweite Figur; das geht ohne einen Lauf.
- **Die Kosten zählen, sobald der Prototyp eine Schleife über alle Figuren hat.** Die erste Form kostete 4,0 % bei 20
  Figuren, die vierte 0,4 %. Gezählt vor dem Festschreiben, wie seit Runde 151 verlangt, hat das eine Runde gespart.
  `callgrind_annotate` auf `out/cost/cg/new3.out` zeigt, welche Funktion die Befehle trägt.
- **`tests.sh` gegen einen alten Stand braucht dessen Header.** Mit der Bibliothek des alten Stands und den Headern des
  neuen liest der Test Felder an falscher Stelle und meldet Fehler, die es nicht gibt. Den neuen Test in einen Baum des
  alten Stands kopieren und mit `NB_SRC` dort bauen.
- **Antwortet der Nutzer auf eine Entscheidungsfrage mit einer anderen Richtung, wird nicht gebaut.** Der geprüfte Bau
  wird als Patch neben der Regel abgelegt, die Runde als geprüft und nicht gebaut geschlossen, und die Frage nicht
  wiederholt.
- **Die Tabelle misst, was der Nutzer sieht.** Die Runden 128 bis 152 feilten an Wechseln im Rennen mit festem Blick.
  Der Nutzer verlangte den Gang, wie ein Spieler ihn führt: losgehen, gehen, rennen, drehen. Runde 155 hat das zum
  ersten Mal mit Blick in Laufrichtung gemessen und einen Fehler in Grad gefunden, den Standfuß, der sich bis 140 Grad
  mitdreht, den die Sprünge in Millimetern nie gezeigt hätten. Vor einer Reihe von Runden am selben Maß fragen, ob das
  Maß die Szenen des Nutzers abdeckt, und nicht nur Sprünge messen, sondern auch Drehen am Boden, Rutschen, Beckenhöhe
  und Schrittdauer.
- **Rutschen nur zählen, solange der Fuß flach steht.** Der Weg des Punkts zwischen Ferse und Zehen zeigte im stetigen
  Gehen 40 mm, weil der Fuß abrollt. Gezählt, solange Ferse und Zehen auf gleicher Höhe sind, sind es 0,1 mm.
- **Erst prüfen, ob der Fuß überhaupt abheben kann.** Runde 156 ließ zuerst den verdrehten Standfuß abheben und gewann
  fast nichts: In der Wende schwingt der andere Fuß fast durchgehend. Die Spur Update für Update, wer wann steht und
  schwingt, zeigte das in einer Minute. Vor einem Eingriff am Abheben die Stützphasen der Szene ausdrucken.
- **Einen Eingriff aus mehreren Teilen teilweise bauen und einzeln messen.** Der Becken-Ruck in der Kehrtwende im Rennen
  kam allein vom vorgedrehten Landeplatz; die Variante mit nur diesem Teil zeigte es sofort, und nur der Fuß selbst wird
  nun vorgedreht. Jeden Teil hinter einen eigenen Schalter legen.
- **Der ganze Testlauf früh, schon mit dem Prototyp.** Zwei Tests früherer Runden schlugen an und zeigten Kosten, die
  die neue Tabelle im Mittel nicht zeigte. `tests.sh` mit `NB_SRC` auf den Versuchsbaum kostet zwei Minuten.
- **Die Toleranz einer Summe ist mindestens so groß wie der Ausschlag einer Stelle.** Runde 157 erlaubte der Summe der
  Schienbeine über 37 mm 2 mm mehr. Auf der Entwicklung sank sie um 20 und 9 mm, auf den frischen Sätzen stieg sie um 10
  mm, 9 davon an einer einzigen von 688 Stellen. Eine Summe über wenige harte Stellen schwankt um den Wert einer Stelle;
  die Toleranz dafür aus dem größten einzelnen Überschuss der Entwicklung nehmen, nicht aus dem Boden von 2 mm.
- **Abschlussläufe in Schritten mit eigenem Protokoll.** Beim Neustart des Arbeitsprozesses brachen die Läufe nach
  Builds und Tests ab. Weil jeder Schritt sein Ende ins Protokoll schreibt, war klar, was fertig war; der Rest lief ganz
  neu, wie es die Regel für abgebrochene Läufe verlangt.
- **Ein Hebel an der Kapsel wird in jeder Szene geprüft, in der die Kapsel fährt.** Runde 158 hielt die Kapsel im Gehen
  zurück, solange ein Bein seinen Fuß verliert. Auf der Tabelle half es; der Testlauf zeigte drei Stellen, an denen es
  schadete: Trümmer, festen Blick, die Kehrtwende. Erst die Beschränkung auf das, was gemeint war (Drehen der
  Blickrichtung, ebener Boden, Gehtempo), ließ alles andere bytegleich. Vor dem Festschreiben den Probelauf mit den
  Schutzbedingungen fahren: Er zeigte hier noch, dass auch das Anhalten aus dem Rennen berührt war.
- **Sagen, was eine Runde nicht gelöst hat.** Die Trippelschritte blieben, und das Becken sackt in Wenden weiter, halb
  so viel. Die Wurzel, eine Kapsel, die nicht auf die Füße wartet, steht als großer offener Punkt in der Tabelle, statt
  mit weiteren Sonderfällen zugedeckt zu werden.
- **Eine Höhe wird mit dem Gang verglichen, zu dem sie gehört.** Runde 155 maß das Becken nach jedem Wechsel gegen seine
  Höhe davor und meldete beim Wechsel ins Rennen 78 mm Sacken; das Becken liegt im Rennen aber 7 cm tiefer als im Gehen.
  Runde 159 wurde dafür vorgeschlagen und gewählt und fand in der ersten Stunde, dass es nichts zu beheben gibt. Wo eine
  Szene Tempo oder Gangart ändert, ist der Bezug der stetige Gang beim neuen Tempo (`under` in `gait155.py`), nicht der
  Zustand davor.
- **Vor einem Vorschlag prüfen, ob der Befund ein Fehler ist.** Ein Lauf des stetigen Gangs beim Zieltempo neben dem
  auffälligen Lauf hätte genügt. Das gehört zu Frage 1 des Skills: Wie groß ist das Thema, gemessen an dem, was richtig
  wäre, nicht an dem, was vorher war.
- **Erst fragen, warum die Figur langsam wird.** Die Runden 158 und 160 suchten die Ursache des Sackens in Wenden bei
  Beschleunigung und Bremsen der Kapsel. Der Grund lag eine Zeile davor: Mit Blick in Laufrichtung begrenzt
  `nbMoveSteered` die Bewegung quer zur Blickrichtung auf 0,3 m/s, die Figur bleibt in der Wende fast stehen. Den Weg
  des Tempos durch die Funktion von oben lesen (gewünscht, begrenzt, beschleunigt, geklippt), bevor an einer Stelle
  gedreht wird.
- **Wo Blickrichtung und Bewegung einander folgen, dreht sich nichts.** Der erste Bogen-Versuch ließ die Figur in
  Blickrichtung gehen, und die Blickrichtung folgte der Bewegung: Sie drehte nie. Ein Maß, das „nie“ meldet (`again` und
  `faced` gleich -1), ist ein Fehler im Versuch, kein Ergebnis.
- **Eine Entwurfsrunde misst den Preis in der Größe, die das Spiel spürt.** Für den Bogen sind das nicht Millimeter am
  Becken, sondern Meter am Weg: wie weit die Figur noch in die alte Richtung geht und wie weit sie zur Seite ausweicht.
- **Zwei Fälle, die Verschiedenes brauchen, trennt ein Zeitpunkt, kein Einblenden.** Runde 161: Der Bogen half in Wenden
  aus dem Gehen und schadete beim Losgehen aus dem Stand. Ihn mit dem Tempo einzublenden, das die Figur schon hat,
  kostete den halben Gewinn, weil der Bogen sich das Tempo selbst nimmt. Einmal am Beginn der Wende zu entscheiden,
  hielt beides.
- **Ein Schalter auf null ist eine Schutzbedingung.** Bekommt ein Verhalten einen Schalter, verlangt die Regel, dass der
  Bau mit abgestelltem Schalter die Tabelle des Stands davor bytegleich schreibt (Runde 161, D3). Das Messprogramm setzt
  das Feld dafür aus der Umgebung, hinter einem Makro, damit es auch gegen ältere Stände baut.
- **Ein Test, der die alte Mechanik prüft, bekommt den Schalter, keine weichere Schwelle.** `SteerKneeTest` verlangte
  die Beugung der Knie beim harten Bremsen in der Kehrtwende. Mit dem Bogen bremst die Kapsel dort nicht mehr; der Test
  läuft mit `walkBend` 0 mit unveränderten Schwellen weiter, und die Roadmap sagt es.
- **Ein Test für ein neues Feld läuft auf dem alten Stand ohne die eine Zeile.** Der Helfer, der das Feld setzt, ist die
  einzige Stelle; im Baum des alten Stands wird sie durch `(void)` ersetzt, dann zeigt der Test auf der alten
  Bibliothek, dass er fehlschlägt.
- **Eine Regel für das Steuern misst auch, was ein Spieler tut, nicht nur einzelne Wechsel.** Runde 161 prüfte den Bogen
  an einzelnen Wenden aus dem stetigen Gang, alles erfüllt. Runde 162 steuerte fortlaufend, Kreis, Zickzack, Hin und
  Her, und fand, dass die Figur mit dem Bogen schnellem Hin und Her nicht mehr folgt. Seitdem gehört `steer162.py` als
  Schutz in jede Regel, die an Kapsel oder Blickrichtung dreht, mit der Größe „folgt“.
- **Ruhe des Körpers und Folgen des Sticks sind zwei Größen, und beide stehen in der Regel.** Eine Änderung, die nur an
  der einen gemessen wird, kann die andere unbemerkt kosten.
- **Eine verfolgte Ursache schlägt drei Varianten.** Runde 163: Sanft bremsen allein half kaum. Erst die Spur Update für
  Update zeigte, dass die Kapsel rückwärts von dem Fuß wegging, der eine Schrittlänge voraus gelandet war; darauf zu
  warten, halbierte das Sacken.
- **Alte Ausgaben einer Variante gleichen Namens sind eine falsche Abweichung.** Der Probelauf meldete die Tabelle mit
  festem Blick als verschieden, weil im Ordner der Variante Dateien einer früheren Variante desselben Namens lagen.
  Prüfskripte räumen ihre Ausgaben vor dem Lauf, und eine verfehlte Schutzbedingung wird erst auf ihre Ursache geprüft,
  bevor sie als Fehler des Baus gilt.
- **Eine Zählung über gemischte Tempi hängt am Mix.** A2 der Runde 163 zählte Läufe mit gesacktem Becken über alle
  Gehtempi. Auf der Entwicklung 11 und 9 %, auf den frischen Sätzen 23 %, 22 von 29 Läufen bei den zwei langsamsten
  Tempi. Eine solche Bedingung wird je Tempo angesehen, bevor ihre Grenze festgeschrieben wird, und das langsamste
  frische Tempo liegt nicht unter dem langsamsten der Entwicklung, wenn langsam die ungünstige Seite ist.
- **Die Grenze eines älteren Tests lockert nur der Nutzer.** `SteerTurnStepTest` hielt das Ziel aus Runde 157 fest. Dass
  die ruhige Umkehr es verfehlt, stand vor dem Bau als Frage an den Nutzer, mit der schnelleren Variante als
  Alternative.
- **Erst sehen, welche Größe springt.** Runde 164: Das Becken sprang, aber die Spur zeigte, dass schon die Wurzel
  sprang, über ihr die Tiefe vor der Landung und über der das Landeziel des Fußes, 0,71 m in einem Update. Drei Spalten
  mehr in der Spur fanden die Ursache in zwei Läufen.
- **Ist die Ursache nicht zu nehmen, wird das Symptom so eng begrenzt wie sein Auslöser.** Das Landeziel langsamer
  wandern zu lassen, machte alles schlimmer. Die Wurzel immer nur begrenzt schnell tiefer gehen zu lassen, half, brach
  aber Trümmer und den Abstieg im Rennen. Erst die Grenze nur in der Zehntelsekunde nach einem Sprung des Sticks ließ
  alles andere unberührt: 6 statt 51 geänderte Stellen, alle Tests bestehen. Was bleibt, hier der Fuß, der in der Luft
  umkehrt, steht als offene Zeile in der Tabelle.
- **Alle gesackten Läufe ansehen, nicht nur den schlimmsten.** Runde 165: Die Spur eines Laufs zeigte eine Umkehr mitten
  in der Umkehr. Erst `sink165.py`, das jeden der 67 Läufe im tiefsten Update ansieht, zeigte die Klasse dahinter: 38
  drehen gerade auf der Stelle. Eine Abhilfe für den einen Lauf hätte die Klasse verfehlt.
- **Eine verworfene Abhilfe sagt etwas über die Ursache.** Die Blickrichtung warten zu lassen, solange das Standbein
  seinen Fuß verliert, half nicht: Dieses Maß trifft das Sacken beim Drehen auf der Stelle also nicht, und die Ursache
  liegt im Ort des Fußes, nicht im Tempo der Drehung.
- **Eine Variante bekommt einen Namen, den es noch nicht gab, und ihr Bau wird geprüft.** Ein Versuch dieser Runde baute
  nicht, und das Messskript lief auf einem alten Programm gleichen Namens weiter und lieferte Zahlen, die nichts
  bedeuteten.
- **Ein Prüfskript, das einen Fehler verschluckt, erfindet Befunde.** Runde 166: `tests.sh` meldete 19 Fehlschläge
  einzelner Figuren, während die Messwerkzeuge dieselbe Bibliothek bytegleich fanden. Die Testdatei übersetzte nicht,
  und das Skript linkte die Objektdatei eines früheren Laufs mit dem alten Aufbau der Figur. Widersprechen sich zwei
  Prüfungen, wird erst das Prüfwerkzeug angesehen; die Zahl der Tests in der letzten Zeile hätte es sofort gezeigt, 201
  statt 202.
- **Ein abgelegter Bau wird neu geprüft, nicht nachgeholt.** Der Patch aus Runde 154 passte nach zehn Runden nicht mehr.
  Er bekam eine eigene Regel, die den Fehler der alten berichtigt, und neue frische Begegnungen; die frischen von damals
  zählen nur noch als Entwicklung.
- **Beim Zusammenführen von Hand wird jede Datei übersetzt, bevor gemessen wird.** „Beide Seiten behalten“ setzte einen
  Test mitten in eine andere Funktion.
- **Kreuzen ist nicht Durchdringen.** Bis Runde 167 prüften die Regeln, ob Füße und Knie quer zum Becken kreuzen, und
  der Gang galt als ruhig. Der Nutzer sah die Beine durcheinander gehen. `legs167.py` misst die Formen; was der Nutzer
  sieht, wird an den Formen gemessen, nicht an Mittelpunkten.
- **Ein Versatz, der vom Zustand abhängt, wird einmal festgelegt.** Das Ausweichen des schwingenden Fußes in jedem
  Update neu zu rechnen hat viermal so viele Fuß-Sprünge gemacht. Beim Abheben festgelegt und weich ein- und
  ausgeblendet macht es fast keine.
- **Ein Test, der an der ersten Grenze abbricht, versteckt die zweite.** `SteerWalkBendTest` riss an der Zeit; dass die
  Figur auch 0,3 m weiter geradeaus geht, stand in derselben Ausgabe und wurde erst nach dem Lockern geprüft. Schlägt
  ein Test auf dem Prototyp an, alle seine gedruckten Werte gegen alle seine Grenzen halten, bevor die Regel den Preis
  nennt.
- **Eine Bedingung über das Tiefste hängt an einem Lauf.** Ziele über Formen als Zahl der Läufe über einer Schwelle
  schreiben und das Tiefste daneben berichten.
- **Ein Preis, der ein Schritt ist, lässt sich nicht wegregeln.** Runde 168 hat vier Wege gemessen, die wartende Drehung
  aus Runde 167 schneller zu machen. Jeder tauschte die Zeit gegen Rucke oder gekreuzte Beine. Vor einer Runde, die
  einen Preis zurückholen soll, fragen, ob er aus einer Schwelle kommt oder aus der Zeit, die ein Fuß für seinen Weg
  braucht.
- **Die Summen über die Wenden hängen an der Schrittphase.** Runde 169: Ein Anfahren von 9,5 statt 10 m/s² aus dem Stand
  macht aus 93 mm Schienbein-Summe 134, ohne dass am Gang etwas anders wäre. Wer den Start verändert, vergleicht die
  Wenden nicht mehr über diese Summen, sondern misst am Start selbst und nennt das Rauschen mit einer Fassung, die fast
  nichts ändert.
- **Was der Nutzer sehen muss, wird ein Schalter.** Ob das Losgleiten das war, was ihn störte, ließ sich nicht messen.
  Die Runde baute es als Parameter mit Standard aus, bytegleich, und als Regler in der Demo; den Standard entscheidet er
  danach.
- **Ein neuer Standard ist ein Umbau.** Den Standard von `acceleration` zu senken ließ 14 Tests anschlagen. Ein
  Parameter, den ein Spiel setzt, ändert die Bibliothek für niemanden, der ihn nicht setzt.
- **Ein einzelner Lauf ist eine Probe, kein Wert.** Runde 170: Ein Laufschritt dauert fast genau 44 Updates, und wo er
  zwischen zwei Updates fällt, hängt am Anlauf. Derselbe Stand gab in vier leicht verschobenen Proben 147 bis 167
  Schienbein-Sprünge, ein anderer Start in einem Lauf 316. Zwei Stände werden mit `check170.sh` und `compare170.py`
  verglichen, und ein Unterschied zählt nur außerhalb der Spannen.
- **Rauschen nicht mit einer Störung messen, die das System vergisst.** Länger stehen vor dem Start änderte nichts; erst
  ein Prozent mehr Tempo vor der Änderung verschob den Schritt gegen die Updates.
- **Eine Grenze mitten im stetigen Band zählt Zufall.** 37 mm liegt dort, wo ein Schienbein im stetigen Lauf je nach
  Lage des Schritts 30 bis 38 mm springt. Grenzen über dem stetigen Gang (Extra in `where129.py`) oder Spannen über
  Proben nehmen.
- **Jede Regel prüft die Kurse mit.** Runde 171 hat die Runden 155 bis 167 nachgemessen. Die verdeckten Preise der
  Runden 156, 157 und 161 lagen alle dort, wo ihre Regeln nicht hinsahen: auf den Kursen aus `steer162.py` oder im Hin
  und Her. Eine Regel nimmt die Kurse und `legs167.py` als Schutz, auch wenn die Runde nur einzelne Wenden ändert.
- **Ein Fehlschlag der Regel wird mit Proben geprüft, bevor er erklärt wird.** A2 von Runde 163 war echt, S2 und S5 von
  Runde 167 waren Rauschen. Beides hätte sich mit vier Proben vor der Entscheidung gezeigt.
- **Eine seltene Stelle wird am verfolgten Lauf behoben und über Proben bewiesen.** Runde 172: ein Lauf von 408, Update
  für Update verfolgt, eine Zeile in der Entscheidung, welcher Fuß abhebt. Der Beweis war die Zahl der Läufe über 20 mm
  über vier Proben auf drei bekannten und vier frischen Sätzen, nicht ein Maximum. Der Test fährt genau den verfolgten
  Kurs; ein ähnlicher Kurs traf die Stelle nicht.
- **Erst fragen, ob es eine Stelle ist oder ein Rand.** Runde 172: ein Lauf, in jeder Probe, 24 mm gegen sonst 11, eine
  Ursache, eine Zeile. Runde 173: zwölf Läufe knapp über der Grenze, in zehn Kursen, keiner in jeder Probe, mehrere
  Ursachen; eine Abhilfe für eine davon lag im Rauschen. Vor dem Verfolgen die Verteilung ansehen: Wie weit liegt die
  Stelle über den 95 % der Läufe, und kommt sie in jeder Probe?
- **Ein Preis, der knapp verfehlt ist, hat eine Ursache.** Runde 174 verfehlte den erlaubten Preis um einen halben Kurs,
  der Nutzer ließ nicht bauen. Runde 175 fand die Ursache in einem verfolgten Lauf, die Kapsel kroch rückwärts gegen die
  Beine, und einen Regler, der Sacken gegen enge Beine tauscht; ein Wert dazwischen hielt beides im Rauschen. Vor
  „trotzdem bauen“ einen Lauf verfolgen, in dem der Preis neu auftritt.
- **Ein Regler mit zwei Seiten wird über mehrere Werte gemessen.** Fünf Werte zeigten den stetigen Tausch und wo beide
  Seiten im Rauschen liegen. Die Sätze, auf denen der Wert gewählt ist, zählen danach als Entwicklung, und die Prüfung
  braucht neue frische Sätze.
- **Beide Programme eines Benchmark-Paars werden am selben Rechner gebaut.** Runde 175: Gegen das Programm der Runde
  davor, das vor einem Neustart der Umgebung gebaut war, erschienen alle fünf Szenen 8 bis 15 % langsamer, auch die ohne
  Figuren, bei unveränderter Befehlszahl. Gegen den neu gebauten alten Stand: 0,99 bis 1,04. Nach jedem Neustart den
  alten Stand neu bauen, bevor gepaart wird.
- **Ansehen, nicht nur zählen.** Runde 176: `strip176.py` zeichnet die Figur als Strichbilder. Zwanzig Runden Zahlen
  über Sprünge von Knochen hatten nicht gezeigt, was im ersten Bild auffiel: Die Kapsel ändert ihr Tempo in 0,1 s, die
  Beine brauchen 0,3 bis 1,9 s. Vor jeder Runde am Gang die Szene zeichnen und ansehen, vorher und mit dem Prototyp.
- **Körper gegen Beine messen.** `walk176.py` misst, wann die Kapsel ihr neues Tempo hat, wann der erste Fuß landet,
  wann die Füße stehen und wie viele noch landen, nachdem der Körper steht. Für das Gehen ist das die erste Größe einer
  Regel zu Losgehen, Anhalten und Wenden.
- **Ein Regler der Steuerung ist eine Aussage an das Spiel.** Runde 177: `braking` hieß „so schnell steht sie“, und ein
  Test prüfte genau das. Wer das Verhalten ändert, baut es als eigene Option mit Standard (`stopBraking`), lässt den
  alten Weg mit null offen und prüft beide im Test.
- **Was die Füße tun, hängt am Ort, an dem der Körper stehen wird.** Ein Fuß, der zielen kann, bekommt diesen Ort; für
  einen, der es nicht mehr kann, richtet sich der Körper nach ihm. Beides zusammen nahm dem Anhalten das Zurücksetzen.
  Ein Ziel, das springt, wird mit begrenztem Tempo nachgeführt, nicht sofort.
- **Eine Bedingung gilt in dem Moment, in dem die Eingabe sich ändert.** „Ist eine Sekunde gegangen“ traf auch das Ende
  eines Halts aus dem Rennen, weil der Gang dort ausläuft; gemerkt beim Loslassen des Sticks (`walkedTo`), trifft sie
  nur das Gehen.
- **Die Demo wird vor dem Push gebaut, wenn sie sich ändert.** Runde 177: Ein Zeilenumbruch in einem Hinweistext der
  Demo brach den Bau auf allen drei Plattformen, und die Abschlussläufe bauen die Demo nicht mit. Mit `apt-get update`
  und `apt-get install -y libgl-dev libx11-dev libxi-dev libxcursor-dev` baut sie auch in der Umgebung: `cmake -S . -B
  <dir> -DNEBENAN_DEMO=ON` und das Ziel `nebenan_demo`. Text für C-Quellen nie mit `\n` aus einem Python-Text ohne `r`
  schreiben.
- **Ein Start aus dem Stand hat kein Rauschband aus Proben.** Runde 178: `NUDGE` verschiebt den Moment des Schritts, an
  dem eine Änderung kommt; ein Start kommt an keinem. Seine Größen werden über die Sätze verglichen (Median und Spanne),
  und `compare170.py` nennt eine einzelne Startgröße schnell „schlechter“, die über die Sätze in beide Richtungen geht.
- **Tests, die Updates vom Start an zählen, sind Stichproben eines Schrittmoments.** Ein anderes Anfahren ließ 13 Tests
  anschlagen; mit drei Werten des Anfahrens schwankten sie in beide Richtungen. Solche Tests behalten das Anfahren, mit
  dem sie gesetzt sind (`AddSteeredFigure`), und was dabei an schlechten Momenten auffällt, kommt als offener Punkt in
  die Liste, nicht unter den Tisch.
- **Eine Option, die Standard wird, ist ein kleiner Bau mit großer Wirkung auf alles, was danach kommt.** Auch die
  Bibliothek selbst hängt an ihrem Standard: Die Aufnahme der Gang-Clips startete eine Figur mit `nbDefaultFigureDef`,
  und die Clips begannen an anderer Stelle des Zyklus.
- **Ein Prototyp wird Update für Update nachgesehen, bevor nach ihm entschieden wird.** Runde 179: Der Nutzer wählte
  zwischen Zahlen einer Fassung, die nur 0,2 s sanft bremste und den Rest wieder hart, weil die Bedingung „Kehrtwende“
  mit dem drehenden Blick endete. Erst die Zeile mit dem Tempo je Update zeigte den Knick. Die vollständige Fassung
  hatte andere Zahlen, und das gehört offen in den Bericht.
- **Hartes Bremsen der Kapsel war eine Ursache für sackendes Becken und Beine ineinander.** Anhalten (177) und
  Kehrtwende (179) rollen aus, und auf den Kursen im Gehen sackt das Becken auf 7 statt 12,5 Kursen. Wo ein Körper
  schneller die Richtung wechselt, als Beine es können, zahlen die Beine.
- **Nicht jede Auffälligkeit ist ein Fehler, und nicht jede lässt sich ohne Preis beheben.** Runde 180: Drei
  Auffälligkeiten des Drehens waren gemessen; die Bilder zeigten eine Wende, die stimmt, die Tempo-Stufe war keine
  Ursache, und das Drehen des Fußes ließ sich nur gegen Zeit und Sprünge tauschen. Dann wird nicht gebaut: Die Runde
  legt die Zahlen vor, und der Nutzer sagt, was stört.
- **Ein Schalter der Messprogramme reicht für die erste Frage.** `BRAKE` und `ACC` von `lab101.c` beantworteten in einer
  Viertelstunde, ob ein sanfter Übergang hilft, bevor eine Zeile Bibliothek geschrieben war.
- **Größe der Figur.** Winkelbeschleunigungen wachsen bei kleineren Figuren mit 1 durch `nbFigureScale`, Längen
  und Geschwindigkeiten mit ihr.

## Arbeiten in der Cloud

- Lange Läufe als Hintergrundaufgabe, nicht mit `sleep` darauf warten. Während Zeiten gemessen werden, nichts Schweres
  daneben.
- Einen Hintergrundjob, der auf einen anderen wartet, über seine Prozessnummer beenden und nachsehen, dass er weg ist:
  Ein nur halb beendeter Job hat in Runde 123 später Ergebnisse überschrieben.
- Die Maschine kann neu starten: vor langen Messungen `git diff` als Patch sichern, unterbrochene Messungen ganz neu.
- Ausgaben kurz halten (`cut`, `grep`, `tail`), der Nutzer zahlt die Token. Zwischenstände mit „Kurz:“.
- Commits: `user.name=Claude`, Trailer `Co-Authored-By: Claude <noreply@anthropic.com>` und `Claude-Session`, kein
  Modellname, nie Geschichte umschreiben. Kommentare englisch und auf 120 umbrochen, `(b3Vec3){` bleibt eng.
- `clang-format` läuft dort nicht, Version 18 kennt einen Schlüssel der `.clang-format` nicht und meldet dann auch
  keine Warnung: Längen neuer Zeilen mit Python prüfen, Kommentare 120 mit Tab als 1, Code 130 mit Tab als 4. Nie
  ganze Dateien neu umbrechen, nur die eigenen Blöcke: Bestehende Zeilen sind zum Teil länger (Runde 127).
- Was nur im Arbeitsordner der Sitzung liegt, ist nach ihr weg, und was nur im Kontext stand, nach der nächsten
  Zusammenfassung. Vor dem Ende einer Runde kommen ins Repository: die Werkzeuge, die eine Entscheidung getragen haben,
  das Rezept jedes gemessenen, nicht gebauten Prototyps (Funktion, Größe, Zahl) in Roadmap und Optimierungen, und jede
  Berichtigung einer schon gemeldeten Zahl.
- Auf die CI mit einer geplanten Erinnerung warten, nicht mit einer Warteschleife. Nach grüner CI das Zip laden, seine
  Prüfsumme mit der von GitHub vergleichen und es dem Nutzer geben.
- Das Repository ist öffentlich: keine Namen von Nutzerkonten aus Pfaden übernehmen, `<Nutzer>` schreiben, keine
  E-Mail-Adressen.

## Quellen

Aus anderen Projekten nur Ideen, nie Zahlen. Ziele aus der Literatur (Maße des Menschen, Gangdaten) mit Quelle. Nie
benutzen: den GTA-Gist mit durchgesickertem Code, die Repositories von NaturalMotion. DMS/Delirium ist GPL-3 und
`AshimPoudel7/ProceduralLocomotion` hat keine Lizenz: nur Ideen.
