# endorphin 2.7.1: Referenz der Verhaltens- und Physiksimulation

Rekonstruiert durch Reverse Engineering von `endorphin.exe` (NaturalMotion endorphin 2.7.1 Learning Edition) mit Ghidra 12.2.
Ziel: die Simulation verstehen und nachbauen können.

## Hinweise für die Arbeit mit dieser Datei

- **Belegstufen:** „sicher“ = durch Klassennamen, Strings, Disassembly oder eindeutige Struktur belegt.
  „vermutet“ = aus der Struktur abgeleitet. „erschlossen“ = mehrere Indizien passen zusammen, kein direkter Beleg.
  Ohne Kennzeichnung gilt „sicher“.
- **Adressen** sind virtuelle Adressen im Programm (Image-Basis `0x400000`). **Offsets** wie `+0x2c` sind Byte-Offsets
  im jeweiligen Objekt. **Slots** wie `0x74` sind Byte-Offsets in der vtable (Slot-Index = Offset / 4).
- **Grenze:** Behaviour- und Charakterdateien (`.nmb`, `.elc`, `.els`, `.elp`, `.cmf`) sind verschlüsselt.
  Sie werden nicht entschlüsselt (§ 95a UrhG: Umgehungsverbot, gilt auch privat und für Forschung). Ebenso nicht den
  Lizenzschutz (HASP) umgehen. Alles hier stammt aus dem Programmcode, der mitgelieferten Hilfe (`endorphin.chm`) und
  unverschlüsselten Begleitdateien (BVH).
- **Quellen auf dem Analyse-Rechner** (falls verfügbar): Ghidra-Projekt `C:\Users\<Nutzer>\RE\endorphin\endorphin.gpr`
  (rund 300 Funktionen benannt, Plate-Kommentar `[endorphin-RE]`), Namenslisten `scripts\names_*.tsv`,
  ausführliche Notizen `Einstiegspunkte.md`.

---

## 1. Programm

| Merkmal | Wert |
|---|---|
| Binärdatei | `endorphin.exe`, PE32 x86, 9 256 960 Byte, SHA-256 `7a1274d9…bd505` |
| Compiler | MSVC 2005 (Linker 8.0), MFC 8.0, nicht gepackt |
| Funktionen / Klassen | ca. 49 500 Funktionen, 2 475 RTTI-Klassen (1 864 mit vtable) |
| PDB | Verweis auf `Endorphin.pdb` (nicht mitgeliefert) |
| Physik | MathEngine **Karma** (statisch gelinkt, C-Code ab ca. `0x7c0000`, ohne RTTI): `Mdt*` Dynamik, `Mcd*` Kollision, `Mst*` Brücke, Solver `MdtArthur` |
| Schutz | HASP-Dongle-Code; Ressourcen-Dateien verschlüsselt (Entropie 8,0) |

Architektur in einem Satz: **Die Engine enthält kaum Verhaltenslogik.** Ein Behaviour ist ein Zeitfenster mit einem
hierarchischen Zustandsautomaten, dessen Bedingungen und Aktionen Ausdrücke in einer eingebauten Skriptsprache sind.
Die Ausdrücke lesen Sensoren und schreiben Effektor-Parameter; die Effektoren regeln Gelenkmotoren in Karma.

---

## 2. Ein Simulationsschritt

```
SimulationTicker::vfunction1                      00473c50
└─ RuntimeEngine::tickPhysics                     00472e30   World an +0xcc; tick++ (+0x108); bei == numTicksToRun (+0x174) stopSimulation 00472350
   └─ World::step                                 00597250
      1. frameNumber++                            World+0xf0
      2. für jeden Charakter (World+0x23c):
         CharacterData::tick                      0059f400
           ├─ Ausdrücke des Charakters            0059ded0 → ExpressionEntryList::evaluateAll 006750b0
           └─ für jeden Effektor: Slot 0x74 = update   (gibt Sollwerte an die Karma-Motoren)
      3. globale Ausdrücke (World+0x274)          006750b0
      4. globale Zustandsautomaten (World+0x2ac, Knoten 0x168 Byte): State::tick 0051eac0
      5. frame = getCurrentEventFrame()           00596330
      6. Triggered Events (World+0x26c → +0x34), zwei Durchläufe in Listenreihenfolge:
         a) alle: preTick(frame) = Slot 0x154     (nur AnimationEventData überschreibt ihn)
         b) alle: tick(frame)    = Slot 0x150     (8 Event-Typen, siehe Abschnitt 9)
      7. PhysicsAdapter::step                     00642680 → stepAll 00648960
         ├─ +0x28 DynamicsWorld::step             0065bf70   Karma-Weltschritt (MdtWorldStep 007c8140), Zeit += h
         ├─ +0x2c CollisionSpace::update          0065bb70   Broadphase auf neuen Positionen
         └─ +0x30 ContactManager::step            0065b5e0   Narrowphase, Karma-Kontakte, Kontaktflags neu
```

### Verzögerungen (wichtig für Nachbau)

| Effekt | Ursache |
|---|---|
| Sensoren sehen den Zustand nach dem **vorherigen** Physikschritt | Ausdrücke laufen vor Schritt 7 |
| Kontakte wirken **einen Schritt verspätet** | Dynamik (7) vor Kollisionserkennung; Solver in Schritt n nutzt Kontakte vom Ende von n−1 |
| Parameter aus Behaviours, Events, globalen Ausdrücken und Zustandsautomaten erreichen den Motor **im nächsten Tick** | Effektor-Setter speichern nur Felder; `Effector::update` läuft in Stufe 2 |
| Ausdrücke des Charakters selbst wirken **im selben Tick** | laufen in Stufe 2 vor dem Effektor-Update |

Reaktionskette eines Behaviours: etwa zwei Ticks vom Sensorwert bis zur Motorwirkung.

**Karma-Solver:** Der Weltschritt teilt die Bodies in unabhängige Partitionen (Inseln) und löst pro Partition das
Constraint-System. Im Binary stehen zwei Löser: ein direkter (Kea, `keaFunctions_Vanilla`; Meldung „Single partition larger than
preallocated memory size … use matrix size capping“) und der iterative **MdtArthur** (`solvePartition` 007d7700; Eingaben
`invMworld`, `unconstVel`, `externalForce`, Ausgaben `lambda`, `constraintForce`, `bodyForce`; Fehlerfall `arthurFailure`).
Arthur arbeitet in **Blöcken zu je 4 Zeilen** (deshalb die durchgehende Vierer-Indizierung der Zeilenpuffer; Varianten
`arthurSolver_vanilla` und `arthurSolver_sse`) und begrenzt die Iterationen pro Partition (`computeIterationCaps` 007d7990:
Methode 0 anteilig zur Partitionsgröße, Methode 1 feste Anzahl). Genaues Verfahren (vermutlich projiziertes Gauß-Seidel)
und welcher Löser standardmäßig aktiv ist: nicht ermittelt.

### Kollisionserkennung (Karma Mcd) und Kontakte (Mst)

Namen: `scripts/names_collision.tsv`.

**Formen** (registriert über `McdGeometryRegisterType` 007c8890): Kugel, Quader, Zylinder, Kapsel (`McdSphyl`), Ebene,
konvexes Netz, Dreiecksnetz/-liste, Aggregat. Schnitttests liegen in einer **Tabelle je Formpaar**; fehlt ein Eintrag, wird das Paar
ignoriert („No intersection function available for this pair combination“). Für Dreieckslisten gibt es eine „SafeTime“-Berechnung
(Zeitpunkt des Aufpralls; bei sehr hoher Winkelgeschwindigkeit werden die Unterschritte begrenzt).

**Schnitttests je Formpaar** (Namen: `scripts/names_pairs.tsv`). Typ-IDs: 0 Null, 1 Kugel, 2 Quader, 3 Ebene, 4 Zylinder,
5 Kapsel, 6 Dreiecksliste, 7 konvexes Netz, 8 Aggregat. Registrierung über `McdFrameworkRegisterInteraction` 007c8960:
Tabelleneintrag (0x18 Byte) für [a][b] und gespiegelt [b][a] mit Tausch-Flag, Felder Schnitttest, SafeTime-Funktion und
Kontaktreduktion ja/nein.

| | Kugel | Quader | Ebene | Zylinder | Kapsel | Dreiecksliste |
|---|---|---|---|---|---|---|
| **Kugel** | 007fe360 | 007fb660 | 007faf80 | 007ecb20 | 007e1760 | 007e8530 |
| **Quader** | | 007fe040 ★ | 007fc020 ★ | 007eedb0 ★ | 007e1d70 ★ | 007e7a20 ★ |
| **Ebene** | | | – | 007ed290 ★ | 007e1350 ★ | – |
| **Zylinder** | | | | 007ec400 ★ | 007e2b30 ★ | 007e5d10 ★ |
| **Kapsel** | | | | | 007e1a10 ★ | 007e3db0 ★ |

★ = mit Kontaktreduktion (mehrere Kontaktpunkte möglich). Aggregate (8) sind mit allen Typen registriert (007dc220, zerlegt in
Teilmodelle). **Konvexe Netze (7) haben keinen einzigen Schnitttest**; Ebene–Ebene, Ebene–Dreiecksliste und Dreiecksliste–Dreiecksliste
ebenfalls nicht (statisch gegen statisch). Paare ohne Eintrag werden ignoriert.

Bekannte Verfahren:
- **Kugel–Kugel** (007fe360): analytisch, Normale = Mittelpunktdifferenz, Punkt nach Radien gewichtet zwischen den Mittelpunkten.
- **Quader–Quader** (007fe040): **Trennachsentest über 15 Achsen** (3 Flächennormalen je Quader, 9 Kantenkreuzprodukte; nahezu
  parallele Kanten übersprungen), kleinste Eindringung bestimmt die Kontaktnormale; dann Clipping der Referenzfläche (007fdf50),
  höchstens so viele Punkte wie im Framework eingestellt, sehr flache Punkte (< 1 % Tiefe) verworfen.
- **Kapsel–Kapsel** (007e1a10): nächste Punkte zweier Segmente (007f7660), dann Radien abziehen.
- **Kugel–Zylinder** (Kern 007ec720, exakt): Kugelmittelpunkt in Zylinderkoordinaten (Achse z, Halbhöhe h, Radius R,
  Kugelradius r). Kandidaten: Deckel `|z| − h − r`, Mantel `√(x²+y²) − R − r`, liegt der Punkt außerhalb beider: Abstand zur
  Deckelkante − r. Der größte Wert gewinnt; Normale (0,0,±1), radial oder zur Kante; Kontaktpunkt = Mittelpunkt + Normale·r.
- **Zylinder–Ebene** (Kern 007ed130): Tiefe über die Stützfunktion `Abstand − (h·|n_z| + R·√(1−n_z²))`; Kontaktpunkte am Rand
  mit Fallunterscheidung **stehend** (cos² > 0,999999, Deckel liegt auf, mehrere Punkte), **liegend** (< 1e-6, Mantellinie)
  und **schräg** (ein Kantenpunkt).
- **Quader–Zylinder** (Kern 007ee320): Trennachsentest über Quader-Flächennormalen, Zylinderachse und Kreuzprodukte Achse × Quaderachsen;
  Projektion des Zylinders auf eine Achse n: `h·|n·a| + R·√(1−(n·a)²)`.
- **Zylinder–Zylinder** (Kern 007ea220, 8,7 KB): Kandidatenachsen nacheinander, jeweils mit Abbruch, sobald eine Trennung größer
  als die Toleranz gefunden ist: Achse A, Achse B, A × B, dann Richtungen zwischen Deckel und Rand bzw. Rand und Rand über
  Stützpunkte (`McdCylinderSupport` 007e51f0). Für den schwierigsten Fall **Rand gegen Rand** sucht Karma die Richtung **numerisch**
  (`McdNumericMinimize1D` 007fac20, Toleranz 0,001). Je nach gewonnenem Fall: Deckel gegen Deckel geclippt (007ea060), sonst Kanten-
  bzw. Punktkontakte (Ausgabe 007e98f0).
- **Dreieckslisten** sind eine **Callback-Geometrie**: Karma fragt über einen Rückruf (`+0x30` der Geometrie) die Dreiecke in einer
  Kugel um das andere Modell ab (Radius = Ausdehnung + Toleranzen; höchstens so viele wie der Puffer `+0x28` fasst) und testet
  jedes Dreieck einzeln: Kugel–Dreieck 007e8150, Quader–Dreieck 007e7000, Zylinder–Dreieck 007e52c0, Kapsel–Dreieck 007e0110.
  **Kugel–Dreieck:** Ebenenabstand ≤ Radius + Toleranz; liegt die Projektion im Dreieck (Vorzeichen der Spatprodukte), Flächenkontakt
  mit der Dreiecksnormale; sonst nächster Punkt auf den drei Kanten (Kanten- bzw. Eckkontakt). Ein Flag macht Dreiecke einseitig.

  **Gemeinsame Dreiecks-Flags** (Feld `+0x18` des Dreiecks): Bit 2 = zweiseitig; Bits 4, 8, 0x10 = Kante 0/1/2 **aktiv**.
  Nur aktive Kanten liefern Kanten-Achsen bzw. Kantenkontakte. So stoßen Körper beim Gleiten über ein Netz nicht an den
  inneren Kanten zwischen zwei ebenen Dreiecken an (bekanntes „internal edge“-Problem). Einseitige Dreiecke ignorieren Achsen
  von der Rückseite (Skalarprodukt < −0,99).

  **Quader–Dreieck** (007e7000): Trennachsentest über **13 Achsen**: Dreiecksnormale (bei einseitigen Dreiecken umgedreht, wenn der
  Quader dahinter liegt), 3 Quaderachsen, 9 Kreuzprodukte Quaderachse × Dreieckskante (nur aktive Kanten, nahezu parallele übersprungen).
  Eine Kanten-Achse ersetzt die bisher beste nur, wenn sie um mehr als einen Mindestabstand besser ist (Hysterese gegen Flackern);
  Quaderachsen werden nur genommen, wenn sie deutlich besser sind als die Dreiecksnormale. Gewinnt die Dreiecksnormale, wird der
  Quader am Dreieck geclippt (007e6b80); gewinnt eine Quaderfläche, Flächenkontakt; sonst Kante gegen Kante.

  **Kapsel–Dreieck** (007e0110): kleinster Abstand zwischen dem Kapselsegment und dem Dreieck: beide Endpunkte gegen die Ebene
  (mit Inside-Test), Segment gegen jede aktive Kante (`McdSegmentSegmentClosest` 007f7240). Kontakt, wenn Abstand < Radius + Toleranz.
  Liegt das Segment nahezu **parallel** zur Dreiecksfläche (Toleranz 0,00025), werden **zwei Kontakte** an beiden Segmentenden
  erzeugt, damit eine liegende Kapsel ruhig liegt statt zu kippeln.

  **Zylinder–Dreieck** (007e52c0): Trennachsentest über Dreiecksnormale (Zylinderbreite `h·|n·a| + R·√(1−(n·a)²)`), Zylinderachse
  (Dreiecksecken auf die Achse projiziert, Intervall gegen [−h, h]) und Kreuzprodukte Achse × Dreieckskante; Fallunterscheidung
  stehend, liegend, schräg wie bei Zylinder–Ebene. Kontakte durch Clipping an der Dreiecksfläche (007e4b60) und zusätzlich je
  aktiver Kante (007e4a30).
- **SafeTime für Dreieckslisten** (007f8550): bestimmt den frühesten Aufprallzeitpunkt entlang der Bewegung im Schritt
  (Kandidaten-Dreiecke entlang des Wegs, nach Zeit sortiert); bei sehr hoher Winkelgeschwindigkeit wird die Zahl der Unterschritte
  begrenzt. Das ist eine kontinuierliche Kollisionsprüfung nur für Dreieckslisten.
- **Kontakt-Toleranz:** Jedes Modell hat eine Toleranz; ein Kontakt entsteht schon, wenn der Abstand kleiner ist als die Summe der
  Toleranzen (spekulative Kontakte mit negativer Eindringtiefe), nicht erst bei Durchdringung.
- **Kontaktreduktion** (`McdReduceContacts` 007f7000): Kontakte mit ähnlicher Normale (Skalarprodukt > 0,9) werden gruppiert und
  ausgedünnt, bevor sie an die Kontaktgruppe gehen.

