#!/bin/bash
# check143.sh variant...: the independent check of round 143 for each built variant (lab101, bal103), see round 143 in
# docs/Roadmap.md: the speed sets h84 and h85 with a fixed heading and hf92 and hf93 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py, lifts137.py and lifts138.py on the three pairs of sets h84 to h89, steady runs of 9 s forward facing
# where it moves, and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the
# balance table. Then: python3 -I eval143.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h84 h85 hf92 hf93 || exit 1
  python3 -I "$M/rhythm123.py" $v hf92 hf93 > "$W/tmp/rhythm143_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/where143_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/hops143run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/flips143_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/lifts143_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/pitch143_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/liftsb143_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run h84 h85 h86 h87 h88 h89 > "$W/tmp/liftsc143_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.635 4.705; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.3075 1.4075 1.6 3.385 4.325 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
