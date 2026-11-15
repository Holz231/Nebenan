#!/bin/bash
# check134.sh variant...: the independent check of round 134 for each built variant (lab101, bal103), see round 134
# in docs/Roadmap.md: the speed sets h41 and h42 with a fixed heading and hf65 and hf67 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py and lifts134.py on the
# three pairs of sets h41 to h46, steady runs of 9 s forward facing where it moves, and sideways, backward and
# diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the balance table.
# Then: python3 -I eval134.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h41 h42 hf65 hf67 || exit 1
  python3 -I "$M/rhythm123.py" $v hf65 hf67 > "$W/tmp/rhythm134_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h41 h42 h43 h44 h45 h46 > "$W/tmp/where134_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h41 h42 h43 h44 h45 h46 > "$W/tmp/hops134_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h41 h42 h43 h44 h45 h46 > "$W/tmp/lifts134_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.67 4.565; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.195 1.615 1.6 3.32 4.34 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
