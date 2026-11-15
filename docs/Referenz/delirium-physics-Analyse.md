# Delirium Physics: Referenz der Ragdoll- und Behaviour-Simulation

Quelle: https://github.com/RyanCraighead/delirium-physics (Stand des Klons 2026-11-15, letzter Commit `98e6e2f` vom
2026-10-30, Version 0.2.6). Geklont nach `C:\Users\<Nutzer>\RE\delirium-physics`. Analysiert wurde nur durch Lesen des
Quelltexts; nichts wurde gebaut oder ausgeführt. Belegstellen sind relativ zu `crates/delirium/src/`, sofern nicht anders
angegeben. „(E)“ = aus dem Code erschlossen, nicht ausdrücklich so geschrieben.

## Wichtige Hinweise vorab

- **Was es ist:** Laut README ein per Reverse Engineering gewonnener **Rust-Nachbau von NaturalMotion Euphoria**, so wie
  es in GTA IV läuft, „aus dem Programmcode des Spiels neu geschrieben und gegen das laufende Spiel geprüft“, mit
  KI-Werkzeugen erstellt (README Zeile 22). Euphoria ist die Echtzeit-Schwester von endorphin.
- **Lizenz:** GPL-3.0-only (`LICENSE`, `Cargo.toml`). Wer Code daraus übernimmt, muss das eigene Spiel ebenfalls unter
  GPL-3 veröffentlichen.
- **Rechtlicher Status unklar:** Der Code ist eine Neufassung von Euphoria-Code aus GTA IV. Ob das Projekt Rechte von
  NaturalMotion/Rockstar/Take-Two verletzt, ist offen; das Projekt selbst betont, nicht mit ihnen verbunden zu sein.
  Für ein eigenes Spiel ist es deshalb riskant, Code oder die SDK-Bibliothek zu verwenden.
- **Die Zahlen fehlen:** Rund 1000 Abstimmwerte stehen nicht im Repo. Der Code enthält Platzhalter `kv!(n)`, die zur
  Laufzeit aus der eigenen `GTAIV.exe` bzw. `default.xex` gelesen werden (`crates/delirium-data/src/kv.rs:1-4, 7, 18-46`;
  ohne Freischaltung bricht die Engine ab, `nmrs.rs:33`). Im Engine-Crate werden 584 verschiedene Slots benutzt, davon
  etwa 163 im Physikkern (einige davon sind nur Strukturkonstanten). **Das Repo zeigt Aufbau und Algorithmen, aber keine
  Werte.** Jedes Programm, das die Engine nutzt, braucht beim Spieler eine eigene GTA-IV-Kopie (README Zeile 2, 31, 92).
- **Fazit für dein Spiel:** Als **Lehrbuch** (wie aktive Ragdolls und Reaktions-Behaviours aufgebaut sind) sehr
  wertvoll; als **Bibliothek** ungeeignet. Ideen und Prinzipien nutzen, eigenen Code schreiben (Abschnitt 11).

---

## 1. Aufbau

| Crate | Inhalt |
|---|---|
| `crates/delirium` | Engine: Starrkörper, Gelenke, Kollision, Charakter, alle Behaviours. Reines Rust, ohne Grafik (≈ 25 000 Zeilen) |
| `crates/delirium-data` | Dateileser für Spielformate, `kv.rs` (Zahlen aus der EXE) |
| `crates/delirium-sdk` | C-Schnittstelle `nm_*` plus Bindungen für C++, C#, Unity, Python (`include/delirium_physics.h`) |
| `crates/delirium-room` | „Playroom“-App (Bevy + egui) zum Ausprobieren, inkl. trainiertem Aufsteh-Netz |
| `crates/skate-*` | Fremdprojekt „Skate 3 Rust Engine“ (GPL-3) für den Skateboard-Modus |

Dokumentation: `docs/how-it-works.md` (Engine, Körper, Schritt, Behaviours) und `docs/modding.md` (alle Nachrichten und
Parameter, Skripting).

**Grundidee** (`docs/how-it-works.md:3-8`): Der Körper besteht aus 21 Kapseln und 20 Gelenken, jedes Gelenk hat einen
Muskel. Behaviours sind kleine Regler, die viele Male pro Sekunde entscheiden, wie steif jeder Muskel ist und wohin er
zielt. „Die Physik bewegt den Körper, die Behaviours bewegen die Muskeln.“

## 2. Ein Simulationsschritt

