#!/bin/bash
# check161.sh old new set: the rule of round 161 for two variants of build.sh that have lab101, the new one built with
# -DLAB_BEND. The sets: dev1 and dev2, on which the change was developed, and fresh, whose speeds had run on no state
# before the rule was fixed, see docs/Regeln/RULE161.txt. Also D3: with nbFigureSteering::walkBend at zero, BEND=0, the
# new variant writes the table of the old one to the byte. With "table" as fourth argument also D2: the table of the
# changes at a run with a fixed heading, where129.py, on its twelve sets, the same to the byte for both.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h174 h175 h176 h177";;
  *) echo "check161.sh old new dev1|dev2|fresh [table]"; exit 1;;
esac
O=$W/r161; mkdir -p "$O"
for v in $1 $2; do
  python3 -I "$M/gait155.py" $v $SETS > "$O/$3_$v.txt" || exit 1
  cp "$W/res/$v/gait155.json" "$O/$3_$v.json"
done
python3 -I "$M/eval161.py" "$O/$3_$1.json" "$O/$3_$2.json"; R=$?
BEND=0 python3 -I "$M/gait155.py" $2 $SETS > "$O/$3_$2_bend0.txt" || exit 1
cp "$W/res/$2/gait155.json" "$O/$3_$2_bend0.json"; cp "$O/$3_$2.json" "$W/res/$2/gait155.json"
if cmp -s "$O/$3_$1.json" "$O/$3_$2_bend0.json"; then
  echo "met     D3: with walkBend at zero the table is the one before, to the byte"
else echo "MISSED  D3: with walkBend at zero the table differs from the one before"; R=1; fi
if [ "$4" = table ]; then
  for v in $1 $2; do
    python3 -I "$M/where129.py" $v run h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29 > "$O/where_$v.txt" || exit 1
    rm -rf "$O/where_$v"; mkdir -p "$O/where_$v"; cp "$W/res/$v"/where_*.json "$O/where_$v/"
  done
  if diff -rq "$O/where_$1" "$O/where_$2" > /dev/null; then
    echo "met     D2: the table with a fixed heading is the same to the byte, $(ls "$O/where_$2" | wc -l) files"
  else echo "MISSED  D2: the table with a fixed heading differs"; R=1; fi
fi
exit $R
