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
nur ein Thread rechnet, ein Gelenk oder ein Kontakt mit einem Netz in den Farben liegt, Box3D den Rückprall fortpflanzt
oder eine Insel mehr als die Hälfte der Arbeit hält und die Worker für ein Team zu wenige sind, siehe „Ein Team für das
größte Paket“.

**Sperren, solange andere Programme die Kerne belegen.** Verliert ein Thread mitten in einem Paket seinen Kern an ein
anderes Programm, warten am Ende alle auf dieses Paket, während mit Sperren die anderen die Blöcke übernehmen, die er
noch nicht angefangen hat. Jetzt misst der aufrufende Thread nach seinem letzten Paket, wie lange er auf die anderen
wartet. Ein Warten ab 1 ms, das mindestens die Hälfte der Zeit der Pakete ausmacht, zählt dreifach, jeder Schritt ohne
eines zieht eins ab, und ab neun rechnet der Löser 120 Schritte lang mit Sperren, kommt es innerhalb der doppelten Länge
danach wieder, doppelt so lange, bis zu 960 Schritte (`b3UpdatePackHold` in `src/solver.c`, `b3World::packHoldSteps` in
`src/physics_world.h`). Ein einzelnes langes Warten, wie es ohne Last ab und zu vorkommt, zählt so nicht. Die Ergebnisse
sind mit und ohne Sperren dieselben.

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
Bündel, einer breiten Bedingung zu acht Spuren oder zweien zu vier (`b3KnotBatch` in `src/solver.h`), der erste Kontakt
mit einem Partner in den ersten Bündeln, der zweite in den nächsten. Jede Spur rechnet gegen eine Kopie des Knotens, die
ihren Anteil an seiner Masse und Trägheit trägt, und danach bekommt der Knoten den Mittelwert seiner Kopien, nach „Mass
Splitting for Jitter-Free Parallel Rigid Body Simulation“ von Tonge, Benevolenski und Voroshilov (2012). Eine Spur
rechnet dasselbe wie in den Farben, die Kerne teilen sich die Rechnung einer breiten Bedingung (`b3WarmStartContactW`,
`b3PushContactW`, `b3SolveContactW` und `b3ApplyRestitutionW` in `src/contact_solver_wide.inl`), nur schreiben die
Spuren den Knoten nicht zurück, sondern zählen ihre Änderungen zusammen (`b3WarmStartKnots`, `b3PushKnots`,
`b3SolveKnots` und `b3ApplyKnotRestitution`). Vorbereitet und zurückgeschrieben werden die Bündel mit den konvexen
Kontakten der Farben, hinter denen ihre breiten Bedingungen stehen, über eine Spanne mehr in `b3WidePrepareSpan`, die
die Masse der Kopien skaliert. Die übrigen Kontakte des Überlaufs löst Box3D nach den Knoten weiter einzeln. Die Bündel
folgen nur aus der Reihenfolge des Überlaufs, mit jeder Zahl von Threads und in beiden Breiten sind es dieselben, und
die Pakete aus „Ganze Inseln pro Thread“ rechnen ihre Bündel in derselben Reihenfolge. Ein kinematischer Knoten behält
seine Geschwindigkeit. `b3Profile::overflow` misst, wie lange der aufrufende Thread allein am Überlauf rechnet, und
`b3Counters` zählt mit `knotContactCount` und `knotBatchCount` die Kontakte in Bündeln und die Bündel.

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
`src/physics_world.c`). Die Reihenfolge bleibt dieselbe.

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

Die erste, die zweite, die vierte bis achte, die zehnte, die dreizehnte bis einunddreißigste, die dreiunddreißigste bis
fünfunddreißigste, die neununddreißigste bis dreiundvierzigste und die fünfundvierzigste bis siebenundvierzigste ändern
an den Ergebnissen nichts. Die dritte und die neunte ändern sie in den letzten Bits, dafür hängen sie nicht mehr davon
ab, in welcher Reihenfolge Formen in die Bäume kommen. Die elfte ändert sie, weil Kontakte zu anderen Zeitpunkten
entstehen, die zwölfte, weil sie weniger Punkte bekommen, die zweiunddreißigste, weil die Kontakte eines Knotens
gebündelt gegen Kopien rechnen statt nacheinander, die sechsunddreißigste, weil die Kerne dieselbe Rechnung anders
zusammenfassen und deshalb anders runden, und die siebenunddreißigste, weil die Suche andere Kantenpaare aussiebt und
getrennte Hüllen erkennt, die die alte übersah. Die achtunddreißigste ändert sie nur, wo Kugeln Kugeln oder Kapseln
berühren, Kontakte ohne vorausschauende Punkte rechnen oder der PreSolve-Callback Kontakte abschaltet, in Nebenans
Szenen nicht. Die vierundvierzigste ändert sie nur, wo die volle zweite Suche einen Kontakt zwischen Hüllen ergab, die
eine Fläche trennt. Die Ergänzungen stehen in `CMakeLists.txt`, `src/CMakeLists.txt`, `include/box3d/base.h`,
`include/box3d/box3d.h`, `include/box3d/types.h`, `include/box3d/constants.h`, `src/core.h`, `src/core.c`,
`src/shape.h`, `src/shape.c`, `src/body.c`, `src/contact.h`, `src/contact.c`, `src/contact_solver.h`,
`src/contact_solver.c`, `src/contact_solver_wide.inl`, `src/wide8.c`, `src/simd.c`, `src/simd_avx2.h`,
`src/constraint_graph.h`, `src/manifold.h`, `src/convex_manifold.c`, `src/convex_manifold_avx2.c`,
`src/world_snapshot.c`, `src/recording.c`, `src/solver.h`, `src/solver.c`, `src/broad_phase.h`, `src/broad_phase.c`,
`src/dynamic_tree.h`, `src/dynamic_tree.c`, `src/parallel_for.c`, `src/platform.h`, `src/scheduler.h`,
`src/scheduler.c`, `src/physics_world.h` und `src/physics_world.c`, jeweils mit „Added for Nebenan“ markiert.
`B3_HAS_UNIQUE_HULLS`, `B3_HAS_SHAPE_BATCH`, `B3_HAS_SHAPE_CONTACT_LISTS`, `B3_HAS_EXTERNAL_HULLS`,
`B3_HAS_BODY_PREFETCH`, `B3_HAS_STEP_GUARD`, `B3_HAS_PREVIOUS_TRANSFORM` und `B3_HAS_STATIC_OVERLAP` zeigen die zweite,
die vierte, die fünfte, die sechste, die neunzehnte, die dreißigste, die fünfunddreißigste und die zweiundvierzigste an.
Fehlen sie, etwa mit einem eigenen Box3D, läuft Nebenan wie zuvor, nur wachsen Box3Ds Arrays dann durch Umkopieren, die
Hüllen gehen durch die Tabelle, jede Form geht einzeln in den Baum, jedes stehende Bruchstück bekommt einen eigenen
Körper, Box3D hält eine Kopie jeder Hülle, Nebenan holt die Körper der Trümmer nicht voraus, `nbWorld_Step` rechnet auf
dem aufrufenden Thread, `nbWorld_GetDrawPoses` gibt die Lagen nach dem Schritt, und Schutt wird in allen Bäumen gesucht.