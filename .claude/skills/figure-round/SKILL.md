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
- Was wiederholt werden darf, steht vorher in der Regel. Eine Messung, die nicht passt, wird nie wiederholt, bis sie
  passt. Verfehlt der saubere Bau eine Bedingung, wird nicht gebaut, oder es steht offen in der Roadmap, warum doch,
  und der Nutzer entscheidet.

Grenzen der Messgrößen: Sprung ist die zweite Differenz des Ursprungs eines Knochens je Update. Füße, Schienbeine,
Zehen und Fersen 37 mm, Hände 15 mm, Becken 10 mm, Becken tief ab 15 cm unter dem Stand, Flug ab 0,2 s.

## Lehren

- **Zeit messen streut mehr als kleine Grenzen.** Zwei Binaries desselben Quelltexts liegen je nach Compiler und Modus
  bis 0,5 µs auseinander. Kosten deshalb an Befehlen messen (Runde 120).
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
- **Größe der Figur.** Winkelbeschleunigungen wachsen bei kleineren Figuren mit 1 durch `nbFigureScale`, Längen
  und Geschwindigkeiten mit ihr.

## Arbeiten in der Cloud

- Lange Läufe als Hintergrundaufgabe, nicht mit `sleep` darauf warten. Während Zeiten gemessen werden, nichts Schweres
  daneben.
- Die Maschine kann neu starten: vor langen Messungen `git diff` als Patch sichern, unterbrochene Messungen ganz neu.
- Ausgaben kurz halten (`cut`, `grep`, `tail`), der Nutzer zahlt die Token. Zwischenstände mit „Kurz:“.
- Commits: `user.name=Claude`, Trailer `Co-Authored-By: Claude <noreply@anthropic.com>` und `Claude-Session`, kein
  Modellname, nie Geschichte umschreiben. Kommentare englisch und auf 120 umbrochen, `(b3Vec3){` bleibt eng.

## Quellen

Aus anderen Projekten nur Ideen, nie Zahlen. Ziele aus der Literatur (Maße des Menschen, Gangdaten) mit Quelle. Nie
benutzen: den GTA-Gist mit durchgesickertem Code, die Repositories von NaturalMotion. DMS/Delirium ist GPL-3 und
`AshimPoudel7/ProceduralLocomotion` hat keine Lizenz: nur Ideen.
