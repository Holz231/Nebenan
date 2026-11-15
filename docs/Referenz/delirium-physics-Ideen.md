# Referenz: Ideen aus der Analyse von Delirium Physics

Diese Datei fasst zusammen, was sich aus der Analyse des Nutzers von [Delirium
Physics](https://github.com/RyanCraighead/delirium-physics) für Nebenan als Idee mitnehmen lässt. Die Analyse selbst
liegt daneben, [delirium-physics-Analyse.md](delirium-physics-Analyse.md), vom Nutzer erarbeitet und am 15. November
2026 auf seine Entscheidung aufgenommen; darin ist nur der Benutzername in zwei Pfaden ersetzt.

Delirium Physics ist nach eigener Angabe ein zurückentwickelter Nachbau von Euphoria, wie es in GTA IV läuft, unter
GPL-3.0-only, und die Rechte daran sind ungeklärt. Für Nebenan gilt daraus deshalb nur die Idee, kein Code, keine Zahl,
keine Formel im Wortlaut, kein Dateiformat und kein Aufbau von Modulen. Für die Arbeit an Nebenan wird sein Quelltext
nicht gelesen; was hier steht, stammt aus der Analyse des Nutzers und aus der Beschreibung des Projekts. Was gebaut
wird, wird in Nebenan selbst gemessen und begründet, wie alles andere. Die Abstimmwerte fehlen in dem Projekt ohnehin,
es liest sie aus einer eigenen Kopie des Spiels.

## Aufbau

- **Körper und Muskeln getrennt von den Verhalten.** Die Physik bewegt den Körper, die Verhalten stellen nur, wie steif
  jeder Muskel ist und wohin er zielt. So ist Nebenan auch gebaut: Die Figur setzt Ziele und Stärken der Gelenke, Box3D
  rechnet.
- **Verhalten als Nachrichten.** Ein Verhalten hat einen Namen und Parameter, ein Spiel startet es, stellt es nach und
  stoppt es. Eine Maske sagt, für welchen Bereich des Körpers es gilt, und was außerhalb der Maske liegt, ist dabei
  gesperrt: Beine balancieren, während ein Arm zeigt und der Kopf schaut.
- **Feste Reihenfolge statt Vorrang.** Schreiben zwei Verhalten auf denselben Muskel, gilt das spätere in einer festen
  Reihenfolge. Das ist einfach und bleibt deterministisch.
- **Übergaben und Rückmeldungen.** Ein Verhalten beendet andere oder startet das nächste, übergeordnete schalten ihre
  Teile selbst, und nach jedem Schritt steht in einer Liste, was geschehen ist, etwa dass die Balance aufgegeben hat.
  Die Logik des Spiels liest diese Liste.
- **Aufgaben über den Verhalten.** Eine Schicht übersetzt Ereignisse des Spiels in Folgen von Verhalten: Ein Treffer ist
  der Impuls auf das getroffene Teil und getrennt davon die Reaktion, mit einem Satz Einstellungen je Art des Treffers.

## Antriebe

- **Stärke als Eigenfrequenz, Dämpfung als Dämpfungsgrad.** So stellt Box3D seine Federn schon, und Nebenan mit ihm.
- **Zusatzträgheit im Gelenk** für schlaffe Glieder, die träger schwingen. Nebenan hat sie seit Runde 29, F48.
- **Schwerkraft gegenhalten.** Im Stand wird entlang der Kette zum Standfuß aufsummiert, was jedes Gelenk trägt, und als
  Moment vorgegeben, sodass die Federn nur den Rest ausgleichen. Das ist der kleinste Teil der Vorsteuerung aus inverser
  Dynamik, F73, und könnte ihre erste Stufe sein.
- **Weiche Grenze vor der harten.** Jenseits einer Schwelle zieht eine Feder mit Dämpfer zurück, bevor die harte Grenze
  greift. Passive Momente der Bänder nahe den Grenzen stehen bei F45.

## Verhalten als Liste für eine Kampffigur

- **Balance.** Den Schwerpunkt mit seiner Geschwindigkeit vorausschätzen, wie beim Capture Point, den Nebenan schon
  nutzt; liegt er außerhalb der Füße, tritt der Fuß, der weiter von seinem Ziel weg ist. Wird der Abstand zu groß,
  gemessen an der Höhe des Schwerpunkts, gibt die Balance auf, meldet das und übergibt an eine Sturzreaktion, statt bis
  zuletzt zu kämpfen.
- **Abfangen.** Die Arme werden so steif, wie die Figur Bewegungsenergie hat, jede Hand geht dorthin, wo ein Strahl in
  Fallrichtung auftrifft, und beim Kontakt geben die Arme nach, statt zu stemmen.
- **Hoher Sturz.** Rudern im Flug, die Zeit bis zum Boden aus Höhe und Fallgeschwindigkeit schätzen, das Becken zur
  Landung ausrichten, und am Boden nach der Lage wählen: abrollen, balancieren oder abfangen.
- **Treffer.** Eine Folge kurzer Zustände: getroffenes Teil und Seite auswerten, kurz versteifen und wieder lockern,
  eine Drehung des Rumpfs vom Treffer weg, eine Hand zur getroffenen Stelle, der Kopf zur Stelle und zum Gegner. Für
  Nebenan zählen davon Versteifen, Lockern, Drehung und Kopf; F32, F64 und F66 haben Teile davon.
- **Zucken und Decken.** Zur Bedrohung hin oder von ihr weg, mit einer Hysterese, damit die Richtung nicht flattert, und
  mit etwas Rauschen, damit es nicht jedes Mal gleich aussieht. Rauschen müsste in Nebenan deterministisch sein.
- **Weiteres:** abstützen gegen etwas, das kommt, greifen, über eine niedrige Wand fallen, eine Treppe hinunterrollen,
  einrollen, sich winden. Als Liste dessen, was eine Figur können könnte; Treppe und Schutt kommen nach dem Gang.

## Lernen

Das Aufstehen dort ist gelernt, aber über den Verhalten statt an ihrer Stelle: Ein kleines Netz wählt alle paar Schritte
ein Verhalten und stellt Ziele und Steifigkeiten nach. Die Idee für Nebenan: Ein Optimierer oder ein gelerntes Netz
stellt, was die Bibliothek schon kann, und ersetzt es nicht. Das passt zu F74 und F87, und es müsste in Nebenan selbst
gelernt werden, weil das Ergebnis mit den Kontakten von Box3D gelten muss.

## Was nicht passt

- **Der Löser.** Dort sind die Gelenke in reduzierten Koordinaten gerechnet, Kontakte und Grenzen mit einem eigenen
  Verfahren. Nebenan bleibt bei Box3D.
- **Das Gehirn in fremder Physik.** Die Schnittstelle, die nur Kräfte und Momente je Teil liefert, braucht das Projekt
  selbst und damit GPL-3 und eine Kopie des Spiels beim Spieler.
- **Bitgleichheit** ist dort kein Ziel, in Nebenan eine feste Regel.

## Was davon in der Roadmap steht

- F88, Reaktionen als Verhalten aus Daten: Nachrichten, Masken, Reihenfolge, Übergaben, Rückmeldungen, Aufgaben.
- F73, Vorsteuerung aus inverser Dynamik: die Schwerkraft gegenhalten als mögliche erste Stufe.
- F74 und F87, Zahlen mit einem Optimierer abstimmen: lernen über dem, was die Bibliothek kann.
- Vierte Durchsicht fremder Projekte in der [Roadmap](../Roadmap.md), mit der Grenze.
