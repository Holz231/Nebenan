# Regeln für die Arbeit an Nebenan

Nebenan ist eine Bibliothek: `include/nebenan`, `src` und das ergänzte Box3D in `extern/box3d`. Demo, Benchmark und
Tests sind Programme, die sie benutzen. Ein Spiel bekommt nur die Bibliothek.

## Zuerst die Bibliothek

Jede Optimierung und jedes Verhalten, das ein Programm mit Nebenan brauchen würde, gehört in die Bibliothek. Demo,
Benchmark und Tests rufen es nur auf. In `physik-16` lief die Physik parallel zum Zeichnen nur in der Demo, und ein
Spiel hätte sie nachbauen müssen. Das darf nicht wieder passieren.

- Vor jeder Zeile in `demo/` oder `benchmark/` fragen: Bräuchte ein Spiel mit eigener Engine das auch? Wenn ja, gehört
  es in die Bibliothek, und die Demo ruft es auf.
- In die Demo gehört nur, was ein Spiel selbst macht und eine Physik-Bibliothek nicht kann: Fenster, Zeichnen,
  Kamera, Eingabe, Menü, die Szenen und die Messwerkzeuge.
- Steht dieselbe Logik in Demo und Benchmark, gehört sie in die Bibliothek.
- Verdächtig ist alles in der Demo, das Threads startet, Zeit misst, um die Welt zu sparen, oder entscheidet, wann die
  Welt etwas tut. Das prüfen, bevor es gebaut wird.
- Wer etwas misst, misst es in der Demo, baut es aber in der Bibliothek.

## Feste Regeln

- Kein Trümmerteil wird gelöscht, keine Kollision wird abgeschaltet, nichts bleibt in der Luft hängen.
- Ergebnisse bleiben deterministisch und bitgleich, mit jeder Zahl von Threads und auf jeder Plattform.
- `main` wird nie geändert. Jede Runde bekommt eine eigene Branch `physik-N`, gepusht mit grüner CI.
- Größere Umbauten nur auf einer neuen Branch. Nichts auf Krampf, es muss Sinn machen.
- In `docs/Optimierungen.md` wird nichts gelöscht. Verworfenes bleibt mit Grund stehen.
- Pull Requests nur auf Auftrag. Keine Modellnamen in Commits oder Code.

## Messen

- Die Bibliothek misst der Benchmark, gepaart gegen die Runde davor, mehrere Runden im Wechsel.
- Die Demo misst der Nutzer auf seinem PC (Ryzen 7 7800X3D) mit dem Knopf „Parallel gegen nacheinander messen“. Auf der
  Cloud-VM zeichnet llvmpipe ohne Grafikkarte, dort sagen Bildzeiten der Demo nichts.

## Roadmap

Die nächsten möglichen Schritte stehen mit Gewinn, Voraussetzung und Stand in `docs/Roadmap.md`. Vor jeder neuen Runde
dort nachsehen, nach jeder Runde den Stand nachtragen.

## Sprache

Mit dem Nutzer Deutsch, per du. Code und Kommentare auf Englisch, README und `docs/` auf Deutsch.
