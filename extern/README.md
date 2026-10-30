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
abhängen. Seit `active-ragdoll-6` wartet in der Klammer auch, was ein Typwechsel an anderen Körpern nachführt: Verlässt
ein Körper seinen Satz, rückt der letzte des Satzes an seinen Platz, und dessen Kontakte merken sich, wo sein Sim liegt
(`encodedBodySimA` und `B`). Box3D ging dafür sofort alle Kontakte des gerückten Körpers durch, einen nach dem anderen,
und wartete auf jeden. Erstarrt Schutt in einem Schub, rückte so mit jedem Stück ein anderer Körper mit rund 16
Kontakten. Jetzt merkt sich die Welt in der Klammer nur die gerückten Körper (`b3World::movedBodies`), und am Ende der
äußersten Klammer geht `b3RefreshMovedBodies` (`src/body.c`) die Listen von acht Körpern zugleich durch, so sind acht
Kontakte zugleich unterwegs. Die Indizes kommen genauso heraus wie vorher, auch für einen Körper, der zweimal rückte. In
den 12 langsamsten Bildern eines Laufs der Stadt mit Figuren kostet das Nachführen auf der VM 0,14 statt 0,5 bis 0,7 ms.

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
zur Fläche (`b3SATCache::gap` in `include/box3d/types.h`), die Ebene der Fläche und die Grenzen der anderen Hülle als
Mitte und halbe Ausdehnung, jeweils im Rahmen ihres Körpers (`b3ConvexContact` in `src/contact.h`). Daraus folgen zwei
untere Schranken für den Abstand jetzt. Erstens der gemerkte Abstand, verschoben um die Bewegung der Mitte entlang der
Normalen, abzüglich dessen, was die Drehung der Normalen im Rahmen der anderen Hülle aus der halben Ausdehnung macht.
Zweitens die Grenzen, wie sie jetzt liegen, gegen die Ebene. Bleibt die größere mehr als 1 mm über dem spekulativen
Abstand, fände der Test die Hüllen wieder getrennt, und Box3D erledigt nur seine Buchführung, die gemerkte Lage und den
Zwischenspeicher, als hätte es getestet (`b3CollideTask` in `src/physics_world.c`). Der Rand von 1 mm liegt hundertfach
über der Rechenungenauigkeit des Abstands. Über ein Kantenpaar geht das nicht, dessen Achse dreht sich mit den Körpern.
Mit Ebene und Ausdehnung ist ein Kontakt 248 statt 232 Byte groß. Die Schleife holt deshalb auch das Ende dieser Daten
voraus, das in einer fünften Cache-Zeile liegen kann (`b3PrefetchContact`). Bis `physik-32` war die Schranke gröber: die
volle Verschiebung der Mitte und der Bogen der halben Diagonale, ohne die Ebene.

**Zweistufige Kontakt-Schleife.** Die Schleife der Kollision entscheidet für jeden Kontakt erst nur, ob er nicht mehr
überlappt, wiederverwendet wird, beweisbar getrennt ist oder getestet werden muss. Die zu testenden sammelt sie, und
eine zweite Schleife testet sie und holt dabei ihre Formen vier Kontakte voraus, die Köpfe ihrer Hüllen zwei und den
Rest der Hüllen, bis 2 KB je Hülle, einen (`b3PrefetchContactShapes`, `b3PrefetchContactHulls` und
`b3PrefetchContactHullArrays`). Vorher wartete der Test auf alles nacheinander, denn erst die Form sagt, wo die Hülle
liegt, und erst deren Kopf, wo ihre Arrays liegen. Die Arrays holt die Schleife seit `physik-34` voraus, davor wartete
der Test noch auf sie, beim Stützpunkt einer gemerkten Fläche, beim Schnitt der Flächen und bei der vollen Suche nach
der Trennachse. Jeder Kontakt wird für sich gerechnet, die Reihenfolge ändert nichts.

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

**Worker warten im Schritt.** Der eingebaute Scheduler weckte für jede Aufgabe einen Worker über eine Semaphore, und
weil die Worker zwischen den parallelen Abschnitten eines Schritts schliefen, ging jedes Wecken über den Kernel und
jeder Abschnitt begann spät. Jetzt warten die Worker, solange `b3World_Step` läuft, mit Pausen auf die nächste Aufgabe
(`b3SchedulerSetStepActive` in `src/scheduler.c`), und eine neue Aufgabe weckt nur, wenn ein Worker schläft. Zwischen
den Schritten schlafen sie wie bisher. Die Rolle des Dirigenten im Löser behält beim eingebauten Scheduler der
aufrufende Thread, der die Stufen gerade vorbereitet hat (`b3Solve` in `src/solver.c`). Welcher Thread was rechnet,
ändert nichts.

**Weg nach oben beim Einfügen kürzer.** Nach dem Einfügen eines Blatts rechnete Box3D jeden Vorfahren bis zur Wurzel neu
und versuchte auf jeder Ebene zu rotieren. Jetzt hört der Weg in `b3InsertLeaf` und beim gesammelten Einbau beim ersten
Vorfahren auf, der mit Box, Höhe und Merkern so herauskommt, wie er war (`src/dynamic_tree.c`): Die darüber hängen nur
von ihm ab. Es entfallen nur die Rotationen über ihm, und die fanden so weit über einem neuen Blatt kaum etwas. So kommt
erstarrender Schutt billiger in den statischen Baum, in dem die ganze Stadt steht. Die Bäume sehen dadurch etwas anders
aus, die Ergebnisse hängen nicht davon ab.

**Manifolds ohne Sperre außerhalb der Kontakt-Schleife.** Box3D sperrte seinen Allokator für die Manifolds der Kontakte
bei jedem Holen und Freigeben, weil die Kontakt-Schleife beides auf mehreren Threads tut. Erstarren und Auftauen bauen
die Kontakte eines Körpers aber auf dem aufrufenden Thread ab, und dort kostete die Sperre Zeit, ohne dass je ein
anderer Thread wartete. Jetzt setzt `b3Collide` `b3World::manifoldsShared`, solange die Kontakt-Schleife auf mehreren
Threads läuft, und nur dann sperrt der Allokator (`src/physics_world.h` und `src/physics_world.c`).

**Ganze Inseln pro Thread.** Mit mehreren Threads rechnete Box3D jeden Schritt in Stufen: die Bedingungen vorbereiten,
in jedem Teilschritt die Geschwindigkeiten integrieren, Warmstart, Lösen und Entspannen Farbe für Farbe, dazwischen die
Lagen integrieren, am Ende der Rückprall und das Zurückschreiben der Stöße. Nach jeder Stufe warteten alle Threads, bis
die letzte fertig war, bei ×2 mit 23 Farben über 300-mal pro Schritt. Zwei Inseln teilen sich aber nie einen beweglichen
Körper. Jetzt verteilt der Löser die wachen Inseln auf vier Pakete pro Worker mit etwa gleich viel Arbeit, gewichtet mit
viermal ihren Kontakten und einmal ihren Körpern, die größten Inseln zuerst, jede auf das Paket mit der wenigsten Arbeit
(`b3BuildPacks` in `src/solver.c`). Die Worker und der aufrufende Thread nehmen sich ganze Pakete, die größten zuerst,
und rechnen jedes allein, vom Vorbereiten seiner Bedingungen über alle Teilschritte und den Rückprall bis zum
Zurückschreiben der Stöße, ohne eine einzige Sperre (`b3SolvePacks`). Dafür gehen die konvexen Kontakte jeder Farbe
Paket für Paket, jedes Paket mit leeren Spuren zu ganzen breiten Bedingungen aufgefüllt, damit keine breite Bedingung
zwei Pakete mischt (`b3OrderPackContacts`, eine leere Spur hat die Nummer `B3_NULL_INDEX`, siehe `b3PrepareContactWide`
in `src/contact_solver.c`). Weil ein Paket in den meisten Farben nur wenige breite Bedingungen hat, holt es beim
Vorbereiten die Kontakte, ihre Manifolds und Körper über die Farben hinweg voraus (`b3PreparePackContacts_Convex`). Die
Overflow-Kontakte gehen ebenso Paket für Paket, und die Körper jedes Pakets stehen in einer Liste
(`b3StepContext::packBodies` in `src/solver.h`). All das baut der Löser, bevor die Teilung einer Insel neben ihm
anläuft. Jeder Körper durchläuft seine Schritte in derselben Reihenfolge wie mit Sperren. Den Rückprall rechnet ein
Paket, sobald einer seiner Kontakte welchen hat. Rechnet es ihn nur, weil ein anderes Paket das Zeichen dafür gesetzt
hat, ändert das nichts, weil Box3D Kontakte ohne Rückprall überspringt. Mit Sperren wie bisher rechnet der Löser, wenn
nur ein Thread rechnet, ein Kontakt mit einem Netz in den Farben liegt, Box3D den Rückprall fortpflanzt oder eine Insel
mehr als die Hälfte der Arbeit hält und die Worker für ein Team zu wenige sind, siehe „Ein Team für das größte Paket“.
Bis zu „Gelenke in Paketen und Gebieten“ tat er das auch, solange ein Gelenk in den Farben lag.

**Sperren, solange andere Programme die Kerne belegen.** Verliert ein Thread mitten in einem Paket seinen Kern an ein
anderes Programm, warten am Ende alle auf dieses Paket, während mit Sperren die anderen die Blöcke übernehmen, die er
noch nicht angefangen hat. Jetzt misst der aufrufende Thread nach seinem letzten Paket, wie lange er auf die anderen
wartet. Ein Warten ab 1 ms, das mindestens die Hälfte der Zeit der Pakete ausmacht, zählt dreifach, jeder Schritt ohne
eines zieht eins ab, und ab neun rechnet der Löser 120 Schritte lang mit Sperren, kommt es innerhalb der doppelten Länge
danach wieder, doppelt so lange, bis zu 960 Schritte (`b3UpdatePackHold` in `src/solver.c`, `b3World::packHoldSteps` in
`src/physics_world.h`). Ein einzelnes langes Warten, wie es ohne Last ab und zu vorkommt, zählt so nicht. Die Ergebnisse
sind mit und ohne Sperren dieselben. `b3Counters::packHoldSteps` sagt, wie viele Schritte der Löser noch mit Sperren
rechnet. Der Test mit dem Thread-Pool verlangt ganze Pakete nur, wenn keine solche Strecke lief: Auf einem macOS-Runner
rechnete keiner der gemessenen Schritte ein ganzes Paket, und die gibt es dort nur in den ersten 90 Schritten nach den
Granaten, die eine solche Strecke von 120 Schritten ganz abdecken kann.

