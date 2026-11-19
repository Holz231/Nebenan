#!/bin/bash
# check145.sh variant...: the independent check of round 145 for each built variant (lab101, bal103), see round 145 in
# docs/Roadmap.md: the speed sets h102 and h103 with a fixed heading and hf108 and hf109 facing where it moves with
# rhythm123.py, switching sides walking and running with a fixed heading on the other four sets h104 to h107, the eight
# changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py, pitch136.py, lifts137.py
# and lifts138.py on the three pairs of sets h102 to h107, steady runs of 9 s forward facing where it moves, and
# sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the balance table.
# Then: python3 -I eval145.py old new
. "$(dirname "$0")/common.sh"
SIX="h102 h103 h104 h105 h106 h107"
for v in "$@"; do
  "$M/sweeps.sh" $v h102 h103 hf108 hf109 || exit 1
  ONLY=switchLR16,switchLR30 "$M/sweeps.sh" $v h104 h105 h106 h107 || exit 1
  python3 -I "$M/rhythm123.py" $v hf108 hf109 > "$W/tmp/rhythm145_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run $SIX > "$W/tmp/where145_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run $SIX > "$W/tmp/hops145run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run $SIX > "$W/tmp/flips145_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run $SIX > "$W/tmp/lifts145_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run $SIX > "$W/tmp/pitch145_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run $SIX > "$W/tmp/liftsb145_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run $SIX > "$W/tmp/liftsc145_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.6475 4.7125; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.2825 1.4375 1.6 3.4125 4.3125 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
