#!/bin/bash
# table.sh variant tag hz hold shift: one balance table of 8 928 hits for out/res/<variant>/bal103 into
# out/res/tab/<variant>_<tag>.csv, in four parts at once. The table of the rounds: table.sh <variant> same 60 own 0
. "$(dirname "$0")/common.sh"
mkdir -p "$W/res/tab"; cd "$W"
for p in 0 1 2 3; do HZ=$3 SHIFT=$5 HOLD=$4 PART=$p PARTS=4 res/$1/bal103 res/tab/${1}_$2_p$p.csv > /dev/null 2>&1 & done; wait
head -1 res/tab/${1}_$2_p0.csv > res/tab/${1}_$2.csv
for p in 0 1 2 3; do tail -n +2 res/tab/${1}_$2_p$p.csv >> res/tab/${1}_$2.csv; done
wc -l < res/tab/${1}_$2.csv