**CCD-Vortest gegen die Flächen der statischen Hülle.** Box3Ds CCD rechnete für jede statische Form, die die Bahn eines
schnellen Körpers berührt, die Zeit bis zum Treffer aus. Bei kleinen Bruchstücken unter Beschuss sind das Hunderte pro
Schritt, und fast keine trifft. Ein Körper bewegt seinen Schwerpunkt beim Sweep geradlinig, und jede seiner Formen
bleibt in einer Kugel um diesen Schwerpunkt. Liegt diese Kugel am Anfang und am Ende des Schritts jenseits derselben
Flächenebene einer statischen Hülle, kann die Form die Hülle dazwischen nicht erreichen, und der Test fällt weg
(`b3IsBeyondHullFace` und `b3GetShapeReach` in `src/solver.c`). Der Abstand von vier linearen Slops ist ein Vielfaches
dessen, was die Zeit bis zum Treffer noch als Berührung zählt. Dazu liest die CCD nicht mehr für jeden Test zweimal die
Uhr, für Warnungen bei langen Tests, die upstream seit [#179](https://github.com/erincatto/box3d/pull/179) auch nicht
mehr hat. Die Ergebnisse sind dieselben.

**Löser mit acht Spuren.** Box3D rechnete die Kontakte jeder Farbe in Bündeln zu vier, auch auf x86 nur mit SSE2. Jetzt
stehen die breiten Kontakt-Bedingungen in `src/contact_solver_wide.inl`, die `src/contact_solver.c` mit vier Spuren
übersetzt und `src/wide8.c` mit acht und AVX2, wie upstream Box3D seit
[#174](https://github.com/erincatto/box3d/pull/174). Eine Welt nimmt beim Anlegen die Breite, die der Prozessor kann
(`b3GetSIMDWidth` und `b3IsAVX2Available` in `src/simd.c`, `b3World::simdWidth`), `b3SetSIMDWidth` erzwingt eine für
Tests, und `BOX3D_AVX2=OFF` lässt die acht Spuren weg. MSVC übersetzt `src/wide8.c` mit `/arch:AVX2`, GCC und Clang nur
die Kernel darin (`B3_AVX2_BEGIN` in `src/core.h`). Eine Spur rechnet in beiden Breiten dieselben Bits: Die Kontakte
einer Farbe teilen sich keinen bewegten Körper, eine leere Spur schreibt nichts zurück, und die Operationen in
`src/simd_avx2.h` runden jede Spur wie die mit SSE2, ohne FMA. Die Pakete aus „Ganze Inseln pro Thread“ füllen ihre
Spuren in der Breite der Welt auf (`b3OrderPackContacts` in `src/solver.c`).

**Wächter für den Schritt auf einem anderen Thread.** `b3World_SetStepGuard` hängt an eine Welt eine Funktion, die
Box3D aufruft, bevor der Thread, der den Wächter gesetzt hat, die Welt anfasst: in den vier Funktionen, die eine Welt
nachschlagen, und in `b3Body_IsValid`, `b3Shape_IsValid`, `b3Joint_IsValid` und `b3Contact_IsValid`, über die jeder
Zugriff von außen geht. Nebenan wartet darin auf den Schritt, den sein eigener Thread rechnet, siehe `nbWorld_Step`,
und nimmt den Wächter danach wieder weg. Jeder andere Thread geht durch, der Schritt-Thread und die Worker
eingeschlossen. Ein Thread erkennt sich an einer Nummer, die er beim ersten Mal zieht (`b3GetThreadToken` in
`src/core.c`), und die Welt hält die Nummer des Besitzers in einem atomaren Feld (`b3World::stepGuardOwner`), null ohne
Wächter. Ohne Wächter kostet ein Zugriff damit ein Laden mehr, mit Wächter dazu den Blick auf die eigene Nummer.
`b3World_IsValid` liest nur, was kein Schritt ändert, und wartet nicht.

**Zeiten und Inseln für das Profil.** `b3Profile` hat zwei Felder mehr für die Teile des Schritts, die auf dem
aufrufenden Thread allein laufen: `createContacts`, das Anlegen der neuen Kontakte im Paar-Update in der Reihenfolge der
Paare, und `contactStates`, die Kontakte, die nach der Kontakt-Schleife zu berühren beginnen, damit aufhören oder sich
trennen, mit ihren Inseln und Farben. `b3Counters` sagt mit `islandWork` und `largestIslandWork`, wie viel Arbeit die
wachen Inseln im letzten Schritt hatten, gewogen wie in „Ganze Inseln pro Thread“, viermal die Kontakte und einmal die
Körper, und wie viel davon die größte, die ein Paket einem Thread allein gibt. `b3BuildPacks` merkt sich beides, und in
Schritten ohne Pakete zählt `b3MeasureIslandWork` in `src/solver.c` die Inseln, nur in Welten mit Workern, denn in einer
ohne Worker könnten es viele sein. Nebenan liest beides nach jedem Schritt für `nbWorld_GetProfile`.

**Knoten im Überlauf.** Findet der Constraint-Graph für einen Kontakt keine Farbe, in der beide Körper frei sind, kommt
er in den Überlauf, und den löste der aufrufende Thread vor jeder Runde über die Farben allein, Kontakt für Kontakt mit
dem skalaren Kern, während die Worker warteten. Beim Abriss sind das fast nur die Kontakte von Knoten, großen
abgebrochenen Teilen aus Dutzenden bis Hunderten Bruchstücken mit Hunderten Trümmern daran, siehe „Der Überlauf beim
Abriss“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md). Jetzt baut der Löser in jedem Schritt aus dem Überlauf
Bündel (`b3BuildKnots` in `src/solver.c`). Knoten ist von den beiden wachen Körpern eines Kontakts der mit mehr
Kontakten im Überlauf, sobald er zwei hat. Bis zu acht seiner Kontakte mit verschiedenen Partnern stehen in einem
Bündel, einer breiten Bedingung zu acht Spuren oder zweien zu vier (`b3KnotBatch` in `src/solver.h`). Ein Kontakt kommt
in das älteste Bündel seines Knotens, das noch nicht voll ist und nach dem letzten seines Partners kommt. Bis
`kollaps-7` kam der erste Kontakt mit einem Partner in die ersten Bündel, der zweite in die nächsten, und am Ende jeder
dieser Runden blieb ein Bündel halb leer. Jede Spur rechnet gegen eine Kopie des Knotens, die ihren Anteil an seiner
Masse und Trägheit trägt, und danach bekommt der Knoten den Mittelwert seiner Kopien, nach „Mass Splitting for
Jitter-Free Parallel Rigid Body Simulation“ von Tonge, Benevolenski und Voroshilov (2012). Eine Spur rechnet dasselbe
wie in den Farben, die Kerne teilen sich die Rechnung einer breiten Bedingung (`b3WarmStartContactW`, `b3PushContactW`,
`b3SolveContactW` und `b3ApplyRestitutionW` in `src/contact_solver_wide.inl`), nur schreiben die Spuren den Knoten nicht
zurück, sondern zählen ihre Änderungen zusammen (`b3WarmStartKnots`, `b3PushKnots`, `b3SolveKnots` und
`b3ApplyKnotRestitution`). Vorbereitet und zurückgeschrieben werden die Bündel mit den konvexen Kontakten der Farben,
hinter denen ihre breiten Bedingungen stehen, über eine Spanne mehr in `b3WidePrepareSpan`, die die Masse der Kopien
skaliert. Die übrigen Kontakte des Überlaufs löst Box3D nach den Knoten weiter einzeln. Die Bündel folgen nur aus der
Reihenfolge des Überlaufs, mit jeder Zahl von Threads und in beiden Breiten sind es dieselben, und die Pakete aus „Ganze
Inseln pro Thread“ rechnen ihre Bündel in derselben Reihenfolge. Ein kinematischer Knoten behält seine Geschwindigkeit.
`b3Profile::overflow` misst, wie lange der aufrufende Thread allein am Überlauf rechnet, und `b3Counters` zählt mit
`knotContactCount` und `knotBatchCount` die Kontakte in Bündeln und die Bündel.

**Ein Team für das größte Paket.** In den Paketen aus „Ganze Inseln pro Thread“ rechnet ein Thread jede Insel allein, und
ein Paket mit deutlich mehr Arbeit als dem Anteil eines Threads bestimmt, wie lange der Löser braucht. Beim Abriss in der
Demo hält es mit 8 Threads ein Drittel der Arbeit, siehe „Größte Insel im Team“ in
[`docs/Optimierungen.md`](../docs/Optimierungen.md). Hält das erste und größte Paket mehr als fünf Viertel des Anteils
eines Workers (`B3_TEAM_SHARE_NUM` und `B3_TEAM_SHARE_DEN` in `src/solver.c`), rechnen es jetzt alle Threads gemeinsam
in den Stufen, Farbe für Farbe mit einer Sperre nach jeder, wie der Löser ohne Pakete. Die Blöcke der Stufen umfassen
dann nur seine Körper und Bedingungen, eine Spanne pro Farbe und eine für seine Bündel von Knoten
(`b3InitRangeBlocks`), und den Überlauf rechnet der aufrufende Thread nur für dieses Paket, das in der Reihenfolge der
Pakete vorne steht (`b3StepContext::team` in `src/solver.h`). Findet ein Worker keine neue Stufe, weil der aufrufende
Thread den Überlauf allein rechnet oder die anderen eine Stufe beenden, nimmt er sich eines der übrigen Pakete und
rechnet es ganz, und was am Ende übrig ist, nimmt der aufrufende Thread (`b3SolveOtherPack`). Eine Insel mit mehr als der
Hälfte der Arbeit bekommt so ebenfalls ein Paket für sich und ein Team, mit Farben über alle Inseln rechnet der Löser sie
nur noch, wenn die Worker für ein Team zu wenige sind. Die Körper des ersten Pakets durchlaufen ihre Schritte in
derselben Reihenfolge wie in einem Paket, die Ergebnisse bleiben dieselben. `b3Counters::teamWork` sagt, wie viel Arbeit
das Paket des Teams im letzten Schritt hatte.

**Zeiten des Teams.** Wo die Bedingungen eines Schritts mit Team hingehen, misst der Löser in jedem achten solchen
Schritt (`B3_TEAM_SAMPLE` in `src/solver.c`): die Zeit des Teams auf dem aufrufenden Thread, den Teil davon, den er
allein am Überlauf des Pakets rechnet und am Ende der Stufen auf die anderen wartet, und über alle Worker, wie lange sie
auf eine neue Stufe warten und wie lange sie die übrigen Pakete rechnen (`b3Profile::teamTime` und die Felder danach).
`b3Counters::teamSample` sagt, ob der letzte Schritt so gemessen hat, dazu seine Threads, die Stufen mit einer Sperre
und wie oft die Worker beim Warten ihren Kern abgegeben haben. Die Worker zählen in ihrem `b3TaskContext`. Nur dieser
Schritt liest an jeder Stufe die Uhr, die anderen kosten nichts. `b3Profile::packWait` misst in jedem Schritt mit
Paketen, wie lange der aufrufende Thread auf die letzten wartet.

**Lage vor dem Schritt im Bewegungsereignis.** Ein `b3BodyMoveEvent` trug nur die Lage eines Körpers nach dem Schritt.
Um zwischen zwei Schritten zu zeichnen, braucht ein Programm auch die Lage davor und den Schwerpunkt im Rahmen des
Körpers, denn bei den Trümmern eines Hauses liegt der Ursprung oft Meter daneben, und nur der Schwerpunkt läuft auf
einer Geraden. `b3FinalizeBodiesTask` merkt sich die Lage, bevor es sie fortschreibt, und schreibt sie mit dem
Schwerpunkt in das Ereignis (`b3BodyMoveEvent::previousTransform` und `b3BodyMoveEvent::localCenter`). Setzt die CCD
einen Körper auf die Lage seines Treffers zurück, liegt er zwischen der Lage davor und dieser. Nebenan nutzt das in
`nbWorld_GetDrawPoses`.

**Normalimpulse wie upstream #189.** Für jeden Punkt eines Kontakts rechneten die breiten Kerne die Geschwindigkeit
beider Körper am Punkt aus, mit einem Kreuzprodukt pro Körper, und brachten den Normalimpuls mit zwei weiteren
Kreuzprodukten auf beide Körper, linear und als Drehung. Wie upstream Box3D seit
[#189](https://github.com/erincatto/box3d/pull/189) rechnen `b3PushContactW` und `b3SolveContactW` in
`src/contact_solver_wide.inl` pro Punkt jetzt nur die beiden Richtungen, in die ein Normalimpuls die Drehungen ändert,
führen die relative Geschwindigkeit entlang der Normalen über die Punkte mit und bringen den linearen Teil der
Normalimpulse einmal nach den Punkten auf, beim Lösen zusammen mit der Reibung. Ein Normalimpuls ändert die lineare
Geschwindigkeit nur entlang der Normalen, die Reibung sieht deshalb dieselbe Geschwindigkeit quer dazu wie vorher.
Anders als upstream begrenzt der Fork den Impuls wie zuvor, denn upstreams `b3MaxW( -negImpulse, -normalImpulses )` kann
zwei Nullen mit verschiedenem Vorzeichen vergleichen, und SSE, NEON und der skalare Weg geben dort verschiedene zurück.
Die Drehreibung um die Normale bleibt ebenfalls, upstreams neue Form spart dort nichts. Die Knoten aus „Knoten im
Überlauf“ rechnen mit denselben Kernen.

**Trennachsen-Suche wie upstream #173 und #177.** Die Suche nach der Trennachse zweier Hüllen (`b3ComputeSeparatingAxis`
in `src/convex_manifold.c`) ist die von upstream Box3D seit [#173](https://github.com/erincatto/box3d/pull/173) und
[#177](https://github.com/erincatto/box3d/pull/177): Ein neuer Test prüft, ob ein Kantenpaar die beste Fläche noch
schlagen kann, die übrigen Paare siebt sie ein zweites Mal gegen einen Punkt der anderen Hülle, und die Trennung der
Fläche, mit der sie beginnt, rechnet sie nur einmal. Die breiten Vektoren dafür, die upstream in `src/simd.h` hat,
stehen am Anfang von `src/convex_manifold.c`, denn die Kerne des Lösers benennen ihre eigenen nach der Zahl der Spuren.
Anders als upstream rechnet der Fork den Kosinus zwischen den beiden Flächen einer Kante jedes Mal aus ihren Normalen,
statt ihn in der Hülle zu speichern. So behalten die Hüllen, die Nebenan selbst baut und in seinen Paketen ablegt, ihr
Format. Und er behält die volle zweite Suche, wenn der Schnitt der Flächen keinen Punkt ergibt, die upstream in #173
gestrichen hat. Ohne sie bekämen zwei Hüllen dann gar keinen Kontakt, auch wenn sie sich überlappen.

**Fehlerbehebungen wie upstream #187.** Wie upstream Box3D seit [#187](https://github.com/erincatto/box3d/pull/187)
bekommen eine Kugel und eine zweite Kugel oder eine Kapsel einen vorausschauenden Kontakt, sobald sie näher als der
spekulative Abstand sind, nicht erst, wenn sie sich berühren (`b3CollideSpheres` und `b3CollideCapsuleAndSphere` in
`src/convex_manifold.c`). Ein Kontakt wird nur noch wiederverwendet, wenn er vorausschauende Punkte hat
(`b3CreateContact` in `src/contact.c`), und schaltet der PreSolve-Callback einen Kontakt ab, vergisst Box3D auch die
Lage, mit der es ihn wiederverwenden würde (`b3UpdateConvexContact`). Nebenan selbst nutzt weder Kugeln noch PreSolve
und lässt die vorausschauenden Kontakte an, für seine Trümmer ändert sich nichts. Ein Spiel mit Kugeln, etwa für
Geschosse, bekommt die Fehlerbehebungen mit.

**Kollision mit AVX2.** `src/convex_manifold_avx2.c` übersetzt die Kollision konvexer Formen aus `src/convex_manifold.c`
ein zweites Mal für AVX2, unter GCC und Clang zwischen `B3_AVX2_BEGIN` und `B3_AVX2_END`, unter MSVC mit `/arch:AVX2`
wie `src/wide8.c`. Dort tragen die Funktionen `AVX2` am Ende ihres Namens, etwa `b3CollideHullsAVX2`, deklariert in
`src/manifold.h`. Eine Welt mit acht Spuren kollidiert mit ihnen, siehe `b3ComputeConvexManifold` in `src/contact.c`.
Ohne FMA schreibt der Übersetzer dieselben Befehle nur in ihrer AVX-Form, die Ergebnisse bleiben dieselben.

**Neue Kontakte und Zustandswechsel vorausladen.** Zwei Schleifen eines Schritts laufen allein auf dem aufrufenden
Thread und warteten vor allem auf Speicher. Die eine legt die neuen Kontakte an (`b3UpdateBroadPhasePairs` in
`src/broad_phase.c`): Für jedes Paar liest `b3CreateContact` die beiden Formen und ihre Körper, die Köpfe ihrer
Kontaktlisten und die Paar-Tabelle und schreibt den neuen Kontakt. Die Paare kommen sortiert, also holt die Schleife
vier Schlüssel voraus die Formen und den Platz in der Tabelle, zwei voraus die Körper, die Köpfe der Listen der Formen
und den Kontakt, den der Pool der Ids als nächsten vergibt, und einen voraus die Köpfe der Listen der Körper
(`b3PrefetchPairShapes`, `b3PrefetchPairBodies` und `b3PrefetchPairBodyContacts`). Die andere arbeitet die Kontakte ab,
die zu berühren beginnen, aufhören oder sich nicht mehr überlappen (`b3Collide` in `src/physics_world.c`). Sie sammelt
sie jetzt zuerst in der Reihenfolge ihrer Bits und holt jeden Kontakt vier und seine Formen und Körper zwei Plätze
voraus (`b3PrefetchContactStateData`). Die Reihenfolge bleibt dieselbe.

**Abbau der Kontakte vorausladen.** Wo Box3D viele Kontakte hintereinander löscht, holt es für den nächsten voraus, was
`b3DestroyContact` außer dem Kontakt selbst anfasst: den Platz des Paars in der Paar-Tabelle, die Manifolds, beide
Körper, die Nachbarn in den Kontaktlisten der Körper und der Formen, die Insel und den Platz im Array der Farbe oder des
Satzes (`b3PrefetchContactDestroy` in `src/contact.c`). Das gilt für die Kontakte eines Körpers, der den Typ wechselt,
abgeschaltet oder gelöscht wird (`b3DestroyBodyContacts` in `src/body.c`), dort mit dem Kontakt danach in der Liste des
Körpers, und für die Paare, deren Boxen sich nicht mehr überlappen, in der Schleife der Zustandswechsel (`b3Collide` in
`src/physics_world.c`). Die Reihenfolge bleibt dieselbe. Seit `active-ragdoll-6` läuft `b3DestroyBodyContacts` drei
Kontakte voraus. Während es einen Kontakt löscht, holt es den dritten danach, für den zweiten danach, was
`b3PrefetchContactDestroy` holt, und für den nächsten, was dessen Löschen über das Geholte erreicht
(`b3PrefetchContactMoves` in `src/contact.c`): die Kontakte, die aus den letzten beiden Plätzen der Arrays seiner Insel
und seiner Farbe oder seines Satzes an seine Stelle rücken, und seine Nachbarn in den Listen der berührenden Kontakte.
Dazu kommen die Zeile hinter dem Platz des Paars, in die die Paare dahinter rücken können, der Eintrag in den Listen der
berührenden Kontakte und die Manifolds, die das Glied der freien Liste aufnehmen, jetzt zum Schreiben. In den 12
langsamsten Bildern eines Laufs der Stadt mit Figuren kostet so ein Kontakt, den das Budget beim Erstarren löscht, auf
der VM 241 statt 293 ns und der ganze Typwechsel 6,3 statt 7,6 µs, in denselben Welten, bitgleich.

**Abfrage nur der statischen Formen.** `b3World_OverlapStaticAABB` findet wie `b3World_OverlapAABB` die Formen in einer
Box, durchsucht aber nur den Baum der statischen Körper. Nebenan sucht damit den Schutt, den ein wegrückendes Stück oder
ein Einschlag freigibt (`nbThawRubbleInBoxes`), denn Schutt ist statisch. Die Abfrage wird nicht aufgezeichnet, wie das
gesammelte Einsortieren der Formen.

**Paarsuche in einem Durchgang.** Box3D suchte neue Paare in zwei parallelen Schleifen nacheinander: erst den Baum der
bewegten Körper gegen die Bäume der statischen und kinematischen, dann gegen sich selbst. Für die zweite sammelte der
aufrufende Thread vorher allein die Geschwisterpaare mit einem bewegten Knoten. Mit vielen Threads warteten die, die
mit ihrem Teil der ersten Schleife fertig waren, an ihrem Ende, und die zweite weckte die Worker noch einmal. Jetzt
laufen beide Suchen in einer Schleife (`b3FindPairsTask` in `src/broad_phase.c`): erst die Startpaare gegen den
statischen und den kinematischen Baum, dann Abschnitte von je 16 Geschwisterpaaren, in denen jeder Thread die bewegten
selbst findet. Welcher Thread ein Paar findet, spielt keine Rolle, denn Box3D sortiert die Paare, bevor sie Kontakte
werden.

**Zweite Suche nur bei Überlappung.** Die volle zweite Suche aus „Trennachsen-Suche wie upstream #173 und #177“ läuft
nur noch, solange keine Fläche die Hüllen trennt (`b3CollideHulls` in `src/convex_manifold.c`). Die erste Suche behält
jedes Kantenpaar, das die Hüllen weiter trennen könnte als die beste Fläche. Trennt schon eine Fläche, kann die volle
Suche also nur ein Paar finden, das sie weniger trennt, und dessen Kontakt behauptete eine Überlappung, wo die Hüllen
auseinanderliegen. In den Städten des Benchmarks ergaben 124 215 solche Suchen genau vier verschiedene Kontakte, alle
mit 3 cm bis 1,2 m Überlappung zwischen Hüllen, die GJK 1 bis 4 mm auseinander oder in einem Punkt berührend fand.
Überlappen sich die Hüllen auf allen Flächen, läuft die volle Suche weiter.

**Vorausladen zum Schreiben.** Die Schleifen aus „Neue Kontakte und Zustandswechsel vorausladen“ und „Abbau der Kontakte
vorausladen“ holten alles nur zum Lesen voraus. Was sie schreiben, hatten aber meist kurz vorher andere Threads gelesen
oder geschrieben: die Paar-Tabelle in der Paarsuche, die Kontakte in der Kollision. Eine solche Zeile kam geteilt an,
und das Schreiben wartete dann ein zweites Mal, bis die Kopien in den anderen Kernen ungültig waren. Jetzt holen die
Schleifen, was sie schreiben, mit `b3PrefetchWrite` voraus (`src/platform.h`): beim Anlegen den Platz in der
Paar-Tabelle, die Köpfe der Kontaktlisten, die Körper und den Kontakt, den der Pool der Ids als nächsten vergibt
(`src/broad_phase.c`), beim Abbau alles außer den Manifolds (`b3PrefetchContactDestroy` in `src/contact.c` und
`b3DestroyBodyContacts` in `src/body.c`), und in der Kollision und der Schleife der Zustandswechsel die Kontakte und
Körper (`b3PrefetchContact` und `b3PrefetchContactStateData` in `src/physics_world.c`). Auf x86 ist das `PREFETCHW`,
den Intel-Prozessoren vor Broadwell als NOP ausführen, auf ARM `PRFM PSTL1KEEP`. Die Ergebnisse bleiben dieselben.

**Paare mit einem Radix-Sort sortieren.** Box3D sortiert die neuen Paare, bevor sie Kontakte werden, damit ihre
Reihenfolge nicht vom Weg durch die Bäume abhängt (`b3UpdateBroadPhasePairs` in `src/broad_phase.c`). Ab 64 Paaren nimmt
es dafür jetzt einen Radix-Sort (`b3SortPairKeys`): Für jedes Byte, in dem sich die Schlüssel unterscheiden, verteilt er
sie einmal stabil auf 256 Fächer, vom niedrigsten Byte zum höchsten. Bytes, die in allen Schlüsseln gleich sind,
überspringt er, bei Paaren einfacher Formen die des Index des Kindes und die hohen Bytes kleiner Ids. Die Reihenfolge
ist dieselbe wie mit `QSORT`, das für weniger Paare bleibt.

**Manifolds der Worker in der Kontakt-Schleife.** Auch in der Kontakt-Schleife auf mehreren Threads, siehe „Manifolds
ohne Sperre außerhalb der Kontakt-Schleife“, holte und gab jeder Kontakt sein Manifold einzeln unter der Sperre des
Allokators, wenn er zu berühren begann oder aufhörte. In der Stadt unter dem Trommelfeuer bei ×4, in der viele Etagen
einstürzen, waren das 750 pro Schritt, und die Threads warteten dabei zusammen 0,7 ms pro Schritt aufeinander, bei
1,7 ms für die ganze Schleife. Jetzt hält jeder Worker in `b3TaskContext::manifoldCache` einen Vorrat einzelner
Manifolds für die Kontakte konvexer Formen, holt ihn unter der Sperre 32 auf einmal, legt dort ab, was seine Kontakte
freigeben, und gibt nach der Schleife alles übrige zurück (`b3AllocateWorkerManifold`, `b3FreeWorkerManifolds` und
`b3GiveBackManifolds` in `src/physics_world.h` und `src/physics_world.c`, aufgerufen in `src/contact.c`). Netze mit
mehreren Manifolds nehmen den alten Weg. Ein Kontakt bekommt anderen Speicher, sonst ändert sich nichts.

**Knoten in Gruppen.** Die Bündel aus „Knoten im Überlauf“ rechnete der aufrufende Thread allein, eines nach dem
anderen, während die Worker warteten, beim Abriss in der Demo bei ×2 mit 8 Threads auf dem Ryzen 0,43 ms pro Schritt,
siehe „Überlauf an Knoten auf mehreren Threads“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md). Jetzt verteilt
`b3BuildKnots` in `src/solver.c` die Bündel auf acht Gruppen (`B3_KNOT_GROUPS` in `src/solver.h`), sobald an den Knoten
großer Inseln mindestens 256 Kontakte liegen (`B3_KNOT_GROUP_MIN`). Groß ist eine Insel mit mindestens einem Achtel der
Arbeit aller wachen Inseln, so gewogen wie für die Pakete (`B3_KNOT_GROUP_SHARE`, `b3GetIslandWork`): So eine Insel kann
mit vielen Threads ein Team rechnen, eine kleinere rechnet immer ein Thread ganz, und dort kosteten Kopien nur. Ein
Knoten einer großen Insel verteilt sich auf eine Gruppe für je volle 32 seiner Kontakte (`B3_KNOT_GROUP_CONTACTS`), auf
mindestens eine und höchstens alle acht, jeder andere bleibt in einer. Seine Kontakte gehen der Reihe nach an diese
Gruppen, ein bewegter Partner mit allen seinen Kontakten an dieselbe, auch mit anderen Knoten, ein Partner, der sich
nicht bewegt, an jede. Zwei Gruppen teilen sich so keinen bewegten Körper außer den Knoten selbst, und ein Kontakt
zwischen zwei Knoten bleibt bei den übrigen Kontakten des Überlaufs. In jeder Gruppe baut der Löser die Bündel wie
bisher, und ein Knoten in mehreren Gruppen hat in jeder eine Kopie (`b3KnotCopy`), die ihren Anteil an seiner Masse
trägt: Die Spuren eines Bündels rechnen gegen die Kopie ihrer Gruppe, mit der Masse des Knotens geteilt durch die Spuren
und die Kopien, und sind alle Gruppen durch, bekommt der Knoten den Mittelwert seiner Kopien (`b3KnotSplit` und
`b3MergeKnots`), wie mit den Kopien der Spuren eines Bündels, nur über die Gruppen hinaus. Vor jeder Runde über die
Farben rechnen die Threads die Gruppen in einer eigenen Stufe, einen Block pro Gruppe (`b3ExecuteKnotStage`, Blöcke vom
Typ `b3_knotBlock`), mit einem Team nur die seines Pakets, und danach mittelt der aufrufende Thread. Die Pakete aus
„Ganze Inseln pro Thread“ rechnen ihre Gruppen nacheinander und mitteln selbst. Eine Gruppe hängt von keiner anderen ab,
also nicht davon, wer sie rechnet und wann, und Gruppen, Kopien und Bündel folgen nur aus der Reihenfolge des Überlaufs
und den Inseln. Die Ergebnisse sind deshalb mit jeder Zahl von Threads und in beiden Breiten gleich. Ohne Gruppen
rechnet der Löser die Bündel wie vorher. `b3Counters` zählt mit `knotGroupBatchCount` die meisten Bündel, die ein Thread
nacheinander rechnet, und mit `knotCopyCount` die Kopien.

**Stufen des Teams.** Wie sich Arbeit und Warten auf die Stufen eines Teams verteilen, zeigten die Zeiten aus „Zeiten
des Teams“ nur in Summe. In jedem achten Schritt mit Team schreibt jetzt jeder Thread für jede Stufe, an der er
teilnimmt, einen Eintrag (`b3TeamEntry` in `src/solver.h`): der aufrufende Thread, wann er die Stufe öffnet, wann er
keinen Block mehr findet und wann er alle fertig sieht, ein Worker, wann er hineinkommt und wann er keinen Block mehr
findet, dazu die Blöcke, die er gerechnet hat, und ob er beim Warten seinen Kern abgegeben hat. Eine Stufe mit einem
Block rechnet der aufrufende Thread ohne Sperre allein, auch das schreibt er auf, und ein Worker schreibt jedes der
übrigen Pakete auf, das er rechnet. Nach dem Schritt fasst `b3SumTeamStages` in `src/solver.c` die Einträge zusammen, in
Zeilen nach dem, was die Stufen rechnen (`b3TeamStageRow`): Aufbau der Bedingungen, Integration, die Gruppen der Knoten,
eine Zeile pro Farbe und das Speichern der Impulse. Pro Zeile zählt `b3TeamStage` die Stufen mit Sperre und die allein
gerechneten, ihre Blöcke, wie viele davon der aufrufende Thread rechnete, und die Einträge der Blöcke, also Körper,
Bedingungen oder Gruppen, dazu wie lange die Stufen offen waren und wie sich diese Zeit über alle Threads aufteilt:
Blöcke der Stufe rechnen, eines der übrigen Pakete rechnen, vor dem Hineinkommen warten und nach dem letzten Block
warten. Ein Worker, der erst nach dem Ende einer Stufe zurückkommt, hat sie verpasst, und ihre ganze Zeit war er im
Paket oder hat gewartet. `b3TeamStages` hält außerdem, wie lange der aufrufende Thread zwischen den Stufen allein
rechnet, vor allem den Überlauf außerhalb der Gruppen, davon das Mitteln der Kopien der Knoten, und wie lange nach der
letzten Stufe, und wie spät die Worker im Mittel in eine Stufe kommen und wie viele sie verpassen.
`b3World_GetTeamStages` gibt das für den letzten Schritt, wenn er gemessen hat. Nur dieser Schritt liest die Uhr, einmal
mehr pro Stufe und Thread als vorher, und rechnet die Ticks mit `b3TicksToMilliseconds` aus `src/timer.c` um. Die
Rechnung des Lösers ändert sich nicht. Die neuen Felder stehen am Ende von `b3World`, `b3StepContext` und
`b3TaskContext`, damit die übrigen bleiben, wo sie waren: Mitten in diesen Strukturen kostete die Stadt des Benchmarks
bei ×1 auf der VM mit einem Thread 3 bis 6 % mehr, gleichmäßig über alle Stufen, obwohl mit einem Thread nichts davon
läuft.

**Große Pakete im Team.** Das Team aus „Ein Team für das größte Paket“ rechnete nur das erste Paket. Hielt eine zweite
Insel ebenfalls mehr als den Anteil eines Threads, rechnete ein Worker ihr Paket allein, und am Ende wartete das Team
auf ihn: beim Abriss in der Demo auf der VM mit 4 Threads in einem Drittel der Schritte mit Team, auf dem Ryzen mit 8
Threads ein Viertel der Zeit des Teams, siehe „Was nach der letzten Stufe bleibt“ in
[`docs/Optimierungen.md`](../docs/Optimierungen.md). Jetzt nimmt das Team mit dem ersten Paket jedes weitere, das mehr
als den Anteil eines Threads hält (`B3_TEAM_JOIN_NUM` und `B3_TEAM_JOIN_DEN` in `src/solver.c`). Die Pakete stehen nach
ihrer Arbeit in der Reihenfolge, und ihre Körper, die Bedingungen jeder Farbe, die Bündel und Gruppen der Knoten und der
Überlauf liegen Paket für Paket hintereinander. So umfassen die Blöcke der Stufen die ersten
`b3StepContext::stagePackCount` Pakete statt eines. Die Stufen bleiben dieselben und bekommen nur mehr Blöcke, die
Gruppen der Knoten aller Pakete des Teams rechnen in derselben Stufe, und die Kopien ihrer Knoten mittelt der aufrufende
Thread danach. Die übrigen Pakete nehmen sich die Worker wie bisher. Zwei Inseln teilen sich keinen bewegten Körper, und
jeder Körper durchläuft seine Schritte in derselben Reihenfolge wie in seinem Paket, die Ergebnisse bleiben dieselben.
`b3Counters::teamPackCount` sagt, wie viele Pakete das Team im letzten Schritt rechnete, und `b3Counters::teamWork`
zählt ihre ganze Arbeit.

**Der Aufbau des Lösers neben den Knoten.** Vor den Stufen baut der aufrufende Thread den Aufbau des Lösers allein, beim
Abriss in der Demo auf der VM mit 4 Threads 0,33 ms pro Schritt, auf dem Ryzen mit 8 Threads 0,127 ms. Gut die Hälfte
davon kosten die Knoten aus „Knoten im Überlauf“, fast den ganzen Rest die Pakete der Körper und Kontakte aus „Ganze
Inseln pro Thread“ und das Ordnen der konvexen Kontakte aus „Kontakte jeder Farbe nach Körpern ordnen“. Jetzt verteilt
der aufrufende Thread nur noch die Inseln auf die Pakete (`b3BuildPacks` in `src/solver.c`) und gibt den Rest zwei
Aufgaben (`b3SetupTask`): Die erste trägt jeden Kontakt in sein Paket ein und ordnet die konvexen Kontakte jeder Farbe,
die zweite trägt jeden Körper in sein Paket ein. Hat der Überlauf mindestens 256 Kontakte (`B3_SETUP_TASK_OVERFLOW`),
nehmen Worker die Aufgaben, während der aufrufende Thread die Knoten baut. Auf die erste wartet er erst, wo die Knoten
die Pakete ihrer Kontakte brauchen, auf die zweite am Ende des Aufbaus, bevor die Teilung einer Insel die Inseln der
Körper ändert, und hat noch kein Worker eine Aufgabe genommen, rechnet er sie selbst. Mit weniger Kontakten im Überlauf
sind die Knoten zu schnell gebaut, um die Aufgaben zu überdecken, und er rechnet sie gleich selbst, wie vorher. Speicher
vom Stapel nimmt nur er, die Aufgaben füllen, was er vorher genommen hat. Jedes Ergebnis ist dasselbe, gleich welcher
Thread es rechnet, die Ergebnisse bleiben dieselben. `b3Profile` misst mit `setupPacks`, `setupTasks`, `setupKnots` und
`setupWait`, was die Teile kosten.

**Zeiten der Kontakt-Schleife.** Wohin die Zeit der Kontakt-Schleife (`b3Collide` in `src/physics_world.c`) geht, misst
Box3D jetzt in drei Teilen vor den Zustandswechseln aus `contactStates`: `b3Profile::gatherContacts`, in dem der
aufrufende Thread die wachen Kontakte in ein Array sammelt und die Bits der Worker leert, `updateContacts`, die
parallele Schleife über die Kontakte, und `mergeContacts`, in dem er zusammenführt, was die Worker gefunden haben, und
die Manifolds zurückgibt, die sie freigegeben haben. In jedem achten Schritt (`B3_COLLIDE_SAMPLE`) misst die Schleife
ihre Blöcke über `b3ParallelForOrdered` in `src/parallel_for.c`: Jede Aufgabe liest die Uhr um jeden ihrer Blöcke und
schreibt in eine eigene Cache-Zeile, wann sie anfing und aufhörte, wie lange sie in Blöcken rechnete, wie viele es waren
und wie lange der längste dauerte. `b3ParallelForTimes` fasst das über die Aufgaben zusammen: die Zeit in Blöcken, vor
dem Start einer Aufgabe, zwischen ihren Blöcken und nach ihrem letzten, zusammen der Aufruf mal die Aufgaben. Die
Kontakt-Schleife teilt die Zeit in Blöcken noch auf die zwei Stufen aus „Zweistufige Kontakt-Schleife“ auf, die
Entscheidung über jeden Kontakt und den Test der übrigen (`b3Profile::collideWork` und die Felder danach).
`b3Counters::narrowphaseCount` zählt in jedem Schritt die Kontakte, die getestet wurden, `collideSample` sagt, ob der
letzte Schritt gemessen hat, dazu seine Aufgaben, Blöcke und die Aufgaben, die keinen Block mehr fanden. Nur dieser
Schritt liest die Uhr an jedem Block, in den anderen liest die Kontakt-Schleife sie fünfmal statt zweimal. Die Rechnung
ändert sich nicht.

**Kontakte von beiden Enden.** Die Threads der Kontakt-Schleife nahmen ihre Blöcke von vorne nach hinten. Die teuersten
Kontakte liegen aber an beiden Enden des Arrays: vorne in den ersten Farben die Kontakte zwischen zwei bewegten Körpern,
die der Graph von unten füllt, während Kontakte mit statischen Körpern die Farben von oben füllen, und hinten die Paare,
die in diesem Schritt dazukamen und zum ersten Mal getestet werden. Beim Abriss in der Demo trägt das erste Fünftel 45 %
der Zeit, in der Stadt bei ×4 das letzte Zehntel 27 %. Von vorne nach hinten kamen die teuren Blöcke von hinten zuletzt,
und die anderen Threads warteten am Ende auf sie. Jetzt nehmen die Threads die Blöcke abwechselnd von vorne und von
hinten (`b3_blocksFromBothEnds` in `src/parallel_for.h`), die billigen Blöcke der Mitte kommen zuletzt. Auf der VM mit 4
Threads warten sie nach ihrem letzten Block damit 55 bis 77 % kürzer. Mit einer Aufgabe bleibt die Reihenfolge, wie sie
war. Jeder Kontakt wird für sich gerechnet, die Reihenfolge ändert an den Ergebnissen nichts.

**Kontakte mehrerer Körper nebeneinander.** `b3Body_GetContactDataOfBodies` gibt für bis zu 64 Körper einer Welt die
berührenden Kontakte, jedem Körper dieselben wie `b3Body_GetContactData` und in derselben Reihenfolge (`src/body.c`).
Box3D führt die Kontakte eines Körpers in einer Liste, in der jeder Eintrag erst sagt, wo der nächste liegt, und in
einem Haufen Trümmer berühren die meisten Einträge nicht: Ein kleines ruhiges Stück beim Abriss in der Demo hat im
Mittel 28 Einträge für 6 Kontakte, die berühren, jeden an einer anderen Stelle im Speicher. Die Funktion geht die Listen
deshalb nebeneinander durch, in jeder Runde einen Eintrag jeder Liste, und lädt den nächsten Eintrag einer Liste voraus,
sobald er bekannt ist, so überlappen sich die Wartezeiten der Listen. Für einen berührenden Kontakt lädt sie voraus, was
danach gelesen wird, seine Generation, die Einträge seiner Formen und den Körper auf der anderen Seite, und holt die
Generationen der Ids in einem zweiten Durchgang. Nebenan holt damit in der Ruhe-Prüfung die Kontakte aller ruhigen
Stücke eines Blocks auf einmal, siehe „Kontaktlisten der ruhigen Stücke nebeneinander“ in
[`docs/Optimierungen.md`](../docs/Optimierungen.md). In einer Kopie mit Zählern kostete ein Eintrag beim Abriss auf der
VM so 61 statt 184 Takte. Seit „Listen der berührenden Kontakte“, siehe unten, gehen die Listen nur noch über berührende
Kontakte.

**Lange Kontaktlisten über ihre Formen.** In `b3Body_GetContactDataOfBodies` läuft ein Körper mit langer Liste, ab 256
Kontakten und 8 Formen, zuerst die Listen seiner Formen nebeneinander ab, bis zu 64 zugleich, in jeder Runde einen
Eintrag jeder Liste, und lädt dabei jeden Kontakt voraus (`b3PrefetchContactsOfShapes` in `src/body.c`). Seit „Kontakte
pro Form“ führt jede Form ihre Kontakte in einer eigenen Liste, dieselben wie die Liste des Körpers, nur in kurzen
Stücken. Danach läuft die Liste des Körpers wie zuvor in ihrer Reihenfolge ab und findet den nächsten Eintrag und die
Flags jedes Kontakts im Cache, die Kontakte und ihre Reihenfolge bleiben dieselben. Beim Abriss in der Demo liegt in
einem Fünftel der Updates der Rest einer Etage mit über 4 000 Einträgen und 200 Formen ruhig, und seine Liste lief
allein weiter, lange nachdem die der anderen Stücke seines Blocks geendet hatten. Auf der VM dauert der längste Block
der Ruhe-Prüfung damit 10 bis 17 % kürzer, siehe „Blick auf große Stücke billiger“ in
[`docs/Optimierungen.md`](../docs/Optimierungen.md). Seit „Listen der berührenden Kontakte“, siehe unten, laufen die
Abfragen die Liste des Körpers nicht mehr ab, und das Vorausladen ist entfallen.

**Listen der berührenden Kontakte.** Box3D führt für jeden Körper, der nicht statisch ist, neben der Liste aller seiner
Kontakte eine Liste nur der berührenden (`b3Body::headTouchKey` in `src/body.h`). Ihre Verweise stehen nicht im Kontakt,
sondern daneben in einem Array nach Kontakt-Id, zusammen mit den Formen des Kontakts (`b3ContactTouch` in
`src/contact.h`): Ein Kontakt bleibt 248 Bytes groß, die Kontakt-Schleife liest nicht mehr als zuvor, und wer die Liste
abläuft, liest von einem Kontakt nur diesen Eintrag und seine Manifolds. Ein Kontakt kommt an den Anfang der Listen
seiner Körper, wenn er zu berühren beginnt, und geht wieder hinaus, wenn er aufhört oder gelöscht wird
(`b3LinkTouchingContact` und `b3UnlinkTouchingContact` in `src/contact.c`). Statische Körper führen keine Liste, ihre
wäre die längste, und nichts fragt sie oft ab. Ein Körper wechselt seinen Typ erst, wenn er alle Kontakte verloren hat,
also nimmt jeder Kontakt dieselben Verweise heraus, die er bekommen hat. `b3Body_GetContactData` und
`b3Body_GetContactDataOfBodies` laufen für die anderen Körper nur noch diese Listen ab. In einem Haufen Trümmer berühren
vier von fünf Kontakten nicht: Beim Abriss in der Demo hat ein ruhiges Stück im Mittel 31 Einträge für 6 berührende
Kontakte, der Rest einer Etage 4 610 für 1 094. Die Reihenfolge bleibt die der Liste aller Kontakte. Dort setzt ein
Körper jeden neuen Kontakt an den Anfang, sie läuft also vom zuletzt angelegten Kontakt zum ersten. Jeder Kontakt
bekommt beim Anlegen die Zahl der Kontakte, die die Welt vor ihm angelegt hat, und die Abfragen sortieren, was sie
finden, absteigend danach, bis 32 Kontakte durch Einfügen, darüber durch Mischen. Hat der Aufrufer weniger Platz, als
der Körper berührende Kontakte hat, läuft die Abfrage die Liste aller Kontakte ab wie zuvor, denn dann gibt sie deren
erste. In Debug-Builds vergleicht jede Abfrage ihr Ergebnis mit der Liste aller Kontakte, die Validierung prüft die
Listen (`b3ValidateContacts`), und der Snapshot einer Aufnahme trägt sie mit (`src/world_snapshot.c`). Beim Einlesen
eines Snapshots rief Box3D für eine Welt ohne Sensoren oder Inseln `memset` auf ein Array ohne Speicher auf, jetzt nicht
mehr. Das Vorausladen über die Listen der Formen, siehe oben, ist entfallen. Beim Abriss in der Demo auf der VM kostet
der Blick auf ein ruhiges Stück damit 20 bis 24 % weniger, und die Pflege der Listen kostet in der seriellen Schleife
der Zustandswechsel 0,014 bis 0,024 ms pro Schritt. Die Ergebnisse bleiben bitgleich, siehe „Nur berührende Kontakte
ablaufen“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md).

**Arbeit des Lösers pro Einheit.** Wie lange die Threads für dieselbe Arbeit des Lösers brauchen, im Team und in den
Paketen gegen einen Thread allein, zeigten die Zeiten des Teams nicht. Jeder achte Schritt misst es jetzt
(`B3_SOLVER_SAMPLE` in `src/solver.c`, `b3Counters::solverSample`). Er misst die Arbeit der wachen Inseln, vier
Einheiten pro Kontakt und eine pro Körper wie für die Pakete, auch in einer Welt ohne Worker, die das sonst lässt. Jeder
Thread misst, wie lange er ganze Pakete rechnet (`b3TaskContext::packTime` in `src/physics_world.h`), und der Schritt
gibt die Summe über die Threads in `b3Profile::packSolveTime` und die Arbeit dieser Pakete in
`b3Counters::packSolveWork`, mit einem Team die der übrigen Pakete. Mit der Zeit der Bedingungen, der Arbeit des Teams
und der Zeit seiner Blöcke aus den Stufen des Teams, siehe oben, ergibt das, was eine Einheit allein, in den Paketen und
in den Blöcken des Teams kostet. Nur dieser Schritt liest die Uhr, zweimal pro Paket, die Rechnung des Lösers ändert
sich nicht. Die neuen Felder stehen am Ende von `b3World`, `b3TaskContext` und `b3StepContext`, damit die übrigen
bleiben, wo sie waren. Beim Abriss in der Demo auf der VM bei ×2 kostet eine Einheit mit 4 Threads in den Paketen
1,18-mal und in den Blöcken des Teams 1,39-mal so viel wie mit einem Thread, siehe „Was eine Einheit Arbeit im Löser
kostet“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md).

