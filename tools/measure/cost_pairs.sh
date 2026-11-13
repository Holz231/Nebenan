#!/bin/bash
# cost_pairs.sh old_src old_build new_src new_build [out]: the cost of 20 steered figures per update on two cmake
# builds (Release, each with its source tree) in turn, PAIRS pairs per mode (default 36), the order changing every pair:
# mode 2, to the side facing ahead, direction and walk or run changing every 2 s, mode 3, walking forward and running
# backward facing ahead, mode 4, walking steadily to the side at 1.6 m/s, and mode 0, standing. Nothing else may run on
# the machine. Then: python3 -I cost.py [out]
. "$(dirname "$0")/common.sh"
OUT=${5:-pairs.txt}; PAIRS=${PAIRS:-36}
mkdir -p "$W/cost/bin"; cd "$W/cost"
build() { # label src build
  gcc -O2 -DNDEBUG -std=gnu17 -o bin/cost_$1 "$M/cost110.c" -I"$2/include" -I"$2/src" -isystem "$2/extern/box3d/include" \
    "$3/src/libnebenan.a" "$3/extern/box3d/src/libbox3d.a" -lm -lpthread || exit 1
}
build old "$1" "$2"; build new "$3" "$4"
rm -f $OUT
for m in 2 3 4 0; do
  for r in $(seq 1 $PAIRS); do
    if [ $((r % 2)) = 1 ]; then order="old new"; else order="new old"; fi
    for b in $order; do
      echo "$m $r $b $(bin/cost_$b $m 20)" >> $OUT
    done
  done
done
echo finished >> $OUT
