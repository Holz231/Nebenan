# Vergleich mit Parallelstand 57, 4. November 2026

Geprüft wurde der vom Nutzer genannte Ordner `Nebenan-active-ragdoll-57` innerhalb dieses Arbeitsverzeichnisses.
Stand 57 und unsere lokale Weiterentwicklung von Stand 51 sind parallele Entwicklungen mit unterschiedlichen
Reglern und Vorgaben. Ein vollständiges Ersetzen würde Funktionen der jeweils anderen Entwicklung verlieren.

## Was Stand 57 ergänzt

- Runde 52: Zehen im Schwung um bis zu 0,15 rad anheben, damit sie nicht früh aufsetzen. Die Hüfte bremst das Kippen
  über ein Drehmoment am Becken und das entgegengesetzte am Oberschenkel; dieses Moment wird vom verfügbaren
  Gelenkmoment abgezogen.
- Runden 53–55: Becken zum vorausgehenden Fuß drehen und Brust/Schultern über die Wirbelsäule dagegen drehen.
  Die Vorgabe läuft glatt vom Anfang zum Ziel des Schritts, statt der schnellen Richtungsänderung des tatsächlichen
  Schwungfußes zu folgen. Die Drehung entsteht über Gelenkziele und wirkt in der Physik.
- Runde 56: Bei Schritten gerade rückwärts nimmt das Becken keine solche Drehung an; der Anteil geht glatt mit der
  Schrittrichtung über.
- Runde 57: Gegendrehung bei Schritten zurück auf 85 % begrenzen und Sohlenreibung von 0,9 auf 1,1 erhöhen. Das
  vermindert in den dokumentierten Messungen das seitliche und drehende Rutschen des Standfußes.

Die wesentlichen Funktionen stehen in `Nebenan-active-ragdoll-57/src/figure_balance.c`: `nbLiftToes`, `nbStepTurn`,
`nbStepShare`, `nbTurnTrunkBack` und `nbHoldUpright`. Die Sohlenreibung steht in dessen `src/figure.c`.

## Was aus unserer lokalen Weiterentwicklung dort fehlt

Stand 57 enthält weder `responsiveTrunk` mit vollständigem dreidimensionalem Treffermoment und kurz nachgebenden
Wirbelsäulenmotoren noch unsere zusätzliche Standbremse, Ruhegeschwindigkeitsglättung, erste seitliche Fußwahl oder
frühere Auslösung bei langsamem Takt. Auch `collisionImpulse` und die Hinderniskontakt-Szene sind dort nicht enthalten.
Das ergibt sich aus dem direkten Quellvergleich; es handelt sich um unterschiedliche Ausgangslinien.

## Eigene Prüfung

Dieser Abschnitt dokumentiert den Ausgangsvergleich vor der anschließenden Integration.

Stand 57 wurde frisch in `build/review57` mit GCC 16.1, Release und Warnungen als Fehler gebaut, ohne seine
Quelldateien oder seinen mitgelieferten Build zu ändern. Der vollständige Testsatz besteht: 126 Einzeltests.

Die bestehende Messreihe von 168 leichten Oberflächen-Treffern, 40–80 N s in Zweierschritten auf die obere Brust
aus acht Richtungen bei 60 Hz, wurde gegen diese Bibliothek erneut gerechnet: kein Fehlschlag statt vier in unserer
aktuellen Variante. Damit wird auch der bisher offene 72-N-s-Stolperfall in Stand 57 abgefangen. Das Ergebnis gilt
für dessen Regler und Sohlen; es ist kein isolierter Nachweis für einen einzelnen Baustein.

Zusätzlich dieselben fünf 60-N-s-Treffer mit Balance durch die Beine, jeweils die Brustbewegung relativ zum Becken
in der ersten Sekunde gemessen. Die drei für die Rumpfreaktion verwendeten Proben:

