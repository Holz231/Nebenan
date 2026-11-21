#!/bin/bash
# check150.sh variant...: the independent check of round 150 for each built variant (lab101, bal103), see round 150 in
# docs/Roadmap.md: the twelve speed sets h120 to h131 with a fixed heading and their starts from standing of
# starts150.py, hf132 and hf133 facing where it moves with rhythm123.py, the eight changes at a run with a fixed heading
# of where129.py, hops131.py, lifts134.py, flips135.py, pitch136.py, lifts137.py and lifts138.py on h120 to h125, the
# steady runs of track147.py, and the balance table. Then: python3 -I eval150.py old new
. "$(dirname "$0")/common.sh"
TWELVE="h120 h121 h122 h123 h124 h125 h126 h127 h128 h129 h130 h131"
SIX="h120 h121 h122 h123 h124 h125"
# steady runs: forward facing where it moves walking and running, forward with a fixed heading, to the side and backward
TRACK="f:1.1975 f:3.7175 f:4.5375 x:4.2875 s:2.4375 s:4.1675 b:3.8425 b:4.6125"
for v in "$@"; do
  "$M/sweeps.sh" $v $TWELVE hf132 hf133 || exit 1
  python3 -I "$M/starts150.py" $v $TWELVE > "$W/tmp/starts150_$v.log" || exit 1
  python3 -I "$M/rhythm123.py" $v hf132 hf133 > "$W/tmp/rhythm150_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run $SIX > "$W/tmp/where150_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run $SIX > "$W/tmp/hops150run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run $SIX > "$W/tmp/flips150_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run $SIX > "$W/tmp/lifts150_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run $SIX > "$W/tmp/pitch150_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run $SIX > "$W/tmp/liftsb150_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run $SIX > "$W/tmp/liftsc150_$v.log" || exit 1
  python3 -I "$M/track147.py" $v $TRACK > "$W/tmp/track150_$v.log" || exit 1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
