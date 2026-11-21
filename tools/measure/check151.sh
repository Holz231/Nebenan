#!/bin/bash
# check151.sh variant...: the independent check of round 151 for each built variant (lab101, bal103), see round 151 in
# docs/Roadmap.md: the standing shin from running backward to running forward of stand151.py and where129.py and the
# starts from standing of starts150.py on the twelve speed sets h134 to h145, the sets h134 and h135 with a fixed
# heading and hf146 and hf147 facing where it moves with rhythm123.py, the eight changes at a run with a fixed heading
# of where129.py, hops131.py, lifts134.py, flips135.py, pitch136.py, lifts137.py and lifts138.py on h134 to h139, the
# steady runs of track147.py, and the balance table. Then: python3 -I eval151.py old new
. "$(dirname "$0")/common.sh"
TWELVE="h134 h135 h136 h137 h138 h139 h140 h141 h142 h143 h144 h145"
SIX="h134 h135 h136 h137 h138 h139"
# steady runs: forward facing where it moves walking and running, forward with a fixed heading, to the side and backward
TRACK="f:1.1675 f:3.7325 f:4.5125 x:4.2625 s:2.4125 s:4.1875 b:3.8625 b:4.5875"
for v in "$@"; do
  "$M/sweeps.sh" $v h134 h135 hf146 hf147 || exit 1
  python3 -I "$M/stand151.py" $v backToFwd30 $TWELVE > "$W/tmp/stand151_$v.log" || exit 1
  python3 -I "$M/starts150.py" $v $TWELVE > "$W/tmp/starts151_$v.log" || exit 1
  python3 -I "$M/rhythm123.py" $v hf146 hf147 > "$W/tmp/rhythm151_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run $SIX > "$W/tmp/where151_$v.log" || exit 1
  python3 -I "$M/where129.py" $v backToFwd30 h140 h141 h142 h143 h144 h145 > "$W/tmp/where151b_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run $SIX > "$W/tmp/hops151run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run $SIX > "$W/tmp/flips151_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run $SIX > "$W/tmp/lifts151_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run $SIX > "$W/tmp/pitch151_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run $SIX > "$W/tmp/liftsb151_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run $SIX > "$W/tmp/liftsc151_$v.log" || exit 1
  python3 -I "$M/track147.py" $v $TRACK > "$W/tmp/track151_$v.log" || exit 1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
