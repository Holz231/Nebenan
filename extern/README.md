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

**Formen gesammelt einfügen.** Box3D sortiert jede Form in ihren Suchbaum, mit einer Suche von der Wurzel aus.
`b3World_BeginShapeBatch` und `b3World_EndShapeBatch` klammern das Anlegen von Formen, das Erstarren und Auftauen von
Körpern und das Wiedereinschalten: Dazwischen warten ihre Proxys außerhalb der Bäume, jeder in einer Liste für seinen
Baum, dem statischen oder dem der bewegten Körper. Am Ende bekommen sie einen eigenen Teilbaum, geteilt wie beim Neubau
eines Baums, und der geht mit einer einzigen Suche dorthin, wo eine Form mit seinen Grenzen hinginge. Liegen die Formen
nicht beisammen, wäre seine Box groß und jede Abfrage müsste hinein. Dann teilt `b3DynamicTree_InsertBatch` sie entlang
einer Morton-Kurve in Hälften, bis jeder Teil kompakt ist: die Oberfläche seiner Box nicht größer als die seiner Formen
zusammen. Jeder Teil bekommt seinen eigenen Teilbaum. Klammern lassen sich schachteln, die Formen gehen hinein, wenn die
äußerste endet. Nebenan klammert so jeden Einbau, beim Laden alle Bruchstücke eines Hauses, die als ein Teil
hineingehen, bei einem Einschlag seine Bruchstücke, und den Schutt, der in einem Update erstarrt oder auftaut. Zwischen
den beiden Aufrufen darf die Welt nicht rechnen, nicht abgefragt werden, ihren statischen Baum nicht neu bauen und keine
Aufnahme beginnen. Bis auf die Abfragen prüft Box3D das mit Asserts. Aufnahmen merken sich die Klammern nicht, die
Wiedergabe fügt einzeln ein. Das simuliert dasselbe, seit die Ergebnisse nicht mehr von der Reihenfolge in den Bäumen
abhängen.

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

**Statische Blätter kürzer herausnehmen.** Box3D rechnete beim Herausnehmen einer Form jeden Vorfahren bis zur Wurzel
neu. Formen des statischen Baums gehen jetzt über `b3DynamicTree_DestroyStaticProxy`, das beim ersten Vorfahren aufhört,
der so herauskommt, wie er war: Die darüber hängen nur von ihm ab. Im statischen Baum ist jeder innere Knoten die
Vereinigung seiner Kinder, denn dort wird kein Rahmen vergrößert, und der gesammelte Einbau baut jeden inneren Knoten aus
seinen Kindern. Deshalb bleibt der Baum genau so, wie ihn der ganze Weg ließe. Mit Validierung prüft jeder vorzeitige
Halt, dass der ganze Weg die Vorfahren darüber gelassen hätte, wie sie sind. Der dynamische und der kinematische Baum
gehen weiter den ganzen Weg.

**Worker nach Arbeit.** Jede Stufe eines Schritts weckte alle Worker und wartete auf sie, auch wenn nur ein paar hundert
Kontakte wach waren, und das kostete mehr als die Arbeit selbst. Jetzt weckt ein Schritt einen Worker für je 250 wache
Kontakte und Gelenke (`B3_CONTACTS_PER_WORKER`, `b3GetStepWorkerCount`), darunter rechnet der aufrufende Thread allein.
Eine Parallel-for-Schleife mit nur einer Aufgabe läuft gleich auf dem aufrufenden Thread, und rechnet er allein, baut
er auch die Bäume selbst neu und teilt Inseln selbst. Die Ereignisse aller Worker leert und sammelt der Löser weiter,
auch die der Worker, die er nicht geweckt hat.

**Explosionen in fester Reihenfolge.** `b3World_Explode` stieß die Formen im Umkreis in der Reihenfolge an, in der der
Baum der bewegten Körper sie fand. Davon hing ab, welche schlafende Insel zuerst aufwachte und in welcher Reihenfolge
sich die Stöße auf einen Körper mit mehreren Formen addierten, und damit die letzten Bits. Jetzt sammelt die Abfrage
die Formen, und sie bekommen ihren Stoß in der Reihenfolge ihrer Ids. Nebenan selbst löst keine Explosion aus, die Demo
und der Benchmark schon.