**Broadphase = Sweep and Prune** (`McdAxisSortUpdateModel` 007dda80): je Modell 6 Endpunkte (Minimum/Maximum je Achse)
in sortierten Listen je Achse; der Paarzustand liegt in einer Dreiecksmatrix mit 4 Bit je Paar. Deaktivierte Modelle werden
auf FLT_MAX gesetzt. Die Paaranzahl ist fest vorgegeben (`maxNumCollisionPairs`; „Resizing not implemented“).

**Narrowphase und Kontaktbrücke** (`MstBridgeUpdateContacts` 007cce40 → `MstHandleCollisions` 007cd110):
```
wiederhole, bis alle Paare abgearbeitet sind ("MstHandleCollisions loop done %u times"):
    McdBatchIntersect: Schnitttests für ein Paket von Paaren, Kontaktpuffer höchstens 4096
    für jedes Paar mit Kontakten:
        Callbacks: Modell (+0x3c), Materialpaar (Tabelle, 0x58 Byte je Eintrag)
        Kontaktgruppe des Modellpaars holen oder anlegen (bleibt über Schritte bestehen)
        für jeden Kontakt: Normale, Punkt, Eindringtiefe (= −Abstand), Materialparameter (76 Byte), Oberflächengeschw. 0
        übrige alte Kontakte der Gruppe entfernen
        Gruppe aktivieren, wenn sie Kontakte hat, sonst deaktivieren
```
Danach setzt endorphins `ContactManager` die Kontaktflags für die Sensoren (Abschnitt 2).

**Kontaktzeilen im Solver** (`MdtContactGroupAddRows` 007cea00) mit dem Parameterblock an Kontakt `+0x84`
(Feldbedeutung aus dem Code; Zuordnung zu den Karma-Namen erschlossen):

| Offset | Bedeutung |
|---|---|
| +0x84 | Kontakttyp: Anzahl bzw. Art der Reibungsrichtungen (1, 2; 3 = Richtung aus der Gleitgeschwindigkeit) |
| +0x88 / +0x8c | Reibungsmodell je Richtung: 1 = proportional (Grenze = Koeffizient · Normalkraft-Schätzung), sonst feste Grenze („Box-Reibung“) |
| +0xa4 / +0xa8 | Richtung 1: Grenze / Koeffizient |
| +0xb0 / +0xb4 | Richtung 2: Grenze / Koeffizient |
| +0x90 | Optionen: Bit 2 = Rückprall, Bit 0x100 = Oberflächengeschwindigkeit |
| +0x94 / +0x98 | Rückprall: Restitution / Schwellgeschwindigkeit (nur wenn Annäherung schneller als Schwelle: v_ziel = −e·v_n) |
| +0x9c | Weichheit der Normalzeile (CFM) |
| +0xa0 | maximale Haftkraft (Untergrenze der Normalzeile = −Haftkraft; sonst einseitig λ ≥ 0) |
| +0xb8 / +0xbc / +0xc0 | Schlupf und Hilfswerte der Reibungszeilen |

Das deckt sich mit endorphins Materialparametern `friction`, `restitution`, `softness`, `primarySlip`, `secondarySlip`,
`maxAdhesiveForce`.

### Weitere Aufrufer des Physikschritts

Weitere Aufrufer von `PhysicsAdapter::step`: `RigEventData::runExtraPhysicsSteps` 004d3de0 (Zusatzschritte) und
`ToolEngine::settleCharacters` 004a8030 (Editor: 15 Schritte Ausdrücke + Physik nach Bearbeitungen).
`PhysicsAdapter::stepCollisionOnly` 00648940 = nur Kollision und Kontakte, ohne Dynamik.

---

## 3. Behaviours, Zustandsautomat, Ausdrücke

**BehaviourData::tick** 004abae0 (Slot 0x150): Zeitfenster von Startframe (`+0x4`) bis Endframe (`+0x160`).
- frame == Start: `State::enter`; Start < frame < Ende: `State::tick`; frame == Ende: `State::exit`, dann alle Effektoren auf
  Kalibrierung `"Default"` (`setCalibration`).
- `BehaviourData::resetRuntime` 004abf60 (Slot 0x15c, vermutet): Laufzeit-Automat (`+0x170`) löschen.

**Hierarchischer Zustandsautomat:**

| Funktion | Adresse | Verhalten |
|---|---|---|
| `State::enter` | 0051e3d0 | `active=1`. Typ 1 (exklusiv): ersten Startübergang (`+0x134`) mit wahrer Bedingung nehmen (`enterInitialChild` 0051e360). Sonst (parallel): alle Unterzustände betreten. Danach onEnter-Ausdruck. |
| `State::tick` | 0051eac0 | Übergänge (`+0x124`, je 0xc0 Byte) prüfen; feuert einer → `takeTransition`. Sonst onTick-Ausdruck, dann Unterzustände (`+0x144`, je 0x168 Byte). |
| `State::exit` | 0051e440 | `active=0`, Unterzustände rekursiv verlassen, onExit-Ausdruck. |
| `State::takeTransition` | 0051e710 | exit → Übergangsaktion (`runAction` 0051ddd0) → enter |
| `Transition::evaluateCondition` | 0051e000 | Bedingungsausdruck (`+0xb8`) auswerten, als bool lesen |

**Ausdrücke:** `ExprParser::Interpreter::evaluate` 00680800 (wirft `ExprParser::ExpressionException`).
Operatoren u. a. Add, Subtract, Power, Sin, Cos, Tan, Sinh, Tanh, Sqrt, Not, NotEquals, Or, Conditional, Random, Print,
Vektor-Konstruktor. Objekte (Sensoren, Effektoren, Netze, World) stellen **benannte Symbole** bereit (Lesen/Schreiben/Methoden),
gebunden über Templates `ExprParser::ReadMethodType`, `WriteMethodType`, `ReadWriteMethodType`, `FunctionType`.
World-Symbole: `TickNumber`, `FrameNumber`, `FrameTime`, `ChannelFrameTime`, `TimeStep`, `SetGravity`, `SetFriction`,
`SimulationFrameRate`, `getCollisionWithSensor(a, b)`.
`ExpressionEntry::evaluateAndAssign` 006746d0: Ausdruck auswerten und Ergebnis über Setter (Slot 0xa4) in die Zielvariable schreiben.

---

## 4. Sensoren

**Pull-Modell:** keine Update-Phase. Jeder Getter rechnet beim Lesen frisch aus dem Karma-Zustand (Muster: `compute()` schreibt
Cache an `+0x30`, Getter liefert ihn).

**Schnittstelle (`Sensor`-vtable):**

| Slot | Funktion | Ausdrucks-Symbol |
|---|---|---|
| 0x38 | `getVector3` | `Vector` |
| 0x3c | `getTM` (Matrix) | |
| 0x40 | `getComponent(i)` | |
| 0x44 | `getValue` (roh) | `UnNormalized` |
| 0x48 / 0x4c / 0x50 | Komponente 0/1/2 | `X`/`Alpha`, `Y`/`Beta`, `Z`/`Gamma` |
| 0x5c | Komponente normiert | |
| 0x60 | Wert normiert | `Normalized` |
| 0x64 | gehört zu Metabody? | |

**Normierung** `Sensor::mapToUnitRange` 005ff080, mit `Lower`/`Upper` aus den Sensordaten (XML auch `OutputScale`):
```
x ≤ Lower → −1;  x ≥ Upper → +1;  sonst  n = (x − (Lower+Upper)/2) · 2 / (Upper − Lower)
```
Gleiche Konvention wie `DesiredValue` der Effektoren: ein Sensor-`Normalized` lässt sich direkt (mit Faktor) auf einen Effektor-Sollwert schreiben.

**Sensortypen** (Registrierung `MetaBodySensor::registerSensorTypes` 00602810, 40 Typen):

| Gruppe | Typen | Berechnung |
|---|---|---|
| Position | AbsolutePosition, AbsoluteHeight, AbsolutePosAlongX/Z | Karma-Body-Position (`+0x140`) |
| Geschwindigkeit | AbsoluteVelocity, …Vertical…, …Horizontal…(Mag), …Az, …X/Z | Body-Lineargeschwindigkeit (`+0xb0`); horizontal = sqrt(vx² + vz²) |
| Schwerpunkt | TreeCentreOfMass(X/Y/Z), TreeCentreOfMassVelocity(X/Y/Z) | über alle Bodies des Teilbaums Σ mᵢ·vᵢ / Σ mᵢ (Masse `+0x1c8`) |
| Orientierung | AbsoluteTM, AbsoluteOrientation, RelativeOrientation, AbsoluteShoulderEulers/Alpha/Beta/Gamma, AbsoluteGeneralEulers | Rotationsmatrix; `SetReference` speichert aktuelle Rotation (`+0x54`), `DiffTwistSwing` = R · R_refᵀ als Twist/Swing (Abschnitt 7) |
| Gelenk | ActualHingeAngle, ActualHingeAngularVelocity | Winkel über Gelenk des Metabody; **Winkelgeschwindigkeit = (θ − θ_alt)/(t − t_alt) zwischen zwei Lesezugriffen** |
| Kontakt | IsColliding, IsCollidingWith, IsCollidingWithCharacter, CollisionPosition/Orientation | Kontaktliste des Metabody (`+0x160`), aktiv = Flag `+0x46` (vom ContactManager gesetzt) |
| Zeit | AbsoluteTime, RelativeTime, MBRelativeTime | Simulationszeit bzw. Zeit seit `resetTime` |
| Gedächtnis | Integral | Ringpuffer: `Insert(x)` ersetzt ältesten Wert, laufende Summe; `UnNormalized` = Summe, `Average` = Summe/Größe, `Resize(n)` |
| Achsen | VerticalAxis, HorizontalAxis | Verbindungsvektoren zwischen Body-Paaren aufsummiert und normiert (gemittelte Körperachse) |

**Zusammengesetzte Sensoren** (Eingänge = andere Sensoren; `CompoundSensor::registerSensorTypes` 0062e120):

| Typ | Berechnung |
|---|---|
| AbsoluteDifference | in0 − in1 (Vektor) |
| Average | Mittelwert der Eingangsvektoren |
| Minimum | Minimum der Eingänge |
| SimpleErrorFilter | skalar: in0.Normalized + gain(+0x48) · (in1 − in2 − 0,03); vektoriell ohne Offset |
| ConvertRefFrame | Vektor in anderes Bezugssystem (vermutet) |
| FrontalPlane / SagittalPlane / HorizontalPlane | Ebene durch Referenzpunkt aus Vertikal- und Horizontalachse bzw. deren Kreuzprodukt; `checkDistance(p)` = vorzeichenbehafteter Abstand („Fuß vor dem Schwerpunkt?“) |
| RelativeAngle, RelativeAngleEl | Winkel zwischen Eingängen (nicht im Detail geprüft) |

**Fallen:** Hinge-Winkelgeschwindigkeit hängt vom Lesezeitpunkt ab (zweimal im selben Tick gelesen → Division durch 0).
`Integral` ist eine Fenstersumme ohne Zeitschritt, das Behaviour muss pro Tick `Insert` aufrufen.

---

## 5. Effektoren (Gelenkantriebe)

Zwei Ebenen: Daten `NMData::Effector*Data` (XML, Editor) → `createRuntime` (Slot 0x3c, z. B. 0060cde0) erzeugt Laufzeitobjekt
(`EffectorBase` → `Effector`, `EffectorWithMemory`, `EffectorWithModes`, `EffectorSkeletal`, `EffectorGroup`) und hängt es über
`m_jointName` an ein Gelenk. `CharacterData::tick` ruft pro Tick Slot 0x74 = `update`.

### 5.1 Hinge-Effektor (`Effector`): P-Regler auf Geschwindigkeitsebene

```
Effector::update 00603010 → applyServo 00603720 → computeMotorVelocity 00603230
θ      = joint->getAngle(achse)                 RelationshipHinge::getAngle 00658b90 = −MdtLimitGetPosition
fehler = θ − θ_soll                              (+0x30 = fehler, θ_soll an +0x2c)
ω      = clamp(−Gradient · fehler, ±GradientCap)  (Gradient +0x4c, GradientCap +0x50)
joint->setAxisMotorVelocity(achse, ω);  joint->setAxisMaxForce(achse, aktiv ? BaseForceCap : 0)
joint->apply = RelationshipHinge::applyMotor 006593e0 → MdtLimitSetLimitedForceMotor(limit, −ω, kraft) 007c9e80
```
- Kein PD-Moment; die Dämpfung entsteht, weil Karma die Geschwindigkeit implizit einregelt.
- `DesiredValue` ∈ [−1, 1] über den Gelenkbereich: `θ_soll = unten + (DesiredValue + 1)/2 · (oben − unten)`;
  oberer Anschlag minus 0,001 rad.
- Kalibrierung `setCalibration(name)` 006032f0 lädt `Gradient`, `GradientCap`, `BaseForceCap` aus einem benannten Satz;
  Standard 20 / 3 / 5 (`createDefaultCalibration` 0060be10). Behaviour-Ende setzt `"Default"`.
- `scaleStrength(f)` 00603370 (Slot 0x70) skaliert alle drei Werte (globale „Muskelkraft“).

### 5.2 Effektoren mit Gedächtnis: adaptive Stärke statt I-Anteil

Überschreiben nur `applyServo` (Slot 0x84). Sollgeschwindigkeit wie 5.1; das Gedächtnis erhöht die **Kraftgrenze**:
```
puffer.insert(|fehler|)                               Ringpuffer, Länge MemorySpan (Ticks!), Betrag des Fehlers
WithMemory (0063ca70): F_max = BaseForceCap + IntegralGradient · mittel(|fehler|)          (keine Obergrenze)
WithModes  (0063d4a0, 0063ce60): F_max = min(BaseForceCap + Σ_k fenster[k]·|fehler|_k, MaxAccumulation)   k=0 ältester
```
Fensterfunktionen (`buildWindowFunction` 0063cfd0; N = MemorySpan, G = WindowGain):
`Flat` = G; `DiagUp` (Standard) = G·k/(N−1); `DiagDown` = G·(N−1−k)/(N−1); `Triangle` = steigt bis zur Mitte, fällt dann.
Pendeln um das Ziel macht das Gelenk stärker (Betrag wird gespeichert). `SetIntegralGradient` per Ausdruck änderbar.

### 5.3 Kugelgelenk-Effektor (`EffectorSkeletal`)

`EffectorSkeletal::update` 00630810: inaktiv → Antriebe aus; erst nach `timeDelay` aktiv (Reflexlatenz); dann am `SkeletalJoint`:

| Slot | Setter | Constraint-Feld (KarmaSkeletal) |
|---|---|---|
| 0x50 | setAngularDriveMode | +0x1f4 |
| 0x54 | setLinearDriveMode | +0x1e4 |
| 0x58 / 0x5c / 0x60 | Linear Stiffness / Damping / ForceLimit | +0x1e8 / +0x1ec / +0x1f0 |
| 0x64 / 0x68 / 0x6c | **Swing** Stiffness / Damping / ForceLimit | +0x204 / +0x208 / +0x20c |
| 0x70 / 0x74 / 0x78 | **Twist** Stiffness / Damping / ForceLimit | +0x1f8 / +0x1fc / +0x200 |
| 0x7c | setOrientationTarget (Quaternion) | +0x170 |
| 0x80 | setLinearTarget | +0x160 |
| 0x84 | setLinearVelocityTarget | +0x180 |
| 0x88 | setAngularVelocityTarget | +0x190 |

Kalibriersatz `SkeletalCalibrationVariables`: twist/swing/linear Stiffness, Damping, ForceLimit, DriveMode, timeDelay.
`scaleStrength` 006309e0 skaliert alle 9 Werte.

### 5.4 Ausdrucks-Schnittstelle der Effektoren

`EffectorBase::registerExpressionSymbols` 006094e0 / `…Methods` 00609590:
`DesiredDisplacement`, `ActualDisplacement`, `PosedDisplacement`, `ScaleCalibration`, `ScaleForceCap`, `DesiredVelocity`,
`getForce`, `getTorque`, `getTorqueZ`, `setActive`, `setEnabled`, `setDesiredPose`, `setCalibration`, `setRawDesiredPoseRotation`,
`setRawDesiredPoseAngVelocity`, `scaleCalibration`, `clampCalibration`. Hinge zusätzlich `Gradient`, `GradientCap`, `BaseForceCap`.
Skeletal: Twist/Swing/Linear-Parameter, `TimeDelay`, Drive-Modi, `DesiredQuat`, `DesiredTwistAndSwing(Angle|Normalized|Velocity)(Raw)`,
`DesiredPosition(Normalized)`, `DesiredLinearVelocity`.
**Alle Setter speichern nur Felder** (z. B. 00603090, 00630480); keiner ruft den Motor.

