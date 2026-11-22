#!/bin/bash
# check154.sh old new set: the rule of round 154 on one set of meetings, for two variants of build.sh that were built
# with hit153.c and meet154.c. The sets: dev1 and dev2, on which the change was developed, and fresh, whose speeds had
# run on no state before the rule was fixed, see docs/Regeln/RULE154.txt. Each is toward, ahead, side and the speeds of
# the first figure for meet154.c.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SET="1.4 1.0 0.15 1.4 4.3";;
  dev2) SET="1.0 0.6 0.2 1.0 2.0 3.0";;
  fresh) SET="1.93 0.47 0.11 0.97 1.77 2.65 3.97 5.05";;
  *) echo "check154.sh old new dev1|dev2|fresh"; exit 1;;
esac
O=$W/r154; mkdir -p "$O"
for v in $1 $2; do
  "$W/res/$v/meet154" "$O/$3_$v.csv" $SET || exit 1
  [ -f "$O/hits_$v.csv" ] || "$W/res/$v/hit153" "$O/hits_$v.csv" "$O/meet_$v.csv" || exit 1
done
python3 "$M/eval154.py" "$O/$3_$1.csv" "$O/$3_$2.csv" "$O/hits_$1.csv" "$O/hits_$2.csv" "$O/meet_$1.csv" "$O/meet_$2.csv"