- Feste Rate **60 Hz**; `dt` wird in `round(dt·60)` Teilschritte zerlegt (1 bis 8) (`docs/how-it-works.md:106-108`,
  `nm.rs:2144-2146`).
- **LEAD-Modus** (Delirium simuliert selbst), je Teilschritt (`nm.rs:1204-1224`):
  1. Physik mit dem Antrieb, den die Behaviours im *vorigen* Teilschritt festgelegt haben,
  2. Behaviours lesen den neuen Zustand und schreiben den nächsten Antrieb.
- **Physik** (`ragdoll.rs:2385-2654`):
  1. Kräfte sammeln (Schwerkraft mit optionaler Entlastung `lift`, Zusatzbeschleunigung) (`ragdoll.rs:2451-2473`).
  2. Muskel- und weiche Grenzmomente als Paar auf Kind und Elternteil (`ragdoll.rs:2477-2480, 571-578`).
  3. Positionen mit alter Geschwindigkeit integrieren, Rotation exakt (`rigid.rs:81-89`); Kindteile zurück auf die Pivots
     projizieren (`rigid.rs:114-125`).
  4. **Featherstone-Artikulation** in reduzierten Koordinaten berechnet die freie Bewegung (Kugelgelenk 3 FG, Scharnier
     1 FG, Wurzel Becken) (`artic.rs:371-456, 507-526`). Gelenke sind also keine Constraints, sondern exakt.
  5. Kontakte, Gelenkgrenzen, Griffe als Constraint-Zeilen; **projiziertes Gauß-Seidel** mit Coulomb-Reibkegel,
     Warmstart aus gehaltenen Kontaktpunkten, Restitution (`ragdoll.rs:698-876, 2172`).
  6. Geschwindigkeiten begrenzen und leicht dämpfen (`ragdoll.rs:2610-2614`), zweiter Lauf gegen Durchdringung, der nur
     Positionen verschiebt (`ragdoll.rs:2627-2643`).
- **Kollision:** Dreiecksnetz in Gitterzellen, Kapsel/Box gegen Dreieck per GJK mit Rand, kontinuierliche Erkennung
  für schnelle Teile (`collision.rs`, `gjk.rs`, `ragdoll.rs:1906-2044`). Reibung: Fuß 1,0, Hand und Rest aus kv, mal
  Materialwert (`ragdoll.rs:19-21, 2047-2050`). `bump.rs` wertet Kontakte nur für Schaden und Reaktionen aus.

## 3. Körper

- 21 Teile: Becken (Wurzel), je Bein Oberschenkel/Schienbein/Fuß, 4 Wirbelsäulensegmente, Hals, Kopf, je Arm
  Schlüsselbein/Oberarm/Unterarm/Hand (`docs/how-it-works.md:58-68`). Z oben, Meter, Kilogramm.
- Gelenke: Kugelgelenk (elliptischer Kegel + Twist-Grenze) oder Scharnier (Knie, Ellbogen) (`ragdoll.rs:1137-1153`).
- Je Gelenk im Asset: Position, Twist- und Lean-Achse, Grenzen, `strength`, `damping`, `muscle_stiffness`, `max_torque`
  (`crates/delirium-data/src/nm_asset.rs:313-358`).
- Körper aus Spieldaten, Textdump oder JSON `nm-character-1`; Anpassung an andere Skelette per `fit` (`ragdoll.rs:145-306`,
  `crates/delirium-sdk/src/character.rs`). Abgestimmt auf ca. 1,8 m.
- **Masken** für Körperbereiche: `fb` ganzer Körper, `ub` Oberkörper, `ul/ur` Arme, `ll/lr` Beine, `us` Wirbelsäule,
  `un` Hals, `uw` Handgelenke, `uc` Schlüsselbeine (`nmrs.rs:620-642`).

## 4. Muskeln (Effektoren)

**Parameter:** `set_stiffness(s, d)` setzt `strength = s²`, `damping = 2·s·d` (`nmrs.rs:464-470`), also Steifigkeit
als Eigenfrequenz und `d` als Dämpfungsgrad. `set_relaxation(x)` skaliert die Ausgangswerte mit `x²` bzw. `x`
(`nmrs.rs:472-475`). Verletzung schwächt: Faktor `1 − injury` (`nmrs.rs:702-716`).

