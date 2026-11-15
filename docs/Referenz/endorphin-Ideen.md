# Referenz: Ideen aus der Analyse von endorphin

Diese Datei fasst zusammen, was sich aus der Analyse des Nutzers von endorphin 2.7.1 (NaturalMotion) für Nebenan als
Idee mitnehmen lässt. Die Analyse selbst liegt daneben, [endorphin-Analyse.md](endorphin-Analyse.md), vom Nutzer
erarbeitet und am 15. November 2026 auf seine Entscheidung aufgenommen; darin ist nur der Benutzername in zwei Pfaden
ersetzt.

Für Nebenan gilt daraus nur die Idee, keine Zahl, keine Formel im Wortlaut, kein Dateiformat und kein Aufbau von
Klassen. Was gebaut wird, wird in Nebenan selbst gemessen und begründet, wie alles andere. Diese Zusammenfassung nennt
deshalb keine Werte aus dem Programm.

## Aufbau

- **Die Engine kennt kaum Verhalten.** Ein Verhalten ist ein Zeitfenster mit einem hierarchischen Zustandsautomaten.
  Seine Bedingungen und Aktionen sind Ausdrücke einer kleinen Sprache: Sie lesen Sensoren und schreiben Sollwerte und
  Stärken der Gelenkantriebe. Neue Verhalten brauchen so keinen neuen Code in der Engine.
- **Sensoren** liefern auf Abruf, was der Körper gerade tut: Lage und Tempo von Körperteilen, Schwerpunkt eines
  Teilbaums, Gelenkwinkel, Kontakt, Zeit, dazu gemittelte Körperachsen und Ebenen durch den Schwerpunkt, etwa für die
  Frage, ob ein Fuß vor dem Schwerpunkt steht. Sensoren und Sollwerte teilen eine Normierung, sodass ein Sensor einen
  Antrieb fast direkt führen kann.
- **Verzögerung gehört dazu.** Vom Sensorwert bis zur Wirkung am Gelenk vergehen einige Schritte, und ein Antrieb kann
  eine eigene Reflexverzögerung haben, bevor er überhaupt reagiert.
- **Drei Arten von Verhalten:** aktive treiben die Gelenke sofort auf ein Ziel, bedingte warten auf einen Auslöser, etwa
  festen Stand, Fallen nach vorn oder nach hinten, in der Luft sein oder aus dem Gleichgewicht sein, und physikalische
  Effekte wirken als Kraft oder Bindung auf Körper statt über Gelenke.

## Antriebe

- **Zielgeschwindigkeit mit Kraftgrenze.** Ein Scharnier bekommt aus dem Fehler seines Winkels eine Zielgeschwindigkeit,
  und der Löser hält sie mit begrenzter Kraft. Die Dämpfung kommt aus dem Löser, nicht aus einem eigenen Glied. In
  Nebenan ist ein Gelenk eine Feder mit Dämpfer und Drehmomentgrenze, F45; ein Umbau bräuchte eine Messung, die dafür
  spricht.
- **Antrieb mit Gedächtnis.** Hält ein Gelenk sein Ziel über mehrere Schritte nicht, wächst seine Kraftgrenze mit dem
  Fehler der letzten Schritte. Es spannt an, wo es nicht nachkommt, auch wenn es um das Ziel pendelt. Das steht bei F66,
  Anspannung und Entspannung.
- **Animation als Sollwert.** Eine Animation oder eine feste Pose wird abgetastet und den Antrieben als Ziel gegeben,
  sanft eingeblendet und mit einer Stärke gewichtet, die Physik bleibt an. Nebenan tut Ähnliches mit den Clips des
  Gangs.

## Verhalten als Liste für eine Kampffigur

Die mitgelieferten Verhalten, nach dem Handbuch, als Liste dessen, was eine Figur können sollte, die getroffen wird und
fällt. Wie sie innen arbeiten, ist nicht bekannt und für Nebenan nicht nötig.

- **Schützen:** Arme vor die Brust, Hände vors Gesicht, Hände vor den Schritt.
- **Gleichgewicht:** auf festem Stand ausbalancieren, auch mit getragenen Gegenständen; aus dem Gleichgewicht
  Schutzschritte machen und mit den Armen rudern.
- **Fallen:** nach vorn mit den Armen abfangen; nach hinten fallend umdrehen und dann abfangen oder den Kopf schützen;
  in der Luft die Landung vorbereiten und in die Hocke gehen; sich zusammenrollen.
- **Greifen und Sehen:** mit einer Hand oder einem Bein nach einem Ziel reichen und den Kopf dorthin drehen, mit
  Vorhersage, wo das Ziel sein wird.
- **Den Gegner fassen:** kommt er schnell in Armlänge, die Arme um ihn legen.
- **Steifigkeit:** den ganzen Körper oder Teile versteifen oder lockern.
- **In der Luft:** mit Armen und Beinen rudern.
- **Springen:** nach vorn springen und landen, oder springen und flach aufkommen.
Gemeinsame Stellgrößen dieser Verhalten, als Idee für die Schnittstelle eines Spiels: wie schnell das Ziel erreicht
wird, mit wie viel Kraft, wie steif der Rest des Körpers seine Haltung hält, ein Zufallsanteil mit festem Startwert,
eine Wartezeit vor dem Beginn und eine Zeit, in der die Kraft ansteigt.

## Lernen der Regler

Die Regler dort wurden mit klassischer Neuroevolution gefunden, nach der Forschung der Gründer: ein kleines Netz gibt
Sollwinkel, die Bewertung läuft in der Simulation, die Besten werden kopiert und verändert. Heute nähme man dafür eher
CMA-ES oder bestärkendes Lernen. Für Nebenan steht das Abstimmen von Zahlen mit einem Optimierer bei F74 und F87.

## Was davon in der Roadmap steht

- F88, Reaktionen als Verhalten aus Daten: Automaten aus Sensoren und Antrieben in der Bibliothek, mit
  Reflexverzögerung, sodass ein Spiel eigene Reaktionen zusammenstellt.
- F66, Anspannung und Entspannung: der Antrieb mit Gedächtnis als Idee.
- F45, Muskel-Skelett-Modell: der Vergleich der Antriebe.
- F74 und F87: Zahlen mit einem Optimierer abstimmen.

## Öffentliche Quellen

- T. Reil, P. Husbands: Evolution of Central Pattern Generators for Bipedal Walking in a Real-Time Physics Environment,
  IEEE Transactions on Evolutionary Computation 6(2), 2002, DOI 10.1109/4235.996015.
- Patent US 7 685 081 B2, Artificial multiped and motion controller therefor, Erfinder Torsten Reil.
- Das Benutzerhandbuch von endorphin, Kapitel Behaviour Library.