**Ansprüche vor dem Tausch lesen.** In einer Stufe mit Sperre läuft jeder Thread den Ring der Blöcke einmal ab, von
seinem Heimatblock an, und nimmt jeden freien Block mit einem atomaren Vergleichstausch auf seinen Anspruch
(`b3ExecuteStage` in `src/solver.c`). Die meisten Blöcke haben andere schon genommen, wenn er zu ihnen kommt. Ein
fehlgeschlagener Vergleichstausch holt die Cache-Zeile trotzdem exklusiv und nimmt sie allen anderen Threads weg, mit 8
Threads beim Abriss in der Demo rund 17 600 Mal pro Schritt. Jetzt liest der Thread den Anspruch zuerst und versucht den
Tausch nur bei einem freien Block, einen genommenen kostet ihn das Lesen einer geteilten Zeile. Wer in die Stufe kam,
sieht jeden Anspruch der Stufe davor, das Lesen hält einen freien Block also nie für genommen. Auf der VM mit 4 Threads
kostet ein vergeblicher Versuch damit 0,024 bis 0,025 statt 0,042 bis 0,057 µs. Die Ansprüche dicht in einem eigenen
Array oder jeden auf einer eigenen Cache-Zeile zu halten, war beides teurer, siehe „Vergebene Blöcke billiger
überspringen“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md). Der Kopfkommentar von `src/solver.h` beschreibt
noch, wie Box2D die Blöcke nimmt, vorwärts und rückwärts bis zum ersten genommenen Block, und sagt jetzt, dass Box3D den
ganzen Ring abläuft. Welcher Thread einen Block rechnet, ändert sich, was er rechnet, nicht.

