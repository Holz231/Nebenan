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

## Ablauf

1. **Lesen.** Roadmap und Optimierungen. Was dort mit ⛔ steht, ist schon versucht: nicht noch einmal, außer der Grund
   ist weggefallen.
2. **Arbeitsplatz.** Ein Arbeitsbaum auf der neuen Branch vom letzten Stand, ein zweiter als Versuchsbaum (detached,
   bekommt die Schalter), ein dritter als sauberer alter Stand für Kosten und Benchmark. Die cmake-Bauten des alten
   Stands behalten, sie sind der Vergleich. Der Hauptcheckout bleibt unberührt.
3. **Ursache verfolgen, bevor etwas gebaut wird.** Die härtesten Stellen aus den Ergebnissen der letzten Prüfung
   holen, eine davon mit `lab101` nachfahren und Update für Update ansehen (`jumps119.py`, `lift121.py`, eine kleine
   Tabelle in Python). Erst wenn im Lauf zu sehen ist, welche Größe springt und welche Zeile sie setzt, steht die
   Ursache. Eine Vermutung ist keine: In Runde 121 und 122 waren je zwei naheliegende Vermutungen falsch.
4. **Varianten mit Schaltern.** Im Versuchsbaum hinter `#ifdef NB_X_…`, eingebaut von einem kleinen Python-Skript im
   Scratchpad, gebaut mit `build.sh name "-DNB_X_…"`. Zuerst an der verfolgten Stelle, dann auf den Sätzen der
   Entwicklung (`sweeps.sh`, `facing119.py`, `fixed121.py`). Jede verworfene Variante mit ihrer Zahl notieren, sie
   kommt in die Optimierungen.
5. **Stetigen Gang prüfen.** Läufe stetig gehend, rennend (3,0, 4,5, 4,8 m/s), seitwärts und rückwärts mit `cmp` gegen
   den alten Stand. Ein Eingriff für Wechsel soll den stetigen Gang bitgleich lassen, sonst ist er größer als gedacht.
6. **Toleranzen und Zielgröße.** `tolerance<N>.py` auf dem alten Stand. Die Zielgröße so wählen, dass der Gewinn auf
   den Entwicklungssätzen deutlich über der Streuung zwischen ähnlichen Sätzen liegt, siehe Lehren.
7. **Frische Geschwindigkeiten.** `tools/measure/speeds.py v …` sagt, ob ein Wert je gelaufen ist, und rechnet die
   Spreizung über einen Zyklus aus der Kadenzformel, ohne einen Lauf. Neue Sätze in `common.sh`.
8. **Regel festschreiben.** `docs/Regeln/RULE<N>.txt`, `tol<N>.json`, `check<N>.sh`, `eval<N>.py`, als eigener Commit
   ohne die Bibliothek, und pushen, bevor irgendein Lauf mit den frischen Geschwindigkeiten existiert. Der Push gibt
   der Regel einen Zeitstempel außerhalb der Maschine.
9. **Sauberer Bau** in `src/`, mit Kommentar, bitgleich zum Prototyp: dieselben JSONs auf mindestens zwei Sätzen.
10. **Unabhängige Prüfung.** `check<N>.sh alt neu`, dann `eval<N>.py alt neu`. Das Ergebnis gilt, wie es kommt.
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
    seine Prüfsumme mit der von GitHub vergleichen, Bericht mit „Kurz:“ und einem Vorschlag für die nächste Runde.

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

## Quellen

Aus anderen Projekten nur Ideen, nie Zahlen. Ziele aus der Literatur (Maße des Menschen, Gangdaten) mit Quelle. Nie
benutzen: den GTA-Gist mit durchgesickertem Code, die Repositories von NaturalMotion. DMS/Delirium ist GPL-3 und
`AshimPoudel7/ProceduralLocomotion` hat keine Lizenz: nur Ideen.
