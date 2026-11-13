#!/bin/bash
# cost_count.sh: the instructions the figure update executes for 20 steered figures over 1 200 updates on the two
# builds cost_pairs.sh linked (out/cost/bin/cost_old and cost_new), counted with valgrind's callgrind inside
# nbUpdateFigures, in the modes 3, 2 and 4 of cost110.c. The count is the same in every run, unlike the times of
# cost_pairs.sh: it says whether a build does more work, not how the machine ran it.
. "$(dirname "$0")/common.sh"
mkdir -p "$W/cost/cg"; cd "$W/cost"
for m in 3 2 4; do
  for b in old new; do
    valgrind --tool=callgrind --toggle-collect=nbUpdateFigures --callgrind-out-file=cg/$b$m.out bin/cost_$b $m 20 1200 > cg/$b$m.txt 2>&1
    echo "$m $b $(grep -o 'Collected : [0-9]*' cg/$b$m.txt)"
  done
done