**Zonen für den Profiler Tracy.** Mit `BOX3D_PROFILE` holt Box3D den Client von Tracy 0.13.1 und markiert Abschnitte des
Schritts als Zonen, Nebenan schaltet das mit `NEBENAN_PROFILE` ein. Upstream lagen die Zonen in den Kernen des Lösers,
die ein Paket für jede Farbe und jeden Teilschritt aufruft. Beim Abriss in der Demo waren das auf der VM 836 000 Zonen
in 10 s, und die Zeitleiste zeigte nicht, wann das Team auf eine Stufe wartet. Jetzt haben die Kerne und die Integration
ihre Zonen nur noch mit `BOX3D_PROFILE_DETAIL`. Dafür bekommt jeder Block einer Stufe des Teams eine Zone mit dem Namen
seiner Stufe, die Blöcke der Knoten „Knots“. Eigene Zonen haben auch jedes Paket („Pack“), jedes Stück Überlauf, das der
aufrufende Thread allein rechnet („Overflow“), das Mitteln der Kopien der Knoten („Merge Knots“), die Zeit eines Workers
im Löser („Solver Worker“) und der Aufbau der Pakete und der Knoten (`src/solver.c`). Zwischen seinen Zonen wartet ein
Thread. Dieselbe Aufnahme hat damit 332 000 Zonen und 2,1 MB. Die Speicheranforderungen meldet Box3D Tracy nur noch mit
`BOX3D_PROFILE_MEMORY`. Tracy läuft in Box3D mit eigener Lebensdauer, und jede Zone braucht es laufend.
`b3StartProfiler` in `include/box3d/base.h` startet es einmal, auch wenn mehrere Threads zugleich kommen, und es endet
mit dem Programm. `b3CreateWorld` ruft das auf, ein Programm, das vorher Threads benennt oder eigene Zonen öffnet,
ebenso. Hat das Programm Tracy selbst gestartet, bleibt es dabei. Tracy schreibt in jede Aufnahme den Anmeldenamen und
den Namen des Rechners. `src/tracy_hide_user.cmake` ändert das nach dem Download in „(hidden)“, damit sich eine Aufnahme
teilen lässt. Ohne `BOX3D_PROFILE` kommt nichts davon in den Build, und `b3StartProfiler` tut nichts.

