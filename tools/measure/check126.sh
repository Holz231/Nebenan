#!/bin/bash
# check126.sh variant...: the independent check of round 126 for each built variant (lab101 and bal103), see round 126
# in docs/Roadmap.md: four balance tables with the hits at fresh heights, the speed sets h27 and h28 with a fixed heading
# and hf37 and hf39 facing where it moves with rhythm123.py, steady with a fixed heading for 8 s to the side and backward
# (out/tmp/h_<variant>_<name>.csv) and running forward for 9 s at 3.0 and 4.8 m/s (out/tmp/k_<variant>_<speed>.csv).
# Then: python3 -I eval126.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  [ -f "$W/res/tab/${v}_up.csv" ] || "$M/table.sh" $v up 60 own 0.037 > /dev/null
  [ -f "$W/res/tab/${v}_down.csv" ] || "$M/table.sh" $v down 60 own -0.053 > /dev/null
  [ -f "$W/res/tab/${v}_slow.csv" ] || "$M/table.sh" $v slow 30 own 0.037 > /dev/null
  [ -f "$W/res/tab/${v}_free.csv" ] || "$M/table.sh" $v free 60 none -0.053 > /dev/null
  "$M/sweeps.sh" $v h27 h28 hf37 hf39 || exit 1
  python3 -I "$M/rhythm123.py" $v hf37 hf39 > "$W/tmp/rhythm126_$v.log" || exit 1
  for sp in 1.27 1.41 3.57 4.53; do "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/h_${v}_side$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 1.27 3.57; do "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/h_${v}_back$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 3.0 4.8; do "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/k_${v}_$sp.csv" 9 > /dev/null 2>&1; done
  echo "checked $v"
done