**Blöcke auf Cache-Zeilen.** Box3D richtete jeden Block auf 16 Byte aus. Ein Körperzustand im Löser ist 64 Byte groß,
so groß wie eine Cache-Zeile, lag damit aber meist über zweien: In der Stadt mit einfacher Bruchstückgröße fing das Array
der wachen Zustände in 263 von 300 Schritten mitten in einer Zeile an. Dann schreiben zwei Worker, die in einer Farbe des
Graphen benachbarte Körper rechnen, in dieselbe Zeile. Jetzt beginnt jeder Block auf 64 Byte (`B3_ALIGNMENT` in
`src/core.h`), auch in der Arena des Lösers.

**Kleinerer Rand um bewegte Formen.** Box3D bläht die Box jeder bewegten Form im Suchbaum um ein Achtel ihrer Ausdehnung
auf, höchstens 5 cm, damit ein Körper sie nicht in jedem Schritt verlässt. Im dichten Schutt überlappen sich so die
Boxen vieler Formen, die einige Zentimeter auseinanderliegen, und jedes solche Paar ist ein Kontakt, den die Kollision
in jedem Schritt prüft: Bei einfacher Bruchstückgröße führte Box3D 16 400 wache Kontakte, von denen 3 100 berührten.
Jetzt ist der Rand höchstens 5 mm (`B3_MAX_SHAPE_AABB_MARGIN` in `include/box3d/constants.h`). Ob ein Kontakt
rechtzeitig da ist, ändert das nicht, denn die Box einer Form enthält immer den spekulativen Abstand, damals 2 cm, und
erst darin bekommt ein Kontakt Punkte. Kontakte entstehen aber später und verschwinden früher, so ändern sich ihre
Reihenfolge und die Ergebnisse, deterministisch bleiben sie. `B3_MAX_AABB_MARGIN` bleibt die Grenze, mit der Mesh- und
Höhenfeld-Abfragen rechnen.

**Kleinerer spekulativer Abstand.** Box3D gibt einem Kontakt Punkte, sobald sich zwei Formen bis auf den spekulativen
Abstand nähern, und der Löser rechnet mit ihnen, damit Körper, die aufeinander zufliegen, sich im nächsten Schritt nicht
durchdringen. Er war das Vierfache von `B3_LINEAR_SLOP`, 2 cm, und um ihn ist auch die Box jeder Form im Suchbaum
größer. Im dichten Schutt liegen viele Stücke wenige Zentimeter auseinander, und jedes solche Paar war ein Kontakt mit
Punkten, den Kollision und Löser in jedem Schritt rechneten, ohne dass sich etwas berührte. Jetzt ist er das Doppelte,
1 cm (`B3_SPECULATIVE_DISTANCE` in `include/box3d/constants.h`). Zwei Stücke, die schneller als 0,6 m/s aufeinander
zufliegen, haben dadurch öfter noch keine Punkte, wenn sie sich treffen, und geraten etwas tiefer ineinander, bevor der
Löser sie trennt. Gegen statische Formen verfolgt Box3D schnelle Trümmer weiter über den ganzen Schritt
(`debrisSweepDistance`).

**Grenzen der Hülle in der Form.** Nach jedem Schritt rechnet Box3D die Box jeder bewegten Form neu, aus der Box ihrer
Hülle im eigenen Rahmen. Die lag in den Hüllendaten, an einer anderen Stelle im Speicher als die Form, und kostete für
jede Form in jedem Schritt einen Cache-Fehlzugriff. Jetzt liegt sie in der Form neben dem Zeiger auf die Hülle, in der
Union, so bleibt eine Form 216 Byte groß (`b3Shape::hullBounds` in `src/shape.h`). Box3D setzt sie überall, wo es den
Zeiger setzt.

