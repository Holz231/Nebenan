#!/bin/bash
# check166.sh old new set: the rule of round 166 on one set of meetings, for two variants of build.sh that were built
# with hit153.c and meet154.c. The sets: dev1, dev2 and dev3, the three sets of round 154, the last of them fresh then,
# and fresh, whose speeds had run on no state before the rule was fixed, see docs/Regeln/RULE166.txt. Each is toward,
# ahead, side and the speeds of the first figure for meet154.c.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SET="1.4 1.0 0.15 1.4 4.3";;
  dev2) SET="1.0 0.6 0.2 1.0 2.0 3.0";;
  dev3) SET="1.93 0.47 0.11 0.97 1.77 2.65 3.97 5.05";;
  fresh) SET="1.69 0.71 0.14 1.1565 1.83 2.75 3.91 4.93";;
  *) echo "check166.sh old new dev1|dev2|dev3|fresh"; exit 1;;
esac
O=$W/r166; mkdir -p "$O"
for v in $1 $2; do
  "$W/res/$v/meet154" "$O/$3_$v.csv" $SET || exit 1
  [ -f "$O/hits_$v.csv" ] || "$W/res/$v/hit153" "$O/hits_$v.csv" "$O/meet_$v.csv" || exit 1
done
python3 -I "$M/eval166.py" "$O/$3_$1.csv" "$O/$3_$2.csv" "$O/hits_$1.csv" "$O/hits_$2.csv" "$O/meet_$1.csv" "$O/meet_$2.csv"