**Regelgesetz** (`ragdoll.rs:1597-1636`, selbst nachgeprüft):
```
Fehler e     = Sollwinkel − Istwinkel            (Swing-Komponenten + Twist; beim Scharnier ein Winkel)
Beschleunigung a = S·e − D·ω_rel                  (PD-Regler auf den Winkel)
Überschwingschutz: würde der Winkel im Schritt übers Ziel springen, a = 0
Drehmoment τ = M_eff · a                          (M_eff = effektive Trägheit aus der Artikulation, artic.answer)
Begrenzung: |τ| ≤ max_torque / (1 − blend)
```
- Der Sollwinkel wird vorher abhängig von der Stärke auf die Gelenkgrenzen geklemmt (`nmrs.rs:514-528`).
- **Muscle stiffness** geht nicht ins PD-Moment ein, sondern als zusätzliche Gelenk-Trägheit („Armatur“) in den
  Featherstone-Solver: wirkt wie implizite Dämpfung/Versteifung, schlaffe Glieder schwingen träger (`artic.rs:209-218, 391`)
  (E aus der Rechnung).
- **Weiche Grenzen** (SoftLimit): jenseits einer Grenze eine Feder-Dämpfer-Rückstellung, optional geschwindigkeitsverstärkt,
  nicht auf `max_torque` begrenzt (`nmrs.rs:415-443`). Harte Grenzen zusätzlich als einseitige Constraints
  (`ragdoll.rs:1762-1838`).
- **Schwerkraftkompensation** (`oppose_gravity`): Beim Stehen werden die Massen entlang der Kette zum Standfuß aufsummiert
  und als Moment gegengehalten (`nmdbm.rs:1321-1416`).

## 5. Behaviours

Ein Behaviour ist eine Nachricht mit Parametern (55 Stück, Liste in `docs/modding.md` „Message reference“, Tabelle in
`msg.rs`). Start mit `start=true`, Parameter-Updates ohne `start`, Stopp mit `start=false` oder `stopAllBehaviours`.
Einige enden selbst oder übergeben an das nächste (`docs/how-it-works.md:182-210`).

### 5.1 Balance (das Herzstück)

`bodyBalance` startet drei Teile: Oberkörper (`nmbb.rs`), **Dynamic Balancer** für Beine und Schritte (`nmdbm.rs`) und
Kopfblick (`nm.rs:576-597`). Ablauf je Tick (`nmdbm.rs:1234-1263`):

1. **Neigen:** Lean-Vorgabe kippt die Soll-Hochachse oder wird als horizontale Kraft am Becken umgesetzt
   (`nmdbm.rs:1159-1180`).
2. **Aufgeben prüfen** (`nmdbm.rs:1200-1216`): Abbruch, wenn der horizontale Abstand Schwerpunkt ↔ Füße größer ist als
   `balanceAbortThreshold × Schwerpunkthöhe`, wenn der Schwerpunkt zu tief sinkt oder keine Schritte mehr übrig sind.
   Dann Feedback „balance“, `fallen = true` (`nm.rs:1379-1385`); der Oberkörper-Teil startet Catch Fall (`nmbb.rs:140-146`).
3. **Boden tasten:** Strahl je Fuß von Kniehöhe nach unten (`nmdbm.rs:1218-1232`).
4. **Fußplatzierung** (`nmdbm.rs:400-488`, selbst nachgeprüft):
   - vorhergesagter Schwerpunkt `pc = com + v_horizontal · lead` (feste Vorausschau, Capture-Point-Idee) (`nmdbm.rs:411`),
   - Test, ob `pc` innerhalb der Stützfläche (Viereck aus beiden Füßen) liegt (`nmdbm.rs:292-319, 412`),
   - Fußziele = `pc` seitlich versetzt um einen Schrittabstand; es tritt der Fuß, der weiter von seinem Ziel weg ist
     (`nmdbm.rs:369-398, 418-423`); Reichweite auf eine Ellipse um das Becken begrenzt (`nmdbm.rs:321-333`),
   - Schritthöhe aus Annäherungsgeschwindigkeit und Hüftfreiheit, Landung wenn Kontakt und halber Weg geschafft
     (`nmdbm.rs:458-487`), Schrittzähler `maxSteps`.
5. **Becken:** Gier dreht zur Laufrichtung; PI-artiger Neigungsregler auf Schwerpunkt-Versatz und Geschwindigkeit
   (`nmdbm.rs:509-558`).