**Kontakt nach knappem Fehlschlag nicht doppelt bauen.** Für zwei Hüllen merkt sich Box3D das trennende Merkmal und
den Abstand. Weicht der Abstand um mehr als `B3_LINEAR_SLOP` davon ab, 5 mm, baut Box3D den Kontakt aus dem gemerkten
Merkmal, verwirft ihn, sucht alle Trennachsen neu und baut den Kontakt für das gefundene Merkmal. Ist es dieselbe
Fläche mit demselben Eckpunkt, ist auch der Kontakt derselbe, denn er hängt von nichts anderem ab. Dann behält Box3D
jetzt den ersten (`b3CollideHulls` in `src/convex_manifold.c`).

**Kontakt-Schleife lädt weiter voraus.** Die Schleife der Kollision über alle wachen Kontakte holte jeden Kontakt vier
Plätze voraus in den Cache, wartete dann aber auf die Boxen seiner beiden Formen und die Daten seiner beiden Körper, die
verstreut im Speicher liegen. Jetzt holt sie den Kontakt acht Plätze voraus, und vier Plätze voraus, wenn er schon im
Cache ist, auch seine Boxen, die Daten seiner Körper und, wenn er berührt, seine Punkte (`b3PrefetchContactData` in
`src/physics_world.c`). Weiter voraus brachte weniger.

**Getrennte Hüllen ohne neuen Test.** Zwei Hüllen, die beim letzten Test eine Fläche der einen auseinanderhielt, testete
Box3D neu, sobald sich ihre Körper zueinander mehr als 1 cm bewegt oder gedreht hatten. Jetzt merkt es sich den Abstand
zur Fläche (`b3SATCache::gap` in `include/box3d/types.h`) und die Grenzen der anderen Hülle als Mitte und halbe
Diagonale (`b3ConvexContact` in `src/contact.h`). Daraus folgt, wie nah die andere Hülle der Fläche seitdem höchstens
gekommen ist: um die Verschiebung der Mitte und den Bogen der halben Diagonale. Bleibt der Abstand damit mehr als 1 mm
über dem spekulativen, fände der Test die Hüllen wieder getrennt, und Box3D erledigt nur seine Buchführung, die gemerkte
Lage und den Zwischenspeicher, als hätte es getestet (`b3CollideTask` in `src/physics_world.c`). Der Rand von 1 mm liegt
hundertfach über der Rechenungenauigkeit des Abstands. Über ein Kantenpaar geht das nicht, dessen Achse dreht sich mit
den Körpern.

**Zweistufige Kontakt-Schleife.** Die Schleife der Kollision entscheidet für jeden Kontakt erst nur, ob er nicht mehr
überlappt, wiederverwendet wird, beweisbar getrennt ist oder getestet werden muss. Die zu testenden sammelt sie, und eine
zweite Schleife testet sie und holt dabei ihre Formen vier und die Köpfe ihrer Hüllen zwei Kontakte voraus
(`b3PrefetchContactShapes` und `b3PrefetchContactHulls`). Vorher wartete der Test auf beides nacheinander, denn erst die
Form sagt, wo die Hülle liegt. Jeder Kontakt wird für sich gerechnet, die Reihenfolge ändert nichts.

**Kontakte für den Löser vorausladen.** Bevor die Teilschritte rechnen, liest `b3PrepareContacts_Convex` für jeden
berührenden Kontakt den Kontakt, seine Punkte und Masse und Trägheit seiner beiden Körper, alles von verstreuten
Stellen. Jetzt holt die Schleife die Kontakte zwei breite Bedingungen voraus und eine voraus, wenn diese Kontakte schon
im Cache sind, ihre Punkte und Körperdaten (`b3PrefetchPrepareContact` und `b3PrefetchPrepareData` in
`src/contact_solver.c`).

**Körper vorausladen.** `b3Body_Prefetch` holt den Eintrag eines Körpers in den Cache, `b3Body_PrefetchMotion` Lage
und Geschwindigkeiten eines wachen Körpers, wofür es den Eintrag liest (`src/body.c`). Beide ändern nichts. Nebenan geht
in jedem Update alle bewegten Trümmerteile durch, fragt Box3D nach ihren Körpern und holt damit die Körper der nächsten
Teile voraus.

