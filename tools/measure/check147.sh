#!/bin/bash
# check147.sh variant...: the independent check of round 147 for each built variant (lab101, bal103), see round 147 in
# docs/Roadmap.md: the speed sets h110 and h111 with a fixed heading and hf116 and hf117 facing where it moves with
# rhythm123.py, switching sides walking and running with a fixed heading on the other four sets h112 to h115, the eight
# changes at a run with a fixed heading of where129.py, hops131.py, lifts134.py, flips135.py, pitch136.py, lifts137.py
# and lifts138.py on the three pairs of sets h110 to h115, the steady runs of track147.py, and the balance table.
# Then: python3 -I eval147.py old new
. "$(dirname "$0")/common.sh"
SIX="h110 h111 h112 h113 h114 h115"
# steady runs: forward facing where it moves and with a fixed heading at the running speeds of the fresh sets, walking
# forward at their walking speeds, and to the side and backward
TRACK="f:3.5375 f:3.6625 f:4.6525 f:4.6975 x:3.3825 x:3.5125 x:3.6125 x:4.3375 x:4.4125 x:4.4375"
TRACK="$TRACK f:1.2475 f:1.4725 f:1.2775 f:1.4575 s:4.3375 s:3.3825 b:4.3375 b:3.3825"
for v in "$@"; do
  "$M/sweeps.sh" $v h110 h111 hf116 hf117 || exit 1
  ONLY=switchLR16,switchLR30 "$M/sweeps.sh" $v h112 h113 h114 h115 || exit 1
  python3 -I "$M/rhythm123.py" $v hf116 hf117 > "$W/tmp/rhythm147_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run $SIX > "$W/tmp/where147_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run $SIX > "$W/tmp/hops147run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run $SIX > "$W/tmp/flips147_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run $SIX > "$W/tmp/lifts147_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run $SIX > "$W/tmp/pitch147_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run $SIX > "$W/tmp/liftsb147_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run $SIX > "$W/tmp/liftsc147_$v.log" || exit 1
  python3 -I "$M/track147.py" $v $TRACK > "$W/tmp/track147_$v.log" || exit 1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
