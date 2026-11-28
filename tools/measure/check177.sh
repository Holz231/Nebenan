#!/bin/bash
# check177.sh state group: walk176.py on one variant of build.sh at the nudges 0 1 2 3 on the sets of a group of
# check170.sh, into out/res/<state>/walk176.<group>.json, for eval177.py. A run that is there already is kept.
. "$(dirname "$0")/common.sh"
case $2 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh177) SETS="h206 h207 h208 h209";;
  fresh178) SETS="h210 h211 h212 h213";;
  fresh179) SETS="h214 h215 h216 h217";;
  fresh182) SETS="h218 h219 h220 h221";;
  fresh183) SETS="h222 h223 h224 h225";;
  fresh184) SETS="h226 h227 h228 h229";;
  fresh185) SETS="h230 h231 h232 h233";;
  fresh186) SETS="h234 h235 h236 h237";;
  fresh188) SETS="h238 h239 h240 h241";;
  fresh189) SETS="h242 h243 h244 h245";;
  fresh191) SETS="h246 h247 h248 h249";;
  fresh192) SETS="h250 h251 h252 h253";;
  fresh193) SETS="h254 h255 h256 h257";;
  *) echo "check177.sh state group, group: dev1, dev2 or fresh<round>, see the case above"; exit 1;;
esac
[ -f "$W/res/$1/walk176.$2.json" ] && exit 0
python3 -I "$M/walk176.py" $1 0,1,2,3 $SETS > "$W/res/$1/walk176.$2.txt" 2>&1 || { echo "walk176 failed on $1"; exit 1; }
mv "$W/res/$1/walk176.json" "$W/res/$1/walk176.$2.json"
