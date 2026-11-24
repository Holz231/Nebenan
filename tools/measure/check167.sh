#!/bin/bash
# check167.sh old new set: the rule of round 167 on one group of sets, for two variants of build.sh that were built with
# lab101.c and "-DLAB_BEND". The groups: dev1 and dev2, on which the rule was written, and fresh, whose speeds had run
# on no state before the rule was fixed, see docs/Regeln/RULE167.txt.
. "$(dirname "$0")/common.sh"
case $3 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h186 h187 h188 h189";;
  *) echo "check167.sh old new dev1|dev2|fresh"; exit 1;;
esac
for v in $1 $2; do
  # REUSE=1 keeps what an earlier run of this variant on this group wrote; without it everything runs again
  [ -n "$REUSE" ] && [ -f "$W/res/$v/steer162.$3.json" ] && continue
  rm -f "$W/res/$v/legs167.json" "$W/res/$v/gait155.json" "$W/res/$v/steer162.json" "$W/res/$v"/*."$3".json
  for tool in legs167 gait155 steer162; do
    python3 -I "$M/$tool.py" $v $SETS > "$W/res/$v/$tool.$3.txt" 2>&1 || { echo "$tool failed on $v"; exit 1; }
  done
  for tool in legs167 gait155 steer162; do
    mv "$W/res/$v/$tool.json" "$W/res/$v/$tool.$3.json" || exit 1
  done
done
cd "$M" && python3 "$M/eval167.py" $1 $2 $3