| Bewegung | Unsere aktuelle Variante | Stand 57 |
| --- | ---: | ---: |
| Beugen nach Stoß in −z | 24,2° | 17,3° |
| Beugen nach Stoß in +z | 37,6° | 17,2° |
| Verdrehung nach Schultertreffer | 18,3° | 39,7° |

Alle fünf Proben fangen sich in beiden Varianten. Stand 57 gibt dem Schritt deutlich mehr Gegendrehung; unsere
Variante gibt beim Auftreffen stärker nach. Wegen der unterschiedlichen Impulsbehandlung, Hüftregelung und
Sohlenreibung dürfen die Unterschiede nicht allein der neuen Schrittdrehung zugeschrieben werden.

Für die weitere Entwicklung sind Schrittdrehung und Zehenführung die naheliegenden getrennt zu prüfenden Bausteine.
Die zusätzliche Schrittrotation muss gemeinsam mit unserer nachgebenden Trefferreaktion und ihren Kraftgrenzen
gemessen werden. Der Quellvergleich allein belegt noch nicht, wie gut die Kombination fängt.

## Anschließende Integration

Am 4. November wurde die Zehenführung für Vorwärtsschritte angepasst und die geplante Gegenrotation mit halber
Stärke als unabhängige Option `nbFigureMotors::turnWithSteps` ergänzt. Die nachgebende Trefferreaktion und unsere
Standregelung bleiben eigene Bausteine. In der Demo sind beide Bewegungsoptionen aktiv und getrennt schaltbar.
Messwerte der Kombination und verworfene Varianten stehen unter „Schrittdrehung und Zehenführung mit nachgebendem
Rumpf“ in [Optimierungen.md](Optimierungen.md).

In der anschließenden Grenzfallprüfung wurde ein Fehler im gemeinsamen Ansatz gefunden: Der Mittelwert der
Eckpunkte einer Stützfläche springt, sobald ein beinahe kollinearer Punkt in ihre Hülle aufgenommen wird. Schon
eine Fußdrehung um 0,000001 rad verschob das Balanceziel um etwa 3,1 cm. Unsere Variante verwendet bei eigener
Balance mit Schrittdrehung nun den Flächenschwerpunkt. Zusätzliche Hüftbremsung, Glättung der Schrittdrehung und
Rücknahme bei drehenden Standfüßen wurden geprüft, aber wegen neuer Fehlschläge nicht übernommen. Die Ergebnisse
stehen unter „Stützflächenmitte ohne Sprung“ in [Optimierungen.md](Optimierungen.md). Stand 57 bleibt unverändert.

Die nächste Runde verbessert die Bremsung nach dem Fangschritt für die Kombination mit Schrittdrehung: Der
gewünschte Bodendruck wird im Stand früher in Richtung der verbleibenden Bewegung verlagert, innerhalb der
vorhandenen Sohlen und Kraftgrenzen. Damit sinken die mittleren 60-Hz-Fehlschläge über drei Treffpunktlagen von
17 auf 8 von 5 760; die Rumpfdrehung bleibt bei etwa 24–26°. Siehe „Restschwung nach dem Fangschritt abbremsen“
in [Optimierungen.md](Optimierungen.md). Die Referenz wurde dafür nicht verändert.

Die folgende Prüfung der Schrittplanung behebt außerdem widersprüchliche Grenzen: Das Kürzen eines weiten
Schrittziels auf die Beinreichweite konnte den zuvor hergestellten Fußabstand wieder unterschreiten. Unsere
Variante mit Schrittdrehung wählt nun ein Ziel innerhalb beider Grenzen. Dazu kommen Geometrieprüfungen für
beide Füße, verschiedene Körpergrößen und Ausrichtungen sowie ein zusätzlich gefangener 114-N-s-Treffer.
Die Messreihen stehen unter „Schrittweite und Seitenabstand gemeinsam begrenzen“ in [Optimierungen.md](Optimierungen.md).
