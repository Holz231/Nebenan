#!/bin/bash
# check135.sh variant...: the independent check of round 135 for each built variant (lab101, bal103), see round 135
# in docs/Roadmap.md: the speed sets h47 and h48 with a fixed heading and hf69 and hf71 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py and
# flips135.py on the three pairs of sets h47 to h52, steady runs of 9 s forward facing where it moves, and sideways,
# backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the balance table.
# Then: python3 -I eval135.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h47 h48 hf69 hf71 || exit 1
  python3 -I "$M/rhythm123.py" $v hf69 hf71 > "$W/tmp/rhythm135_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h47 h48 h49 h50 h51 h52 > "$W/tmp/where135_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h47 h48 h49 h50 h51 h52 > "$W/tmp/hops135_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h47 h48 h49 h50 h51 h52 > "$W/tmp/flips135_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h47 h48 h49 h50 h51 h52 > "$W/tmp/lifts135_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.645 4.585; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.215 1.655 1.6 3.375 4.385 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