**Kinder im Baum vorausladen.** Die Paarsuche geht die Bäume mit einem Stapel durch. Die beiden Kinder eines Knotens
liegen in einer Cache-Zeile, und die fordert sie jetzt schon an, wenn sie die beiden auf den Stapel legt, statt erst,
wenn sie sie wieder herunternimmt (`b3CollideProxyAndSubtree` und `b3VisitPair` in `src/broad_phase.c`).

**Abschluss der Körper vorausladen.** Nach dem Löser geht `b3FinalizeBodiesTask` alle wachen Körper durch und liest für
jeden seinen Eintrag, seine Insel und seine Formen samt deren Boxen, alles von verstreuten Stellen. Jetzt holt die
Schleife den Eintrag acht Körper voraus und vier voraus die Insel, die Felder der ersten Form, die sie liest, und deren
Box (`b3PrefetchFinalizeBody` und `b3PrefetchFinalizeData` in `src/solver.c`).

**Kontakte jeder Farbe nach Körpern ordnen.** Mit mehreren Threads teilt Box3D die Körper und die Kontakte jeder Farbe
in Blöcken unter den Workern auf. Ein Körper hat Kontakte in vielen Farben, und so wanderte sein Zustand von Farbe zu
Farbe in den Cache eines anderen Kerns. Jetzt merkt sich die Kontakt-Schleife für jeden Kontakt den kleineren Index
seiner wachen Körper in einem Feld nach Kontakt (`b3World::contactSortKeys`). Ob er sich geändert hat, sieht sie an
einer Kopie im Kontakt (`b3Contact::sortKey`), den sie ohnehin im Cache hat, und nur dann schreibt sie das Feld. Der
Löser verteilt die Kontakte jeder Farbe damit auf 64 Fächer (`b3OrderColorContacts` in `src/solver.c`), und der Worker,
der einen Bereich von Körpern integriert, rechnet in jeder Farbe vor allem deren Kontakte. In einer Farbe teilen sich
zwei Kontakte keinen beweglichen Körper, die Reihenfolge ändert nichts. Mit einem Thread ordnet der Löser nicht.

Die erste, die zweite, die vierte bis achte, die zehnte und die dreizehnte bis zweiundzwanzigste ändern an den
Ergebnissen nichts. Die dritte und die neunte ändern sie in den letzten Bits, dafür hängen sie nicht mehr davon ab, in
welcher Reihenfolge Formen in die Bäume kommen. Die elfte ändert sie, weil Kontakte zu anderen Zeitpunkten entstehen,
die zwölfte, weil sie weniger Punkte bekommen. Die Ergänzungen stehen in `include/box3d/box3d.h`,
`include/box3d/types.h`, `include/box3d/constants.h`, `src/core.h`, `src/core.c`, `src/shape.h`, `src/shape.c`,
`src/body.c`, `src/contact.h`, `src/contact.c`, `src/contact_solver.c`, `src/convex_manifold.c`, `src/world_snapshot.c`,
`src/recording.c`, `src/solver.c`, `src/broad_phase.h`, `src/broad_phase.c`, `src/dynamic_tree.h`, `src/dynamic_tree.c`,
`src/parallel_for.c`, `src/physics_world.h` und `src/physics_world.c`, jeweils mit „Added for Nebenan“ markiert.
`B3_HAS_UNIQUE_HULLS`, `B3_HAS_SHAPE_BATCH`, `B3_HAS_SHAPE_CONTACT_LISTS`, `B3_HAS_EXTERNAL_HULLS` und
`B3_HAS_BODY_PREFETCH` zeigen die zweite, die vierte, die fünfte, die sechste und die neunzehnte an. Fehlen sie, etwa
mit einem eigenen Box3D, läuft Nebenan wie zuvor, nur wachsen Box3Ds Arrays dann durch Umkopieren, die Hüllen gehen
durch die Tabelle, jede Form geht einzeln in den Baum, jedes stehende Bruchstück bekommt einen eigenen Körper, Box3D
hält eine Kopie jeder Hülle, und Nebenan holt die Körper der Trümmer nicht voraus.