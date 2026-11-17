#!/bin/bash
# check137.sh variant...: the independent check of round 137 for each built variant (lab101, bal103), see round 137 in
# docs/Roadmap.md: the speed sets h59 and h60 with a fixed heading and hf77 and hf79 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py and lifts137.py on the three pairs of sets h59 to h64, steady runs of 9 s forward facing where it moves,
# and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the balance table.
# Then: python3 -I eval137.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h59 h60 hf77 hf79 || exit 1
  python3 -I "$M/rhythm123.py" $v hf77 hf79 > "$W/tmp/rhythm137_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/where137_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/hops137_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/flips137_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/lifts137_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/pitch137_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h59 h60 h61 h62 h63 h64 > "$W/tmp/liftsb137_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.735 4.555; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.225 1.485 1.6 3.495 4.365 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
