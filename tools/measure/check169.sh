#!/bin/bash
# check169.sh state set: the rule of round 169 on one group of sets, for one variant of build.sh that was built with
# lab101.c and "-DLAB_BEND": the tools run on it as it is and with START=4, which lab101 takes for
# nbFigureSteering::startAcceleration, under the name <state>_s4. The groups: dev1 and dev2, on which the rule was
# written, and fresh, whose speeds had run on no state before the rule was fixed, see docs/Regeln/RULE169.txt. REUSE=1
# keeps what an earlier run on this group wrote.
. "$(dirname "$0")/common.sh"
case $2 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh) SETS="h190 h191 h192 h193";;
  *) echo "check169.sh state dev1|dev2|fresh"; exit 1;;
esac
mkdir -p "$W/res/$1_s4" && cp "$W/res/$1/lab101" "$W/res/$1_s4/lab101" || exit 1
for pair in "$1:" "$1_s4:4"; do
  v=${pair%%:*}; s=${pair##*:}
  [ -n "$REUSE" ] && [ -f "$W/res/$v/steer162.$2.json" ] && continue
  rm -f "$W/res/$v/legs167.json" "$W/res/$v/gait155.json" "$W/res/$v/steer162.json" "$W/res/$v"/*."$2".json
  for tool in legs167 gait155 steer162; do
    if [ -n "$s" ]; then START=$s python3 -I "$M/$tool.py" $v $SETS > "$W/res/$v/$tool.$2.txt" 2>&1
    else env -u START python3 -I "$M/$tool.py" $v $SETS > "$W/res/$v/$tool.$2.txt" 2>&1; fi ||
      { echo "$tool failed on $v"; exit 1; }
  done
  for tool in legs167 gait155 steer162; do
    mv "$W/res/$v/$tool.json" "$W/res/$v/$tool.$2.json" || exit 1
  done
done
cd "$M" && python3 "$M/eval169.py" $1 $1_s4 $2
