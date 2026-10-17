---
name: library-first
description: Checklist for every change to Nebenan. Use before writing or changing code in demo/ or benchmark/, before every new optimization round, and before calling a round done. It keeps every optimization and every behavior a game would need in the library instead of the demo.
---

# Zuerst die Bibliothek

In `physik-16` lief die Physik parallel zum Zeichnen nur in der Demo. Ein Spiel mit Nebenan hätte sie nachbauen müssen.
Das darf nicht wieder passieren. Die Regeln stehen in `CLAUDE.md`, das hier ist die Prüfung dazu.

## Vor der Arbeit

1. Was soll besser werden, und wer hat etwas davon? Hat ein Spiel mit eigener Engine etwas davon, kommt es in
   `src/`, `include/nebenan/` oder `extern/box3d/`.
2. Gibt es schon eine Stelle in Demo oder Benchmark, die dasselbe tut? Dann wandert sie mit in die Bibliothek.

## Während der Arbeit

- Die Demo bekommt nur Aufrufe der Bibliothek dazu, und dazu Fenster, Zeichnen, Kamera, Eingabe, Menü, Szenen und
  Messwerkzeuge.
- Threads, Zeitmessung für Entscheidungen, Laden und Entladen, Budgets, die Wahl der Threads: Das gehört in die
  Bibliothek, auch wenn es zuerst nur in der Demo gebraucht wird.

## Vor dem Abschluss

1. `git diff --stat` der Runde ansehen. Ändert sich `demo/` oder `benchmark/` mit mehr als Aufrufen, Zeichnen oder
   Menü, die Frage aus „Vor der Arbeit“ noch einmal stellen.
2. Steht dieselbe Logik in Demo und Benchmark? Dann ist die Runde nicht fertig.
3. Im README sagen, was ein Spiel aufruft, nicht, was die Demo intern macht.