6. **Bein-IK:** analytische Zwei-Glied-IK (Knie per Kosinussatz), Knöchel an der Bodennormale, Überschuss einer Hüfte
   wird auf die andere und den Rumpf verteilt (`nmdbm.rs:655-824`).
7. **Oberkörper** (`nmbb.rs:137-327`): Rumpf-Gegendrehung zur Hüfte, Arm-Gegenschwung, Arme raus bei Stößen/Drehung,
   Ellbogen geben bei Kontakt nach, zufällige Kopf- und Körperdrehungen.

Ohne Charakter-Asset gibt es einen vereinfachten Ersatz mit echter LIPM-Formel `cp = com + v·√(h/g)` und Parabel-Schritt
(`nm.rs:1799-1870`) – ein gut lesbares Minimalbeispiel.

### 5.2 Fallen

- **Catch Fall** (`nmcf.rs`): Steifigkeit folgt √(2·kinetische Energie); Fallrichtung aus der Rumpfgeschwindigkeit; je Arm
  ein Strahl in Fallrichtung, Hand per IK zum Auftreffpunkt, Handgelenk zur Normalen; bei Kontakt Armsteifigkeit abbauen
  (Abfedern); Drehmoment gegen Drehung um die Hochachse (`nmcf.rs:111-341`).
- **High Fall** (`nmhf.rs`): Flugphase (Rudern mit Armen/Beinen), Einrollen, Landevorbereitung. Restflugzeit aus Strahl und
  `(√(vz²+2gh) − |vz|)/g`, PD-Drehmoment richtet das Becken zur Landung aus; bei Bodenkontakt je nach Lage Vorwärtsrolle,
  Balance, Abrollen oder Catch Fall (`nmhf.rs:146-439`).
- **Smart Fall / Teeter / Stumble / Stagger Fall** (`extras_fall.rs`): wählen nach Geschwindigkeit und Lage
  (z. B. Abstand Schwerpunkt ↔ Kante beim Teeter) die passende Folgereaktion; viele davon erzeugen nur Nachrichten an
  andere Behaviours (`extras_fall.rs:282-748`).
- **Roll Down Stairs** (`nmrds.rs`): Drehmoment um `Richtung × oben` bei Kontakt und Vorwärtsbewegung, dazu Einrollen.
- **Fall over Wall** (`nmfow.rs`): erkennt Kontakt an Becken/Hüfte mit niedriger Wand, Kräfte senkrecht zur Kante, Beine
  hochziehen.

### 5.3 Treffer

- **Shot** (`nmshot.rs`) ist eine Folge von Unterzuständen mit eigenen Ein-/Austrittsbedingungen (`nmshot.rs:400-506`):
  Treffer auswerten (getroffenes Teil, Seite, Bein → Einknicken), Schockdrehung um die Wirbelsäule
  (`(Treffer − Schwerpunkt) × Geschossrichtung`), Versteifen und Lockern, Armreaktion, **Hand zur Wunde** (IK zum
  mitbewegten Wundpunkt plus Klebe-Impuls), Kopf abwechselnd zur Wunde und zum Schützen, bei Versagen Haltung einfrieren
  und Catch Fall oder Einrollen (`nmshot.rs:526-1146`). Den physikalischen Impuls bringt der Aufrufer separat auf;
  das Behaviour liefert die *Reaktion* (`docs/how-it-works.md:237-244`).
- **Flinch** (`nmfl.rs`): Richtung zur Bedrohung mit Hysterese, Hände per IK vor die Brust (mit Perlin-Rauschen) oder Ducken
  bei Bedrohung von hinten, Balancer dreht weg (`nmfl.rs:148-334`).

### 5.4 Weitere

Brace (Abstützen gegen heranfliegendes Objekt, `nmbr.rs`), Grab (Griff mit Constraint, `nmgr.rs`), Head Look und Point Arm
(IK mit Sichtkegel, `nmhl.rs`, `nmpa.rs`), Arms Windmill (Sinus/Cosinus an Schulter und Schlüsselbein, `nmaw.rs`), Writhe
(Oszillation mit Perlin-Zeit, `nmbw.rs`), Pedal (Fußziel auf Kreisbahn plus IK, `nmpd.rs`), Roll Up (`nmru.rs`), Foetal
(`nmbf.rs`), dazu Extras wie Electrocute, Dangle, Buoyancy, Injury (`extras*.rs`).

## 6. Mehrere Behaviours gleichzeitig

