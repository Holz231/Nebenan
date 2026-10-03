# Mitgelieferte Bibliotheken

Diese Bibliotheken liegen bei, damit der Build ohne Download auskommt. Bis auf zwei Ergänzungen in Box3D sind sie
unverändert.

| Ordner | Bibliothek | Stand | Lizenz |
| --- | --- | --- | --- |
| `box3d` | [Box3D](https://github.com/erincatto/box3d) von Erin Catto: `CMakeLists.txt`, `include`, `src` und `extern/sokol` | Commit `adec25b3010b7b7a715e57ce79595b0d2184474a` | MIT, `box3d/LICENSE` |
| `box3d/extern/sokol` | [sokol](https://github.com/floooh/sokol) von Andre Weissflog, kommt mit Box3D | wie Box3D | zlib, `box3d/extern/sokol/LICENSE` |
| `imgui` | [Dear ImGui](https://github.com/ocornut/imgui) von Omar Cornut, nur die Kerndateien | Version 1.92.7 | MIT, `imgui/LICENSE.txt` |

Zum Aktualisieren die Dateien aus dem neuen Stand an dieselbe Stelle kopieren, die Ergänzungen unten wieder einsetzen
und den Stand hier eintragen. Wer Nebenan in ein eigenes Projekt einbindet, das schon ein Ziel `box3d::box3d` hat,
verwendet dessen Box3D.

## Ergänzungen in Box3D

**Große Blöcke wachsen an Ort und Stelle.** Box3D lässt alle Arrays über `b3GrowAlloc` wachsen. Bisher kopierte das
ein volles Array in ein doppelt so großes, und in einer Welt mit Hunderttausenden Formen hielt das den Aufruf, der es
auslöste, über 100 ms auf. Jetzt reserviert ein Block ab 1 MB beim Anlegen 4 GB Adressraum. Wächst er, kommen dort
Speicherseiten dazu, und er bleibt, wo er ist. Mehr braucht es nicht, denn Box3D gibt Größen als `int` an, kein Array
wird größer als 2 GB. Adressraum haben 64-Bit-Programme reichlich, Speicher belegen nur die Seiten, die benutzt werden.
Das gilt für 64-Bit-Windows, -Linux und -macOS, solange kein eigener Allocator gesetzt ist (`b3SetAllocator`). Unter
ASan kommen die Blöcke weiter von `malloc`, damit ASan ihre Grenzen prüft. Nebenan macht dasselbe für seine eigenen
Arrays, siehe `src/core.c`.

**`b3World_ReserveHulls`** macht in der Hüllen-Datenbank einer laufenden Welt Platz für mehr Hüllen. Die Datenbank ist
eine Hash-Tabelle und kann nicht an Ort und Stelle wachsen: Wird sie voll, ordnet sie alle Hüllen in eine doppelt so
große Tabelle ein. Nebenan hält deshalb beim Aufbau Platz für doppelt so viele Formen frei, siehe `nbKeepRoom`.

An den Ergebnissen ändert keine der beiden etwas. Sie stehen in `src/core.c`, `include/box3d/box3d.h` und
`src/physics_world.c`, jeweils mit „Added for Nebenan“ markiert. `B3_HAS_RESERVE_HULLS` zeigt die zweite an. Fehlen
sie, etwa mit einem eigenen Box3D, läuft Nebenan wie zuvor, nur wachsen Box3Ds Arrays dann durch Umkopieren.