**Gebiete großer Inseln.** Eine Insel mit mindestens einem Achtel der Arbeit aller wachen Inseln und mindestens 1 024
Kontakten rechnet nicht mehr Farbe für Farbe mit einer Sperre nach jeder Stufe, beim Abriss rund 230 pro Schritt, an
denen alle Threads auf den letzten Block warten, sondern in acht Gebieten (`b3Domains` in `src/solver.c`,
`b3Island::domains` und `B3_DOMAINS` in `src/island.h`). Die Gebiete bleiben von Schritt zu Schritt wie die Farben des
Graphen. Jeder Körper der Insel hat sein Gebiet (`b3Body::domain`), und jeder berührende Kontakt liegt, sobald er zu
berühren beginnt, in seiner Farbe im Array seines Gebiets (`b3GraphColor::domainContacts`,
`b3ConstraintGraph::domainOverflow`), im Gebiet seines Körpers mit weniger Kontakten, die Kontakte eines Knotens also in
den Gebieten seiner Partner. Wie viele Kontakte jeder Körper in jedem Gebiet hat, zählt der Graph mit
(`b3World::bodyDomainCounts`). Ein Körper, der dazukommt, nimmt das Gebiet des Körpers, den er berührt (`b3LinkContact`
in `src/island.c`). Inseln, die sich teilen, schlafen oder aufwachen, behalten ihre Gebiete, und ein Kontakt mit einem
Netz oder eine Insel, die unter die Hälfte einer der beiden Grenzen fällt, gibt sie an die Farben zurück
(`b3RemoveIslandDomains`), bis zu „Gelenke in Paketen und Gebieten“ auch ein Gelenk. Geschnitten werden sie, wenn eine Insel sie bekommt, und neu nur, wenn das größte ein Viertel
mehr Kontakte hält als ihr Mittel, höchstens alle 32 Schritte (`b3UpdateDomains`, `b3CutIslandDomains`). Dann teilt
`b3CutDomains` ein Raster über die zwei längeren Seiten der Box der Körper, jeweils quer zur längeren Seite dort, wo
sich die Kontakte am gleichmäßigsten teilen. Ihre Gebiete bekommt eine Insel meist in Schritten, die ohnehin langsam
sind, wenn viel von einem Haus auf einmal herunterkam, und der Schnitt wartete fast nur auf die Körper und Kontakte, die
verstreut im Speicher liegen, einen nach dem anderen. Seit `active-ragdoll-4` lädt er sie 8 und 16 voraus, und was das
Verschieben eines Kontakts in sein Gebiet schreibt (`b3PrefetchCutBody`, `b3PrefetchDomainMove`). Körper und Kontakte
kommen in derselben Reihenfolge dran, die Ergebnisse bleiben bis aufs Bit gleich, und ein großer Schnitt kostet auf der
VM ein Drittel weniger, siehe „Die langsamsten Schritte mit F8“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md).
Ein Thread rechnet in jedem Durchgang des Lösers ein Gebiet ganz, seine
Knoten, seine übrigen Kontakte im Überlauf und seine konvexen Kontakte in der Reihenfolge der Farben, ohne Sperre
dazwischen, und die Gruppen der Knoten einer großen Insel sind ihre Gebiete. Ein Körper mit Kontakten in mehreren
Gebieten hat in jedem eine Kopie mit einem Anteil seiner Masse, die das Gebiet vor jedem Durchgang aus dem Körper
startet, und nach jedem Durchgang nimmt der Körper das Mittel seiner Kopien (`b3MergeDomains`), wie bei den Knoten nach
Tonge, Benevolenski und Voroshilov. Die Kopien liegen hinter den wachen Zuständen, die Bedingungen eines Gebiets zeigen
auf sie (`b3GetDomainSlot`, `b3GetDomainLanes` in `src/contact_solver_wide.inl`). Gesperrt wird nur noch zwischen den
Durchgängen, mit vier Teilschritten 36 Mal pro Schritt (`b3SolveDomainStages`). Ein Schritt zählt nur die Kopien aus den
Zählern und legt die Kontakte jedes Gebiets Farbe für Farbe in breite Bedingungen. Eine breite Bedingung, die eine Farbe
offen lässt, füllen die nächsten Farben, solange keiner ihrer Kontakte einen bewegten Körper mit einer Spur einer
früheren Farbe darin teilt (`b3FillDomainLanes`). So sieht jeder Körper seine Kontakte in der Reihenfolge der Farben,
und kaum eine Spur bleibt leer, obwohl die meisten Farben eines Gebiets nur wenige Kontakte haben. Beides rechnet ein
Worker, während der aufrufende Thread die Knoten baut (`b3FillDomainBodies`, `b3SetupTask`). Große Inseln brauchen
Pakete, also keine Kontakte mit Netzen in den Farben und keine Weitergabe der Restitution, bis zu „Gelenke in Paketen und
Gebieten“ auch keine Gelenke. Dann sind sie
immer das erste Paket, mit jeder Zahl von Workern, auch mit einem und während andere Programme die Kerne nehmen, und die
übrigen Inseln rechnet je ein Thread ganz, mit einem einzigen Worker alle in einem Paket. Alles folgt aus den Inseln,
ihren Kontakten und der Lage ihrer Körper, wenn sie ihre Gebiete bekommen, und hängt nicht an den Workern, die
Ergebnisse sind mit jeder Zahl von Workern und in beiden Breiten gleich. `b3Counters::domainCount` und `domainCopyCount`
zählen Gebiete und Kopien. Der Stapel von Box3D hält 48 statt 32 Einträge (`src/arena_allocator.h`), ein Schritt nimmt
mit Gebieten drei mehr, während das Teilen einer Insel auf einem anderen Thread eigene nimmt. Gebaut für Schritt 68 der
[Roadmap](../docs/Roadmap.md), siehe „Dauerhafte Gebiete“ in [`docs/Optimierungen.md`](../docs/Optimierungen.md), nach
dem Prototyp von Schritt 62, der die Gebiete in jedem Schritt neu baute. Auf `kollaps-34` behielt eine Insel, wo ihr
Schnitt geteilt hat, und Körper, die dazukamen, nahmen ihr Gebiet nach ihrer Lage. Das brachte auf dem Ryzen mit 8
Threads zu wenig und ist auf `kollaps-35` wieder heraus, siehe „Die Durchgänge der Gebiete zerlegt“ und „Skalierung auf
dem Ryzen mit `kollaps-33` und `kollaps-34`“.