- **Feste Reihenfolge** in `behave` statt Prioritäten: wer später auf einen Muskel schreibt, gewinnt (`nm.rs:1314-1715`) (E).
- **Masken und Sperren:** `lock/unlock(mask)` sichert Effektoren außerhalb der Maske (`nmrs.rs:644-655`).
- **Eltern-Kind:** übergeordnete Behaviours (Brace, High Fall) schalten ihre Kinder selbst (`nmhf.rs:57-67`).
- **Übergänge per Nachricht:** z. B. beendet Catch Fall Balance, Flinch, Windmill und Pedal (`nm.rs:859-863`).
- **Feedback-Liste** je Schritt (`balance`, `catchFall`, `shot`, `HighFall` …) für die Spiellogik (`nm.rs:1205, 1796`).

## 7. Tasks (Spiel-Ereignis → Behaviour-Folge)

`shottask.rs`, `meleetask.rs`, `explosion.rs`, `onfire.rs`, `playerctl.rs` übersetzen Ereignisse in Nachrichtenfolgen, z. B.
Schuss: `stopAllBehaviours` → (in Bewegung) Balance + Lean, (stehend) `shot` mit Parametern aus einer Tuning-Tabelle je
Waffenklasse, abhängig von Gesundheit `stayUpright` oder `shotRelax` (`shottask.rs:47-130`). Explosion: zufällig Rollen,
Zappeln oder Windmühle, kurz vor der Landung Catch Fall (`explosion.rs:63-110`).

## 8. Aufstehen per neuronalem Netz (Playroom)

`getup.net`: zwei verdeckte Schichten à 64 tanh-Einheiten; Eingang Zustand aller 21 Teile, Ausgang Gelenk-Offsets,
Steifigkeitsfaktoren und eine Auswahl unter ~40 Behaviour-„Slots“, Wechsel alle 15 Schritte. Training mit
Evolutionsstrategien (64 antithetische Paare, Rauschen 0,04, Lernrate 0,02, Adam), Belohnung nach Höhe
(`docs/how-it-works.md:246-299`, `crates/delirium-room/src/learn.rs`). Bemerkenswert: Das Netz bedient dieselben Muskeln und
Nachrichten wie ein Skript – lernen *über* den Behaviours statt an ihrer Stelle.

## 9. Nutzung mit fremder Physik-Engine (FOLLOW-Modus)

Deine Engine besitzt den Körper; Delirium ist nur das „Gehirn“ (`docs/modding.md:266-279`):
```
je 1/60 s:  Zustand aller 21 Teile setzen (Position, Rotation, Geschwindigkeit, Drehrate)
            nm_agent_step
            Kraft + Drehmoment je Teil lesen (ohne Schwerkraft) und in der eigenen Engine anwenden
```
Beispiele für MuJoCo und PyBullet (`crates/delirium-sdk/examples/mujoco_follow.py`), Unity-Paket mit `ConfigurableJoint`s.
Einschränkungen (E): Die Momente sind mit Delirums eigener Trägheit berechnet und begrenzt; harte Grenzen, Kontakte und die
Muskel-Armatur fehlen in der Ausgabe, das Ergebnis kann also in einer anderen Engine abweichen (`ragdoll.rs:2481-2490`,
`crates/delirium-sdk/src/lib.rs:541-554`).

## 10. Vergleich mit endorphin

Bezug: `C:\Users\<Nutzer>\Desktop\endorphin-Analyse.md`, Abschnitte 5, 10d.3, 11.