---

## 6. Kugelgelenk-Constraint im Solver

`SkeletalJoint::createConstraint` 0064faa0 → `KarmaSkeletal::create` 007c5d00 → `init` 007c5760:
eigener Karma-Constraint, Solver-Callback `+0xa0` = **`KarmaSkeletal::addRowsToSolver` 007cff30** (13,6 KB), bis 9 Zeilen,
gesperrte DOF `+0x1a0` = 0x700 (drei lineare Achsen gesperrt → Kugelgelenk), Antriebsparameter anfangs FLT_MAX.

**Antrieb = weiche implizite Constraint-Zeilen** (kein explizites Federgesetz):
```
eps   = 1 / (d + k·h)          // CFM, h = Zeitschritt
gamma = k·h · eps              // ERP
Zeile: J·v = v_soll − (gamma/h)·fehler + eps·λ,   −F_max ≤ λ ≤ F_max   (ohne Kraftgrenze ±FLT_MAX)
```
Von eps/gamma werden die globalen Solver-Standardwerte abgezogen (Karma addiert sie wieder). eps < 0,1·Standard → auf 0,1·Standard
begrenzt (Obergrenze der wirksamen Steifigkeit). Stabil auch bei großem k; explizite Feder `F = k·e + d·ė` verhält sich anders.

**Zeilen:**

| Teil | Zeilen | Parameter | Fehler / Ziel |
|---|---|---|---|
| gesperrte DOF | je 1, hart | – | Standard: 3 lineare Achsen |
| Linearantrieb | 3 (freie Achsen) | linear k/d/F | Positionsziel +0x160, Geschwindigkeitsziel +0x180 |
| Twist | 1 | twist k/d/F | Drehung um Gelenk-x-Achse aus (w, x) des Fehler-Quaternions |
| Swing | 2 | swing k/d/F | Kippung der x-Achse |
| Iso (Modus-Bit 8) | 3 statt Twist+Swing | swing k/d/F für alle | Quaternion-Fehler gleichmäßig |
| Grenzen | je 1, nur bei Verletzung | siehe unten | |

**Drive-Modus-Bits** (linear +0x1e4, Winkel +0x1f4): 1 = Positionsfehler, 2 = Geschwindigkeitsziel, 4 = Kraftgrenze, 8 = Iso (nur Winkel).
Namen im Binary: `DriveNone`, `DrivePosition`, `DriveVelocity`, `DrivePosAndVel`, `DriveForceLimit`, `DrivePositionFL`,
`DriveVelocityFL`, `DrivePosAndVelFL`, `DriveIso`.

**Gelenkgrenzen: weich und hart** (Hilfe bestätigt: innerhalb weich nur Dämpfung, zwischen weich und hart Rückstellfeder, an hart unendlich):

| Grenze | hart | weich | weiche k/d |
|---|---|---|---|
| Flags | +0x1a4 | +0x1b8 | |
| Abstand (linear) | +0x1a8 | +0x1bc | +0x1c0 / +0x1c4 |
| Twist (Kosinus) | +0x1ac | +0x1c8 | im Code die Twist-Antriebswerte +0x1f8/+0x1fc (beobachtet, unklar warum) |
| Swing-Kegel (Kosinus je Achse) | +0x1b0, +0x1b4 | +0x1d4, +0x1d8 | +0x1dc / +0x1e0 |

- Twist verletzt, wenn `c²·x² − (1 − c²)·w² > 0` (entspricht |tan(θ/2)| > tan(Grenze/2)); einseitige Zeile.
- Swing = **elliptischer Kegel**: verletzt, wenn `σ_y²·(1+c_y)/(1−c_y) + σ_z²·(1+c_z)/(1−c_z) > K` mit Swing-Komponenten σ;
  `(1+c)/(1−c) = cot²(Grenze/2)`. Korrektur senkrecht zum Kegelrand.
- Harte Zeile schließt an die Federkraft am Übergang weich→hart an.

---

## 7. Twist/Swing-Darstellung

`Quat::toTwistSwingTanQuarter` 00693fe0, q = (w, x, y, z), Gelenkachse x:
```
r       = sqrt(w² + x²)
twist   = x / (w + r)                         = tan(θ_twist / 4)
swing_y = (y·x − w·z) / (r · (r + 1))          |swing| = tan(φ_swing / 4)
swing_z = (z·x + w·y) / (r · (r + 1))
Winkel  = 4 · atan(Komponente)                 (Quat::toTwistSwingAngles 006946c0)
```
Keine Euler-Probleme, keine Singularität unter 360°. `DesiredTwistAndSwing` (roh) = dieser Vektor, `…Angle` = Winkel,
`…Normalized` = bezogen auf die Grenzen. `setDesiredQuat` 00630d90 rechnet so um.

---

## 8. Charakter-Aufbau

```
Charakter (CharacterData)      lokales Koordinatensystem „Character Cube“, Ursprung zwischen den Füßen
└─ Gelenk-Hierarchie (Wurzel = skeleton root joint)
   ├─ Skeletal Joint  3 Drehachsen (Twist + 2× Swing)  → SkeletalJoint / KarmaSkeletal
   ├─ Hinge Joint     1 Drehachse                      → RelationshipHinge / Karma-Hinge mit Limit
   ├─ Simple Joint    Kettenende, ohne Masse und Gelenk
   │  (weitere Klassen, nicht untersucht: MagnetJoint, RelationshipRPROMagnet, RelationshipRPROJoint, RelationshipFixedPathJoint)
   └─ je Gelenk außer Simple: 1 Masseobjekt (MetaBody) → 1 Karma-Body
      └─ 1..n Kollisionsobjekte (CollisionModel)
```
- Grenzen wirken auf einen **Limit-Offset**, zu dem das Kindgelenk relativ versetzt ist.
- Standard-Knie = Skeletal Joint mit sehr kleinen Swing-Grenzen (laut Hilfe).
- CharacterData-Listen: Metabodies `+0x590`, Gelenke `+0x59c`, Effektor-Container `+0x5ec`, Physik-Stufe `+0x5f4`, Standardstufe `+0x5f0`.

**Masseobjekte:**

| Eigenschaft | Umsetzung |
|---|---|
| Formen | Quader, Kugel, Zylinder, Kapsel („Sphyl“) |
| Masse | `m = Volumen · Dichte · 1000` (Dichte relativ zu Wasser; Standardcharakter ca. 1,5–2,0, neu 0,75) |
| Trägheit | Kugel exakt (`setMassSphere` 0065aa30); Quader exakt (0065aab0); **Zylinder als Quader 2r×2r×L** (0065ab40); **Kapsel als Quader 2r×2r×(L+2r)** (0065abe0). Überschätzt bei Gliedmaßen ca. 37–44 %. |
| Klemmung | `MdtBodySetInertiaTensor` 007c6490 klemmt Diagonale auf [m·L²/r, m·L²·r] (Welt-Skalen) |
| Weitere Daten | Linear-/Winkel-Geschwindigkeitsdämpfung (Karma `+0x1cc`/`+0x1d0`), Anfangsgeschwindigkeiten, Kräfte, optional explizite Trägheit (`inertial`) |

Karma-Body-Felder: Position `+0x140`, Orientierung `+0x110`, Lineargeschw. `+0xb0`, Winkelgeschw. `+0xc0`, Masse `+0x1c8`.

**Kollisionsobjekte und Materialien:**
- Gleiche Formen; `useParentScale`; Kollisionsmodi `collidesWithSiblings`, `collidesWithParent`, `collidesWithAncestors`;
  zusätzlich `DisabledCollisionPair`.
- Materialpaar (`materialA`, `materialB`) → `friction`, `restitution`, `softness`, `primarySlip`, `secondarySlip`, `maxAdhesiveForce`.
  Standardcharakter: Haut (Körper), Schwamm (Finger), Gummi (Ferse, Ballen, Zehen).

**Charakter-Arten:** Simulation Character (nur dieser bekommt Behaviours), Reference Character (eigene Skelette aus FBX/BVH/…,
für Motion Transfer), Prop Character (Gegenstände, Kreaturen), Environment.

**Character Rig:** verbindet je ein Masseobjekt zweier Charaktere über einen Rig Node mit je 3 Positions- und 3 Winkelstärken
(lokale Achsen); Gruppen `Standard Motion Transfer`, `Advanced Motion Transfer` (globale Achsen).

**Physik-Stufen** (`CharacterData::setPhysicsLevel` 005a1150; Zuordnung Wert → Name erschlossen):

| Wert | Name | Hinweise |
|---|---|---|
| 0 | NoPhysics | Flag 0x200000 |
| 1 | PosingNoPhysics | |
| 2 | CollisionOnly | ab > 1 Kollision vorbereitet; Flag 0x1000000 |
| 3 | PosingCollision | |
| 4 | CollisionOnlyWithInertia | Flag 0x1000000, `momentumScale` (Charakter `+0x604`) |
| 5 | PosingPhysics | |
| 6 | FullPhysics | Ziel von Transition-Events |
| 7 | Standard | nimmt `+0x5f0` |

**XML-Laden:** in allen `NMData`-Knoten Slot 0x54 = loadXML, 0x58 = saveXML (z. B. MetaBodyData 005c0a50,
CollisionModelData 005c4100, HingeData 005f2f80, SkeletalJointData 005f30e0, SensorData 0060e890).

### 8.1 Charakterdatei-Format (Klartext-XML)

Quelle: die mitgelieferten, unverschlüsselten Exporter-Skripte (`Resources\Third Party\Maya\Scripts\endorphin_Exporter.mel`,
`Resources\Third Party\3dsMax\Scripts\endorphin_*.ms`). Sie schreiben Charaktere (`.nmc`, in der Learning Edition `.elc`)
als **unverschlüsseltes XML**. Eigene Charaktere lassen sich so anlegen, ohne mitgelieferte Dateien zu öffnen.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Character name="prop" releaseVersion="2" createDefaultCollisionModel="false">
  <MetaBodies>
    <MetaBody name="…">
      <CollisionSurface name="…" material="…" collidesWithParent="…" collidesWithSiblings="…"
                        collidesWithAncestors="…" isRendered="true">
        <TransformMatrix> <Vi x= y= z=/> <Vj …/> <Vk …/> <Vp …/> </TransformMatrix>
        <Geometry type="Sphere|Cylinder|Sphyl|Cuboid" renderMaterial="collision" param0= param1= param2=/>
      </CollisionSurface>
      <InertialBody>
        <Geometry type="…" renderMaterial="inertial" param0=…/>
        <Inertia density="…"/>
      </InertialBody>
    </MetaBody>
  </MetaBodies>
  <Skeleton name="…" dofOrder="345012" metaBody="…">
    <ChildJoints> <SkeletalJoint name="…" dofOrder="345012" metaBody="…"> … </SkeletalJoint>
                  <Null name="…" …/> </ChildJoints>
  </Skeleton>
  <Effectors>
    <Effector name="…Effector" jointName="…" type="Skeletal" active="true" linearDriveMode="DriveNone" angularDriveMode="DriveNone">
      <Calibration blinearStiffness="41200000" … btimeDelay="41200000"/>
    </Effector>
  </Effectors>
  <Calibrations>
    <Calibration name="Default">               <SkeletalEffectorParameters name="…Effector" …/> </Calibration>
    <Calibration name="DesiredPoseCalibration"> <SkeletalEffectorParameters name="…Effector" …/> </Calibration>
  </Calibrations>
</Character>
```
- Geometrie-Parameter: Sphere `param0` = Radius; Cylinder/Sphyl `param0` = Radius, `param1` = Höhe bzw. Länge;
  Cuboid `param0..2` = Breite, Höhe, Tiefe (Meter).
- **Attribute mit vorangestelltem `b`** (`bswingStiffness` …) sind IEEE-754-Floats als Hex-Bitmuster:
  `41200000` = 10,0; `41a00000` = 20,0; `447a0000` = 1000,0; `461c4000` = 10 000,0; `00000000` = 0.
- Vorgaben des Exporters für Props: Kalibrierung `Default` alle Werte 10 (`timeDelay` 0);
  `DesiredPoseCalibration` Swing/Twist-Steifigkeit 10 000, alle anderen 1000.
- Hierarchie-Regel: jedes Gelenk außer dem Endgelenk hat genau ein Masseobjekt; ein Masseobjekt beliebig viele Kollisionsobjekte.

### 8.2 Virtuelle Muskeln

Nicht in der Hilfe dokumentiert. Im Code: `CharacterData` hat eine Liste `+0x654` von **benannten Gruppen von Masseobjekten**
(XML-Abschnitt `VirtualMuscleSelection` mit Einträgen `VirtualMuscle` und Kindern `MetaBody`; `loadVirtualMuscles` 005a7d20,
`saveVirtualMuscles` 005a0490). Gelenke (`RelationshipData`) tragen ein Flag `isVirtualMuscle` (`+0x21`, geladen in 005f2970).

Ergebnis der Suche nach allen Zugriffen auf `+0x654` (Skript `FindOffsetUse.java`): Die Liste wird nur beim Laden, Speichern und
im Konstruktor benutzt und von der **Editor-Auswahl** (`MetaBodyJointSelection` → `selectVirtualMuscle` 005a63f0).
Kein Verbraucher in der Simulation. Folgerung (vermutet): „Virtuelle Muskeln“ sind in dieser Version **benannte Auswahlgruppen
im Editor ohne physikalische Wirkung**; das Gelenk-Flag wurde nur beim Laden/Speichern gefunden (Suche danach nicht erschöpfend).

### 8.3 Fluide (Auftrieb und Strömungswiderstand)

Die Kräfte rechnet **kein fester Programmcode**, sondern ein Behaviour („Physical Effect: Fluid“, Varianten Fluid 1 / Thin / Thick;
Inhalt verschlüsselt). Das Programm liefert die Bausteine, die Behaviour-Ausdrücke aufrufen:

- **Fluid-Objekte** (`NMData::FluidObjectData`, Unterklasse von `CollisionModelDataBase`): Formen wie Kollisionsobjekte, je
  Masseobjekt beliebig viele, Schalter `isFluidEnabled`.
- **Ausdrucks-Methoden** an Fluid-/Kollisionsobjekten und Masseobjekten: `getVolume`, `getAreaToNormal(richtung)`,
  `getDistanceToPoint(p)`, `nearestToPoint`, `getVelocityAtPoint`, `addForceAtPostion`, `addImpulseAtPostion`
  (Schreibweise „Postion“ so im Binary), `addForce`, `addTorque`, `addImpulse`.
- **Geometrie** (`Geometry::*Type`):

| Form | Volumen (Slot 0x0c) | Querschnittsfläche `areaToNormal(n)` (Slot 0x14, n lokal) |
|---|---|---|
| Kugel | 4/3·π·r³ | π·r² |
| Quader | x·y·z | \|y·z·n_x\| + \|x·z·n_y\| + \|x·y·n_z\| |
| Zylinder (Achse lokal z) | π·r²·L | 2r·L·(n_x + n_y) + \|π·r²·n_z\| (Summe je Achse, keine exakte Projektion) |
| Kapsel | π·r²·(4/3·r + L) | wie Zylinder, Seitenfläche 2r·L + π·r² |

  `distanceToPoint` (Slot 0x10): Kugel über den nächsten Oberflächenpunkt, Quader durch Klemmen des Punkts auf die Ausdehnung.

**Modell laut Hilfe** (genaue Formel steckt im verschlüsselten Behaviour): Ein **Fluid-Würfel** (Größe, Position) mit
`Fluid Density` (Standard 1000 kg/m³; Luft ≈ 1,2, Meerwasser 1030, Schlamm 1750, Treibsand 2000, Lava 2600).
Auftrieb nach oben für Fluid-Objekte im Würfel, abhängig vom Volumen (Schwimmen, wenn Gesamtvolumen/Masse-Dichte < Fluiddichte).
Widerstand: `Drag` (Standard 200, geschwindigkeitsunabhängiger Anteil), `Turbulent Drag` (Standard 200, proportional zur
Geschwindigkeit), `Angular Drag` (bremst Drehung); maßgeblich ist die **Querschnittsfläche in Bewegungsrichtung** (daher
`areaToNormal`). Kräfte wirken auf das jeweilige Masseobjekt. Überlappende Fluid-Objekte zählen doppelt.
Für einen Nachbau naheliegend: `F_auftrieb = ρ·g·V_eingetaucht`, `F_widerstand = −A(v̂)·(Drag + TurbulentDrag·|v|)·v̂`
am Mittelpunkt jedes Fluid-Objekts (eigene Annahme, nicht aus dem Code).

### 8.4 Weitere Gelenkarten

| Klasse | Umsetzung |
|---|---|
| `RelationshipRPROJoint`, `RelationshipRPROMagnet`, `MagnetJoint` | alle drei erzeugen **denselben Karma-Constraint** (vermutlich Karmas RPRO = „Relative Position, Relative Orientation“): `KarmaRPRO::create` 007c9bc0 / `init` 007c9950 |
| `RelationshipFixedPathJoint` | erzeugt **keinen** Constraint (Slot 0xc0 leer); in dieser Version praktisch ohne Funktion |

**KarmaRPRO** (`addRowsToSolver` 007d3ea0): 6 **harte** Zeilen, 3 für die Relativposition, 3 für die Relativorientierung
(Fehler = halber Vektorteil des Fehler-Quaternions). Je Achse eine Kraft- bzw. Momentgrenze (linear `+0x1cc…`, Winkel `+0x1c0…`);
**Grenze 0 schaltet die Achse ab**. Anbindung je Body: Position `+0xec` / `+0x12c` (`setAttachmentPosition` 007c9aa0),
Orientierung `+0x160` / `+0x174` (`setAttachmentQuaternion` 007c9ae0). Die „Stärke“ eines Magneten ist also eine Kraftgrenze,
kein Feder-Dämpfer.

### 8.5 Motion Transfer (Rig-Event im Detail)

Das Rig-Event legt zwischen den per Rig Node verbundenen Bodies von Quell- und Zielcharakter **RPRO-Magnete** an
(`createMagnets` 004dce50, vermutet). Plausibel und zur Struktur passend (erschlossen, Feldzuordnung nicht verfolgt):
die drei Positions- und drei Winkelstärken eines Rig Node sind die Achs-Kraftgrenzen des Magneten. Pro Tick wird die Zielanbindung
nachgeführt, dann rechnet das Event `physicsTicks` Zusatzschritte (`initialPhysicsTicks` beim Start, optional Subticks).
Einstellungen (`loadXML` 004dbe80): `Standard Character Rig`, `Standard Motion Transfer`, `friction`, `gravity`
(beide während des Rigs abschaltbar und danach wiederhergestellt), `physicsTicks`, `initialPhysicsTicks`, `isUseSubTicksEnabled`,
`isInitTargetPoseEnabled`, `isLocked`, `retargetRefFrame`, `rootBias`.

---

## 9. Triggered Events (8 Typen)

Alle mit Zeitfenster Startframe `+0x4` … Endframe `+0x160`, aktiv-Flag `+0x24`. preTick = Slot 0x154 (Basis leer, `RET 4`),
tick = Slot 0x150.

| Event | tick | Wirkung |
|---|---|---|
| **Behaviour** | 004abae0 | Zustandsautomat im Zeitfenster (Abschnitt 3) |
| **Animation** | 004e4550 | zwei Arten, siehe unten; preTick 004e2760 gibt am Fensterende den Charakter frei, **bevor** andere Events ticken |
| **DesiredPose** | 0048aa10 | feste Pose als Effektor-Sollwert; Stärke kann **Ausdruck** sein (`+0x68`), jeden Tick ausgewertet, min. 1e-5; Ende: Flag `+0xb0` ? eigene Effektoren : alle Effektoren des Charakters auf `"Default"` |
| **Force** | 004c63f0 | einmal am Start: gewichteter Mittelpunkt der Bodies; je Body Impuls = Grundvektor · (1 + Abstand · Faktor `+0x14`) |
| **Sever** | 004c7870 | einmal am Start: Aktion `JoinBodies`, `LockWithParent`, `LockOrientation`, `LockPosition` |
| **Incarnation** | 004cea40 | einmal am Start: Physik-Stufe setzen (`applyMode` 004ce950) |
| **Transition** | 004d2a20 | Start: FullPhysics; pro Tick Mischgewicht += Rate, geklemmt [0,1] (`setBlendWeight` 0059dca0); Ende: Zielstufe |
| **Rig** | 004db020 | Motion Transfer zur Laufzeit: pro Tick Rig aufbauen (temporäres DesiredPose-Event), N Zusatz-Physikschritte (`+0x1ec`), wieder abbauen |

**Animation-Event, zwei Teilobjekte:**
- `+0x9e8` kinematische Wiedergabe: `attach` 004e1e90 setzt Charakter `+0x298` auf die Wiedergabe; `releaseCharacter` 004e1f50 setzt 0.
- `+0x9ec` Effektor-Antrieb (`applyPoseToEffectors` 004e27a0), Physik bleibt aktiv:
```
für jeden Effektor:
    q = Rotation aus Animation zur Zeit t
    falls t < T_einblend: q = slerp(q_start, q, t / T_einblend)     (q_start von capturePose 004e4350)
    falls Stärke·10 ≤ 1e-5: setCalibration("Default")
    sonst: setRawDesiredPoseRotation(q); falls Satz "DesiredPoseCalibration" existiert:
           setCalibration("DesiredPoseCalibration"); scaleCalibration(Stärke); clampCalibration(100)