**Federn mit Grenze.** Die Feder eines Dreh- oder Kugelgelenks zog in Box3D mit jedem Drehmoment, das sie für ihr Ziel
brauchte. Die Figuren nach Overgrowth halten mit ihren Gelenken eine Haltung nur so fest, wie sie gerade Kraft haben,
und geben nach, wenn mehr gegen sie drückt. `b3RevoluteJointDef::maxSpringTorque` und
`b3SphericalJointDef::maxSpringTorque` begrenzen die Feder wie den Motor, `b3RevoluteJoint_SetMaxSpringTorque` und
`b3SphericalJoint_SetMaxSpringTorque` auch später: Der Impuls, den die Feder in einem Teilschritt aufsummiert, bleibt
beim Drehgelenk zwischen minus und plus Grenze mal Teilschritt, beim Kugelgelenk wird der Vektor auf diese Länge gekürzt
(`b3SolveRevoluteJoint` in `src/revolute_joint.c`, `b3SolveSphericalJoint` in `src/spherical_joint.c`).
`b3RevoluteJoint_GetSpringTorque` und `b3SphericalJoint_GetSpringTorque` geben das Drehmoment der Feder im letzten
Teilschritt zurück, so sieht man, wie nah eine Feder an ihrer Grenze arbeitet. Ohne Angabe ist die Grenze `FLT_MAX`, und
die Federn rechnen wie zuvor, bis aufs Bit. Aufnahmen speichern die Grenze mit dem Gelenk und jede Änderung in einer
eigenen Operation (`0x8E` und `0x8F` in `src/recording_ops.inl`), ihr Format hat deshalb die Version 7
(`src/recording.h`). Gebaut für die Figuren von `active-ragdoll-1`, siehe „Figuren“ in der
[Roadmap](../docs/Roadmap.md).

