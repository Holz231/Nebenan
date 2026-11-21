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
- Zeichnen heißt Fenster, Shader und Grafikkarte. Was gezeichnet wird und wo, etwa das Mesh einer Figur und die Lagen
  seiner Haut, braucht ein Spiel auch: Es gehört in die Bibliothek. In `active-ragdoll-125` stand die Haut der
  Gliederpuppe nur in der Demo.
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

Die Roadmap des Spiels liegt als Referenz in `docs/Referenz/`, als PDF und als Text. Wie Häuser einstürzen sollen, steht
dort in Kapitel 17. Dort liegen auch die Analysen des Nutzers von endorphin, von Delirium Physics und von MuJoCo, je mit
einer Zusammenfassung der Ideen: Für Nebenan gelten daraus nur Ideen, kein Code, keine Zahlen, Formeln oder Formate.

## Runden an der Figur

Wie eine Runde an der gesteuerten Figur abläuft, von der Ursache über die festgeschriebene Regel bis zum Bericht, steht
im Skill `figure-round` in `.claude/skills/figure-round/`, mit einer Karte des Codes der Figur und den Fehlern, die schon
einmal gemacht wurden. Vor jeder Runde `active-ragdoll-N` und nach jedem Neustart des Kontexts dort nachsehen.

Vier Fragen daraus gelten bei jeder Runde, seit der Durchsicht von Runde 150, und jede bekommt im Bericht eine Antwort
mit Zahl: vor jedem Vorschlag, wie groß das Thema neben den anderen ist; vor dem Verwerfen einer Variante, ob sie das
Ziel der Runde nimmt und ob sie überhaupt greift; vor dem Festschreiben der Regel, ob der Prototyp das Ziel der Runde
nimmt; nach der Prüfung, ob die Szene der Runde besser wurde. Starts aus dem Stand zählen in Summen einmal je Satz. Was
eine Entscheidung getragen hat und nur im Arbeitsordner oder im Kontext liegt, kommt vor dem Ende der Runde ins
Repository.

## Sprache

Mit dem Nutzer Deutsch, per du. Code und Kommentare auf Englisch, README und `docs/` auf Deutsch.
