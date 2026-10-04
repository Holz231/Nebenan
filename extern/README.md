# Mitgelieferte Bibliotheken

Diese Bibliotheken liegen bei, damit der Build ohne Download auskommt. Bis auf die Ergänzungen in Box3D unten sind
sie unverändert.

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

**Hüllen ohne Hüllen-Datenbank.** Box3D legt jede Hülle in einer Hash-Tabelle ab, damit gleiche Hüllen nur einmal
gespeichert werden. Wird die Tabelle voll, ordnet sie alle Hüllen in eine doppelt so große ein, und das kann sie nicht
an Ort und Stelle: Mit 246 000 Hüllen hielt das ein Update auf der VM 0,4 s auf. Mit `b3ShapeDef::uniqueHull` behält
eine Form stattdessen eine eigene Kopie ihrer Hülle, ohne Tabelle. Nebenan setzt das für alle Bruchstücke. Ihre
Hüllen sind verschieden, solange jedes Objekt einen eigenen `seed` hat. Bekommt eine solche Form mit `b3Shape_SetHull`
eine neue Hülle, teilt sie diese wieder über die Tabelle. Aufnahmen speichern das Feld nicht, ihr Format bleibt. Bei
der Wiedergabe teilen die Formen ihre Hüllen, was dasselbe simuliert.

**Durchschlag-Schutz über den ganzen Schritt.** Box3Ds CCD rechnet für einen schnellen Körper die Zeit bis zum
Treffer mit jeder statischen Form in seiner Nähe aus, in der Reihenfolge des statischen Baums. Bisher nur bis zum
frühesten Treffer davor, und ein kürzeres Intervall verschiebt in den letzten Bits, wo die Suche landet. So hing das
Ergebnis davon ab, wie die Formen in den Baum gekommen waren. Jetzt rechnet es jede Zeit über den ganzen Schritt
(`src/solver.c`). Das kostet nichts Messbares, CCD braucht in der Stadt so oder so rund 1,6 % der Zeit.

**Statische Formen gesammelt einfügen.** Box3D sortiert jede Form in seinen Suchbaum, mit einer Suche von der Wurzel
aus. `b3World_BeginStaticBatch` und `b3World_EndStaticBatch` klammern das Anlegen statischer Formen: Dazwischen warten
ihre Proxys außerhalb des statischen Baums. Am Ende bekommen sie einen eigenen Teilbaum, geteilt wie beim Neubau eines
Baums, und der geht mit einer einzigen Suche dorthin, wo eine Form mit seinen Grenzen hinginge. Nebenan klammert so
jeden Einbau, beim Laden alle Bruchstücke eines Hauses. Zwischen den beiden Aufrufen darf die Welt nicht rechnen, nicht
abgefragt werden, ihren statischen Baum nicht neu bauen und keine Aufnahme beginnen. Bis auf die Abfragen prüft Box3D
das mit Asserts. Aufnahmen merken sich die Klammern nicht, die Wiedergabe fügt einzeln ein. Das simuliert dasselbe, seit
die Ergebnisse nicht mehr von der Reihenfolge im Baum abhängen.

**Kontakte pro Form.** Box3D führt die Kontakte jedes Körpers in einer Liste. Um eine Form zu löschen, ging es bisher
alle Kontakte ihres Körpers durch und suchte die der Form heraus. Jetzt führt jede Form zusätzlich ihre eigene Liste
(`b3Shape::headContactKey`, `b3Contact::shapePrevKey` und `shapeNextKey`). Das Löschen einer Form, das Zurücksetzen
ihres Proxys und `b3Shape_GetContactData` gehen nur noch über die Kontakte der Form. Die Liste hält dieselben Kontakte
in derselben Reihenfolge wie die des Körpers. Nebenan hängt damit alle stehenden Bruchstücke eines Zerstörbaren an einen
gemeinsamen statischen Körper, statt jedem einen eigenen zu geben, und das Löschen eines Bruchstücks wird nicht teurer.
Mit Validierung prüft `b3ValidateContacts` die Listen.

**Hüllen, die beim Besitzer bleiben.** Mit `b3ShapeDef::uniqueHull` kopierte Box3D die Hülle jedes Bruchstücks, obwohl
Nebenan dieselben Ecken und Ebenen in seiner eigenen Form hielt. Mit `b3ShapeDef::externalHull` benutzt eine Form die
Hülle, die sie bekommt, an Ort und Stelle und gibt sie nie frei (`b3_externalHull` in `b3Shape::flags`). Die Hülle muss
unverändert dort bleiben, solange die Form besteht. Nebenan baut die Hülle direkt in die Form jedes Bruchstücks, ihre
Punkte und Ebenen sind dessen Ecken und Flächen, und löscht die Box3D-Formen vor den Bruchstücken. So liegt die
Geometrie nur einmal im Speicher, und beim Laden und beim Umhängen eines Bruchstücks auf einen anderen Körper wird
nichts kopiert. Eine Hülle mit Transformation der Form bekommt wie bei `uniqueHull` eine eigene Kopie, ebenso eine Form,
die aus einem Snapshot kommt, denn der Besitzer ihrer Hülle gehört nicht dazu. Aufnahmen speichern das Feld nicht.

Die erste, die zweite, die vierte, die fünfte und die sechste ändern an den Ergebnissen nichts. Die dritte ändert sie in
den letzten Bits, dafür hängen sie nicht mehr davon ab, in welcher Reihenfolge Formen in den statischen Baum kommen. Die
Ergänzungen stehen in `include/box3d/box3d.h`, `include/box3d/types.h`, `src/core.c`, `src/shape.h`, `src/shape.c`,
`src/contact.h`, `src/contact.c`, `src/world_snapshot.c`, `src/recording.c`, `src/solver.c`, `src/broad_phase.h`,
`src/broad_phase.c`, `src/dynamic_tree.h`, `src/dynamic_tree.c` und `src/physics_world.c`, jeweils mit „Added for
Nebenan“ markiert. `B3_HAS_UNIQUE_HULLS`, `B3_HAS_STATIC_BATCH`, `B3_HAS_SHAPE_CONTACT_LISTS` und
`B3_HAS_EXTERNAL_HULLS` zeigen die zweite, die vierte, die fünfte und die sechste an. Fehlen sie, etwa mit einem eigenen
Box3D, läuft Nebenan wie zuvor, nur wachsen Box3Ds Arrays dann durch Umkopieren, die Hüllen gehen durch die Tabelle,
jede Form geht einzeln in den Baum, jedes stehende Bruchstück bekommt einen eigenen Körper, und Box3D hält eine Kopie
jeder Hülle.