optional (+0x160): Wurzelposition/-orientierung; Animationszeit += Schritt (+0x850); letzter Frame: finish 004e2fe0
```
Die Engine ruft die Effektor-Methoden hier über die Namens-Schnittstelle der Ausdrücke auf.

---

## 10. Evolution und neuronaler Regler

**Genom:** Chromosomen aus Gleitkomma-Genen; Descriptor je Gen: `LowerLimit`, `UpperLimit`, `typicalMutationRate`, `typicalMutationSize`.

**ThreeLayerNet** (eine `NetworkConnectorData`), Gene je Knoten `[Slope, Bias, w₁…wₙ]` (`forward` 005228a0, Disassembly):
```
h_j = 1 / (1 + exp(−(bias_j + Σ x_i · w_ij)))
y_k = 1 / (1 + exp(−(bias_k + Σ h_j · w_jk)))      Ausgänge in (0, 1)
```
**Slope wird gespeichert/ausgegeben, aber nicht verwendet.** Aufruf per Ausdruck `callTickMethod` → `NetworkConnectorData::tick`
00525c40: alle Eingangs-Variablen lesen (z. B. Sensoren), rechnen, jeden Ausgang in seine Zielvariable schreiben (z. B. Effektor-Sollwert),
erster Ausgang als Rückgabe.

**Bewertung:** `Evaluator::evaluateIndividual` 00504230: je Task (`TaskManager`: `TaskTemplate` mit Parameterbereichen →
konkrete `Tasks`, `randomizeTasks`) Szene zurücksetzen, `PhysicalEvaluator::evaluateTask` 00517e10 tickt bis die Szene per
Ausdruck endet (`StopWithFitness`) oder `maxNumPhysicsTicks`. Gesamtfitness nach `accumulationMethod`:
**0 = Durchschnitt, 1 = Maximum, 2 = Minimum**. Fitnessfunktion steht also in der Szene; Richtung `HigherIsFitter`, Ziel `targetFitness`.

**Fortpflanzung:**

| Verfahren | Ablauf |
|---|---|
| EgalitarianElitist (00518150) | sortieren; k = round(N · topFraction), mind. 1, überleben; Bester unverändert; jeder der k bekommt gleich viele mutierte Kopien; **keine Kreuzung** |
| SteadyStateGrid (005196c0, 00519ae0) | Inseln (Raster aus √N); Nachkommen per **Ein-Punkt-Kreuzung** (00514be0) + Mutation; Migration zwischen zwei zufälligen Inseln (00519820, vermutet) |

**Adaptive Mutation** (00514290, 005128a0): Rate und Schrittweite linear aus der Elternfitness (`fitnessMin…fitnessMax` →
`mutationRateMin/Max` bzw. `mutationSizeMin/Max`); Anzahl mutierter Gene = round(|Zufall · Rate · Genanzahl| + 0,5), mind. 1.
**Abbruch:** `maxGenerationsPerRun`, `maxGenWithoutImprovement`, Zielfitness; bester Kandidat `lastBestIndividual`,
übernehmbar per `endorphinateWithCurrentIndividual`. Thread `EvolveEngine::runThread` 005032d0.

Einordnung: klassische Neuroevolution mit fester Topologie (passt zu Reil & Husbands, IEEE Trans. Evol. Comp. 2002).

---

## 10a. Darstellung und Oberfläche

Namen: `scripts/names_ui.tsv`. Für die Simulation unerheblich; zum Verständnis des Programms.

**Bibliotheken (Importe):** OpenGL 1.x (`OPENGL32`, 90 Funktionen) und GLU, GDI (Schrift, Bitmaps), MFC 8.0 (Oberfläche),
Video for Windows (`AVIFIL32`, `MSVFW32`: AVI-Export und Video-Ebenen), `WINMM` (Multimedia-Timer), Softimage `XSIFTK` (XSI-Import),
dazu FBX- und CSL-Code statisch gelinkt.

**Takt:** `SimulationTimer::start` 0067b530 startet einen periodischen Multimedia-Timer (`timeSetEvent`, kleinste Timer-Auflösung
per `timeBeginPeriod`). Dessen Rückruf treibt die Ticker (`RuntimeEngine::tickPhysics` bzw. `tickPhysicsAndUpdateRenderer`).

**Darstellung (feste OpenGL-Pipeline):**
```
OglRenderer::renderFrame 00534f30        nur wenn die Szene als geändert markiert ist
├─ wglMakeCurrent, glClear(Farbe | Tiefe), Hintergrund
├─ für jeden Viewport (Einzel- oder Mehrfachansicht, GL_SCISSOR_TEST):
│   Projektion (Perspektive bzw. NMCamera::Orthogonal mit glOrtho), Kamera, Szene im GL_RENDER-Modus
│   Szene = RenderableList / CompoundRenderable, gefiltert nach Darstellungstypen (RenderGlobals);
│   statische Teile als Display-Listen (ManagedDisplayList: glNewList einmal, danach glCallList)
├─ 2D-Überlagerung (glOrtho): Beschriftungen, Anzeigen
└─ SwapBuffers
```
- Grundformen (`Primitives::*`: Kugel, Kapsel, Zylinder, Quader, Ebene, Netz, Gelenk-, Grenz- und Sensor-Symbole) und Hilfsgrafiken
  (`UtilityGraphics::*`: Gitter, Achsen, Pfeile, Locator, Beschriftungen, `PhysicsDebug` für Kontakte und Kontaktkräfte).
- **Auswahl** (`OglRenderer::pick` 005362c0): klassischer OpenGL-Selektionsmodus (`glSelectBuffer`, `glRenderMode(GL_SELECT)`,
  `gluPickMatrix` um das Mausrechteck, Objekte mit `glPushName`), Treffer in `SelectionBuffer`.
- **Bild- und Videoexport:** `glReadPixels` (005366b0) → `VideoExporter` (AVI über `AVIStreamWrite`).

**Oberfläche (MFC):**
- Hauptfenster `MainFrame`, Ansichten `RenderView` (OpenGL-Viewport; Front, Top, Perspektive, Vierfachansicht), `NodeView`
  (Szenenbaum), Zeitleiste „Timeline Editor“ (`MultiTrackView::*`, als Zustandsautomat der Mausinteraktion: Wait, SelectOrMove,
  Move, DragSelect, Pan, Zoom, DragZoom, ResizeLeft/Right), Zustandsautomat-Editor (`StateMachineEditor::FSMEditView`), viele Dialoge.
- **Werkzeuge:** `ToolEngine` verteilt Maus- und Tastatureingaben an das aktive `Tool`: Auswahl, Verschieben (mit Einrasten),
  Drehen, Größe, Form, Posen, Kraft anwenden, Gelenk-Werkzeug, Kamera. Werkzeuge zeichnen eigene Griffe (`ToolGraphics::*`).
  Nach Bearbeitungen schwingt `ToolEngine::settleCharacters` die Charaktere mit 15 Physikschritten ein (Abschnitt 2).
- **Rückgängig/Wiederholen:** Befehlsmuster. `Command` mit Slots ausführen (0x04), rückgängig (0x08), Bezeichnung (0x0c); viele
  konkrete Befehle (`Commands::ChangePropertyCommand`, `CreateTriggeredEventCommand`, `GoToFrameCommand`, `ReshapeCommand` …),
  `MacroCommand` bündelt mehrere; Verlauf im `UndoHistoryDialog`, Beobachter `CommandHistoryChangedListener`.
- **Eigenschaften:** Objekte sind `PropertySource` bzw. `SymbolSource`; dieselbe Reflexion speist die Eigenschaftsfenster,
  die XML-Serialisierung und die Ausdrucks-Symbole (Abschnitt 3). Farbschema über benannte Farben („Viewport: Background“,
  „Timeline Editor: Keyframes“ …), Tastenkürzel über `HotkeyDialog`.

## 10b. Import und Export

Namen: `scripts/names_io.tsv`.

**Formate** (je eine Klasse, Basis `NMFileIO`):

| Format | Klasse | Umsetzung |
|---|---|---|
| BVH | `BVHIO` | eigener Parser/Writer (HIERARCHY/MOTION; Wurzel 6 Kanäle `Xposition Yposition Zposition Xrotation Yrotation Zrotation`, sonst 3 Rotationskanäle) |
| Acclaim ASF/AMC | `Acclaim2MotionIO` (`ASFData`, `AMCData`) | eigener Parser (`:version`, `:units`, `:root`, `:bonedata`, `:hierarchy`) |
| Character Studio | `CSMMotionIO` | eigener Parser |
| FBX | `FBXIO` | Autodesk FBX SDK, statisch gelinkt („Importing/Exporting FBX file…“) |
| Softimage XSI | `XSIIO` | `XSIFTK-3.6.2.1.dll` (Crosswalk) |
| Vicon V/VSK/VST | `VIO`, `VSKIO`, `VSTParser` | eigene Parser |
| Bilder/Video | `ImageExporter`, `VideoExporter` (AVI), `VideoImporter` | über `glReadPixels` bzw. Video for Windows |
| endorphin-eigene Daten | XML (`AdvXMLParser`) | Charaktere, Szenen, Posen; Behaviours/Charaktere der Installation verschlüsselt |

**Animationsdaten:** Kanäle `NMData::RealChannel` (Skalar) und `NMData::QuatChannel` (Rotation), Interpolation
`const`, `linear` oder `cubic` (`m_interpolationType`).

**Import einer Animation** (`importAnimation` 004fcbd0): Datei mit dem passenden Format laden, daraus einen temporären
**Referenzcharakter** bauen und die Bewegung per **dynamischem Motion Transfer** auf den gewählten Simulationscharakter übertragen
(„Transferring animation…“). Alternativ als Triggered Event (Animation oder Pose) anlegen (`importAsEvent` 004fc280).

**Dynamischer Motion Transfer** (`MotionTransfer::dynamicTransfer` 00557b20) ist eine **Physik-Simulation**, kein reines
Umrechnen von Gelenkwinkeln:
```
temporäres RigEvent (RigRoot, Option "autoretarget") und temporäres DesiredPoseEvent anlegen
für jeden Frame:
    DesiredPoseEvent.tick      Gelenkantriebe halten die Pose
    CharacterData.tick         Effektoren aktualisieren
    RigEvent.tick              RPRO-Magnete ziehen die Ziel-Bodies zu den Quell-Bodies, Zusatz-Physikschritte
    Ergebnis aufzeichnen