**Gelenke in Paketen und Gebieten.** Ein einziges waches Gelenk in den Farben nahm allen Inseln die Pakete aus „Ganze
Inseln pro Thread“ und den großen Inseln ihre Gebiete aus „Gebiete großer Inseln“. Solange ein Ragdoll lag, rechnete
Box3D die ganze Welt Farbe für Farbe mit einer Sperre nach jeder Stufe, auch die Trümmer weit weg. Jetzt rechnet ein
Paket die Gelenke seiner Inseln selbst. Jedes wache Gelenk gehört zur Insel seiner Körper, und der Setup-Task legt die
Gelenke jeder Farbe und des Überlaufs Paket für Paket ab (`b3OrderPackJoints`, `b3StepContext::packJoints` und
`packJointStarts` in `src/solver.h`). In jeder Stufe rechnet ein Paket zuerst seine Gelenke im Überlauf, dann seine
Knoten und übrigen Kontakte im Überlauf, dann Farbe für Farbe seine Gelenke und seine konvexen Kontakte
(`b3ExecutePackConstraints`), die Reihenfolge der Stufen mit Sperren. Jeder Körper durchläuft seine Schritte in
derselben Reihenfolge, die Ergebnisse bleiben bis aufs Bit dieselben, die Ereignisse der Gelenke eingeschlossen
(`b3SolveColorJoint`). Mit einem Team halten die Stufen die Gelenke seiner Pakete in Blöcken vom Typ
`b3_packJointBlock`, und der aufrufende Thread rechnet nur deren Gelenke im Überlauf (`b3ExecuteOverflowJoints`). Ein
Paket ohne Gelenke überspringt sie. Die Pakete wiegen ein Gelenk wie acht Körper (`B3_PACK_JOINT_WORK`,
`b3GetIslandWeight`), denn Box3D rechnet Gelenke einzeln und Kontakte in breiten Bedingungen. Die Arbeit einer Insel und
`b3Counters::islandWork` bleiben ohne Gelenke, denn nach ihr bekommen große Inseln ihre Gebiete und ihre Knoten Gruppen,
was Ergebnisse ändert. Die Listen teilen sich einen Block mit den Anfängen der breiten Bedingungen, so nimmt ein Schritt
nicht mehr Einträge des Stapels als vorher. Das hat zuerst der Branch `ragdoll-7` gebaut, vor den Gebieten und den
Gruppen der Knoten.

