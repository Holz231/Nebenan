#!/bin/bash
# check152.sh variant...: the independent check of round 152 for each built variant (lab101, bal103), see round 152 in
# docs/Roadmap.md: from running sideways to running backward the scene of where129.py and its late landings of
# land139.py, the standing shin from running backward to running forward of stand151.py and the starts from standing
# of starts150.py on the twelve speed sets h148 to h159, the sets h148 and h149 with a fixed heading and hf160 and hf161
# facing where it moves with rhythm123.py, the eight changes at a run with a fixed heading of where129.py, hops131.py,
# lifts134.py, flips135.py, pitch136.py, lifts137.py and lifts138.py on h148 to h153, the steady runs of track147.py,
# and the balance table. Then: python3 -I eval152.py old new
. "$(dirname "$0")/common.sh"
TWELVE="h148 h149 h150 h151 h152 h153 h154 h155 h156 h157 h158 h159"
SIX="h148 h149 h150 h151 h152 h153"
# steady runs: forward facing where it moves walking and running, forward with a fixed heading, to the side and backward
TRACK="f:1.6025 f:3.7425 f:4.5225 x:4.2725 s:2.4225 s:4.1775 b:3.8725 b:4.5775"
for v in "$@"; do
  "$M/sweeps.sh" $v h148 h149 hf160 hf161 || exit 1
  python3 -I "$M/stand151.py" $v backToFwd30 $TWELVE > "$W/tmp/stand152_$v.log" || exit 1
  python3 -I "$M/starts150.py" $v $TWELVE > "$W/tmp/starts152_$v.log" || exit 1
  python3 -I "$M/rhythm123.py" $v hf160 hf161 > "$W/tmp/rhythm152_$v.log" || exit 1
  python3 -I "$M/where129.py" $v run $SIX > "$W/tmp/where152_$v.log" || exit 1
  python3 -I "$M/where129.py" $v LToBack30 h154 h155 h156 h157 h158 h159 > "$W/tmp/where152b_$v.log" || exit 1
  python3 -I "$M/land139.py" $v $TWELVE > "$W/tmp/land152_$v.log" || exit 1
  python3 -I "$M/hops131.py" $v run $SIX > "$W/tmp/hops152run_$v.log" || exit 1
  python3 -I "$M/flips135.py" $v run $SIX > "$W/tmp/flips152_$v.log" || exit 1
  python3 -I "$M/lifts134.py" $v run $SIX > "$W/tmp/lifts152_$v.log" || exit 1
  python3 -I "$M/pitch136.py" $v run $SIX > "$W/tmp/pitch152_$v.log" || exit 1
  python3 -I "$M/lifts137.py" $v run $SIX > "$W/tmp/liftsb152_$v.log" || exit 1
  python3 -I "$M/lifts138.py" $v run $SIX > "$W/tmp/liftsc152_$v.log" || exit 1
  python3 -I "$M/track147.py" $v $TRACK > "$W/tmp/track152_$v.log" || exit 1
  [ -f "$W/res/tab/${v}_same.csv" ] || "$M/table.sh" $v same 60 own 0 > /dev/null
  echo "checked $v"
done
