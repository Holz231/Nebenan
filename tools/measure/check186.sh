#!/bin/bash
# check186.sh state group: rest186.py on one variant of build.sh at the nudges 0 1 2 3 on the sets of a group of
# check170.sh, into out/res/<state>/rest186.<group>.json, for eval186.py. A run that is there already is kept.
. "$(dirname "$0")/common.sh"
case $2 in
  dev1) SETS="h1 h2 h5 h6 h9 h10";;
  dev2) SETS="h11 h12 h13 h14 h15 h29";;
  fresh186) SETS="h234 h235 h236 h237";;
  fresh188) SETS="h238 h239 h240 h241";;
  fresh189) SETS="h242 h243 h244 h245";;
  fresh191) SETS="h246 h247 h248 h249";;
  *) echo "check186.sh state dev1|dev2|fresh186|fresh188|fresh189|fresh191"; exit 1;;
esac
[ -f "$W/res/$1/rest186.$2.json" ] && exit 0
python3 -I "$M/rest186.py" $1 0,1,2,3 $SETS > "$W/res/$1/rest186.$2.txt" 2>&1 || { echo "rest186 failed on $1"; exit 1; }
mv "$W/res/$1/rest186.json" "$W/res/$1/rest186.$2.json"