Ein Gelenk gibt die Gebiete auch nicht mehr zurück, und Gelenke anderswo halten sie nicht mehr auf. Eine Insel mit
Gebieten behält ihre Gelenke in den Farben, etwa ein Ragdoll im Schutt. Der aufrufende Thread rechnet sie vor jedem
Durchgang über die Gebiete allein, erst die im Überlauf, dann Farbe für Farbe, an den Körpern selbst, die nach dem
Durchgang davor das Mittel ihrer Kopien halten (`b3ExecuteDomainJoints` in `src/solver.c`). Die Gebiete starten ihre
Kopien aus dem, was die Gelenke ließen, und ihre Stöße gehen über das Mittel an die Gelenke im nächsten Durchgang. Diese
Zeit zählt `b3Profile::overflow` mit, auf der VM mit vier Threads 0,024 ms pro Ragdoll und Schritt. Mit 20 Ragdolls auf
einer Platte über 4 513 Würfeln kostet der Löser so 3,98 ms, auf `active-ragdoll-1` ohne Gebiete 3,89 ms, im Median aus
fünf Läufen im Wechsel, gleich im Rahmen der Streuung. Die Gelenke in die Gebiete selbst zu legen, gegen Kopien mit
einem Anteil der Masse wie die Kontakte, steht als Schritt F12 in der [Roadmap](../docs/Roadmap.md). Ein Körper, der
über ein Gelenk zu einer Insel mit Gebieten kommt, nimmt das Gebiet des Körpers, an dem er hängt, wie einer über einen
Kontakt, die Kontakte eines schlafenden erst beim Aufwachen (`b3AbsorbIntoDomains` in `src/island.c`).
`b3Counters::packCount` zählt die Pakete des letzten Schritts, null ohne Pakete. In Welten ohne Gelenke ändert sich
nichts. Mit Gelenken rechnen die großen Inseln jetzt in Gebieten statt in den Farben, auch mit einem Ragdoll darin, und
das ändert ihre Ergebnisse wie die sechzigste. Ein Test löst zwei Naben mit je 30 Armen an Motoren, 10 und 11 davon im
Überlauf, vier Ketten, die Ereignisse melden, und den Haufen aus dem Test der Teams als Team mit 1, 2, 4 und 8 Workern
und in beiden Breiten, bitgleich und gleich wie der Löser ohne Pakete. Ein zweiter legt zwei Ketten mit Federn und
Grenzen und ein Pendel an einem Scharnier im Überlauf auf eine Platte mit Gebieten, während ein Pendel am Boden
schwingt: Gebiete in allen Schritten, bitgleich mit jeder Zahl von Workern. Gebaut für Schritt F8, siehe „Figuren“ in
der [Roadmap](../docs/Roadmap.md).

Zum Messen schaltet `b3WorldDef::enableJointPacks` das ab, standardmäßig ist es an. Aus nimmt jedes wache Gelenk dem
ganzen Schritt die Pakete und allen großen Inseln die Gebiete, und eine Insel, die ein Gelenk bekommt, gibt ihre Gebiete
zurück (`b3LinkJoint`), wie bis `active-ragdoll-1`. Die Ergebnisse sind dann bis aufs Bit die von `active-ragdoll-1`,
mit jeder Zahl von Workern, so misst ein Programm den Gewinn in einem Lauf ohne zweiten Build. Der Snapshot einer
Aufnahme behält den Schalter: Das Bit `0x20` der Schalter der Welt steht für aus, so lesen ältere Snapshots ihn an
(`b3SerWorldConfig` in `src/world_snapshot.c`). Ein Test rechnet die Platte mit Gebieten und ein Pendel an ihrem Rand
mit und ohne den Schalter: mit Gebieten in allen Schritten, ohne in keinem und ohne Pakete, mit einem und vier Workern
bitgleich, und eine Aufnahme ohne den Schalter spielt gleich ab.

Die erste, die zweite, die vierte bis achte, die zehnte, die dreizehnte bis einunddreißigste, die dreiunddreißigste bis
fünfunddreißigste, die neununddreißigste bis dreiundvierzigste, die fünfundvierzigste bis siebenundvierzigste, die
neunundvierzigste bis neunundfünfzigste ändern an den Ergebnissen nichts, die einundsechzigste nur an Federn mit einer
Grenze, die nur die Figuren setzen, die zweiundsechzigste nur in Welten mit wachen Gelenken, wo große Inseln jetzt in
Gebieten rechnen. Die dritte und die neunte ändern sie in den letzten Bits, dafür hängen sie nicht
mehr davon ab, in welcher Reihenfolge Formen in die Bäume kommen. Die elfte ändert sie, weil Kontakte zu anderen
Zeitpunkten entstehen, die zwölfte, weil sie weniger Punkte bekommen, die zweiunddreißigste, weil die Kontakte eines
Knotens gebündelt gegen Kopien rechnen statt nacheinander, die sechsunddreißigste, weil die Kerne dieselbe Rechnung
anders zusammenfassen und deshalb anders runden, die siebenunddreißigste, weil die Suche andere Kantenpaare aussiebt und
getrennte Hüllen erkennt, die die alte übersah, und die achtundvierzigste, weil die Bündel der Knoten dichter liegen und
die Knoten großer Inseln in Gruppen gegen eigene Kopien rechnen, und die sechzigste, weil große Inseln in Gebieten
rechnen und ihre Körper an den Grenzen gegen Kopien mit einem Anteil ihrer Masse. Die achtunddreißigste ändert sie nur,
wo Kugeln Kugeln oder Kapseln berühren, Kontakte ohne vorausschauende Punkte rechnen oder der PreSolve-Callback Kontakte
abschaltet, in Nebenans Szenen nicht. Die vierundvierzigste ändert sie nur, wo die volle zweite Suche einen Kontakt
zwischen Hüllen ergab, die eine Fläche trennt. Die Ergänzungen stehen in `CMakeLists.txt`, `src/CMakeLists.txt`,
`include/box3d/base.h`, `include/box3d/box3d.h`, `include/box3d/types.h`, `include/box3d/constants.h`, `src/core.h`,
`src/core.c`, `src/shape.h`, `src/shape.c`, `src/body.h`, `src/body.c`, `src/joint.h`, `src/joint.c`,
`src/revolute_joint.c`, `src/spherical_joint.c`, `src/contact.h`, `src/contact.c`, `src/contact_solver.h`,
`src/contact_solver.c`, `src/contact_solver_wide.inl`, `src/wide8.c`, `src/simd.c`, `src/simd_avx2.h`,
`src/constraint_graph.h`, `src/constraint_graph.c`, `src/island.h`, `src/island.c`, `src/manifold.h`,
`src/convex_manifold.c`, `src/convex_manifold_avx2.c`, `src/world_snapshot.c`, `src/recording.h`, `src/recording.c`,
`src/recording_ops.inl`, `src/recording_replay.c`, `src/solver.h`, `src/solver.c`, `src/solver_set.c`,
`src/arena_allocator.h`, `src/broad_phase.h`, `src/broad_phase.c`, `src/dynamic_tree.h`, `src/dynamic_tree.c`,
`src/parallel_for.h`, `src/parallel_for.c`, `src/platform.h`, `src/scheduler.h`, `src/scheduler.c`, `src/timer.c`,
`src/physics_world.h`, `src/physics_world.c` und `src/tracy_hide_user.cmake`, jeweils mit „Added for Nebenan“ markiert.
`B3_HAS_UNIQUE_HULLS`, `B3_HAS_SHAPE_BATCH`, `B3_HAS_SHAPE_CONTACT_LISTS`, `B3_HAS_EXTERNAL_HULLS`,
`B3_HAS_BODY_PREFETCH`, `B3_HAS_STEP_GUARD`, `B3_HAS_PREVIOUS_TRANSFORM`, `B3_HAS_STATIC_OVERLAP`,
`B3_HAS_CONTACT_DATA_OF_BODIES` und `B3_HAS_SPRING_TORQUE_LIMITS` zeigen die zweite, die vierte, die fünfte, die
sechste, die neunzehnte, die dreißigste, die fünfunddreißigste, die zweiundvierzigste, die vierundfünfzigste und die
einundsechzigste an. Fehlen sie, etwa mit einem eigenen Box3D, läuft Nebenan wie zuvor, nur wachsen Box3Ds Arrays dann
durch Umkopieren, die Hüllen gehen durch die Tabelle, jede Form geht einzeln in den Baum, jedes stehende Bruchstück
bekommt einen eigenen Körper, Box3D hält eine Kopie jeder Hülle, Nebenan holt die Körper der Trümmer nicht voraus,
`nbWorld_Step` rechnet auf dem aufrufenden Thread, `nbWorld_GetDrawPoses` gibt die Lagen nach dem Schritt, Schutt wird
in allen Bäumen gesucht, die Ruhe-Prüfung holt die Kontakte jedes ruhigen Stücks für sich, und die Federn der Figuren
halten ihre Haltung ohne Grenze.
