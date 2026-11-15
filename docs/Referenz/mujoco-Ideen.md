# Referenz: Ideen aus der Analyse von MuJoCo

Diese Datei fasst zusammen, was sich aus der Analyse des Nutzers von [MuJoCo](https://github.com/google-deepmind/mujoco)
für Nebenan als Idee mitnehmen lässt. Die Analyse selbst liegt daneben, [mujoco-Analyse.md](mujoco-Analyse.md), vom
Nutzer erarbeitet und am 15. November 2026 auf seine Entscheidung aufgenommen; darin ist nur der Benutzername in vier
Pfaden ersetzt. Sie vergleicht MuJoCo auch mit endorphin und Delirium Physics, siehe
[endorphin-Ideen.md](endorphin-Ideen.md) und [delirium-physics-Ideen.md](delirium-physics-Ideen.md).

MuJoCo steht unter Apache 2.0, seine Dokumentation unter CC BY 4.0: Code und Zahlen daraus dürften mit Hinweis verwendet
werden. Für Nebenan bleibt es trotzdem bei der Regel für alle fremden Projekte: nur Ideen, keine Zahlen. Was gebaut
wird, wird in Nebenan selbst gemessen und begründet. Diese Zusammenfassung nennt deshalb keine Werte.

## Was die Analyse folgert und was für Nebenan gilt

Die Analyse hält MuJoCo als Physik für ein Spiel für realistisch: freie Lizenz, Gelenke in inneren Koordinaten, die
nicht auseinanderfallen, weiche und stabile Kontakte, und das Verhalten der Figur baut man selbst darüber. Für Nebenan
gilt das nicht ohne Weiteres, und entschieden ist damit nichts:

- **Bitgleichheit.** MuJoCo sagt exakte Reproduzierbarkeit nur innerhalb einer Version auf derselben Architektur zu.
  Nebenan verlangt bitgleiche Ergebnisse mit jeder Zahl von Threads und auf jeder Plattform.
- **Trümmer.** Nebenan ist um Box3D gebaut, mit Häusern, die in viele Teile zerfallen. MuJoCo ist für Figuren und
  Roboter mit Gelenken gebaut. Es zu nehmen hieße, die Bibliothek zu ersetzen.
- **Stand.** Den Löser von Box3D ersetzt MuJoCo deshalb nicht, siehe die vierte Durchsicht fremder Projekte in der
  [Roadmap](../Roadmap.md). Wer das ändern will, entscheidet es als eigenen großen Umbau auf einer eigenen Branch.

## Physik

- **Zusatzträgheit im Gelenk** macht steife Antriebe stabil. Nebenan hat sie seit Runde 29, F48.
- **Weiche Zwangsbedingungen** mit einer Zeitkonstante und einem Dämpfungsgrad statt harter Bedingungen. So stellt Box3D
  seine Kontakte und Gelenke auch.
- **Dämpfung implizit rechnen,** damit auch starke Dämpfung in einem Antrieb stabil bleibt. Wo ein Antrieb von Nebenan
  bei hoher Stärke schwingt, ist das die erste Frage.
- **Schlafen** nur für Figuren ohne Antrieb: Eine aktive Figur bleibt wach, eine liegende, ausgeschaltete darf schlafen.

## Antriebe

- **Ein Baukasten statt eines Antriebs:** Moment, Winkel mit Dämpfung, Geschwindigkeit, ein Regler mit Gedächtnis und
  Begrenzung seines Aufziehens, und für Kugelgelenke ein Antrieb auf die Orientierung mit begrenzter Norm des Moments.
  Nebenan treibt auf den Winkel; der Vergleich steht bei F45.
- **Dämpfung relativ zur kritischen,** gerechnet aus der Trägheit, die am Gelenk hängt, statt als feste Zahl. Ändert
  sich die Stärke im Lauf, muss die Dämpfung mit.
- **Aktivierung mit Trägheit:** Die Kraft folgt dem Befehl verzögert, schneller beim Anspannen als beim Lösen. Nebenan
  hat das als Stufe 1 von F45, als Schalter.
- **Schwerkraft ausgleichen,** auch so, dass der Ausgleich gegen die Grenze des Antriebs zählt. Steht als mögliche erste
  Stufe bei F73.
- **Größen für das Spiel normiert, innen physikalisch.** Ein Spiel stellt Tempo, Kraft und Steifigkeit zwischen 0 und 1,
  die Bibliothek rechnet sie in Eigenfrequenz, Dämpfungsgrad und Grenze des Moments um. Passt zu F66, der Spannung je
  Bereich.

## Sensoren und Verzögerung

- **Wenige Größen reichen für die Balance:** Schwerpunkt und seine Geschwindigkeit, Lage und Aufrichtung, Kontakt der
  Sohlen. Dieselben nutzt die Balance von Nebenan.
- **Verzögerung als Eigenschaft eines Sensors:** eine Latenz und eine Abtastrate je Sensor. Für die Reflexverzögerung
  von F88 wäre das die Stelle: Das Verhalten sieht die Welt ein bis zwei Schritte später.

## Balance rechnen statt abstimmen

Im Beispiel der Dokumentation steht eine Figur auf einem Bein mit einem Regler, der gerechnet ist: Arbeitspunkt aus der
inversen Dynamik, das Verhalten um ihn herum durch kleine Auslenkungen gemessen, Kosten dafür, dass der Schwerpunkt den
Fuß verlässt, und daraus die Verstärkungen. Die Idee für Nebenan: Zahlen der Balance, die heute Variante für Variante
gesucht werden, aus einem gemessenen linearen Modell der eigenen Figur rechnen, außerhalb der Bibliothek und in Nebenan
selbst gemessen. Notiert bei F74.

## Was davon in der Roadmap steht

- Vierte Durchsicht fremder Projekte: MuJoCo ersetzt den Löser von Box3D nicht.
- F48, Zusatzträgheit, gebaut; F83, Körperform nach seinem Menschen, gemessen.
- F45 und F73: Muskel mit Trägheit, Vorsteuerung und Schwerkraft.
- F66: Größen für das Spiel normiert. F74: Balance rechnen. F88: Verzögerung je Sensor.
