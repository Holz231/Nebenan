# Karte der gesteuerten Figur

Wo in `src/figure.c` was entschieden wird, so weit die Runden 118 bis 124 es verfolgt haben. Namen statt Zeilen, die
Zeilen wandern. Vor einer Änderung die Kommentare an den Konstanten lesen: Dort steht mit Zahlen, warum sie so sind und
was schon verworfen wurde.

## Der Gang: `nbSteerFeet`

- **Kadenz.** `nbCadenceModel`, `nbGaitCadence`: Schritte je Sekunde aus dem Tempo in Einheiten von sqrt(g l). Gehen
  bis `NB_GAIT_RUN_FROM`, Rennen ab `NB_GAIT_RUN_TO`. `tools/measure/speeds.py` rechnet daraus die Dauer eines Zyklus.
- **Abheben und Landen.** Ein Fuß hebt nach dem Takt ab, oder früher, wenn sein Bein ihn nicht mehr erreicht
  (`NB_GAIT_LOSE`, `nbTimeToLose`). Beim Abheben werden die Größen des Schwungs gesetzt: `swingHurry`, `swingCalm`,
  `swingHeight`, `swingRun`, `swingSoft`, `swingLift`. Von zwei Füßen am Boden hebt der zuerst ab, den sein Bein zuerst
  verliert, aber nicht einer, der eben gelandet ist (`NB_GAIT_FRESH`, nach der Phase oder nach der Zeit seit der
  Landung, `stanceTime`): Er bleibt stehen, wenn der andere hinter der Hüfte bald verloren geht, oder wenn der andere
  abheben darf und sein eigenes Bein ihn noch `NB_GAIT_FRESH_KEEP` des schnellsten Schwungs hält (Runde 131). Hält es
  ihn kürzer, hebt er sofort wieder ab: Stehen kostete dort das Becken.
- **Takt.** Rennend hält der Gang seinen Takt (`gaitPhase`), und die Füße halten sich daran: Wer vor seiner Phase
  abhebt, schwingt langsamer, bis `NB_GAIT_KEEP_MOST` eines Zyklus, wer danach abhebt, schneller (`late` beim Abheben).
  Früh oder spät heißt der nähere Weg: Mehr als ein halber Zyklus nach der Phase ist früh für den nächsten Schwung
  (Runde 123). Den Takt rennend den Füßen nachzuholen, ist zweimal verworfen.
- **Fersenlandung.** Der Schwung rollt den Fuß ab `NB_GAIT_HEEL_FROM` auf die Ferse (`heel`), die Landung setzt
  `landRoll`, und der Fuß kommt mit `stanceTime` über `NB_GAIT_HEEL_LOAD` eines Zyklus flach. So lange gilt er nicht als
  hinter der Hüfte. Seit Runde 124 dreht er die Zehen nur so weit nach oben, wie er vor der Hüfte landet
  (`NB_GAIT_HEEL_AHEAD`, `heelPart`), und landet flach, wo er hinter ihr landet, solange die Beine vorwärts gehen
  (`stepBack` des Schwungs und `legBack` der Beine): Gehen die Beine rückwärts, ist derselbe Fuß vor der Hüfte.
- **Eile.** `swingHurry` bis `NB_GAIT_HURRY`: Der Schwung läuft schneller, wenn der andere Fuß verloren geht. Sie darf
  nur so groß sein, wie der Fuß für den Weg, den er noch hat, mit `NB_GAIT_SWING_ACCEL` bremsen kann (Runde 119).
- **Hub.** `nbSwingLift`: gehend ein früher Buckel (`nbSwingEarly`, Scheitel bei einem Viertel des Schwungs) plus ein
  gleichmäßiger Teil, rennend ein Bogen. `swingCalm` ist der Anteil, mit dem ein eiliger Schwung hebt und kippt, 1
  durch das Quadrat der Eile. Er folgt der Eile nach unten, solange der Buckel steigt, bis `NB_SWING_EARLY_TOP`, und
  der Buckel steigt dabei so schnell weiter wie im letzten Update (`swingBump`, `swingBumpWas`, Runde 121).
