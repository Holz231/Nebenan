#!/bin/bash
# check163.sh old new set: the rule of round 163 for two variants of build.sh that have lab101, both built with
# -DLAB_BEND. The sets: dev1 and dev2, on which the change was developed, and fresh, whose speeds had run on no state
# before the rule was fixed, see docs/Regeln/RULE163.txt. Runs gait155.py and steer162.py for both, and both again with
# nbFigureSteering::walkBend at zero, BEND=0: D3, the new variant writes the tables of the old one to the byte then.
# With "table" as fourth argument also D2: the table of the changes at a run with a fixed heading, where129.py, on its
# twelve sets, the same to the byte for both.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h178 h179 h180 h181";;
  *) echo "check163.sh old new dev1|dev2|fresh [table]"; exit 1;;
esac
O=$W/r163; mkdir -p "$O"
for v in $1 $2; do
  for b in bend spot; do
    if [ $b = spot ]; then export BEND=0; else unset BEND; fi
    python3 -I "$M/gait155.py" $v $SETS > "$O/$3_${v}_gait_$b.txt" || exit 1
    cp "$W/res/$v/gait155.json" "$O/$3_${v}_gait_$b.json"
    python3 -I "$M/steer162.py" $v $SETS > "$O/$3_${v}_steer_$b.txt" || exit 1
    cp "$W/res/$v/steer162.json" "$O/$3_${v}_steer_$b.json"
  done
  unset BEND
  cp "$O/$3_${v}_gait_bend.json" "$W/res/$v/gait155.json"; cp "$O/$3_${v}_steer_bend.json" "$W/res/$v/steer162.json"
done
python3 -I "$M/eval163.py" "$O/$3_$1_gait_bend.json" "$O/$3_$2_gait_bend.json" "$O/$3_$1_steer_bend.json" \
  "$O/$3_$2_steer_bend.json" "$O/$3_$1_steer_spot.json"; R=$?
if cmp -s "$O/$3_$1_gait_spot.json" "$O/$3_$2_gait_spot.json" &&
   cmp -s "$O/$3_$1_steer_spot.json" "$O/$3_$2_steer_spot.json"; then
  echo "met     D3: with walkBend at zero both tables are the ones before, to the byte"
else echo "MISSED  D3: with walkBend at zero a table differs from the one before"; R=1; fi
if [ "$4" = table ]; then
  for v in $1 $2; do
    # tables an earlier variant of the same name left behind would count as a difference
    [ -n "$v" ] && [ -d "$W/res/$v" ] && rm -f "$W/res/$v"/where_*.json
    python3 -I "$M/where129.py" $v run h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29 > "$O/where_$v.txt" || exit 1
    rm -rf "$O/where_$v"; mkdir -p "$O/where_$v"; cp "$W/res/$v"/where_*.json "$O/where_$v/"
  done
  if diff -rq "$O/where_$1" "$O/where_$2" > /dev/null; then
    echo "met     D2: the table with a fixed heading is the same to the byte, $(ls "$O/where_$2" | wc -l) files"
  else echo "MISSED  D2: the table with a fixed heading differs"; R=1; fi
fi
exit $R
