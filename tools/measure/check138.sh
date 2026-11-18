#!/bin/bash
# check138.sh variant...: the independent check of round 138 for each built variant (lab101, bal103), see round 138 in
# docs/Roadmap.md: the speed sets h65 and h66 with a fixed heading and hf81 and hf83 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py, lifts137.py and lifts138.py on the three pairs of sets h65 to h70, steady runs of 9 s forward facing
# where it moves, and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the
# balance table. Then: python3 -I eval138.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h65 h66 hf81 hf83 || exit 1
  python3 -I "$M/rhythm123.py" $v hf81 hf83 > "$W/tmp/rhythm138_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/where138_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/hops138_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/flips138_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/lifts138_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/pitch138_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/liftsb138_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run h65 h66 h67 h68 h69 h70 > "$W/tmp/liftsc138_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.695 4.525; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.295 1.425 1.6 3.435 4.375 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