- **Sanfter Anlauf.** `swingSoft`: Ein Bein, das zwischen `NB_SWING_SOFT_FROM` und `NB_SWING_SOFT_TO` gestreckt über die
  Zehen abhebt, hebt mit einer Rampe. Rennend ist das Bein nie so gestreckt, dort greift er nicht. Seit Runde 133 auch
  beim Bremsen im Rennen: Das Tempo des Gangs (`gaitSpeed`) folgt dem der Kapsel mit `NB_GAIT_FOLLOW` und hinkt beim
  Bremsen bis 1,7 m/s hinterher, der Gang rennt also noch, wenn die Figur fast steht. Ist sie um mehr als
  `NB_SWING_SLOW_FROM` langsamer als ihr Gang, hebt ein Fuß über die Zehen mit `NB_SWING_SLOW` des sanften Anlaufs.
  Nicht mehr: Mit der ganzen Stärke bleibt das Bein fast gestreckt. Nicht für Füße, die über die Ferse abheben: Die
  brauchen das schnelle Heben. Seit Runde 134 geht ein Fuß, der beim Bremsen nicht über die Zehen abhebt, ab
  `NB_SWING_SLOW_ARC` dieses Rückstands über die erste Hälfte seines Bogens so sanft hoch wie einer, der im
  Rückwärtsrennen abhebt (`swingBack`, `nbSwingLift`): Ein Fuß vor der Hüfte hob sonst mit gestrecktem Bein auf dem
  Bogen des Vorwärtsrennens ab. Mehr als dieser Bogen, der halbe Hub oder der sanfte Anlauf schaden dort.
- **Weg.** `nbSwingPath`: eine Kubik von dort, wo der Fuß ist, zur Ruhe am Ziel, in jedem Update neu geplant.
  `nbSwingTime` begrenzt, wie lange ein Schwung dauern darf, wenn der Gang ins Rennen wechselt (Runde 118).

## Wurzel und Rollen: `nbLowerSteered`

- **Die Wurzel sinkt** für Füße, die nicht hinter der Hüfte sind, so weit ihr Bein sie sonst nicht erreicht (`drop`).
  Danach bringt eine Feder sie hoch, `NB_ROOT_SETTLE`, und behält dabei die Hälfte des Tempos, mit dem sie sank,
  `NB_ROOT_SETTLE_KEEP` (Runde 120). Für Füße hinter der Hüfte kommt dazu, was nach dem Rollen noch fehlt (`more`).
- **Hinter der Hüfte** (`behind`) ist ein Fuß, wenn die Hüfte vor seinen Zehen liegt (`along > 0`). Dreht der Körper,
  wechselt das von einem Update zum nächsten.
- **Länge des Beins im Schwung.** Ein schwingender Fuß geht, wohin sein Bein reicht (`NB_LEG_SWING_SLACK`), und sein
  Bein kommt bremsend auf die Länge, mit der es landet, höchstens auf seine Reichweite (`NB_LEG_BRAKE`, `nbBrakeStep`,
  `legLong`, Runde 129); ein Fuß, der früher landet, kommt in ein bis zwei Updates an seinen Platz (`legGap`). Nicht auf
  Treppen (`stairWeight`). Ein fast gestrecktes Knie bewegt sich mehrfach so weit, wie das Bein länger wird: Wo ein
  Schienbein springt, zuerst nachsehen, wie schnell die Länge des Beins sich ändert und wo sie anhält. Der Fuß landet
  mit dem Fersenrollen, auf das sein Schwung gekommen ist (`swingHeel`). Ein Bein, das in seinem Schwung gehalten wurde,
  lässt seine Länge sanft los, es wird je Update nur begrenzt schneller kürzer (`NB_LEG_LET_GO`, `legRate`, `legKept`,
  Runde 130). Beide Stärken liegen knapp über dem, was das stetige Rennen selbst tut, gesucht mit `steady130.py`: Wer
  sie ändert, sucht neu.