| endorphin-Konzept | Umsetzung in Delirium (Euphoria/GTA IV) |
|---|---|
| Effektor: Sollgeschwindigkeit + Kraftgrenze (Abschnitt 5.1) | PD-Regler auf Winkel → Soll-Beschleunigung → Drehmoment über effektive Trägheit, Begrenzung `max_torque` (Abschnitt 4) |
| Strength, Damping, Stiffness als Behaviour-Parameter | `strength = s²`, `damping = 2·s·d`, zusätzlich muscle stiffness als Armatur |
| Gedächtnis-Effektoren / adaptive Stärke (Abschnitt 5.2) | nicht gefunden; Steifigkeit wird von den Behaviours direkt gesetzt (z. B. aus kinetischer Energie bei Catch Fall) |
| Kugelgelenk Twist/Swing, weiche/harte Grenzen | gleich aufgebaut: elliptischer Swing-Kegel + Twist, SoftLimit + harte Constraint-Grenzen |
| Karma-Solver (Gauß-Seidel) | Featherstone-Artikulation für Gelenke + Gauß-Seidel nur für Kontakte/Grenzen/Griffe |
| Behaviours als verschlüsselte Zustandsautomaten (`.nmb`) | als Rust-Code lesbar: je Behaviour ein Modul mit Unterzuständen |
| aktiv / bedingt / physikalische Effekte | dieselbe Idee; bedingte Reaktionen lesen Sensoren (Fußkontakt, Schwerpunkt, Fallrichtung, Strahlen) |
| Teil-Automaten (Balance → Stagger → Catch Fall → Hands Reach) | Eltern-Kind-Behaviours, Übergänge per Nachricht, Feedback-Ereignisse |
| Speed 0…1 → 500…20 Frames (gemessen, nicht linear) | keine direkte Entsprechung; Geschwindigkeit ergibt sich aus Steifigkeit und Dämpfung des PD-Reglers |
| Simulation 120 Hz | 60 Hz, bis 8 Teilschritte |
| Evolution / neuronaler Regler (Abschnitt 10) | Aufsteh-Netz, trainiert mit Evolutionsstrategien |

Delirium ist damit eine **echtzeittaugliche, auf Spiele zugeschnittene** Variante derselben Ideen: einfacher Körper
(21 Kapseln), feste 60 Hz, Behaviours als Code-Module, Steuerung über Nachrichten und Feedback.

## 11. Folgerungen für dein Spiel

**Direkt übernehmbar (als Idee, nicht als Code):**
1. **Muskel = PD-Regler mit Drehmomentgrenze.** In jeder Engine verfügbar (Unity `ConfigurableJoint`-Drives, PhysX D6,
   Godot `Generic6DOFJoint`, Bullet Motoren). Steifigkeit als `s²` und Dämpfung als `2·s·d` parametrieren – dann ist `s`
   intuitiv „wie schnell“, `d` „wie gedämpft“.
2. **Balance:** Schwerpunkt mit Geschwindigkeit vorausschätzen (`com + v·√(h/g)`), liegt er außerhalb der Füße → Schritt
   dorthin; Bein-IK analytisch; Schwerkraft entlang der Standbein-Kette kompensieren; bei zu großem Abstand aufgeben und an
   eine Fall-Reaktion übergeben. Der vereinfachte Ersatz in `nm.rs:1799-1870` ist dafür das kompakteste Vorbild.
3. **Reaktionen als kleine Zustandsautomaten** mit klaren Auslösern (Bodenkontakt, Fallrichtung, Höhe, Abstand) und
   Übergaben: Balance → (gestoßen) Stagger → (fällt) Catch Fall → (liegt) Relax/Aufstehen.
4. **Treffer = Impuls + Reaktion getrennt:** Physik-Impuls aufs getroffene Teil, dazu ein Reaktions-Behaviour (Hand zur
   Wunde, Kopf zur Wunde, Schockdrehung, kurz versteifen, dann lockern).
5. **Masken** für Körperbereiche, damit Oberkörper-Reaktion und Beinbalance parallel laufen.
6. **Feedback-Ereignisse** („ist gefallen“, „Arme haben abgefedert“) als Schnittstelle zur Spiellogik.
7. **Aufstehen lernen** statt per Hand bauen, aber *über* vorhandenen Behaviours (Netz wählt Behaviour + Offsets).

**Selbst bauen musst du:** alle Zahlen (Steifigkeiten, Grenzen, Schwellen – im Repo nicht enthalten), den Charakter, die
konkrete Abstimmung auf deine Engine und deine Spielmechanik.

**Nicht empfehlenswert:** die SDK-Bibliothek oder Code einbinden (GPL-3-Pflicht, ungeklärte Rechtslage, GTA IV beim
Spieler nötig).

## 12. Offene Punkte

- Alle Zahlenwerte (kv-Slots) sind unbekannt und werden hier bewusst nicht beschafft.
- Wo genau der Kugelimpuls in der Engine aufgebracht wird, ist nicht geklärt (vermutlich im Aufrufer).
- Bedeutung vieler Bit- und Slot-Nummern (Zustandsbits bei High Fall, Feedback-Slots) bleibt offen, da sie aus kv kommen.
- Playroom (`delirium-room`), Skate- und Portal-Modus wurden nur am Rand betrachtet.
