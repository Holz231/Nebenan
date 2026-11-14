# Gliederpuppe der Demo

Die Demo zeichnet ihre Figuren seit `active-ragdoll-125` als Gliederpuppe nach einem Modell von Quaternius, exportiert
aus ActionForge, gemeinfrei unter CC0 1.0, siehe [LICENSE.txt](LICENSE.txt). Dieses Verzeichnis macht aus dem Modell die
Daten, die die Demo einbindet. Die Bibliothek braucht nichts davon: Ein Spiel hängt sein eigenes Modell an die Körper
der Knochen, deren Lage es je Bild aus der Bibliothek liest.

| Datei | |
| --- | --- |
| `actionforge_mannequin_rest_pose.glb` | das Modell in seiner Ruhepose: zwei Meshes, Körper und Gelenke, 13 744 Dreiecke, 71 Knochen, 1,83 m hoch |
| `glb.py` | liest die Datei, ohne etwas aus ihr auszuführen |
| `dump_bones.c` | schreibt die Knochen des Menschen der Bibliothek bei 1,8 m als `bones.json`: Ruhepose und Formen |
| `bones.json` | diese Knochen, Stand `active-ragdoll-125` |
| `make_skin.py` | passt das Modell an die Figur an, bindet jeden Punkt an ihre Knochen und schreibt `demo/mannequin_skin.h` |

Das Modell besteht aus getrennten Schalen: Oberarm, Unterarm, Hand, Fingerglieder, Oberschenkel, Wade, Fuß, Becken und
Bauch sind je eine, Kopf und Brust hängen zusammen, dazwischen liegen die violetten Schalen der Gelenke. Jede Schale
wird mit dem Knochen angepasst, an dem sie am meisten hängt: Arme und Beine so, dass ihre beiden Gelenke auf den
Gelenken der Figur liegen, Rumpf und Kopf behalten ihre Form und werden auf 1,8 m verkleinert, der Fuß wird so lang und
so hoch wie der Fuß der Physik, damit die Sohle stimmt. Die Finger sind zur Faust gekrümmt, die kleinen Gelenke der
Finger, die Hälfte aller Dreiecke, sind weggelassen. Es bleiben 3 870 Punkte und 6 352 Dreiecke je Figur.

Die Schultern der Figur liegen über denselben Hüften 6,4 cm weiter vorn, 1,6 cm weiter außen und 1,6 cm tiefer als die
des Modells. Deshalb liegt die Brust 3 cm weiter vorn, mittig auf der Brust der Physik, der Hals führt von ihr zum Kopf,
und das Schlüsselbein dreht und streckt sich um sein Gelenk, bis das Schultergelenk des Modells auf dem der Figur liegt.
Was an diesen Knochen hängt, folgt ihnen mit seinem Gewicht: die Pfanne der Brust und der obere Rand des Oberarms.

Das Ergebnis ist ein Mesh in der Ruhepose der Figur. Jeder Punkt hat eine Bindung aus seinen Gewichten im Modell: einen
Knochen der Figur, oder eine Lage zwischen zwei Knochen, von denen einer über dem anderen liegt, in Vierteln, mit dem
Gelenk dazwischen als Drehpunkt. Die Demo setzt je Bindung in jedem Bild eine Lage aus den Lagen der Körper, siehe
`PoseFigureSkin` in `demo/demo.cpp`: So biegen sich die Schalen an den Gelenken wie die Haut des Modells, ohne einen
Shader, der mischt. Punkte verschiedener Schalen, die aufeinanderliegen, haben dieselbe Bindung, das Werkzeug zählt die
Ausnahmen und meldet null.

Neu erzeugen, aus dem Wurzelverzeichnis, mit einer gebauten Bibliothek:

```
cc -o dump_bones tools/mannequin/dump_bones.c -Iinclude -Iextern/box3d/include <libnebenan.a> <libbox3d.a> -lm -lpthread
./dump_bones > tools/mannequin/bones.json
python3 -I tools/mannequin/make_skin.py
```

Ändern sich die Maße des Menschen der Bibliothek, sind beide Schritte nötig, sonst passt das Mesh nicht mehr zu den
Gelenken.
