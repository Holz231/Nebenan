#!/bin/bash
# check122.sh variant...: the independent check of round 122 for each built variant (lab101 and bal103), see round 122
# in docs/Roadmap.md: the speed sets h21 and h22 with a fixed heading and hf25 and hf27 facing where it moves, steady
# with a fixed heading for 8 s to the side and backward (out/tmp/h_<variant>_<name>.csv), running forward for 9 s at
# 3.0 and 4.8 m/s (out/tmp/k_<variant>_<speed>.csv), and the balance table. Then: python3 -I eval122.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h21 h22 hf25 hf27 || exit 1
  for sp in 1.28 1.38 3.72 4.28; do "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/h_${v}_side$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 1.28 3.72; do "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/h_${v}_back$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 3.0 4.8; do "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/k_${v}_$sp.csv" 9 > /dev/null 2>&1; done
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