- **Rollen.** Hinter der Hüfte rollt ein Fuß so weit über die Zehen, wie sein Bein braucht (`nbRollToReach`), mit einem
  Blick voraus (`NB_FOOT_ROLL_AHEAD`), dazu der Abstoß des Gangs (`footPush`, `NB_GAIT_PUSH_IN`, `NB_GAIT_PUSH_SPEED`).
  Über die Ferse und von den Zehen herunter höchstens `NB_FOOT_HEEL_RATE`. Über die Zehen hinauf wird das Rollen je
  Update höchstens um `NB_FOOT_ROLL_GAIN` schneller, nur für Beine, die vorwärts gehen (`footRollGain`, Runde 122). Seit
  Runde 128 wird es dabei auch höchstens um so viel langsamer, über `NB_FOOT_ROLL_MOST` hinaus, wenn es sein muss, und
  das Tempo, mit dem ein Fuß von der Ferse flach kam, zählt nicht mit (`footRollGain`, `footRolled`).
- **`footRoll` und `footRolled`.** `footRoll` setzt auch der Gang, etwa wenn ein Fuß von einer Landung herunterkommt.
  `footRolled` ist das Rollen, das der Fuß am Boden zuletzt bekam.
- **Rückwärts** (`side < 0`) landen Füße auf den Zehen und bleiben darauf (`tips`, `leads`, `landed`).

## Das Bein

- Ein Bein streckt sich höchstens auf `NB_LEG_REACH` seiner Länge, gehend auf `NB_LEG_REACH_WALK`, gehend in einer
  Wendung auf `NB_LEG_REACH_BENT`. `legReach` ist der Wert für jetzt. Beim Bremsen im Rennen, wo die Figur langsamer
  ist, als ihr Gang rennt, um `NB_LEG_REACH_SLOW` weniger (Runde 134): 0,005 bis 0,015 weniger halten, ab 0,02 wird es
  von seitwärts auf rückwärts schlechter.
- **Das Knie verstärkt.** An einem fast gestreckten Bein wandert das Knie mehrfach so weit, wie sich der Abstand von
  Hüfte zu Knöchel ändert, bei 0,98 gut doppelt so weit. Fast jeder harte Sprung eines Schienbeins ist ein Knöchel,
  der an einem gestreckten Bein schnell näher kommt: durch den Hub, das Rollen oder die sinkende Wurzel.

## Messen an einer Stelle

- `lab101 "t:vx:vz[:grad],…" out.csv ende` fährt ein Skript von Geschwindigkeiten und schreibt jedes Update: Ursprung
  und Drehung jedes Knochens (`b<n>x…`), ob ein Fuß schwingt, `along`, `hurry`, `roll`, `behind`, Zehen und Fersen,
  `rootY`, `rdrop`. Mit drei Werten je Schlüssel schaut die Figur, wohin sie geht, mit vier hält sie den Blick.
- Knochen: Becken 0, Hände 9 und 12, Oberschenkel 13 und 16, Schienbeine 14 und 17, Füße 15 und 18.
- Die Stelle `i` einer Szene liegt bei `a = a0 + i · Spreizung / 24`, die Szenen stehen in `sweepf112.py` und
  `sweeph111.py`.
- `tests.sh` baut die Tests gegen einen gebauten Stand. Für die Zahl eines neuen Tests auf dem alten Stand die
  Testdatei in den sauberen alten Arbeitsbaum kopieren, bauen, messen, zurücksetzen.

## Tests

`test/test_figure.c`: `SteerRunFlightTest` (Runde 118), `SteerTurnLandTest` (119), `SteerTurnSettleTest` (120),
`SteerTurnLiftTest` (121), `SteerTurnRollTest` (122), `SteerTurnRhythmTest` (123), `SteerTurnHeelTest` (124),
`SteerTurnPushTest` (128), `SteerSideToBackTest` (129), `SteerSwitchSidesTest` (130), `SteerFreshFootTest` (131),
`SteerStopLiftTest` (133), `SteerBrakeBackTest` (134), und für den Körper `FigureShoulderTest` (126) und
`FigureSkinTest` (127): Ändert eine Runde die Knochen, fällt er, bis `tools/mannequin/make_skin.py` die Haut neu
geschrieben hat, erst `dump_bones`, dann das Werkzeug. Die Hashes `NB_EXPECTED_…_HASH` ändern sich nur, wenn sich ein
Lauf ändert, der dort festgehalten ist: Das ist erlaubt, wird aber in der Roadmap genannt.
