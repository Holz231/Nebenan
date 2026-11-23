#!/bin/bash
# check158.sh old new set: the rule of round 158 for two variants of build.sh that have lab101. The sets: dev1 and
# dev2, on which the change was developed, and fresh, whose speeds had run on no state before the rule was fixed, see
# docs/Regeln/RULE158.txt. With "table" as fourth argument also D2: the table of the changes at a run with a fixed
# heading, where129.py, on its twelve sets, the same to the byte for both.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h170 h171 h172 h173";;
  *) echo "check158.sh old new dev1|dev2|fresh [table]"; exit 1;;
esac
O=$W/r158; mkdir -p "$O"
for v in $1 $2; do
  python3 -I "$M/gait155.py" $v $SETS > "$O/$3_$v.txt" || exit 1
  cp "$W/res/$v/gait155.json" "$O/$3_$v.json"
done
python3 -I "$M/eval158.py" "$O/$3_$1.json" "$O/$3_$2.json"; R=$?
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
