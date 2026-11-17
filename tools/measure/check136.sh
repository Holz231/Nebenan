#!/bin/bash
# check136.sh variant...: the independent check of round 136 for each built variant (lab101, bal103), see round 136
# in docs/Roadmap.md: the speed sets h53 and h54 with a fixed heading and hf73 and hf75 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py,
# flips135.py and pitch136.py on the three pairs of sets h53 to h58, steady runs of 9 s forward facing where it
# moves, and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the
# balance table.
# Then: python3 -I eval136.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h53 h54 hf73 hf75 || exit 1
  python3 -I "$M/rhythm123.py" $v hf73 hf75 > "$W/tmp/rhythm136_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h53 h54 h55 h56 h57 h58 > "$W/tmp/where136_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h53 h54 h55 h56 h57 h58 > "$W/tmp/hops136_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h53 h54 h55 h56 h57 h58 > "$W/tmp/flips136_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h53 h54 h55 h56 h57 h58 > "$W/tmp/lifts136_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h53 h54 h55 h56 h57 h58 > "$W/tmp/pitch136_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.715 4.535; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.245 1.665 1.6 3.445 4.415 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