Events aufräumen
```
Unterschiedliche Proportionen und Gelenkhierarchien werden so über die Physik ausgeglichen (Rig-Knoten verbinden nur ausgewählte
Masseobjekte; dazwischen entscheidet die Simulation). Beim Export läuft der Weg umgekehrt (Simulations- auf Referenzcharakter),
dann schreibt die Formatklasse die Datei.

## 10c. Forschungshintergrund und Abgleich mit dem Code

**Quellen:**
- T. Reil, P. Husbands: *Evolution of Central Pattern Generators for Bipedal Walking in a Real-Time Physics Environment*,
  IEEE Transactions on Evolutionary Computation 6(2), 2002, S. 159–168, DOI 10.1109/4235.996015 (Universität Sussex/Oxford).
  Volltext gelesen (PDF von users.sussex.ac.uk; Fließtext in Times, Formeln als Inline-Bildmasken je Zeichen, Tabellen als
  CCITT-Bilder; mit eigenen Skripten gerendert und abgelesen). Kerndaten:
  - Physik: MathEngine SDK 1.0.5 („Fast Dynamics Toolkit“), 13 Bodies, 11 Gelenke; Hüfte 2 Freiheitsgrade (Nicken/Rollen),
    Knie 1, zusammen 6; keine Füße, stattdessen Kugel-Ebene-Kontakt (Radius 8 cm).
  - Körper: Becken 0,5 m / 1 kg, Oberschenkel 0,5 m / 0,6 kg, Unterschenkel 0,5 m / 0,6 kg.
  - Gelenkgrenzen: Knie −1,4…0,0 rad, Hüfte seitlich −0,8…0,8 rad, Hüfte vor/zurück −1,3…1,6 rad.
  - Antrieb (1): `T = k_s(θ_d − θ) − k_d·θ̇` mit **k_s = 5, k_d = 4** für alle Gelenke (PD-Regler, kein FLVC).
  - Netz (2): `τ_j·dA_j/dt = −A_j + Σ_i w_ij·O_i`; (3): `O_j = 1/(1 + e^(t_j − A_j))`, t_j = Bias;
    10 voll vernetzte Neuronen, Zeitschritt 0,02 s; Neuronen 1–6 sind Motorneuronen, Ausgang 0…1 auf die Gelenkgrenzen skaliert.
  - Genom: n·n Gewichte, dann n Zeitkonstanten, dann n Biases (bei n = 10: 120 Gene); Bereiche [−16, 16], [0,5; 5], [−4, 4].
  - Initialisierung/Mutation: Gauß, σ = 8 / 1,5 / 3 (Gewicht / Zeitkonstante / Bias), Mittelwert bei Initialisierung 0 / 2,75 / 0,
    bei Mutation 0; Werte außerhalb des Bereichs werden abgeschnitten; Rate so, dass im Mittel eine Änderung je Chromosom.
  - Population **50**, rangbasiert mit Anteil 0,5 (untere Hälfte durch Kopie der oberen ersetzt), keine Kreuzung.
  - Fitness (4): `W = √((x_t − x_0)² + (z_t − z_0)²)` (Abstand vom Start in der Ebene bei Abbruch); Abbruch, sobald der
    Schwerpunkt unter eine Höhe fällt, sonst nach 50 s; Start mit Aktivierungen 0 in aufrechter Haltung.
  - Ergebnis: 100 Läufe à 120 Generationen, 10 % ergaben stabile Geher (im Mittel 20,6 m); keine Propriozeption nötig.
- Patentanmeldung **US 2004/0148268 A1** „Artificial multiped and motion controller therefor“, Erfinder Torsten Reil, Priorität
  GB 28.02.2001, später übertragen an ISIS Innovation (Oxford) und 2008 an NaturalMotion; aufgegeben, fortgeführt als US 7 685 081 B2.
  Gelesen über Google Patents (wörtlich: die ersten ~40 000 Zeichen; Formeln sind dort Bilder).

**Abgleich** (✓ = deckt sich mit dem Code, ≠ = Unterschied):

| Thema | Patent / Paper | endorphin.exe |
|---|---|---|
| Physik | „Dynamics Toolkit 0.0.5“ von **MathEngine plc, Oxford** | ✓ MathEngine **Karma** (Nachfolger), statisch gelinkt |
| Antrieb | bevorzugt **kraftbegrenzte Geschwindigkeits-Constraint** (FLVC): Sollgeschwindigkeit aus der Differenz Ist-/Sollwinkel, Drehmoment vom Solver, begrenzt auf τ_max; „zuverlässiger als PD/PID“ | ✓ genau der **Hinge-Effektor**: `ω = clamp(−Gradient·(θ−θ_soll), ±GradientCap)`, Kraftgrenze `BaseForceCap`, über `MdtLimitSetLimitedForceMotor` |
| Alternative | PD-Regler (I-Anteil ≈ 0); **im Paper von 2002 der einzige Antrieb**: `T = 5(θ_d − θ) − 4θ̇` | Kugelgelenk-Antrieb als weiche implizite Feder-Dämpfer-Zeile (Abschnitt 6) |
| Sollwerte | Motorneuronen liefern **Sollwinkel** (Ausgang 0…1, auf Gelenkwinkel skaliert) | ✓ Effektor-`DesiredValue`, normiert auf den Gelenkbereich; Netzausgänge in (0, 1) |
| Netz | rekurrentes Netz als Mustergenerator (CPG): zwei gekoppelte Ketten aus je zwei Oszillatoren, je 6 Knoten; Knoten mit Zeitkonstante τ, Bias t, Gewichten w; `τ·dA/dt = −A + Σ w·O`, `O = 1/(1+e^(t−A))` (im Paper bestätigt; dort ein einzelnes Netz aus 10 voll vernetzten Neuronen, Schritt 0,02 s) | ≠ `ThreeLayerNet` ist **vorwärtsgerichtet** (Eingang → Hidden → Ausgang), ohne Rückkopplung und ohne Zeitkonstanten; Sigmoid mit Bias ✓; „Slope“ gespeichert, aber ungenutzt |
| Sensorik | Rückkopplung vorgesehen, im Beispiel nicht genutzt | Eingänge des Netzes sind beliebige Ausdrucks-Variablen, z. B. Sensoren (Abschnitt 10) |
| Genom | reellwertig; Bereiche Gewichte [−16, 16], Zeitkonstanten [0,5, 5], Bias [−4, 4] | ✓ reellwertige Gene mit Unter-/Obergrenze je Gen (`Chromosome::Descriptor`) |
| Selektion | rangbasiert, **beste Hälfte überlebt**, schlechtere Hälfte durch Kopien der besten ersetzt (Population: Patent 100, Paper 50) | ✓ `EgalitarianElitist`: Beste `topFraction` überleben, gleich viele Kopien je Elite |
| Mutation | im Mittel **eine Änderung pro Individuum**; Paper: gaußverteilt, σ = 8 / 1,5 / 3 je Gentyp, am Bereich abgeschnitten | ✓ `round(|Zufall·Rate·Genanzahl| + 0,5)` Gene, mindestens eins |
| Kreuzung | **keine** | ✓ keine bei `EgalitarianElitist` (nur `SteadyStateGrid` kreuzt) |
| Fitness | Patent: Strecke des hintersten Fußes; Paper: `√((x_t−x_0)² + (z_t−z_0)²)`; beide: Abbruch, wenn der Schwerpunkt unter eine Schwelle fällt | ✓ Fitness aus Simulationsläufen, Abbruch per Ausdruck (`StopWithFitness`) bzw. Tick-Grenze |
| Hilfsstabilisator | Stütze am Rumpf, wird über die Optimierung schrittweise abgebaut (nur im Patent, nicht im Paper) | in endorphin.exe **keine eigene Funktion** (keine passenden Strings oder Klassen); nachbildbar mit `RPROMagnet`/`PoseMagnet` (RPRO-Constraint mit Kraftgrenze, Abschnitt 8.4), deren Kraftgrenze man über die Generationen senkt. Ob ein verschlüsseltes Behaviour so etwas tut, ist nicht prüfbar |
| Sollkräfte am Gelenk | Drehmoment „gleich und entgegengesetzt“ auf beide Körper | ✓ Karma-Constraints wirken auf beide Bodies |

**Fazit:** Der Laufzeitkern von endorphin setzt die Forschung aus Oxford direkt um: MathEngine-Physik, kraftbegrenzte
Geschwindigkeitsmotoren, Gelenk-Sollwinkel aus einem neuronalen Netz, Optimierung mit „beste Hälfte kopieren und mutieren“
ohne Kreuzung. Erweitert wurde das um Behaviours als Zustandsautomaten mit Ausdrücken, Sensoren, Kugelgelenk-Antriebe,
Events und den physikalischen Motion Transfer. Das Netz in endorphin ist gegenüber Paper und Patent vereinfacht (vorwärtsgerichtet statt rekurrent).
Entwicklung: Das Paper (2002, eingereicht 2000) nutzt noch PD-Regler; das Patent (Priorität 2001) bevorzugt schon den
kraftbegrenzten Geschwindigkeitsmotor, den endorphin im Hinge-Effektor verwendet. Die PD-Form lebt im Kugelgelenk-Antrieb weiter.

## 10d. Ressourcendateien (Analyse von außen)

Untersucht wurden nur Ablageort, Dateiname, Größe und Bytestatistik. Die Dateien wurden **nicht** entschlüsselt
(siehe Hinweis oben). Grundlage: alle 271 Dateien der Installation, ausgewertet mit PowerShell.

| Endung | Anzahl | Ablage | Größe | Inhalt (Ablage + Hilfe) |
|---|---|---|---|---|
| `.nmb` | 64 | `Resources\Behaviours` (`M2001`–`M2026`, `M3001`–`M3038`) | 1,9–88 KB | **Behaviours** (Zustandsautomat + Ausdrücke, Abschnitt 3). Die Nummern sind interne IDs; die Anzeigenamen („Fall“, „Brace“ …) stehen in den Dateien selbst bzw. im Programm. Zwei Serien: 20xx (26) = die Version-1-Behaviours, 30xx (38) = neuere Versionen und spätere Behaviours (Zuordnung in 10d.2) |
| `.elc` | 67 | `Resources\Characters\…`, `Resources\Props`, Tutorials, `Application\DefaultGround.elc` | 0,4 KB–889 KB | **Charaktere** jeder Art (Simulation, Reference `_ref`, Prop, Environment/Boden). Klartext-Gegenstück `.nmc` mit dem XML aus Abschnitt 8.1. Groß sind Props mit Dreiecksnetzen (`Lorry_ref` 889 KB, `Motorcycle_ref` 569 KB) |
| `.els` | 78 | `Resources\Scenes\Behaviour Scenes` (34), `Sample Scenes` (23), Tutorials, `Application\DefaultScene.els`, `DefaultEmptyScene.els` | 2,3 KB–3,9 MB | **Szenen**: Charaktere, Umgebung, Triggered Events, Zeitleiste. „Arms crossed on chest.els“ ist eine **Demo-Szene** zu einem Behaviour (Ordner `Behaviour Scenes`), keine Pose. Die größten Szenen enthalten aufgezeichnete Animation (`Multi Martial` 3,9 MB) |
| `.elp` | 57 | `Resources\Poses` (15), `Resources\Behaviours\BehaviourPoses` (31), Tutorials | 1,2–6,7 KB | **Posen** (Gelenkstellungen eines Charakters, z. B. `TPose`, `Crouching`, `WeaponInRightHand`). Die Kurznamen in `BehaviourPoses` (`cf1–6`, `rsk1–17`, `r1–4`, `dp`, `Scuba1–3`) sind Posen, die Behaviours als Sollpose laden (DesiredPose, Abschnitt 5/9); **keine Presets** |
| `.cmf` | 5 | nur `Application` | 64 B–38,5 KB | **Programmkonfiguration**: `NaturalMotion.cmf` (38,5 KB, XML-Schema `xmlSchema`), `Compatibility.cmf` (1,9 KB, Behaviour-Kompatibilitätstabelle), `Translations_8_to_9`, `_9_to_14`, `_14_to_17` (Umbenennungstabellen für alte Dateiversionen). Belege in 10d.1 |

**Gemeinsame Merkmale** (gemessen):
- Alle Dateigrößen sind **Vielfache von 16 Byte**, Entropie 7,995–8,000 Bit/Byte (zufällig wirkend, keine Klartext-Kopfzeile, keine Magic Number).
- **Alle 271 Dateien enden mit demselben 16-Byte-Block** (`71 1B 8D 34 …`), in keiner Datei kommt er an anderer Stelle vor.
  Das passt zu einer Blockverschlüsselung mit 16-Byte-Blöcken, einem einzigen Schlüssel für alle Typen und einem
  Abschluss- bzw. Füllblock gleichen Inhalts (vermutet).
- Erste Blöcke sind meist verschieden; gleiche erste Blöcke treten nur bei Dateien mit gleichem Anfang auf
  (z. B. vier `.elp` und vier `.elc`), was auf eine Verschlüsselung ohne zufälligen Startwert hindeutet (vermutet).
- Folgerung für einen Nachbau: Die Dateien sind nicht nutzbar; eigene Daten im **Klartext-XML** anlegen (Charaktere nach
  Abschnitt 8.1; Szenen, Posen und Behaviours werden laut Abschnitt 10b ebenfalls als XML über `AdvXMLParser` geladen).
  Behaviour-Inhalte lassen sich nur über das Verhalten im Programm (Editor, Zeitleiste, Sensor-/Effektorwerte) beschreiben.

### 10d.1 Ladeweg im Programm (Ghidra)

Namen: `scripts/names_files.tsv` (17 Funktionen). Die Dateilese-Funktion mit der Entschlüsselung (`thunk_FUN_006633b0`) wurde
bewusst **nicht** geöffnet; alles hier liegt davor (Pfadauflösung) oder danach (Verarbeitung des XML-Texts).

**Endungen:** Die Learning Edition benennt nur die Endungen der Vollversion um (`Application\ConvertExtensions.bat`, Klartext):
Szene `.ens` → `.els`, Charakter `.nmc` → `.elc`, Pose `.nma` → `.elp`. Dateifilter im Binary: „endorphin LE Characters“ und
„endorphin LE Environments“ (beide `*.elc`), „LE Poses“ (`*.elp`), „LE Scenes“ (`*.els`), dazu Projekte `*.nmp` und Kameras
`*.nmcam`. **Für `.nmb` und `.cmf` gibt es keinen Dateifilter und keinen String mit der Endung**: Diese Dateien öffnet nur das
Programm selbst, über logische Namen.

**Logische Namen statt Pfade** (`FileProject`): Beim Start lädt `FileProject::FileProject` 00668fc0 die Projektdatei
`bin\SystemPathsInstalled.nmp` (ebenfalls verschlüsselt; im Binary steht als Entwickler-Pfad
`SharedCodeLib\Resources.EncryptedLearning\SystemPathsSourceCodeEncryptedLearning.nmp`). Sie ordnet Kategorien und Schlüsseln
Dateien zu; aufgelöst wird über `FileProject::resolvePath(Kategorie, Schlüssel)` 00669ad0 bzw. mit Unterkategorie 00669b50.
Gefundene Aufrufe:

| Kategorie / Unterkategorie | Schlüssel | Datei (erschlossen aus Namen und Ablage) |
|---|---|---|
| `XML` | `xmlSchema` | `NaturalMotion.cmf` (XML-Schema: erlaubte Elemente/Attribute; Fehlermeldung „The element <%s> is not defined in the XML schema“) |
| `XML` | `symbolTranslations_8_to_9`, `_9_to_14`, `_14_to_17` | `Translations_*.cmf` |
| `CharacterManager` | `behaviourCompatibilityTable` | `Compatibility.cmf` |
| `BehaviourImport` / `StateImport` | Behaviour-Name aus der Szene | `Resources\Behaviours\M….nmb` |
| `DesiredPose` | Attribut `fileName` des DesiredPose-Events | `.elp` (z. B. `BehaviourPoses\rsk7.elp`) |
| `Character`, Ersatz `CharacterImport` | Charaktername; `defaultCharacter` | `.elc` |
| `VersionControl` / `Preferences` / `KeyboardProfile` / `TimeLine` / `Material` / `Renderer` / `Lights` | `versionControl`, `preferences`, `defaultKeyMapping`, `userTimeLine`, `userMaterial`, `userRenderer`, `defaultLights` | Einstellungen (`Application\*.xml` im Klartext, `.nme`, `.nmw`, `.nmm`; die letzten drei verschlüsselt) |

**Versionen und Übersetzungstabellen** (`LoadableFromXMLBase::loadFromText` 0065cce0, gilt für alle XML-Dateien):
```
v = Attribut releaseVersion der Wurzel
v > 18          → InvalidXMLException (Datei zu neu)
v < 9           → Tabelle symbolTranslations_8_to_9 anwenden
v < 14          → Tabelle symbolTranslations_9_to_14 anwenden
v < 17          → Tabelle symbolTranslations_14_to_17 anwenden      (kumulativ: eine Datei mit v = 2 durchläuft alle drei)
wurde etwas ersetzt → übersetzten Text neu parsen
```
Beim Speichern schreibt endorphin immer `releaseVersion="18"` (0x12), optional `buildVersion` (`saveToFile` 0065cb50).
Die aktuelle Formatversion von endorphin 2.7 ist also **18**.

**Aufbau einer Übersetzungstabelle** (`SymbolTranslationTable::load` 00698390, `apply` 00698240): Text, `\r\n` → `\n`, je Zeile
`alt,neu` (Zeilen mit genau zwei Feldern zählen). Angewendet wird **reines Suchen und Ersetzen im XML-Text**, jedes Paar global,
vor dem Parsen. Umbenannte Element-, Attribut- und Symbolnamen alter Dateien werden so auf die aktuellen Namen gebracht.
Für eigene Dateien: gleich `releaseVersion="18"` schreiben, dann greift keine Übersetzung.
(Der Maya-/3ds-Max-Exporter schreibt `releaseVersion="2"`; dessen Dateien laufen durch alle drei Tabellen.)

**Behaviour-Kompatibilität** (`CharacterManager::loadBehaviourCompatibility` 004ae5c0, `Compatibility.cmf`): ebenfalls
Text, Zeilen mit Komma getrennt (CSV). Zeile 1 ist die Kopfzeile. Jede weitere Zeile mit mindestens 3 Feldern: Feld 1 ist ein
Behaviour-Name (es wird geprüft, dass er sich über `BehaviourImport/StateImport` auflösen lässt), Feld 2 ein zweiter Name zum
selben Eintrag. Ab Feld 3 bedeutet ein **`X`**, dass der Eintrag der Spalte aus der Kopfzeile zugeordnet wird.

**Was die Spalten sind** (Abfrage in `BehaviourData::bindCharacter` 004accd0, einziger Leser der Tabelle): Beim Zuweisen eines
Charakters schlägt das Behaviour-Event zweimal in der Tabelle nach:
```
liste        = tabelle[ charakter.typeName ]              → Event +0x2f4 (Anzahl +0x2f8)
liste_legacy = tabelle[ charakter.typeName + "_legacy" ]  → Event +0x2fc (Anzahl +0x300)
```
`typeName` ist ein Attribut der Charakterdatei (gespeichert in `CharacterData::saveXML` 005a2fb0, Feld `+0x458`).
Die **Spaltenköpfe sind also Charakter-Typnamen**, jeweils mit einer zweiten Spalte `<typeName>_legacy`. Jede Liste enthält Paare
aus Feld 1 und Feld 2 der markierten Zeilen. Das passt zur Hilfe („Behaviour versions“): Welche Behaviours ein Charakter
anbietet, hängt von seiner internen Version ab (Charakter auf Basis 1.6 → nur Version-1-Behaviours; nach „Upgrade Character“
die neueren). Die `_legacy`-Liste hält ältere Behaviour-Versionen für alte Szenen bereit („for backwards compatibility“).
Ist im Event schon ein Name gesetzt, wird er in den Listen gesucht und der `index` daraus bestimmt; sonst wird der Eintrag mit dem
gespeicherten Index genommen (Standard: erster Eintrag). Welche `typeName`-Werte es gibt, steht in den (verschlüsselten)
Charakterdateien; im Programm sieht man den Typ im Charakter-Eigenschaftsfenster.

**Behaviours (`.nmb`)** (`BehaviourData::loadXML` 004ad080, `loadStateMachine` 004ac270):
- Ein Behaviour-Event in der Szene speichert nur `TargetSelection`, `behaviourName` (bei Charakterversion ≤ 4 heißt das
  Attribut `name`), `index` und `Parameters` (Werte der Behaviour-Variablen, Typ `Variable`).
- Der Name wird über `BehaviourImport/StateImport` in eine Datei aufgelöst; fehlt sie, gibt es eine `NotFoundException`.
  Hat sich der Pfad geändert, lädt das Event den **Zustandsautomaten** (`State`, Abschnitt 3) aus dieser Datei; während des
  Ladens wird die Simulation angehalten.
- Eine `.nmb` ist also eine **ausgelagerte Zustandsautomat-Datei** („StateImport“). Die Zuordnung Anzeigename → `M2001` …
  steht in `SystemPathsInstalled.nmp` (verschlüsselt); sie wurde stattdessen durch Beobachtung der Dateizugriffe ermittelt (10d.2, vollständig für alle 64 Dateien).

### 10d.2 Zuordnung M-Nummer → Behaviour (Beobachtung, ohne Entschlüsselung)

**Verfahren:** Programm nach `C:\Users\<Nutzer>\RE\endorphin-copy` kopiert (dort dürfen die Zugriffszeiten ohne Adminrechte gesetzt
werden; Windows speichert sie, `DisableLastAccess = 2`). Je Szene: Zugriffszeit aller `.nmb` auf 2020 zurücksetzen,
`endorphin.exe "<Szene>.els"` starten, 30–50 s warten, Programm beenden, nachsehen, welche `.nmb` gelesen wurden und in welcher
Reihenfolge. Gelesen wird nur, was das Programm beim Laden selbst öffnet. Skript: Scratchpad `probe_scenes.ps1`, Rohdaten
`decomp\files\scene_nmb*.csv`, `sample_nmb.csv`. Kontrolle: Start **ohne** Szene liest keine `.nmb`.

**Beobachtungen:**
- `M3033` und `M2009` werden in **jeder** Szene als Erstes gelesen, und ebenso beim Anlegen eines neuen Behaviour-Events über
  „Character → Create Behaviour Event“, auch für einen 1.6-Charakter, dessen Liste diesen Eintrag gar nicht hat. Oberfläche gemessen:
  `M3033` ist „Arms Windmill 2“, `M2009` ist „Hands From Behind Back 1“ und zugleich Teil-Automat von „Arms Windmill 2“ (siehe unten).
  Ein neues Event startet also mit dem Standardnamen **„Arms Windmill 2“** und lädt ihn, bevor es dem Charakter zugewiesen wird.
  Nur wenn der Name in der Liste des Charakters fehlt, gilt Index 0 (beim 1.6-Charakter „Arms Crossed On Chest 1“, `M2026`,
  gleich danach gelesen). Das passt zu `bindCharacter` (10d.1). Die frühere Deutung „Eintrag mit Index 0 der Liste“ war falsch:
  Index 0 ist bei allen Typen „Arms Crossed On Chest …“.
- Danach folgt fast immer zuerst das Haupt-Behaviour der Szene, dann weitere Dateien. Diese weiteren Dateien sind entweder
  zusätzliche Events der Demo-Szene oder **eingebundene Teil-Automaten**: Die Unterkategorie heißt `StateImport`, ein Behaviour kann
  also andere Zustandsautomaten importieren. Das ist jetzt über die Oberfläche bestätigt: Dieselben Gruppen werden auch geladen,
  wenn man nur den Namen umschaltet (z. B. `M2003 M2010 M2015 M2021` bei Jump 1 und Land And Crouch 1/2).
- Posen aus `BehaviourPoses` (`.elp`) werden beim Laden **nicht** gelesen, also erst zur Laufzeit bzw. beim Simulieren.
- In der Learning Edition heißen die Endungen der Szenen anders; die Hilfe zitiert noch `.ens`.

**Messung über die Oberfläche (2026-11-15):** Weil eigene Szenendateien gesperrt sind (unten), wurde jeder Eintrag der
Dropdown-Liste „Name“ im Property Editor eines Behaviour-Events einzeln gewählt. Davor wurden die Zugriffszeiten aller `.nmb` auf
2020 zurückgesetzt, danach 3 s gewartet und die neu gelesenen Dateien in Zugriffsreihenfolge notiert. Die Liste wurde per Mausklick
bedient (Computer Use). Den Text der gewählten Zeile hat ein PowerShell-Skript zur Kontrolle aus der offenen Listbox gelesen
(`LB_GETTEXT`). Gearbeitet wurde nur in der Kopie `RE\endorphin-copy`: Szenen „Jump“ (2 Charaktere) und „Physical Effect Push“
(2 Charaktere), dazu über „Character → Add Character“ die Charaktere „Standard endorphin 1.6/2.0/2.5 Character“ aus
`Resources\Characters\Previous`, je mit einem neuen Behaviour-Event. Nichts wurde gespeichert. Rohdaten:
`decomp\files\ui_names.csv` (Messung je Eintrag, 76 Zeilen) und `decomp\files\ui_lists.csv` (vollständige Listen je Charaktertyp).

**Die Liste hängt vom Charaktertyp ab.** Den Typ zeigt der Tooltip einer Charakterzeile in der Zeitleiste („Version: …“). Das ist
der `typeName` aus 10d.1, also der Spaltenkopf in `Compatibility.cmf` (erschlossen aus dem Code, die Spaltenköpfe selbst sind
verschlüsselt):

| `typeName` (Tooltip „Version“) | Charakter | Einträge in „Name“ |
|---|---|---|
| `SkeletalCharacter` | Standard endorphin 1.6 Character | 26, alle in Version 1 |
| `SkeletalCharacter2.0` | Standard endorphin 2.0 Character | 28 |
| `SkeletalCharacter2.5` | Standard endorphin 2.5 Character | 32 |
| `SkeletalCharacter2.6` | Charaktere der Demo-Szene „Jump“ | 36 |
| `SkeletalCharacter2.7` | Charaktere der Demo-Szene „Physical Effect Push“ | 42 |

Die Dropdown-Liste zeigt nur **eine** Liste je Charakter. Ob die `_legacy`-Liste dort mit einfließt, lässt sich an der Oberfläche
nicht erkennen. Neue Versionen ersetzen alte: Ab 2.0 heißt es „Arms Crossed On Chest 2“ statt „… 1“, unveränderte Behaviours
behalten die „1“. „Hold 1“ heißt ab 2.7 „Physical Effect: Hold 1“ (dieselbe Datei). „Physical Effect: Damping 1“ gibt es bis 2.6,
ab 2.7 nur noch „Damping 2“.

**Ergebnis: Listenname → M-Datei** (alle 64 Dateien; Beleg jeweils „Oberfläche gemessen“, Typ = erster Charaktertyp, in dessen
Liste der Eintrag vorkommt). „Haupt-Datei“ ist die Datei, die nur dieser Eintrag lädt. „Mitgeladen“ sind Teil-Automaten
(`StateImport`), die selbst eigene Listeneinträge sind.

| Listenname | Datei | ab Typ | mitgeladen (Teil-Automaten) |
|---|---|---|---|
| Tackle 1 | M2001 | 1.6 | M2015, M2003 |
| Stagger 1 | M2002 | 1.6 | M2015 |
| Hands Reach And Look At 1 | M2003 | 1.6 | |
| Legs Straighten 1 | M2004 | 1.6 | |
| Legs Kick 1 | M2005 | 1.6 | |
| Land And Crouch 1 | M2006 | 1.6 | M2003, M2009, M2010, M2015, M2021 |
| Jump And Dive 1 | M2007 | 1.6 | M2005, M2009, M2014, M2015, M2021 |
| Jump 1 | M2008 | 1.6 | M2003, M2009, M2010, M2015, M2021 |
| Hands From Behind Back 1 | M2009 | 1.6 | |
| Legs Reach 1 | M2010 | 1.6 | |
| Fall Back, Twist With Passive Arms 1 | M2011 | 1.6 | |
| Fall Back, Twist And Catch Fall 1 | M2012 | 1.6 | M2015 |
| Fall Back, Twist And Cover Face 1 | M2013 | 1.6 | M2023, M2009 |
| Writhe In Mid-Air 1 | M2014 | 1.6 | M2021, M2005, M2009 |
| Catch Fall 1 | M2015 | 1.6 | |
| Writhe 1 | M2016 | 1.6 | |
| Whole Body Stiffness 1 | M2017 | 1.6 | |
| Body Stiffness 1 | M2018 | 1.6 | |
| Body Foetal 1 | M2019 | 1.6 | |
| Arms Zombie 1 | M2020 | 1.6 | M2009 |
| Arms Windmill 1 | M2021 | 1.6 | M2009 |
| Arms Wide Of Head 1 | M2022 | 1.6 | M2009 |
| Hands Covering Face 1 | M2023 | 1.6 | M2009 |
| Hands Protecting Groin 1 | M2024 | 1.6 | M2009 |
| Arms Raised Above Head 1 | M2025 | 1.6 | M2009 |
| Arms Crossed On Chest 1 | M2026 | 1.6 | M2009 |
| Writhe 3 | M3001 | 2.5 | |
| Physical Effect: Fluid 1 | M3002 | 2.7 | |
| Physical Effect: Twist 1 | M3003 | 2.7 | |
| Tackle 5 | M3004 | 2.6 | M3025, M3016 |
| Tackle 4 | M3005 | 2.6 | M3025, M3016 |
| Tackle 3 | M3006 | 2.5 | M3025, M3016 |
| Tackle 2 | M3007 | 2.0 | M2015, M3016 |
| Stagger 3 | M3008 | 2.6 | M3025, M3016 |
| Stagger 2 | M3009 | 2.0 | M3025, M3016 |
| Physical Effect: Push 1 | M3010 | 2.7 | |
| Physical Effect: Fluid Thick 1 | M3011 | 2.7 | |
| Writhe In Mid-Air 3 | M3012 | 2.5 | M3001 |
| Physical Effect: Damping 1 | M3013 | 2.0 (bis 2.6) | |
| Hands Reach And Look At 4 | M3014 | 2.6 | |
| Hands Reach And Look At 3 | M3015 | 2.5 | |
| Hands Reach And Look At 2 | M3016 | 2.0 | |
| Legs Kick 2 | M3017 | 2.0 | |
| Land And Crouch 2 | M3018 | 2.6 | M2003, M2009, M2010, M2015, M2021 |
| Hold 1 = Physical Effect: Hold 1 (ab 2.7) | M3019 | 2.6 | |
| Physical Effect: Hold 2 | M3020 | 2.7 | |
| Legs Reach 3 | M3021 | 2.6 | |
| Legs Reach 2 | M3022 | 2.0 | |
| Writhe In Mid-Air 2 | M3023 | 2.0 | M3033, M2009, M3017 |
| Catch Fall 3 | M3024 | 2.6 | M3016 |
| Catch Fall 2 | M3025 | 2.0 | M3016 |
| Writhe 2 | M3026 | 2.0 | |
| Physical Effect: Damping 2 | M3027 | 2.7 | |
| Balance With Props 1 | M3028 | 2.6 | M3009, M3025, M3016 |
| Balance 3 | M3029 | 2.6 | M3009, M3025, M3016 |
| Balance 2 | M3030 | 2.6 | M3009, M3025, M3016 |
| Balance 1 | M3031 | 2.0 | M3009, M3025, M3016 |
| Arms Zombie 2 | M3032 | 2.0 | M2009 |
| Arms Windmill 2 | M3033 | 2.0 | M2009 |
| Arms Wide Of Head 2 | M3034 | 2.0 | M2009 |
| Hands Covering Face 2 | M3035 | 2.0 | M2009 |
| Arms Raised Above Head 2 | M3036 | 2.0 | M2009 |
| Arms Crossed On Chest 2 | M3037 | 2.0 | M2009 |
| Physical Effect: Fluid Thin 1 | M3038 | 2.7 | |

Die Zuordnung ist eindeutig: Jede Haupt-Datei wurde nur von genau einem Listennamen als einzige neue Datei gelesen. Teil-Automaten
wurden über ihren eigenen Eintrag bestimmt; zum Beispiel lädt „Hands From Behind Back 1“ nur `M2009`. Die Zugriffsreihenfolge
nennt die Haupt-Datei **nicht** immer zuerst (bei „Jump 1“ einmal `M2008` vorn, einmal an fünfter Stelle). Die Reihenfolge taugt
also nicht als Beleg, nur die Menge.

**Folgerungen:**
- **Serien:** `M2001`–`M2026` sind genau die 26 Version-1-Behaviours der 1.6-Liste. `M3001`–`M3038` sind neuere Versionen (2 bis 5)
  sowie Behaviours, die erst ab 2.0 dazukamen (Balance, Physical Effects). Unveränderte Behaviours (Body Foetal 1, Legs Straighten 1,
  Fall Back …) liegen auch in den neuen Listen weiter in der 20xx-Serie.
- **Teil-Automaten:** Sie sind ganz normale Behaviours. Fast alle Arms- und Hands-Behaviours importieren „Hands From Behind Back 1“
  (`M2009`). Catch Fall und Tackle importieren „Hands Reach And Look At 1/2“. Balance importiert „Stagger 2“ (`M3009`), und das
  importiert „Catch Fall 2“ (`M3025`). „Writhe In Mid-Air“ besteht aus „Arms Windmill“ + „Legs Kick“. „Jump 1“ und „Land And Crouch
  1/2“ nutzen „Legs Reach 1“, „Arms Windmill 1“, „Catch Fall 1“ und „Hands Reach And Look At 1“. Auch Version-2-Behaviours
  importieren Version-1-Dateien („Land And Crouch 2“, „Tackle 2“).
- **Korrekturen an der früheren Szenen-Tabelle:** Jump = `M2008` (`M2003` ist „Hands Reach And Look At 1“). Arms Windmill =
  `M3033`, Hands From Behind Back = `M2009`. `M3026` ist nicht Stagger, sondern „Writhe 2“. Die Demo-Versionen sind Writhe 3,
  Writhe In Mid-Air 3, Catch Fall 3, Stagger 3, Tackle 4, Balance 3, Hold 1.
- **Nur in Beispielszenen gelesene Dateien**, jetzt mit Namen: `M2002` Stagger 1 (Arrows, Balcony), `M2016` Writhe 1 (Heli Leap,
  Catch Rope; dort lädt `M2003` allein, also „Hands Reach And Look At 1“), `M3015` Hands Reach And Look At 3 (Monster), `M3031`
  Balance 1 (Rotating Floor, Trapeze), `M3023` Writhe In Mid-Air 2 (stets mit `M3017` Legs Kick 2, seinem Teil-Automaten).
- **Früher nie gelesen**, jetzt zugeordnet: `M2001` Tackle 1, `M2006` Land And Crouch 1, `M2020` Arms Zombie 1, `M2022` Arms Wide
  Of Head 1, `M2025` Arms Raised Above Head 1, `M2026` Arms Crossed On Chest 1, `M3006` Tackle 3, `M3007` Tackle 2, `M3020`
  Physical Effect: Hold 2, `M3022` Legs Reach 2, `M3030` Balance 2. Es sind also ältere Versionen bzw. Behaviours ohne Demo-Szene.
- **Zweite Quelle, Tutorials:** Die 15 Tutorial-Szenen wurden ebenfalls gemessen (`tutorial_nmb.csv`). Die Hilfeseiten der Tutorials
  nennen die Behaviours mit dem Listennamen samt Version („Jump And Dive 1“, „Physical Effect: Push 1“, „Tackle 5“, „Stagger 3“ …).
  Das stimmt mit der Oberfläche überein.
- **Eigene Testszenen sind gesperrt:** Eine selbst geschriebene Klartext-Szene (`releaseVersion="18"`, ein `BehaviourEvent`) lehnt die
  Learning Edition ab: „Load failure: This file cannot be opened. It may have been created in the endorphin Professional or Educational
  Editions.“ Diese Sperre wird respektiert, deshalb die Messung über die Oberfläche.

**Szenen (`.els`)** (`SceneData::loadXML` 0047dd50), Ladereihenfolge laut Statusmeldungen: Szene leeren → Umgebung
(`Background`) → Charaktere (`Character`: eingebettet oder als Verweis auf eine Charakterdatei, `SceneData::loadCharacter` 0047a040)
→ Posen (`Poses`) → Triggered Events (`Events`) → Animationsoptionen → Welt (`World`) → Kanäle (Animationsdaten) → Vorverarbeitung.
Die Evolution baut je Individuum eine eigene kleine Szene im Speicher (`<Scene releaseVersion=…>` mit `World`, `State`,
`Individual`; 0059b800).

### 10d.3 Inhalt der Behaviours (Version 2.7, ohne Entschlüsselung)

**Quellen:** (1) Das mitgelieferte Benutzerhandbuch `Resources\Help\UserGuide.pdf`, Kapitel 18 „Behaviour Library“ (S. 259–345).
Es nennt je Behaviour die Art, das Ziel der Bewegung und alle Variablen mit Bereich und Standardwert, oft auch die Wirkung der
Extremwerte. Text per `pypdf` ausgelesen und in eigenen Worten zusammengefasst; Rohdaten `decomp\files\behaviour_vars_handbuch.csv`
(306 Variablen). (2) Gegenprobe in der Oberfläche (2026-11-15): Für alle 42 Einträge der 2.7-Liste wurde im Property Editor
abgelesen, welche Variablen mit welchem Standardwert erscheinen (beim Umschalten des Namens setzt endorphin die Standardwerte).
Rohdaten `decomp\files\behaviour_vars_ui27.txt`. Das Handbuch beschreibt meist die neueste Version eines Behaviours. Wo die
Oberfläche abweicht, gilt für 2.7 der Wert aus der Oberfläche.

**Nicht beobachtbar** bleiben Zustände, Übergänge und Formeln im Inneren der Zustandsautomaten (Abschnitt 3). Sie stehen nur in
den verschlüsselten `.nmb`. Beschreibbar ist das Verhalten von außen: Ziel, Auslösebedingung, Parameter und deren Wirkung.

**Drei Arten** (Handbuch):
- **aktiv:** treibt die Gelenke sofort ab Event-Beginn auf ein Bewegungsziel (Pose oder Oszillation).
- **bedingt:** startet erst, wenn eine Bedingung erfüllt ist, z. B. Füße mit festem Bodenkontakt (Balance, Jump), Fallen Richtung
  Boden (Catch Fall), Fallen rückwärts (Fall Back …), in der Luft (Writhe In Mid-Air, Land And Crouch), unausgeglichen (Stagger),
  Zielcharakter in Armlänge und schnelle Annäherung (Tackle), Arme hinter dem Rücken (Hands From Behind Back).
- **physikalischer Effekt:** keine Gelenkantriebe, sondern Kräfte bzw. Constraints auf frei wählbare Massenobjekte beliebiger
  Charaktere (auch Props, Referenz, Umgebung). Die allgemeine Property `Target Objects` ist dabei ungenutzt; jeder Effekt hat eine
  eigene Menge (`TargetMassObjects`, `MassObjects`, `FluidObjects`, `ConstraintFrom/ToObject`). Das Event muss trotzdem auf der
  Zeitleiste eines Simulationscharakters liegen.

**Gemeinsame Parameter und ihre Bedeutung** (Handbuch, für einen Nachbau direkt nutzbar):

| Parameter | Bereich | Bedeutung |
|---|---|---|
| Speed (auch Arms/Leg Speed) | 0–1 | Zeit bis zum Bewegungsziel: 0 ≈ 500 Frames, 1 ≈ 20 Frames (Zwischenwerte nicht angegeben) |
| Strength | 0–1 | Antriebskraft: bei 0 wird das Ziel nur ohne Hindernis und ohne Schwerkraft erreicht, bei 1 gegen die meisten Hindernisse |
| Leg and Torso Stiffness (u. ä.) | 0–1 | wie stark die übrigen Körperteile ihre Ausgangspose halten: 0 schlaff, 1 starr |
| Noise Level / Random Number Seed | 0–1 / 1–10000 | Zufallsanteil der Bewegung; Seed legt die Folge fest (Standard: zufällig gewählt) |
| Amplitude (Shoulder/Elbow/Hip/Knee/Leg) | 0–1 | Ausschlag einer Oszillation: 0 keine Bewegung, 1 voller Ausschlag |
| Velocity | −1…+1 | Frequenz einer Oszillation: Betrag 0 ≈ 500 Frames je Zyklus, 1 ≈ 20 Frames; Vorzeichen = Drehrichtung (erschlossen) |
| Phase Offset | −1…+1 | Startphase: ±1 = einen Zyklus nach bzw. vor dem „idealen“ Start |
| Synchronisation Offset | 0–1 | Phasenversatz links/rechts: 0 gleichphasig, 1 gegenphasig |
| Delay / Blend | 0–1000 s | Wartezeit bis zum Start / Zeit, bis die Gelenkkraft ihr Maximum erreicht |
| Slowdown | 0–1 | Abbremsen vor dem Ziel |
| Target (LookAt/Hand/Leg) | Menge Massenobjekte | Ziel ist der Schwerpunkt der gewählten Objekte; leer = kein Ziel |
| Use Prediction, Prediction Rate, Target/Character Acceleration Threshold, Trajectory Offset, Interception Offset X/Y/Z | | Vorhersage der Zielbewegung zum Abfangen bewegter Ziele |
| Predict Ground Height / Ground Height / Ground Size | an/aus, −100…100, – | Bodenhöhe vorhersagen oder fest vorgeben; Ground Size nur in der Oberfläche (2.7), Bedeutung nicht dokumentiert |

**Die 42 Behaviours von 2.7** (Standardwerte aus der Oberfläche; „HB“ = Abweichung vom Handbuch; Datei und Teil-Automaten in 10d.2):

| Listenname | Art | Bewegungsziel | Variablen (Standard 2.7) |
|---|---|---|---|
| Arms Crossed On Chest 2 | aktiv | Arme vor der Brust kreuzen (Schutz) | Strength 0,6; Speed 0,6; Leg and Torso Stiffness 0,5; Noise 0; Seed |
| Arms Raised Above Head 2 | aktiv | Arme nach oben strecken | Speed 0,8; Strength 0,5; LTS 0,5; Noise 0,17; Seed |
| Arms Wide Of Head 2 | aktiv | Arme seitlich strecken | Speed 0,7; Strength 0,5; LTS 0,5; Noise 0; Seed |
| Arms Windmill 2 | aktiv | Arme seitlich, dann kreisen (Balancieren) | Shoulder Amp 0,6; Elbow Amp 0,8; Velocity 0,8; Sync Offset 0,8; Phase 0; Strength 0,5; YX Plane Offset 0,1 (Mitte vor/hinter dem Körper); ZX Plane Offset 0 (Höhe Hüfte…über Kopf); LTS 0,5 |
| Arms Zombie 2 | aktiv | Arme nach vorn strecken | Speed 0,6; Strength 0,6; LTS 0,5; Noise 0; Seed |
| Balance 2 / Balance 3 | bedingt (Bodenkontakt, aufrecht) | Gleichgewicht halten | COM Offset X 0; COM Offset Z 0; Pitch Offset 0; Pelvis Height 1,0; Use Arms an; Balance Threshold 0,15 (beide Versionen gleich) |
| Balance With Props 1 | bedingt | wie Balance, mit getragenen Props | wie Balance + Props (Menge) |
| Body Foetal 1 | aktiv | zusammenrollen | Torso Stiffness 0,5; Arms Speed 0,5; Arms Strength 0,5; Leg Speed 0,7; Leg Strength 0,5 |
| Body Stiffness 1 | aktiv | Körperteile versteifen/lockern | Head and Neck, Torso, Left/Right Arm, Left/Right Leg je 0,6 |
| Catch Fall 3 | bedingt (fällt Richtung Boden) | Arme vor, um Kopf und Oberkörper abzufangen | Arms Apart 0,5; Arms Speed 0,7; Arms Strength 0,7; Predict Ground Height an; Ground Height 0; **Ground Size 0,8** (nicht im HB) |
| Fall Back, Twist And Catch Fall 1 | bedingt (fällt rückwärts) | umdrehen, dann mit Armen abfangen | Twist Urgency 0,3; Arms Apart 0,6; Arms Speed 0,8; Arms Strength 0,8 |
| Fall Back, Twist And Cover Face 1 | bedingt (fällt rückwärts) | umdrehen, Arme schützen den Kopf | Twist Urgency 0,5; Arms Speed 0,5; Arms Strength 0,5; LTS 0,5 |
| Fall Back, Twist With Passive Arms 1 | bedingt (fällt rückwärts) | nur umdrehen | Twist Urgency 0,5; Arms Stiffness 0,1; LTS 0,5 |
| Hands Covering Face 2 | aktiv | Hände vors Gesicht | Speed 0,7 (HB 0,6); Strength 0,5 (HB 0,6); LTS 0,5; Noise 0,2 (HB 0); Seed |
| Hands From Behind Back 1 | bedingt (Arme hinter dem Rücken) | Arme an die Seiten (neutrale Ausgangslage) | Hip Width Scale 0,4; Speed 0,5; Strength 0,4; LTS 0,5 |
| Hands Protecting Groin 1 | aktiv | Hände vor den Schritt | Speed 0,65; Strength 0,5; LTS 0,8 |
| Hands Reach And Look At 4 | aktiv (Ziele nötig) | je Hand zum Ziel greifen, Kopf zum Ziel drehen | LTS 0,8; LookAt: Delay 0, Blend 0, Speed 0,5, **Use Up Vector an, Up Vector 0/1/0** (nicht im HB); je Hand: Speed 0,8, Slowdown 0,25, Delay 0, Blend 0, Strength 0,8, Palm Facing an, Orientation 0,5, Exaggeration 0, Max Arm Extension 0,9 (HB 1,0), Fingers Angle 0,2, Use Prediction aus (HB an), Prediction Rate 0,01 (HB 0,2), Target Accel Thr 0,01 (HB 0,05), Character Accel Thr 10 (HB 20), Trajectory/Interception Offsets 0 |
| Jump 1 | bedingt (Bodenkontakt) | nach vorn springen, auf den Füßen landen | Strength 0,4; Jump Timing 0,3 |
| Jump And Dive 1 | bedingt (Bodenkontakt + Vorwärtsbewegung) | springen, waagerecht landen | Strength 0,5; Jump Timing 0,75 |
| Land And Crouch 2 | bedingt (in der Luft, aufrecht) | Arme seitlich, vor dem Aufprall abstützen, danach mit Armen abfangen | Leg Strength 0,5; Arms Urgency 0,8; Ground Height 0; **Ground Size 0,8** |
| Legs Kick 2 | aktiv | Beine treten (Oszillation) | Hip Amp 0,6; Knee Amp 0,6; Velocity 0,6; Phase 0; Strength 0,5; Arms Stiffness 0,2; Torso Stiffness 0,5; Noise 0,5; Seed |
| Legs Reach 3 | aktiv (Ziele nötig) | je Bein zum Ziel | Arms and Torso Stiffness 0,8; je Bein: Speed 0,8, Slowdown 0,25, Strength 0,8, Blend 0, Delay 0, Max Leg Extension 1,0, Use Prediction aus (HB an), Prediction Rate 0,5, Character Accel Thr 20, Target Accel Thr 0,1, Offsets 0 |
| Legs Straighten 1 | aktiv | Beine strecken | Straighten Left/Right Leg an; Left/Right Leg Speed 0,8; Left/Right Leg Strength 0,6; Arms and Torso Stiffness 0,3 |
| Stagger 3 | bedingt (unausgeglichen) | Schutzschritte, Arme zum Balancieren | Apply Arms Windmill an; Stagger Squarely aus; Predict Ground Height an; Ground Height 1,00 (abgelesen; HB 0); **Ground Size 0,8**; Maximum Steps 5 (1–20); Arms Speed 0,8; Arms Strength 0,6 |
| Tackle 2 | bedingt (Ziel in Armlänge, schnelle Annäherung) | Arme um den Zielcharakter legen | Arms Strength 0,6; Arms Speed 0,6; LTS 0,5; Predict Ground Height aus; Ground Height 0; Target Up Spine; Target Pelvis |
| Tackle 3 | bedingt | wie Tackle 2 | Arms Strength 0,8; Arms Speed 0,8; LTS 0,5; Predict GH aus; GH 0; Reach Forward Duration 0,05; Widen Arms Duration 0; Target Up Spine; Target Pelvis |
| Tackle 4 / Tackle 5 | bedingt | wie Tackle 3 | wie Tackle 3 + Ground Size 0,8 (HB: Arms Speed 0,5, Predict an) |
| Whole Body Stiffness 1 | aktiv | ganzen Körper versteifen/lockern | Stiffness 0,6 |
| Writhe 2 | aktiv | Arme und Beine zappeln | Amplitude 0,6; Velocity −0,6; Phase 0; Strength 0,6; Noise 0; Seed |
| Writhe 3 | aktiv | wie Writhe 2 | wie Writhe 2, aber Phase 0,2 + Open Legs aus |
| Writhe In Mid-Air 2 | bedingt (in der Luft) | zappeln in der Luft | Shoulder Amp 0,7; Elbow Amp 0,6; Leg Amp 0,5; Phase 0; Velocity 0,6; Strength 0,5; Head Turn 0,6; Back Arch 1,0 |
| Writhe In Mid-Air 3 | bedingt (in der Luft) | wie Version 2 | Amplitude 0,6; Speed 0,6 (HB −0,6); Phase 0 (HB 0,2); Strength 0,5 (HB 0,6); Noise 0; Seed; Open Legs aus; Head Turn 0,6; Back Arch 1,0 |
| Physical Effect: Damping 2 | Effekt | Geschwindigkeit dämpfen | Linear Velocity Damping 10 (0–100); Angular Velocity Damping 10 (0–100); MassObjects (gedämpft wird jeweils das Elterngelenk) |
| Physical Effect: Fluid 1 (Wasser) | Effekt | Auftrieb + Widerstand im Fluidwürfel | Fluid Density 1000; Drag 200; Turbulent Drag 200; Angular Drag 50; Stream aus, Stream Velocity 0/0/0; CubeMotionOnStream aus; Waving aus, Waving 0/0/0; Wave Frequency 0,5; Wave Amplitude 0,2; Wave Initial Offset 0,5; Cube Size 4; Cube Position 0/0/0; Transparency 0,4 (HB 0,3); FluidObjects |
| Physical Effect: Fluid Thin 1 (Luft) | Effekt | wie Fluid 1 | Density 1,2; Drag 12; Turbulent Drag 10; Angular Drag 2; Transparency 0,2; Rest wie Fluid 1 |
| Physical Effect: Fluid Thick 1 (Schlamm) | Effekt | wie Fluid 1 | Density 1730; Drag 3000; Turbulent Drag 400; Angular Drag 450; Stream Velocity Y 0,1 (Stream aus); Transparency 0,5; Rest wie Fluid 1 |
| Physical Effect: Hold 1 | Effekt | dynamisches Constraint bei Kontakt/Nähe | Hold Duration 2 s; Disable Duration 2 s; Use Proximity Trigger aus (HB an); Proximity 0,2; ConstraintFromObject; ConstraintToObject; Constraint Type JoinBodies; Position Strength X/Y/Z 100000; Angular Strength X/Y/Z 100000 |
| Physical Effect: Hold 2 | Effekt | wie Hold 1 | wie Hold 1, aber Use Proximity Trigger an, zusätzlich Stiction 0,25 |
| Physical Effect: Push 1 | Effekt | Kraft über mehrere Frames | Force X/Y/Z 0; Reference Frame World (oder Local); Oscillation aus; Frequency 0,5; Arrow Scale 1,0; TargetMassObjects |
| Physical Effect: Twist 1 | Effekt | Drehmoment über mehrere Frames | Torque X/Y/Z 0; Reference Frame World; Oscillation aus; Frequency 0,5; Arrow Scale 1,0; TargetMassObjects |

**Physikalische Effekte im Detail** (Handbuch, in eigenen Worten):
- **Fluid:** Der Effekt definiert einen achsparallelen Würfel (Kantenlänge `Cube Size`, Mitte `Cube Position`). Nur Fluid-Objekte
  (Kugel, Sphyl, Zylinder, Box; im Character Edit Mode angelegt), die zumindest teilweise im Würfel liegen, erhalten Kräfte; die
  Kraft wirkt auf das jeweilige Eltern-Massenobjekt (vgl. Abschnitt 8.3). **Auftrieb** hängt vom eingetauchten Volumen und von
  `Fluid Density` ab: Ist die mittlere Dichte (Masse/Volumen der Fluid-Objekte) kleiner als die Fluiddichte, steigt der Körper auf,
  sonst sinkt er. Richtwerte: Luft 1,2; Wasser 1000; Meerwasser 1030; Schlamm 1750; Treibsand 2000; Lava 2600. Überlappende
  Fluid-Objekte werden doppelt gezählt (zu viel Auftrieb). **Widerstand:** `Drag` ist ein geschwindigkeitsunabhängiger Faktor,
  `Turbulent Drag` der geschwindigkeitsproportionale Anteil, `Angular Drag` bremst Drehungen. Der Widerstand skaliert mit der
  Querschnittsfläche aller Fluid-Objekte in Bewegungsrichtung: drei kleine statt eines langen Objekts ergeben in Längsrichtung den
  dreifachen Widerstand. `Stream` gibt dem Fluid eine Strömungsgeschwindigkeit, `CubeMotionOnStream` bewegt den Würfel mit,
  `Waving` lässt den Würfel periodisch schwingen (Richtung, Amplitude, Frequenz in Hz: 0,5 = ein Zyklus in 2 s, Startphase).
  Stabile Schwimmkörper: Schwerpunkt unter dem Auftriebsmittelpunkt. Die genaue Kraftformel steht nicht im Handbuch.
- **Hold:** Ein Constraint zwischen `ConstraintFromObject` und `ConstraintToObject` wird aktiv, sobald beide kollidieren oder
  (mit Proximity Trigger) näher als `Proximity` kommen. Es bleibt `Hold Duration` Sekunden aktiv, ist dann `Disable Duration`
  Sekunden aus und kann danach wieder greifen. Arten: Lock, Lock Position, Lock Orientation, Join Bodies. Steifigkeiten je Achse
  getrennt für Position und Winkel. `Stiction` (nur Hold 2) ist im Handbuch nur als zusätzliche Variable genannt. Anwendung:
  klebrige Oberflächen, Fangen bewegter Objekte ohne vorher bekannten Zeitpunkt.
- **Damping 2:** ein Wert für lineare und einer für Winkeldämpfung (0 frei, 100 sehr langsam), angewandt auf das Elterngelenk
  jedes gewählten Massenobjekts.
- **Push / Twist:** Kraft bzw. Drehmoment je Achse, in Welt- oder lokalen Koordinaten, konstant oder oszillierend mit `Frequency`
  (Hz). Der Pfeil wird nur für das letzte Objekt der Menge gezeichnet. Eine Mehr-Frame-Variante des Force-Events (Abschnitt 9).

**Messung: Wirkung von `Speed` (2026-11-15, Oberfläche).** Das Handbuch nennt nur die Endpunkte (0 ≈ 500 Frames, 1 ≈ 20 Frames).
Gemessen mit „Arms Raised Above Head 2“ in der Demo-Szene „Arms raised above head“ an Character02 (vorne rechts), Strength 0,5,
Leg and Torso Stiffness 0,5, Simulation 120 Hz, Anzeige 60 Frames/s. Verfahren: simulieren, dann mit der Pfeiltaste Frame für Frame
durchschalten, je Frame einen Bildausschnitt des Charakters speichern. Ein Skript bestimmt daraus die Höhe der Hände (oberster heller
Pixel außerhalb der Kopfspalten, Auflösung etwa 1/50 des Hubs). Fortschritt 0 = Ausgangspose (Arme waagerecht), 1 = Endhöhe.
Export und Bitmap-Export taugen nicht: Der Export ist in der Learning Edition gesperrt („Unlock Export Now“), und der Bitmap-Export
lieferte nur schwarze Bilder. Rohdaten `decomp\files\speed_messung.csv`.

| Speed | Noise | Bewegungsbeginn (10 %) | 50 % | 90 % |
|---|---|---|---|---|
| 1,0 | 0 | ≈ 7 Frames | ≈ 15 Frames | ≈ 22 Frames |
| 0,6 | 0 | ≈ 16 | ≈ 28 | ≈ 53 |
| 0,3 | 0,17 (Demo-Wert) | ≈ 36 | ≈ 70 | ≈ 150 |
| 0 | 0 | – | – | in 280 Frames keine Hebung; die Arme sinken leicht ab |

- Speed 1 entspricht der Handbuchangabe (≈ 20 Frames bis zum Ziel).
- Der Verlauf ist **nicht linear** in Speed: Linear zwischen 500 und 20 Frames wären es bei 0,6 etwa 210 und bei 0,3 etwa 356 Frames.
  Die Werte passen grob zu einer geometrischen Abbildung T ≈ 500·(20/500)^Speed (0,6 → 72, 0,3 → 190 Frames), mit kürzeren
  gemessenen Zeiten bei mittleren Werten. Grobe Näherung aus den Messpunkten: t90 ≈ 22·e^(2,7·(1 − Speed)) Frames.
- Je kleiner Speed, desto später beginnt die Bewegung: Die Gelenke folgen einem Sollwert, der sich mit Speed-abhängiger Rate bewegt.
  Bei Speed 0 reicht die Kraft (Strength 0,5) nicht gegen die Schwerkraft, das Ziel wird nicht erreicht. Das passt zum Handbuch
  („bei minimaler Kraft nur ohne Schwerkraft“) und zu den Effektoren aus Abschnitt 5 (Sollgeschwindigkeit + Kraftgrenze).
- Gemessen ist nur ein Behaviour bei einer Kraft. Andere Behaviours und Strength-Werte können andere Zeiten ergeben.
- Bei Speed 0,3 lief Noise 0,17 mit (die Werte des Demo-Events, versehentlich zuerst am falschen Charakter geändert). Die drei
  Wiederholungen ergaben identische Kurven, die Simulation ist also bei gleichem Seed deterministisch.

**Folgerungen für einen Nachbau:**
- Fast alle Körper-Behaviours lassen sich als Kombination aus Zielpose oder Oszillator je Gliedmaße, einer Zeitkonstante aus
  `Speed` (20–500 Frames), einer Kraftgrenze aus `Strength` und einer Haltesteifigkeit für den Rest des Körpers nachbilden. Das
  passt zu den Effektoren aus Abschnitt 5 (Sollgeschwindigkeit + Kraftlimit).
- Bedingte Behaviours brauchen Sensoren (Abschnitt 4): Bodenkontakt der Füße, Fallrichtung, Höhe über Boden (vorhergesagt oder
  `Ground Height`), Abstand zum Zielcharakter.
- Zusammengesetzte Behaviours rufen andere als Teil-Automaten auf (10d.2), z. B. Balance → Stagger 2 → Catch Fall 2 → Hands
  Reach And Look At 2.
- Die physikalischen Effekte sind vollständig über Parameter beschrieben und lassen sich mit Standardmitteln einer Physik-Engine
  nachbauen (Auftrieb/Widerstand je Fluid-Objekt, zeitgesteuertes Constraint, Dämpfung, Kraft/Drehmoment).

---

## 11. Entsprechungen für einen Nachbau

| endorphin | PhysX | Bullet | MuJoCo |
|---|---|---|---|
| Hinge-Effektor (ω-Ziel + Kraftlimit) | Revolute-Drive, Geschwindigkeitsziel | `enableAngularMotor` | Velocity-Aktuator mit `forcerange` |
| Gedächtnis-Effektoren | pro Tick Kraftlimit aus Puffer des Fehlerbetrags neu setzen | ← | ← |
| Kugelgelenk Twist / Swing / Iso | D6-Drive TWIST / SWING / SLERP (ebenfalls implizit) | `btGeneric6DofSpring2Constraint`, Twist auf x | Näherung: weiche Gleichheitsbedingung (`solref`) |
| weiche/harte Grenzen, elliptischer Swing-Kegel | D6 Twist-/Swing-Limits mit Feder | Limits je Achse | `range` + Feder, Kegel nur genähert |
| Animations-/DesiredPose-Event | Active Ragdoll: Animation abtasten, einblenden, Sollorientierung auf Antriebe, Stärke als Faktor | ← | ← |
| Reihenfolge Dynamik → Kollision | meist umgekehrt; an Kontakten verhält sich ein Nachbau etwas anders | ← | ← |
| Evolution | heute eher CMA-ES oder Reinforcement Learning (PPO) | ← | ← |

---

## 12. Offene Punkte

- Nur im Aufbau beschrieben: Darstellung und Oberfläche (Abschnitt 10a, nicht Dialog für Dialog) sowie Import/Export
  (Abschnitt 10b; die einzelnen Parser nicht Zeile für Zeile, Exportweg aus der Struktur abgeleitet). Die Kontaktpunkt-Erzeugung der großen Kollisionstests ist nur dem Zweck nach benannt
  (`scripts/names_bigtests.tsv`).
- Teilweise: **Fluide** (Bausteine belegt, Parameter und Wirkung laut Handbuch in 10d.3, genaue Kraftformel im verschlüsselten
  Behaviour), **virtuelle Muskeln** (vermutlich nur
  Editor-Auswahlgruppen), **Karma-Solver** (Aufbau bekannt, Verfahren vermutet), **Motion Transfer** (RPRO-Magnete belegt,
  Zuordnung der Rig-Stärken zu den Kraftgrenzen erschlossen).
- Vermutet bzw. unklar: weiche Twist-Grenze nutzt Twist-Antriebswerte; Migration der Inseln im Detail; ob das neuronale Netz der einzige
  Genom-Lieferant ist; `ConvertRefFrame`, `RelativeAngle(El)`.
- Ressourcendateien: Alle 64 `.nmb` sind über die Oberfläche einem Listennamen samt Version zugeordnet, ebenso die fünf
  `typeName`-Werte (`SkeletalCharacter`, `SkeletalCharacter2.0`, `2.5`, `2.6`, `2.7`) mit ihren Listen (Abschnitt 10d.2).
  Maßgeblich für den Nachbau ist nur die neueste Version, `SkeletalCharacter2.7` mit 42 Einträgen (Liste in `ui_lists.csv`).
  Die `_legacy`-Listen und die älteren Typen werden deshalb nicht weiter untersucht. Die Spaltenköpfe von `Compatibility.cmf`
  bleiben aus dem Code erschlossen.
- Fehlende Daten (nur in verschlüsselten Dateien): Zahlenwerte des Standardcharakters (Maße, Dichten, Grenzen, Steifigkeiten,
  Materialwerte, Rig-Stärken). Legal beschaffbar durch Ablesen im Character Edit Mode von endorphin;
  Skelettproportionen aus den unverschlüsselten BVH-Dateien (`Resources\Animation\EyesJapan\BVH`, Zoll, 19 Gelenke).
- Behaviours: Art, Auslösebedingung, Bewegungsziel sowie alle Variablen mit Standardwerten für die 42 Behaviours von 2.7 sind
  erfasst (10d.3). Offen bleiben die inneren Zustände, Übergänge und Formeln der Zustandsautomaten (nur in den verschlüsselten
  `.nmb`). Die Speed-Kurve ist für ein Behaviour gemessen (10d.3: 1,0 → ≈ 22 Frames, 0,6 → ≈ 53, 0,3 → ≈ 150 bis 90 %, nicht
  linear). Weitere Behaviours und der Einfluss von Strength sind nicht gemessen. Die Bedeutung von `Ground Size` und `Stiction`
  ist nicht dokumentiert.

---

## 13. Weiterarbeiten mit Ghidra (headless)

Voraussetzung: Ghidra 12.2 benötigt JDK 25. In der Claude-Desktop-App (MSIX) Settings/Cache von Ghidra umleiten
(`support\launch.properties`: `-Dapplication.settingsdir=…`, `-Dapplication.cachedir=…`), sonst scheitert Felix OSGi.
Pfade mit `(x86)` brechen `analyzeHeadless.bat`; Programm in einen Pfad ohne Klammern kopieren.
Argumente mit `<`, `>` oder `&` verarbeitet cmd selbst und gibt sie nicht korrekt weiter.

```
analyzeHeadless.bat <projektordner> endorphin -process endorphin.exe -noanalysis [-readOnly]
    -scriptPath <scripts> -postScript <Skript>.java <args> -log <log> -scriptlog <scriptlog>
