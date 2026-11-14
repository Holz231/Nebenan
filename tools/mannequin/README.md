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
| `make_parts.py` | schneidet das Modell in starre Teile, eines je Knochen, und schreibt `demo/mannequin_parts.h` |

Das Modell besteht aus getrennten Schalen: Oberarm, Unterarm, Hand, Fingerglieder, Oberschenkel, Wade, Fuß, Becken und
Bauch sind je eine, nur Kopf und Brust hängen zusammen. Jede Schale geht ganz an den Knochen, an dem sie am meisten
hängt, die Schale von Kopf und Brust wird am Kopf geschnitten. Arme und Beine werden so angepasst, dass ihre beiden
Gelenke auf den Gelenken der Figur liegen, Rumpf und Kopf behalten ihre Form und werden auf 1,8 m verkleinert, der Fuß
wird so lang und so hoch wie der Fuß der Physik, damit die Sohle stimmt. Die Finger sind zur Faust gekrümmt und hängen
starr an der Hand, die kleinen Gelenke der Finger, die Hälfte aller Dreiecke, sind weggelassen. Es bleiben 6 352
Dreiecke je Figur.

Neu erzeugen, aus dem Wurzelverzeichnis, mit einer gebauten Bibliothek:

cc -o dump_bones tools/mannequin/dump_bones.c -Iinclude -Iextern/box3d/include <libnebenan.a> <libbox3d.a> -lm -lpthread
./dump_bones > tools/mannequin/bones.json python3 -I tools/mannequin/make_parts.py

Ändern sich die Maße des Menschen der Bibliothek, sind beide Schritte nötig, sonst passen die Teile nicht mehr zu den
Gelenken.
