#!/bin/bash
# check129.sh variant...: the independent check of round 129 for each built variant (lab101 and bal103), see round 129
# in docs/Roadmap.md: the speed sets h31 and h32 with a fixed heading and hf45 and hf47 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py on h31 and h32, steady runs of 9 s
# forward facing where it moves, and sideways, backward and diagonally with a fixed heading
# (out/tmp/s_<variant>_<name>.csv), and the balance table. Then: python3 -I eval129.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h31 h32 hf45 hf47 || exit 1
  python3 -I "$M/rhythm123.py" $v hf45 hf47 > "$W/tmp/rhythm129_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h31 h32 > "$W/tmp/where129_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.51 4.66; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.31 1.36 1.6 3.61 4.33 4.5; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
