#!/bin/bash
# check144.sh variant...: the independent check of round 144 for each built variant (lab101, bal103), see round 144 in
# docs/Roadmap.md: the speed sets h90 and h91 with a fixed heading and hf96 and hf97 facing where it moves with
# rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py,
# pitch136.py, lifts137.py and lifts138.py on the three pairs of sets h90 to h95, walking from sideways to backward
# of walk144.py on those and the walking sets w96 to w101, steady runs of 9 s forward facing
# where it moves, and sideways, backward and diagonally with a fixed heading (out/tmp/s_<variant>_<name>.csv), and the
# balance table. Then: python3 -I eval144.py old new
. "$(dirname "$0")/common.sh"
for v in "$@"; do
  "$M/sweeps.sh" $v h90 h91 hf96 hf97 || exit 1
  python3 -I "$M/rhythm123.py" $v hf96 hf97 > "$W/tmp/rhythm144_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/where144_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/hops144run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/flips144_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/lifts144_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/pitch144_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/liftsb144_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run h90 h91 h92 h93 h94 h95 > "$W/tmp/liftsc144_$v.log" || exit 1
  python3 -I "$M/walk144.py" $v h90 h91 h92 h93 h94 h95 w96 w97 w98 w99 w100 w101 > "$W/tmp/walk144_$v.log" || exit 1
  for sp in 1.2 1.6 2.2 3.0 3.9 4.5 4.8 4.665 4.725; do
    "$W/res/$v/lab101" "0:0:0,1:0:$sp" "$W/tmp/s_${v}_f$sp.csv" 9 > /dev/null 2>&1
  done
  for sp in 1.3025 1.4025 1.6 3.39 4.3175 4.5 4.8; do
    "$W/res/$v/lab101" "0:0:0:0,1:$sp:0:0" "$W/tmp/s_${v}_s$sp.csv" 9 > /dev/null 2>&1
    "$W/res/$v/lab101" "0:0:0:0,1:0:-$sp:0" "$W/tmp/s_${v}_b$sp.csv" 9 > /dev/null 2>&1
  done
  "$W/res/$v/lab101" "0:0:0:0,1:1.4:1.4:0" "$W/tmp/s_${v}_d2.0.csv" 9 > /dev/null 2>&1
  "$W/res/$v/lab101" "0:0:0:0,1:2.8:2.8:0" "$W/tmp/s_${v}_d4.0.csv" 9 > /dev/null 2>&1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
