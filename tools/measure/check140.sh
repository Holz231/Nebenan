#!/bin/bash
# check140.sh variant...: the independent check of round 140 for each built variant (lab101, bal103), see round 140 in
# docs/Roadmap.md: the speed sets h71 and h72 with a fixed heading and hf85 and hf87 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py, lifts137.py and lifts138.py on the three pairs of sets h71 to h76, hops140.py on h71, h73, h75 and h77
# near the run speeds where a foot lifted again at once, steady runs of 9 s forward facing where it moves, and sideways,
# backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the balance table. Then: python3 -I
# eval140.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h71 h72 hf85 hf87 || exit 1
  python3 -I "$M/rhythm123.py" $v hf85 hf87 > "$W/tmp/rhythm140_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/where140_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/hops140run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/flips140_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/lifts140_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/pitch140_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/liftsb140_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run h71 h72 h73 h74 h75 h76 > "$W/tmp/liftsc140_$v.log" || exit 1
  python3 -I "$M/hops140.py" $v h71 h73 h75 h77 > "$W/tmp/hopsb140_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.615 4.655; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.375 1.415 1.6 3.285 4.395 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
