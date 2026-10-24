# Referenz: Roadmap des Spiels

`Nebenan_Roadmap_V1_7_Active_Ragdoll_Keyframes.pdf` ist die Roadmap des Spiels Nebenan in der Fassung V1.7, 46 Seiten,
mit Ergänzungen vom 9. bis 24. Oktober 2026. Sie beschreibt, was das Spiel werden soll. Gebaut ist davon nur, was das
README und `docs/` als umgesetzt nennen, die Roadmap behauptet selbst keinen Stand.

Seite 6, Kapitel 17.1 bis 17.3, ist die Seite, die am 21. Oktober 2026 für die Fassung V1.4 neu gesetzt wurde, nach dem
Einsturz, wie er in der Demo abgestimmt wurde: Schwellen je Etage aus Bereichen statt fester 40 % und der ganze Rest statt
der 90/10-Regel. V1.7 kam mit der älteren Seite 6, wortgleich mit der ursprünglichen V1.4. Die neue Seite hängt als
Änderung am Ende der Datei, die übrigen 45 Seiten sind Byte für Byte die von V1.7. Die eingebetteten Content Credentials
(C2PA) passen deshalb nicht mehr zur Datei, ein Prüfwerkzeug zeigt sie als nachträglich verändert. Ältere Fassungen liegen
nicht mehr bei, V1.4 steht in der Geschichte des Repositorys.

`Nebenan_Roadmap_V1_7_Active_Ragdoll_Keyframes.txt` ist der Text des PDFs, zum Durchsuchen, erzeugt mit
`pdftotext -layout -enc UTF-8` ohne den letzten Seitenvorschub.