```
Zum Umbenennen `-readOnly` weglassen und vorher prüfen, dass keine `*.lock`-Datei existiert (Ghidra-GUI geschlossen).

Eigene Skripte (Ordner `scripts`):

| Skript | Zweck |
|---|---|
| `DumpCallTree.java <ordner> <tiefe> <adr…>` | rekursiv dekompilieren (`calltree.txt` + `<adr>.c`); legt fehlende Funktionen an Sprungzielen an |
| `ProbeFunctions.java <out> <args…>` | `<adr>`: Aufrufer, vtable-Slots, Strings; `vt:<präfix>` / `vtc:<teilstring>`: vtables; `near:<adr>`: Nachbarfunktionen |
| `FindStringRefs.java <out> <text…>` | Funktionen, die einen String referenzieren (`text*` = Präfix) |
| `FindStringContains.java <out> <text…>` | wie oben, aber Teilstring ohne Groß-/Kleinschreibung (z. B. `.elc`) |
| `DumpAsm.java <out> <adr…>` | Disassembly ganzer Funktionen |
| `ListRange.java <out> <von> <bis>` | alle Funktionen eines Bereichs mit Größe, Aufrufern, Strings |
| `ReadValues.java <out> <adr…>` | Konstanten als float/double lesen |
| `FindOffsetUse.java <out> <offset…>` | alle Funktionen mit Speicherzugriff `[reg + offset]` (z. B. wer ein Feld liest) |
| `ListImports.java <out>` | importierte Bibliotheken mit Funktionen |
| `FindImportCallers.java <out> <name…>` | Funktionen, die bestimmte Importe aufrufen (z. B. `glClear`, `SwapBuffers`) |
| `ApplyNames.java <tsv>` | umbenennen nach `adresse<TAB>Namespace::name<TAB>Kommentar` |
| `ExportEndorphinOverview.java` | Klassenübersicht als Markdown/CSV |

Konventionen im Binary: Slot 0x54/0x58 = loadXML/saveXML aller NMData-Knoten; Ausdrucks-Templates
(`ReadWriteMethodType`) schreiben über ihren Slot 0xa4 bzw. 0xc0 und rufen den echten Setter direkt auf
(so findet man Setter hinter Ausdrucks-Symbolen).
