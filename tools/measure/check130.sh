#!/bin/bash
# check130.sh variant...: the independent check of round 130 for each built variant (lab101 and bal103), see round 130
# in docs/Roadmap.md: the speed sets h33 and h34 with a fixed heading and hf49 and hf51 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py on h33 and h34, steady runs of 9 s
# forward facing where it moves, and sideways, backward and diagonally with a fixed heading
# (out/tmp/s_<variant>_<name>.csv), and the balance table. Then: python3 -I eval130.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h33 h34 hf49 hf51 || exit 1
  python3 -I "$M/rhythm123.py" $v hf49 hf51 > "$W/tmp/rhythm130_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h33 h34 > "$W/tmp/where130_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.59 4.71; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.13 1.54 1.6 3.59 4.41 4.5; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