| Kapitel | Inhalt |
| --- | --- |
| 01 | Spielform: physikbasierte Action-Sandbox mit dichten Maps, Sandbox und Missionen, Ziele für 1.0 |
| 02 | Spielersteuerung nach Warframe, Overgrowth und Mirror's Edge: Sprint, Slide, Wall-Run, Klettern |
| 03 | Nahkampf und Schwert mit physischen Reaktionen |
| 04 | Skateboard, Tricks, Bail und Stunts |
| 05 | Hooks und Seilschwingen |
| 06 | Fahrzeuge mit Crash-Physik nach Burnout |
| 07 | Elementarformen mit Angriff und Verteidigung |
| 08 | Wasser mit MLS-MPM auf der GPU |
| 09 | Feuer und Rauch auf einem dünnen Gitter auf der GPU |
| 10 | Graben und Zerstörung des Geländes |
| 11 | Spielmodus Giant gegen Fighter |
| 12 | Spielmodus Worm gegen Fighter |
| 13 | Sandbox, Missionen, Maps und Bedienung |
| 14 | Gemeinsame Ereignisse aller Systeme, Rendering, Release |
| 15 | Reihenfolge M1 bis M8, siehe unten |
| 16 | Was die Roadmap nicht wiederholt |
| 17 | Zerstörung: Restanker, Buckling und Kollaps, wie Häuser einstürzen sollen. 17.1 bis 17.3 gebaut auf `kollaps-1`, die oberste Etage unter dem Dach wie die übrigen seit `kollaps-2`, der Anteil, der stehen bleibt, einstellbar seit `kollaps-3`, seit `kollaps-4` mit eigenen Werten für jede Etage, gezogen aus Bereichen, eigenen Bereichen für die oberste, und der Rest gebrochen wie in 17.1, in seine Zellen. Am 21. Oktober korrigiert auf die abgestimmten Werte: 17 bis 31 % für alle Etagen, 18 bis 25 % für die oberste, der ganze Rest bleibt stehen. Siehe Einsturz im README, die Rampe in der Decke aus 17.4 ist offen |
| 18 | Parallax Occlusion Mapping |
| 19 | Figuren aus dem Skelett heraus erzeugen |
| 20 | Gebäude streamen, Physik aktivieren, vorbereitete Häuser cachen |
| 21 | Vergleich mit NVIDIA Blast und PhysX im Quellcode |
| 22 | Die CPU bleibt die Quelle der Wahrheit, GPU nur für Darstellung |
| 23 | Aufzeichnung der langsamsten Bilder und ihrer Ursachen |
| 24, 25 | Weitere Referenzsysteme: BEPUphysics, Chrono, ReactPhysics3D, Voro++, MuJoCo, PBD, SPlisHSPlasH, Godot, Steam Audio |
| 26 | Hybride Animation und Trefferreaktionen nach Overgrowth. 26.13 bis 26.15, Schutzhaltung, Ausgleichsschritt und Aufstehen mit Sicherheitsprüfung, gebaut auf `ragdoll-2`, das Aufstehen als Spielhilfe ohne Aufstehclips, siehe Balance im README und `docs/Roadmap.md` |
| 27 | Gelenkketten und Motoren auf Box3D. Die Stufen 1 bis 4 der Messmatrix aus 27.14 gebaut auf `ragdoll-1`: Pendel, Drei-Link-Arm, passive und motorisierte Figur, dazu die Federn mit Drehmomentgrenze in Box3D, siehe Figuren im README und `docs/Roadmap.md`. Schwerpunkt und Capture Point aus 27.9 auf `ragdoll-2`, Stufe 6 der Messmatrix, 1, 10 und 50 Figuren in der Stadt unter Beschuss, im Benchmark und in der Demo auf `ragdoll-4`. Die Kontakte zwischen den Knochen aus 27.13 A gemessen auf `ragdoll-5`: 4 % der Zeit der Figuren in Box3D, ein Filter lohnt nicht. Den Löser getrennt nach Gelenken und Kontakten aus 27.14 gemessen auf `ragdoll-6`: 83 % in den Gelenken, und ein waches Gelenk nimmt den Trümmern die Pakete |
| 28 | Übergaben zwischen Steuerung, Animation und Physik. 28.3, der Ausgleichsschritt, und die Abnahmen A, B und E aus 28.5 gebaut auf `ragdoll-2`, die Übergabe aus 28.4 innerhalb der Bibliothek: Die Figur übernimmt erst, wenn ihr Schwerpunkt über den Füßen ruht. Einen Character Controller, an den sie übergeben könnte, gibt es noch nicht |
| 29 | Angriffswunsch statt sofortiger Schlag |
| 30 | Physikbasiertes PvP nach Sifu und Overgrowth |
| 31 | Keyframe- und prozedurale Humanoid-Animation: Clips, Pose-Graph, Anpassung an die Umwelt. Clips mit Ereignissen und Kontakten und dieselbe Prüfung für Clips von Hand und von einer KI aus 31.2 gebaut auf `ragdoll-3`, Pose-Graph und Anpassung an die Umwelt sind offen |
| 32 | Physik-Pose-Controller: Zielskelett und Physikskelett, Gelenkmotoren, Treffer als Ereignis, optionaler Video-zu-Keyframe-Editor, Stufenplan P0 bis P5. P0 bis P3 gebaut auf `ragdoll-1`, Stützschritt und Recovery auf `ragdoll-2`, das Clipformat aus 32.6 als JSON, der Editor aus 32.5 und P4 mit Referenzbildern, Keypoints, Freigabe und Sperre und Punkt 4 der Abnahme aus 32.8 auf `ragdoll-3`, das Messen aus 32.8 mit Stürzen, Abbrüchen, Motoren an der Grenze und den Szenen mit 1, 10 und 50 Figuren auf `ragdoll-4`. Gizmos, ein Assistent im Editor und das Binärformat sind offen |
| 33 | Active-Ragdoll-Controller und Keyframes: wie aus wenigen Keyframes eine Bewegung wird, die Reihenfolge für den Bau und die Abnahme. Die Schritte 1 bis 4 aus 33.4 gebaut auf `ragdoll-1`, Schritt 5, Balance, Stützschritt, Knockdown und Recovery, auf `ragdoll-2`, Schritt 6, Keyframe-Format, Video-Editor, Vorschläge und Freigabe, auf `ragdoll-3`, mit der Rückkopplung aus 33.2 E: Simulationen werden als eigener Clip gebacken. Siehe `docs/Roadmap.md` |

Die Reihenfolge aus Kapitel 15:

| Meilenstein | Inhalt |
| --- | --- |
| M1 | Spielersteuerung mit Sprung, Slide, Wall-Run und Hook in einer Map neben der Zerstörung |
| M2 | Kicks, Trefferreaktionen, Ragdoll und Bail, kleines Skate-Setup |
| M3 | Ein fahrbares Fahrzeug mit Crash und Schaden |
| M4 | Gelände: Graben, Krater, Tunnel samt Kollision und Speichern |
| M5 | Wasser und Feuer auf der GPU, erst getrennt, dann gekoppelt |
| M6 | Elemente mit je einer Fähigkeit für Angriff und Verteidigung |
| M7 | Giant und Worm in je einer Testarena |
| M8 | Inhalte, Sandbox, Missionen, Render-Optimierung, Audio, Veröffentlichung |
