# Mitgelieferte Bibliotheken

Diese Bibliotheken liegen bei, damit der Build ohne Download auskommt. Bis auf eine Ergänzung in Box3D sind sie
unverändert.

| Ordner | Bibliothek | Stand | Lizenz |
| --- | --- | --- | --- |
| `box3d` | [Box3D](https://github.com/erincatto/box3d) von Erin Catto: `CMakeLists.txt`, `include`, `src` und `extern/sokol` | Commit `adec25b3010b7b7a715e57ce79595b0d2184474a` | MIT, `box3d/LICENSE` |
| `box3d/extern/sokol` | [sokol](https://github.com/floooh/sokol) von Andre Weissflog, kommt mit Box3D | wie Box3D | zlib, `box3d/extern/sokol/LICENSE` |
| `imgui` | [Dear ImGui](https://github.com/ocornut/imgui) von Omar Cornut, nur die Kerndateien | Version 1.92.7 | MIT, `imgui/LICENSE.txt` |

Zum Aktualisieren die Dateien aus dem neuen Stand an dieselbe Stelle kopieren, die Ergänzung unten wieder einsetzen
und den Stand hier eintragen. Wer Nebenan in ein eigenes Projekt einbindet, das schon ein Ziel `box3d::box3d` hat,
verwendet dessen Box3D.

## Ergänzung in Box3D

`b3World_Reserve` macht in einer laufenden Welt Platz für mehr Körper, Formen und Kontakte, so wie
`b3WorldDef::capacity` beim Anlegen. Dazu kommt `b3DynamicTree_Reserve` für den Suchbaum. Ist ein Array von Box3D
voll, kopiert Box3D es in ein doppelt so großes. In einer Welt mit Hunderttausenden Formen hält das den Aufruf, der es
auslöst, über 100 ms auf. Nebenan reserviert deshalb beim Aufbau Platz für noch einmal so viel, siehe `nbKeepRoom`.
An den Ergebnissen ändert das nichts.

Die Ergänzung steht in `include/box3d/box3d.h`, `include/box3d/collision.h`, `src/physics_world.c` und
`src/dynamic_tree.c`, jeweils mit „Added for Nebenan“ markiert. `B3_HAS_WORLD_RESERVE` zeigt sie an. Fehlt sie, etwa
mit einem eigenen Box3D, läuft Nebenan wie zuvor und reserviert nur seine eigenen Arrays.
