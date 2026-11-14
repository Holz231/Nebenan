#!/bin/bash
# check128.sh variant...: the independent check of round 128 for each built variant (lab101 and bal103), see round 128
# in docs/Roadmap.md: the speed sets h29 and h30 with a fixed heading and hf41 and hf43 facing where it moves with
# rhythm123.py and turn128.py, steady with a fixed heading for 8 s to the side and backward
# (out/tmp/h_<variant>_<name>.csv), running forward for 9 s at 3.0 and 4.8 m/s (out/tmp/k_<variant>_<speed>.csv), and
# the balance table. Then: python3 -I eval128.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h29 h30 hf41 hf43 || exit 1
  python3 -I "$M/rhythm123.py" $v hf41 hf43 > "$W/tmp/rhythm128_$v.log" || exit 1
  python3 -I "$M/turn128.py" $v hf41 hf43 > "$W/tmp/turn128_$v.log" || exit 1
  for sp in 1.33 1.43 3.66 4.46; do "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/h_${v}_side$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 1.33 3.66; do "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/h_${v}_back$sp.csv" 8 > /dev/null 2>&1; done
  for sp in 3.0 4.8; do "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/k_${v}_$sp.csv" 9 > /dev/null 2>&1; done
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
