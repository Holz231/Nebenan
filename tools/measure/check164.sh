#!/bin/bash
# check164.sh old new set: the rule of round 164 for two variants of build.sh that have lab101, both built with
# -DLAB_BEND. The sets: dev1 and dev2, on which the change was developed, and fresh, whose speeds had run on no state
# before the rule was fixed, see docs/Regeln/RULE164.txt. Runs gait155.py and steer162.py for both. With "table" as
# fourth argument also D2: the table of the changes at a run with a fixed heading, where129.py, on its twelve sets.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h182 h183 h184 h185";;
  *) echo "check164.sh old new dev1|dev2|fresh [table]"; exit 1;;
esac
O=$W/r164; mkdir -p "$O"
for v in $1 $2; do
  python3 -I "$M/gait155.py" $v $SETS > "$O/$3_${v}_gait.txt" || exit 1
  cp "$W/res/$v/gait155.json" "$O/$3_${v}_gait.json"
  python3 -I "$M/steer162.py" $v $SETS > "$O/$3_${v}_steer.txt" || exit 1
  cp "$W/res/$v/steer162.json" "$O/$3_${v}_steer.json"
  if [ "$4" = table ]; then
    python3 -I "$M/where129.py" $v run h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29 > "$O/where_$v.txt" || exit 1
  fi
done
if [ "$4" = table ]; then
  python3 -I "$M/eval164.py" "$O/$3_$1_gait.json" "$O/$3_$2_gait.json" "$O/$3_$1_steer.json" "$O/$3_$2_steer.json" \
    "$O/where_$1.txt" "$O/where_$2.txt"
else
  python3 -I "$M/eval164.py" "$O/$3_$1_gait.json" "$O/$3_$2_gait.json" "$O/$3_$1_steer.json" "$O/$3_$2_steer.json"
fi
