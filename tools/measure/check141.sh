#!/bin/bash
# check141.sh variant...: the independent check of round 141 for each built variant (lab101, bal103), see round 141 in
# docs/Roadmap.md: the speed sets h78 and h79 with a fixed heading and hf89 and hf91 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py, lifts137.py and lifts138.py on the three pairs of sets h78 to h83, steady runs of 9 s forward facing
# where it moves, and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the
# balance table. Then: python3 -I eval141.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h78 h79 hf89 hf91 || exit 1
  python3 -I "$M/rhythm123.py" $v hf89 hf91 > "$W/tmp/rhythm141_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/where141_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/hops141run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/flips141_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/lifts141_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/pitch141_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/liftsb141_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run h78 h79 h80 h81 h82 h83 > "$W/tmp/liftsc141_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.625 4.685; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.3125 1.4125 1.6 3.365 4.315 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
