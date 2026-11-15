# MuJoCo: Referenz der Physik-Engine für physikbasierte Charaktere

Quelle: https://github.com/google-deepmind/mujoco, geklont 2026-11-15 nach `C:\Users\<Nutzer>\RE\mujoco` (Commit `058cac8`
vom 2026-11-14, Version 4.0.2 laut `unity/package.json:4`). Analysiert nur durch Lesen von Dokumentation und Quelltext;
nichts gebaut oder ausgeführt. „(E)“ = erschlossen, nicht wörtlich so im Repo.

**Kürzel der Belegstellen** (alle relativ zu `C:\Users\<Nutzer>\RE\mujoco\`):
CI = `doc/computation/index.rst`, MO = `doc/modeling.rst`, OV = `doc/overview.rst`, XR = `doc/XMLreference.rst`,
FW = `src/engine/engine_forward.c`, SM = `src/engine/engine_core_smooth.c`, CO = `src/engine/engine_core_constraint.c`,
SO = `src/engine/engine_solver.c`, PA = `src/engine/engine_passive.c`, CD = `src/engine/engine_collision_driver.c`,
UM = `src/engine/engine_util_misc.c`, IN = `src/engine/engine_init.c`.

## Wichtige Hinweise vorab

- **Was es ist:** eine allgemeine Physik-Engine für Mehrkörpersysteme mit Kontakt („Multi-Joint dynamics with Contact“),
  entwickelt für Robotik, Biomechanik, Animation und maschinelles Lernen, gepflegt von Google DeepMind (`README.md:20-26`).
  C-API, Python-Bindings, Unity-Plugin, WebAssembly.
- **Lizenz:** Code **Apache 2.0**, Dokumentation CC BY 4.0 (`README.md:186-199`). Darf frei verwendet werden, **auch
  kommerziell in einem Spiel**; Pflichten: Lizenztext und Copyright-Hinweis beilegen, Änderungen kennzeichnen. Das ist der
  große Unterschied zu Delirium (GPL-3 und unklare Rechtslage) und endorphin (verschlüsselt, Learning Edition).
- **Was es nicht ist:** Es gibt **keine eingebauten Behaviours** (kein Balancieren, Fallen, Abfangen). Aktuatoren sind nur
  lokale Kraftgesetze; das Regelverhalten schreibt man selbst (Abschnitt 10). MuJoCo ist also die „Physik“-Hälfte, nicht
  das „Gehirn“.
- **Stand:** Der Klon ist ein Entwicklungsstand; einige Funktionen sind laut Doku neu oder experimentell (Integrator
  `discrete` „September 2026“, CI:726; Speculative Contacts „Oktober 2026“, MO:1772).

---

## 1. Aufbau des Repos

| Ordner | Inhalt |
|---|---|
| `src/engine/` | die Physik in C (≈ 95 Dateien): Vorwärtsdynamik, Constraints, Solver, Kollision, Sensoren |
| `include/mujoco/` | öffentliche C-API (`mjModel`, `mjData`, Funktionen) |
| `doc/` | ausführliche Dokumentation: `computation/index.rst` (die Rechnung), `modeling.rst` (Modellieren, Tuning), `XMLreference.rst` (alle MJCF-Elemente) |
| `model/` | Beispielmodelle, u. a. `humanoid/` |
| `python/` | Python-Bindings und Tutorials (z. B. `LQR.ipynb`: Humanoid balanciert auf einem Bein) |
| `unity/` | Unity-Plugin (Paket `org.mujoco`) |
| `wasm/` | WebAssembly / JavaScript |
| `mjx/` | MJX: MuJoCo in JAX (GPU, differenzierbar, für Reinforcement Learning) |
| `simulate/`, `studio/` | Betrachter-Apps |

Modelle werden als XML (MJCF) beschrieben und vom eingebauten Compiler in vorab angelegte Datenstrukturen übersetzt;
zur Laufzeit gibt es keine Speicherallokation (OV:113-117).

## 2. Ein Simulationsschritt (`mj_step`)

Bewegungsgleichung: `M·v̇ + c = τ + Jᵀ·f` (CI:224-226), mit M Massenmatrix, c Bias-Kräfte (Coriolis, Zentrifugal,
Schwerkraft), τ = passive + Aktuator- + angewandte Kräfte, f Constraint-Kräfte.

```
mj_step (FW:2668-2710)
  mj_forward (FW:2603-2658)
    POSITION:  Kinematik, Schwerpunkte, Tendons; Massenmatrix M (CRB) und Faktorisierung LᵀDL;
               Kollision -> Constraints aufstellen (J, Residuen, Impedanz) -> Inseln -> Transmissionen
    VELOCITY:  Geschwindigkeiten, passive Kräfte (Federn, Dämpfer, gravcomp), Bias-Kräfte per RNE
    Steuerung: mjcb_control (eigener Regler schreibt ctrl)
    Aktuatoren: qfrc_actuator
    ACCELERATION: qacc_smooth = M⁻¹·(passive − bias + applied + actuator)       (FW:1016-1072)
    CONSTRAINTS: Solver -> efc_force, qfrc_constraint, qacc                     (FW:1206-1300)
  Integrator: Euler | implicit | implicitfast | RK4 | discrete
```

- `mj_step1`/`mj_step2` teilen den Schritt, damit ein eigener Regler nach der Velocity-Stufe Steuerwerte setzen kann
  (CI:2243-2246, FW:2717-2784). Genau hier sitzt später das „Gehirn“ deines Charakters.
- Die vollständige Stufenliste steht in CI:2280-2348.

## 3. Koordinaten und Körper

- **Generalisierte Koordinaten:** Der Zustand sind Gelenkwinkel q und Gelenkgeschwindigkeiten v. Gelenke *fügen*
  Freiheitsgrade hinzu, statt sie zu entfernen; Gelenk-Constraints sind dadurch exakt und können nicht auseinanderdriften
  (OV:996-1014). Körperposen ergeben sich aus der Vorwärtskinematik.
- **Massenmatrix** per Composite-Rigid-Body-Algorithmus, dünnbesetzte LᵀDL-Faktorisierung ohne Fill-in (CI:249-253,
  SM:1718-1845); **Armature** (zusätzliche Gelenkträgheit) liegt auf der Diagonale und verbessert die Stabilität deutlich
  (SM:1744-1746, XR:2676-2677).
- **Gelenktypen:** free (6 FG), ball (3), hinge (1), slide (1). Geschlossene Schleifen nur über Gleichheits-Constraints,
  langsamer und ungenauer (CI:844-848).
- **Passive Kräfte** (PA:901-1270): Federn zur Ruhelage `springref` (bei Kugelgelenken über Quaternionen), Dämpfer,
  Fluidkräfte, Adhäsion, **Schwerkraftkompensation `gravcomp`** (Kraft −m·gravcomp·g am Schwerpunkt, PA:1050-1053); mit
  `actuatorgravcomp` zählt sie gegen die Kraftgrenzen der Aktuatoren (XR:2600-2606).

## 4. Constraints und Kontakte

**Grundprinzip** (CI:1166-1453): Die Beschleunigung wird als Lösung eines **konvexen Optimierungsproblems** bestimmt
(Gauß-Prinzip der kleinsten Zwangskraft). Constraints sind **weich**: Statt harter Bedingungen gibt es eine
Feder-Dämpfer-artige Soll-Beschleunigung `a_ref = −b·(J·v) − k·r` (r = Verletzung, z. B. Eindringtiefe) und eine
Regularisierung R, die zwischen Sollwert und freier Bewegung interpoliert (CI:1588-1614, 1653). Das Problem ist strikt
konvex und hat immer eine eindeutige Lösung; die Inversdynamik ist analytisch.

**solref** – wie hart/schnell der Constraint zurückstellt (Default `0.02 1`, MO:339):
```
Standardformat (timeconst, dampratio):  b = 2 / (d_w·timeconst)
                                        k = d(r) / (d_w²·timeconst²·dampratio²)          (MO:357-360, selbst geprüft)
Direktformat (−stiffness, −damping):    b = damping / d_w,   k = stiffness·d(r) / d_w²   (MO:382-385)
```
- timeconst ≥ 2·Zeitschritt wird erzwungen (MO:362-364); dampratio 1 = kritisch gedämpft.
- Eindringtiefe in Ruhe: `r = a_u·(1−d)·timeconst²·dampratio²` (MO:371-372).

**solimp** – Impedanz d (0…1, „wie viel Kraft der Constraint aufbringen kann“), abhängig von der Verletzung
(Default `0.9 0.95 0.001 0.5 2`, MO:257): d0 bei r = 0, d_w ab r = width, dazwischen eine S-Kurve mit midpoint und power
(CO:1981-2018). Höheres d = härterer Kontakt.

**Constraint-Typen** (CI:826-1109): Gleichheiten (connect, weld, joint, tendon), Reibungsverlust in Gelenken
(`frictionloss`, wirkt wie trockene Reibung), Gelenkgrenzen (einseitig, ab `margin`), **Kontakte** mit condim 1 (nur
Normale), 3 (plus Gleitreibung), 4 (plus Torsionsreibung), 6 (plus Rollreibung); Reibungskegel pyramidal (Standard) oder
elliptisch; `impratio` macht Reibung härter als die Normale (CO:2132-2133). Langsames Gleiten ist Teil des weichen Modells
(OV:864-873); gegen rutschende Füße empfiehlt die Doku elliptische Kegel mit impratio 10–100 (MO:1979-1983).

**Kollision:** Broadphase Sweep-and-Prune, Midphase Hüllkörperbaum, Narrowphase analytisch für Ebene/Kugel/Kapsel/Zylinder/
Ellipsoid/Box, sonst GJK/EPA für Meshes (konvexe Hülle) und Höhenfelder (CI:1769-1876). Filter über `contype`/`conaffinity`;
Eltern- und Kindkörper kollidieren nicht miteinander (CI:1785-1808). `margin`/`gap` steuern, ab welchem Abstand Kontakte
erkannt werden und Kraft wirken (CI:1041-1051).

## 5. Solver

| Solver | Problem | Eigenschaften |
|---|---|---|
| **Newton** (Standard, IN:94) | primal | Hessematrix + Cholesky, exakte Liniensuche; meist wenige Iterationen (MO:557-559) |
| **CG** | primal | nichtlinearer CG, keine Setup-Kosten |
| **PGS** | dual | projiziertes Gauß-Seidel mit zufälliger, aber fest geseedeter Reihenfolge und Nesterov-Momentum (SO:484-736); sinnvoll bei wenigen Kontakten, z. B. Laufen (MO:1976-1977) |

Defaults: iterations 100, tolerance 1e-8, ls_iterations 50, cone pyramidal (IN:56-98, XR:437-505). **Warmstart** aus der
Lösung des Vorschritts (CI:1536-1549). Unabhängige Teilprobleme („Inseln“) lassen sich parallel lösen (CI:1553-1576).

## 6. Integratoren und Zeitschritt

| Integrator | Eigenschaft |
|---|---|
| `Euler` (Standard, IN:91) | semi-implizit; Gelenkdämpfung implizit (CI:519-522, 638-645) |
| `implicitfast` (**empfohlen**, CI:767-774) | implizit in der Geschwindigkeit: Aktuator-Dämpfung `kv` und Tendon-Dämpfung stabil auch bei großen Werten (MO:1954-1957) |
| `implicit` | wie implicitfast plus Coriolis-Ableitungen, teurer |
| `RK4` | 4 Auswertungen pro Schritt, gut für energieerhaltende Systeme |
| `discrete` (experimentell) | auch in der Position implizit: sehr steife Federn und hohe `kp` bleiben stabil (CI:669-698) |

Standard-Zeitschritt 0,002 s (IN:56); für Lokomotion funktionieren laut Doku oft 5–15 ms (MO:1942-1946). Echtzeit heißt:
Rechenzeit pro Schritt < Zeitschritt (XR:357-359).

**Determinismus:** bitgleich reproduzierbar bei gleicher Version und Architektur, wenn der Warmstart mitgespeichert wird
(CI:2374-2388) – nützlich für Replays und Netzwerk.

**Schlafen:** optional; ein Baum mit |v| < 1e-3 über 10 Schritte schläft inselweise ein, Kontakt weckt sofort
(CI:2202-2219). **Aktuierte Bäume schlafen per Default nicht** (XR:2337), aktive Ragdolls bleiben also wach.
**Speculative Contacts** (neu, Default aus) gegen Durchtunneln schneller Objekte (CI:1880-1895).

## 7. Aktuatoren (die „Muskeln“ der Engine)

Ein Aktuator = Transmission + Aktivierungsdynamik + Kraftgesetz (CI:283-285):
```
Kraft p = gain·(act oder ctrl) + b0 + b1·l + b2·l̇          (CI:390-394; FW:703-866)
         l = Aktuatorlänge (z. B. Gelenkwinkel·gear), l̇ = deren Geschwindigkeit
Gelenkkraft = Σ Momentarm · p                               (CI:402-408)
Reihenfolge: ctrl auf ctrlrange klemmen -> Kraft -> auf forcerange klemmen   (FW:401-942)
```
- **Aktivierung (dyntype):** none, integrator (ẇ = u), filter / filterexact (Tiefpass mit Zeitkonstante), muscle, pid,
  dcmotor (CI:349-378, XR:5942-5958).
- **Transmission:** Gelenk, Tendon, Slider-Crank, Site (kartesisch, mit `refsite`), Body (CI:292-331); `gear` skaliert
  Länge und Kraft (XR:5787-5792).

**Kurzformen** (alle sind intern `general`, MO:664-668):

| Kurzform | Kraft | Beleg |
|---|---|---|
| `motor` | u (direktes Drehmoment) | XR:6076-6082 |
| `position` | kp·(u − l) − kv·l̇ → **PD-Regler** | XR:6151-6158 (selbst geprüft) |
| `velocity` | kv·(u − l̇) → **Sollgeschwindigkeit** | XR:6514-6521 |
| `intvelocity` | Sollposition integriert aus Sollgeschwindigkeit, dann PD | XR:6594-6601, MO:805-811 |
| `damper` | −kv·l̇·u (steuerbare Dämpfung) | XR:6696-6708 |
| `pid` | kp(u_pos − l) + kv(u_vel − l̇) [+ Vorsteuerung] [+ ki·∫e], mit Anti-Windup und Ratenbegrenzung | XR:6272-6383 |
| `orientation` | kp·log(q⁻¹·q_soll) − kv·ω für Kugelgelenke, Drehmomentnorm begrenzt | XR:6417-6438 |

- `dampratio` statt kv: Dämpfung relativ zur kritischen, `kv = dampratio·2·√(kp·m)` mit der Masse am Gelenk, einmal beim
  Kompilieren berechnet (XR:6229-6245, `src/engine/engine_setconst.c:1629-1634`). Wer die Stärke zur Laufzeit ändert, muss
  `actuator_gainprm`/`actuator_biasprm` selbst nachziehen (E).
- `kv` wird nur mit `implicit`/`implicitfast` implizit (stabil) behandelt; eine gesättigte Kraft (am `forcerange`) dämpft
  nicht implizit (XR:5807-5811, MO:736-737).

**So bildet man die beiden bekannten Muskelmodelle nach (E):**

| Vorbild | MuJoCo-Entsprechung |
|---|---|
| Delirium/Euphoria: PD auf Winkel, `strength = s²`, `damping = 2·s·d`, Drehmomentgrenze T; Delirium multipliziert mit der effektiven Gelenkträgheit | `<position joint=".." kp="m·s²" dampratio="d" forcerange="-T T"/>` mit m = Trägheit am Gelenk, ctrl = Sollwinkel; für Schulter/Hüfte `orientation` (Herleitung in 12.5; korrigiert, vorher fälschlich kp = s²) |
| endorphin: Sollgeschwindigkeit + Kraftgrenze F | `<velocity joint=".." kv="groß" forcerange="-F F"/>`, ctrl = Sollgeschwindigkeit (weiche Näherung des Constraint-Motors; Doku warnt vor Instabilität reiner Geschwindigkeitsregelung, MO:805-811) |
| endorphin Speed/Strength/Stiffness | `pid` mit `input="pos vel"`: Stiffness → kp, Speed → Sollgeschwindigkeit, Strength → forcerange |

Ein echter Constraint-Motor mit Sollgeschwindigkeit (wie in endorphins Karma) existiert nicht; Aktuatorkräfte werden vor dem
Constraint-Solver berechnet (FW:2644-2651).

## 8. Muskelmodell

Biomechanisches Hill-artiges Modell (MO:924-1073, UM:962-1104):
```
Kraft = −F0 · ( FL(L)·FV(V)·act + FP(L) )
L = normierte Muskellänge, V = normierte Verkürzungsgeschwindigkeit (L/(L0·vmax))
FL: Kraft-Längen-Kurve, stückweise quadratisch, Maximum 1 bei L = 1, null außerhalb [lmin, lmax]
FV: 0 für V ≤ −1, (V+1)² bis V = 0, steigt bis fvmax (Dehnung)
FP: passive Kraft bei Überdehnung (L > 1)
Aktivierung: act' = (ctrl − act)/τ, τ schneller beim Anspannen als beim Lösen (Default 0,01 / 0,04 s)
```
Defaults: range 0,75–1,05, scale 200, lmin 0,5, lmax 1,6, vmax 1,5, fpmax 1,3, fvmax 1,2 (XR:6941-6992). Vereinfachungen
gegenüber OpenSim: unelastische Sehnen, keine Fiederung (MO:1058-1073). Für Spiele meist zu aufwendig; Gelenkantriebe
(Abschnitt 7) reichen (E).

## 9. Sensoren (Eingänge für das „Gehirn“)

Sensoren beeinflussen die Simulation nicht, liefern aber genau die Größen, die Behaviours brauchen (MO:1085-1163,
`src/engine/engine_sensor.c`):
- **Schwerpunkt:** `subtreecom`, `subtreelinvel`, `subtreeangmom` – Kern jeder Balance-Logik.
- **Lage:** `framepos`, `framequat`, `framezaxis` (Aufrichtung), `framelinvel`, `frameangvel`, `framelinacc`.
- **IMU:** `accelerometer`, `gyro`.
- **Kontakt:** `touch` (Normalkraft im Site-Volumen, z. B. Fußsohle), `contact`, `force`/`torque` zwischen Körpern.
- **Gelenke:** `jointpos`, `jointvel`, `ballquat`, `actuatorfrc`, `jointlimitfrc`.
- `delay`/`interval` modellieren Latenz und Abtastrate (MO:1132-1163).

## 10. Humanoid-Modell und Regelung

**`model/humanoid/humanoid.xml`** (aus der DeepMind Control Suite, `model/humanoid/README.md:3-8`):
- 16 Körper, 1 Freejoint am Torso + 21 Scharniergelenke = 27 Freiheitsgrade; Kapseln mit Reibung 0,7.
- 21 `motor`-Aktuatoren (reine Drehmomentsteuerung, ctrl −1…1), gear = Maximaldrehmoment in Nm: Bauch 40, Hüfte x/z 40,
  Hüfte y 120, Knie 80, Knöchel 20, Schulter 20, Ellbogen 40 (`humanoid.xml:203-223`, selbst geprüft).
- Passive Gelenkfedern und -dämpfer, Hamstring-Tendons koppeln Hüfte und Knie, Zeitschritt 5 ms, Euler; Keyframes squat,
  stand_on_left_leg, prone, supine (`humanoid.xml:17, 67-80, 191-265`). Massen über Dichte 1000 (Default).
- Varianten mit 100 Objekten bzw. 22 und 100 Humanoiden.

**Kein eingebauter Regler.** Was es stattdessen gibt:
- **LQR-Tutorial** (`python/LQR.ipynb`): Humanoid steht auf einem Bein. Arbeitspunkt per Inversdynamik, Linearisierung per
  finiten Differenzen (`mjd_transitionFD`), Kosten „Schwerpunkt über dem Fuß“ (Gewicht 1000) plus Gelenkkosten, Riccati
  lösen, `ctrl = ctrl0 − K·Δx` (LQR.ipynb:467-820). Ein sauberes Beispiel, wie man Balance *rechnet* statt von Hand baut.
- **Reinforcement Learning** mit MJX (Laufen für Humanoid und Vierbeiner, `doc/mjx.rst:345-346`), differenzierbare Physik,
  MuJoCo Playground.
- Eigene Regler setzt man über `mjcb_control` bzw. zwischen `mj_step1` und `mj_step2` (`doc/APIreference/APIglobals.rst:142-146`).

## 11. Spiele-Integration

**Unity-Plugin** (`unity/`, `doc/unity.rst`, Apache 2.0):
- MJCF importieren (Menü „Import MuJoCo Scene“) oder Szene exportieren; jede MJCF-Komponente wird eine Unity-Komponente,
  Achsen werden umgerechnet (Unity Y oben, MuJoCo Z oben); Schwerkraft und Zeitschritt kommen aus den Unity-Einstellungen
  (unity.rst:65-173).
- `MjScene.FixedUpdate` ruft `mj_step`; mit Callback läuft `mj_step1 → eigener Regler → mj_step2`
  (`unity/Runtime/Components/MjScene.cs:83, 330-337`).
- Aktuatoren in Unity: nur General, Motor, Position, Velocity, Cylinder, Muscle (`unity/Runtime/Components/MjActuator.cs:25-31`);
  pid, orientation, intvelocity und dampratio fehlen im C#-Code (E).
- Einschränkungen: keine Trigger-Collider, Änderungen zur Laufzeit erst nach Neuaufbau der Szene, keine Plugins, einige
  Sensoren fehlen (unity.rst:101-342). Konverter nach Unitys eigenem ArticulationBody vorhanden
  (`unity/Runtime/Tools/ConvertMj2Rca.cs`).

**WebAssembly** (`wasm/README.md`): npm-Paket `@mujoco/mujoco`, Single- und Multi-Thread; noch in Arbeit, nicht alle APIs.

**Andere Sprachen:** Rust-, Swift-, Java-, Julia-Bindings von Dritten (`README.md:150-157`) – z. B. für Bevy oder eigene
Engines.

## 12. Vergleich: endorphin, Delirium (Euphoria), MuJoCo

Quellen: `C:\Users\<Nutzer>\Desktop\endorphin-Analyse.md` („E“ + Abschnitt), `C:\Users\<Nutzer>\Desktop\delirium-physics-Analyse.md`
(„D“ + Abschnitt) und dieses Dokument („M“ + Abschnitt). Die Spalte **Für dein Spiel** ist eine Empfehlung (E = eigene
Einschätzung).

Kurz vorweg: **endorphin und Euphoria liefern das Gehirn samt eigener Physik, MuJoCo liefert eine sehr gute, freie Physik
ohne Gehirn.**

### 12.1 Überblick

| Thema | endorphin 2.7 | Delirium (Euphoria, GTA IV) | MuJoCo | Für dein Spiel |
|---|---|---|---|---|
| Zweck | Animationswerkzeug (Offline) | Ragdoll-Reaktionen im Spiel (Echtzeit) | allgemeine Physik für Robotik und Forschung | Echtzeit-Spiel → Delirium als Vorbild für die Logik |
| Lizenz, Verfügbarkeit | proprietär, Learning Edition, Daten verschlüsselt | GPL-3, Rechtslage unklar, braucht GTA IV beim Spieler | **Apache 2.0** | nur MuJoCo darf in dein Spiel |
| Quelltext | nein (nur Ghidra) | ja, Rust | ja, C | Delirium und MuJoCo zum Lernen lesen |
| Schnittstelle | Editor, Szenen-XML | C-API, C#, Unity, Python | C-API, Python, C#/Unity, WebAssembly | MuJoCo |

### 12.2 Physik

| Thema | endorphin 2.7 | Delirium | MuJoCo | Für dein Spiel |
|---|---|---|---|---|
| Koordinaten | maximal: jeder Körper frei, Gelenke als Constraints (Karma) (E1, E6) | reduziert: Featherstone-Artikulation (D2) | reduziert: generalisierte Koordinaten, CRB + LᵀDL (M3) | reduziert: Ragdoll kann nicht „auseinanderfallen“ |
| Gelenke im Solver | Constraint-Zeilen (Kugelgelenk bis 9 Zeilen) (E6) | exakt in der Artikulation, keine Zeilen (D2) | exakt, keine Zeilen (M3) | reduziert |
| Kontakte | Karma-Kontakte, Material mit Restitution/Schwelle (E2) | hart, projiziertes Gauß-Seidel, Coulomb-Kegel, Warmstart (D2) | **weich**, konvexes Problem, Newton (Standard), solref/solimp (M4, M5) | MuJoCo: stabil; gegen Rutschen elliptischer Kegel + `impratio` |
| Gleichungslöser | Kea (direkt) oder Arthur (iterativ, Vierer-Blöcke), welcher aktiv ist offen (E2) | Gauß-Seidel nur für Kontakte, Grenzen, Griffe (D2) | Newton / CG / PGS, Inseln parallel (M5) | Newton für wenige Charaktere, PGS bei vielen Kontakten testen |
| Rate | Simulation 120 Hz, Anzeige 60 Bilder/s | 60 Hz, bis 8 Teilschritte (D2) | frei, Standard 2 ms; Lokomotion 5–15 ms (M6) | 60-Hz-Spiel: 2–4 MuJoCo-Schritte pro Bild |
| Integrator | Karma (implizit über Constraints) | Position mit alter Geschwindigkeit, dann Geschwindigkeit lösen (D2) | Euler (Standard), **implicitfast empfohlen**, RK4, discrete (M6) | implicitfast |
| Verzögerungen | Sensoren sehen den vorigen Schritt; Kontakte wirken einen Schritt später; Behaviour → Motor ≈ 2 Ticks (E2) | Physik nutzt den Antrieb des vorigen Teilschritts (D2) | Regler (`mjcb_control`) sieht den aktuellen Zustand im selben Schritt; Kontakte im selben Schritt (M2) | Verzögerung von 1–2 Ticks wirkt natürlicher („Reflexzeit“); bewusst einbauen |
| Kollisionsformen | Kugel, Quader, Zylinder, Kapsel, Ebene, Dreiecksliste; konvexe Netze ohne Tests (E2) | Kapsel/Box gegen Dreiecksnetz per GJK (D2) | Ebene, Kugel, Kapsel, Zylinder, Ellipsoid, Box, konvexes Mesh, Höhenfeld (M4) | Kapseln für den Körper |
| Determinismus | identische Läufe bei gleichem Seed (gemessen, E10d.3) | nicht untersucht | bitgleich bei gleicher Version und Architektur (M6) | MuJoCo: Replays, Netzwerk-Synchronisierung möglich |
| Schlafen | – | schlafende Agenten überspringen Behaviours (Delirium `docs/how-it-works.md:235`) | optional; aktuierte Körper schlafen per Default nicht (M6) | Schlafen für liegende, ausgeschaltete Ragdolls |

### 12.3 Gelenke und Grenzen

| Thema | endorphin 2.7 | Delirium | MuJoCo | Für dein Spiel |
|---|---|---|---|---|
| Kugelgelenk-Winkel | Twist/Swing mit tan(θ/4), keine Singularität unter 360° (E7) | Swing-Achse + Winkel θ, Twist über die Halbierende (Delirium `crates/delirium/src/rigid.rs:159-188`) | Quaternion; Grenze nur auf den **Gesamtdrehwinkel θ** (CI:962-969) | für Grenzen Twist/Swing nötig |
| Grenzform | elliptischer Swing-Kegel + Twist, weich und hart (E6) | elliptischer Kegel + Twist, weiche Grenze + harte Constraint-Zeilen (D4) | Scharnier: Bereich; Kugel: nur Kreis-Kegel um den Gesamtwinkel, Twist inklusive | in MuJoCo Schulter/Hüfte als **drei Scharniere** bauen (so macht es `humanoid.xml`) |
| Weiche Zone | innerhalb weich nur Dämpfung, zwischen weich und hart Rückstellfeder (E6) | Feder-Dämpfer jenseits der Grenze, nicht drehmomentbegrenzt (D4) | alle Grenzen weich über `solreflimit`/`solimplimit`, Beginn über `margin` (M4) | `margin` + weiches solref als Ersatz für die weiche Zone |
| Gelenkreibung, Zusatzträgheit | – | muscle stiffness wirkt als Armatur (D4) | `frictionloss`, `damping`, `armature` (M3, M4) | `armature` erhöht die Stabilität deutlich |

### 12.4 Muskeln (Gelenkantriebe)

| Thema | endorphin 2.7 | Delirium | MuJoCo | Für dein Spiel |
|---|---|---|---|---|
| Regelgröße | Scharnier: **Sollgeschwindigkeit**; Kugelgelenk: weiche Constraint-Zeilen (E5.1, E6) | **Winkel** (PD auf Beschleunigungsebene) (D4) | frei wählbar: Moment, Winkel, Geschwindigkeit, PID, Orientierung (M7) | Winkel-PD (wie Delirium) |
| Formel | ω_soll = clamp(Gradient·(θ_soll − θ), ±GradientCap); Motor regelt ω mit \|F\| ≤ BaseForceCap | a = s²·(θ_soll − θ) − 2·s·d·ω; τ = M_eff·a; \|τ\| ≤ max_torque/(1 − blend) | τ = kp·(u − θ) − kv·θ̇, geklemmt auf `forcerange` | |
| Parameter | Kalibrierung Gradient / GradientCap / BaseForceCap (Standard 20 / 3 / 5); Skeletal: k, d, F_max je Twist/Swing/Linear | s (Steifigkeit), d (Dämpfung), max_torque, muscle stiffness | kp, kv oder dampratio, forcerange, gear | s und d: anschaulich (s ≈ „Tempo“, d ≈ „Weichheit“) |
| Stabilität bei hoher Steifigkeit | sehr gut: implizit über Constraints (E6) | gut: Beschleunigung über effektive Trägheit, Überschwingschutz (D4) | Dämpfung implizit mit implicitfast, Steifigkeit nur mit `discrete` (M7) | in MuJoCo kp moderat halten, dampratio ≈ 1, implicitfast |
| Adaptive Kraft | Gedächtnis-Effektoren: Kraftgrenze steigt mit gemitteltem \|Fehler\| (E5.2) | nein; Behaviours setzen Steifigkeit direkt | nein (pid hat einen I-Anteil auf das Moment) | eigene Erweiterung: forcerange pro Schritt aus Fehlerhistorie |
| Änderung zur Laufzeit | über Ausdrücke (Setter speichern nur, Wirkung im nächsten Tick) (E5.4) | Behaviours schreiben jeden Teilschritt | `actuator_gainprm`, `actuator_biasprm`, `actuator_forcerange` direkt setzen (M7) | pro Schritt aus dem Gehirn |
| Schwerkraftausgleich | nicht beschrieben | `oppose_gravity` entlang der Standbein-Kette (D4) | `gravcomp`, `actuatorgravcomp` (M3) | für stehende Charaktere wichtig |

### 12.5 Umrechnung der Muskelparameter

**Delirium → MuJoCo.** Delirium rechnet massennormiert: τ = M_eff·(s²·e − 2·s·d·ω). Das ist ein harmonischer Oszillator mit
Eigenkreisfrequenz s (rad/s) und Dämpfungsgrad d. In MuJoCo mit m = Trägheit am Gelenk:
```
kp = m·s²,   kv = 2·m·s·d      ⇔   <position joint=".." kp="m·s²" dampratio="d" forcerange="-T T"/>
```
MuJoCo bestimmt m beim Kompilieren einmal in der Ausgangspose (XR:6238), Delirium nutzt in jedem Schritt die aktuelle
effektive Trägheit; bei stark wechselnden Posen (Arm gestreckt/gebeugt) verhalten sich beide etwas verschieden (E).

**endorphin (Scharnier) → MuJoCo.** Solange die Kraftgrenze nicht erreicht ist, regelt Karma die Gelenkgeschwindigkeit
praktisch hart auf ω_soll. Mit einer großen Verstärkung K statt „hart“ gilt τ ≈ K·(ω_soll − ω) = K·Gradient·(θ_soll − θ) − K·ω,
also ein PD-Regler mit kp/kv = Gradient (E). Zwei Umsetzungen:
- genau: eigener Regler setzt jeden Schritt `ctrl = clamp(Gradient·(θ_soll − θ), ±GradientCap)` für
  `<velocity kv="K" forcerange="-BaseForceCap BaseForceCap"/>` (K groß, implicitfast),
- einfach: `<position kp="K·Gradient" kv="K" forcerange="…"/>`, dabei fehlt die Geschwindigkeitsgrenze GradientCap.

Standard-Kalibrierung 20 / 3 / 5 heißt: ungesättigt nähert sich das Gelenk dem Ziel mit Zeitkonstante 1/20 s = 50 ms,
höchstens 3 rad/s, höchstens Kraftgrenze 5 (Einheit im Code nicht geprüft) (E).

**endorphin (Kugelgelenk) → MuJoCo.** k, d und F_max je Twist/Swing direkt als `<orientation kp="k" kv="d" forcerange="0 F"/>`
(begrenzt die Norm des Moments). endorphin ist dabei auch in der Position implizit (ERP/CFM-Zeile) und bleibt bei großem k
stabil, MuJoCo nur mit dem Integrator `discrete` (E).

**endorphin „Speed“ → s.** Für einen kritisch gedämpften Regler (d = 1) ist nach ω·t ≈ 3,9 rund 90 % des Wegs geschafft
(Sprungantwort 1 − (1 + ωt)·e^(−ωt) = 0,9). Mit den gemessenen 90-%-Zeiten aus E10d.3 (Anzeige 60 Bilder/s, Strength 0,5,
inklusive Anlaufzeit, daher grob):

| endorphin Speed | t90 gemessen | entspricht s ≈ 3,9 / t90 |
|---|---|---|
| 1,0 | ≈ 22 Frames = 0,37 s | ≈ 10,6 rad/s |
| 0,6 | ≈ 53 Frames = 0,88 s | ≈ 4,4 rad/s |
| 0,3 | ≈ 150 Frames = 2,5 s | ≈ 1,6 rad/s |

Bemerkenswert: Deliriums Standard-Körpersteifigkeit liegt in derselben Größenordnung (`setStiffness` mit bodyStiffness 12,
Delirium `docs/modding.md:318`) wie endorphins Speed 1,0 (E).

### 12.6 Sensoren, Behaviours, Steuerung

| Thema | endorphin 2.7 | Delirium | MuJoCo | Für dein Spiel |
|---|---|---|---|---|
| Sensoren | 40 Typen + zusammengesetzte, Pull-Modell, Ausgabe auf [−1, 1] normiert (E4) | im Code der Behaviours (Schwerpunkt, Fußstrahlen, kinetische Energie) (D5) | Sensor-Elemente: `subtreecom`, `subtreelinvel`, `touch`, IMU, Gelenke; mit Latenz `delay` (M9) | MuJoCo-Sensoren + eigene Normierung wie endorphin |
| Behaviour-Modell | hierarchische Zustandsautomaten mit eigener Ausdruckssprache, verschlüsselt (E3) | ein Rust-Modul pro Behaviour mit Unterzuständen (D5) | keins | Zustandsautomat pro Reaktion, im Code |
| Ansteuerung | Zeitfenster-Events auf der Zeitleiste (E9) | Nachrichten mit Parametern, Start/Stopp, 55 Stück (D5) | eigener Code über `mjcb_control` bzw. zwischen `mj_step1`/`mj_step2` (M2) | Nachrichten-API wie Delirium |
| Parameter-Stil | normiert 0…1 (Speed, Strength, Stiffness) (E10d.3) | physikalisch (Steifigkeit s, Dämpfung d) | physikalisch (kp, kv) | intern physikalisch, für Designer 0…1 (Umrechnung 12.5) |
| Mehrere gleichzeitig | Effektor-Gruppen, Teil-Automaten (E10d.2) | Masken (`ub`, `ll` …), feste Reihenfolge, Eltern-Kind (D6) | – | Masken pro Körperbereich |
| Meldungen an die Spiellogik | über Ausdrücke und Szenenzustände | Feedback-Liste (`balance`, `catchFall` …) (D6) | – | Feedback-Ereignisse wie Delirium |
| Animation einbinden | Animation- und DesiredPose-Events, Motion Transfer mit RPRO-Magneten (E8.5, E9) | `activePose`, `incomingTransforms`, `hardConstraint`, `softKeyframe` (D5) | Mocap-Körper (`mocap="true"`, XR:2290-2301) + Constraint, oder Position-Aktuatoren auf Animationswinkel | Animation als Sollwinkel der Muskeln, Physik bleibt aktiv |
| Lernen | Evolution, neuronaler Regler (CTRNN) (E10) | Aufsteh-Netz mit Evolutionsstrategien (D8) | MJX / Reinforcement Learning, LQR, differenzierbar (M10) | Aufstehen lernen, über vorhandene Behaviours |

### 12.7 Baukasten für dein Spiel (Zusammenfassung)

| Baustein | Vorbild | Umsetzung |
|---|---|---|
| Physik | MuJoCo | reduzierte Koordinaten, implicitfast, 2–4 Schritte pro Bild, Kapseln, elliptischer Reibungskegel |
| Gelenke | endorphin/Delirium (Grenzform), MuJoCo (Technik) | Schulter/Hüfte als drei Scharniere mit Bereichen, weiche Grenzen über `margin`/solref, `armature` |
| Muskel | Delirium | `position`-Aktuator, kp = m·s², dampratio = d, forcerange; s und d zur Laufzeit setzen |
| Parameter für Designer | endorphin | Speed 0…1 → s (Tabelle 12.5), Strength → forcerange, Stiffness → Steifigkeit der übrigen Gelenke |
| Adaptive Kraft (optional) | endorphin Gedächtnis-Effektor | forcerange aus dem gemittelten Fehler der letzten Ticks |
| Balance | Delirium | Schwerpunkt vorausschätzen, Schritt setzen, Bein-IK, Schwerkraftausgleich; oder LQR (M10) |
| Reaktionen | Delirium + endorphin | Zustandsautomaten mit Auslösern, Teil-Behaviours, Masken, Feedback |
| Reflexzeit | endorphin | Sensorwerte bewusst 1–2 Ticks verzögert verwenden |
| Animation | alle drei | Animation liefert Sollwinkel, Muskeln folgen; bei Treffern Steifigkeit senken |

## 13. Folgerungen für dein Spiel

**MuJoCo als Physik-Basis ist realistisch:**
1. Lizenz erlaubt den Einsatz im Spiel (Apache 2.0).
2. Exakte Gelenke (keine auseinanderfallenden Ragdolls), stabile weiche Kontakte, determinismusfähig.
3. Muskelantriebe sind mit `position` (PD wie Euphoria) oder `velocity` (wie endorphin) direkt im Modell beschreibbar;
   `implicitfast` macht hohe Dämpfung stabil.
4. Unity-Plugin vorhanden (mit Einschränkungen); für andere Engines C-API oder Rust-Bindings.

**Das Gehirn baust du selbst**, mit den Ideen aus den anderen beiden Analysen:
- Balance aus Delirium (Schwerpunkt vorausschätzen, Schritt setzen, Bein-IK, Schwerkraftkompensation) – die nötigen
  Größen liefern `subtreecom`, `subtreelinvel` und `touch` an den Füßen; alternativ LQR wie im Tutorial.
- Reaktionen als Zustandsautomaten (Stagger → Catch Fall → Liegen), Treffer = Impuls (`xfrc_applied`) + Reaktion.
- Steifigkeit/Geschwindigkeit wie bei endorphin zeitlich steuern: `kp`, `kv` und `forcerange` pro Aktuator zur Laufzeit
  ändern (`actuator_gainprm`, `actuator_biasprm`, `actuator_forcerange`).
- Lernen (z. B. Aufstehen) mit MJX oder Evolutionsstrategien, das Ergebnis über dieselben Aktuatoren ausgeben.

**Zu beachten:**
- Weiche Kontakte gleiten leicht → elliptische Kegel, `impratio`, ggf. `noslip`.
- Zeitschritt bewusst wählen (Spiel 60 Hz → mehrere MuJoCo-Schritte pro Frame, z. B. 4 × 4,17 ms wie im
  Delirium-MuJoCo-Beispiel).
- MuJoCo kennt keine Spiele-Features wie Trigger, Fahrzeuge, Netzwerk; das kommt aus deiner Engine.
- Für reine Spiel-Ragdolls ohne Robotik-Genauigkeit sind auch PhysX/Jolt/Bullet mit Gelenkmotoren eine Option; MuJoCo
  punktet bei Genauigkeit, Determinismus und Lernwerkzeugen (E).

## 14. Offene Punkte

- Nicht gebaut und nicht gemessen: Rechenzeit eines aktiven Humanoids bei Spiel-Zeitschritten.
- Unity-Plugin nur aus Doku und C#-Code beschrieben, nicht ausprobiert; der main-Branch passt laut Doku eventuell nicht zur
  Release-Binary (unity.rst:25-28).
- Experimentelle Teile (`discrete`, Speculative Contacts) können sich noch ändern.
- Gesamtmasse des Humanoid-Modells steht nicht im Repo (ergibt sich erst aus Dichte und Geometrie).
