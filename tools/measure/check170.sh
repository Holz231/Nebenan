#!/bin/bash
# check170.sh state group [nudges]: legs167.py, gait155.py and steer162.py on one variant of build.sh (built with
# lab101.c and "-DLAB_BEND") at each nudge, default 0 1 2 3 percent, into <state>_n<p>, for compare170.py. A run that is
# there already is kept. The groups as in check167.sh, and fresh167, fresh169, fresh172, fresh174, fresh175,
# fresh177 and fresh178 for the fresh sets of those rounds.
. "$(dirname "$0")/common.sh"
case $2 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh167) SETS="h186 h187 h188 h189";;
  fresh169) SETS="h190 h191 h192 h193";;
  fresh172) SETS="h194 h195 h196 h197";;
  fresh174) SETS="h198 h199 h200 h201";;
  fresh175) SETS="h202 h203 h204 h205";;
  fresh177) SETS="h206 h207 h208 h209";;
  fresh178) SETS="h210 h211 h212 h213";;
  *) echo "check170.sh state dev1|dev2|fresh167|fresh169|fresh172|fresh174|fresh175|fresh177|fresh178 [nudges]"; exit 1;;
esac
for k in ${3:-0 1 2 3}; do
  v=$1_n$k
  mkdir -p "$W/res/$v" && cp "$W/res/$1/lab101" "$W/res/$v/lab101" || exit 1
  [ -f "$W/res/$v/steer162.$2.json" ] && continue
  for tool in legs167 gait155 steer162; do
    env -u START NUDGE=$k python3 -I "$M/$tool.py" $v $SETS > "$W/res/$v/$tool.$2.txt" 2>&1 ||
      { echo "$tool failed on $v"; exit 1; }
    mv "$W/res/$v/$tool.json" "$W/res/$v/$tool.$2.json" || exit 1
  done
done
