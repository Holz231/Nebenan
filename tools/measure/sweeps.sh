#!/bin/bash
# sweeps.sh variant what...: a built variant (out/res/<variant>/lab101) on sets of points, each what one of
#   dev            the development set with a fixed heading, 504 points: out/res/<variant>/sweep.json
#   devf           the development set facing where it moves, 312 points: out/res/<variant>/sweepf.json
#   h1 h2 h5 ...   a speed set of common.sh with a fixed heading, 504 points: out/res/<variant>_<set>/sweep.json, and
#                  toes and heels on it: out/res/<variant>/t<set>.json
#   hf7 hf9 ...    a speed set of common.sh facing where it moves, 312 points: out/res/<variant>/<set>.json
# ONLY=scene,scene limits the scenes of the speed sets with a fixed heading and skips their toes and heels.
. "$(dirname "$0")/common.sh"
v=$1; shift
[ -x "$W/res/$v/lab101" ] || { echo "build $v first"; exit 1; }
for what in "$@"; do
  case $what in
    dev) python3 -I "$M/sweep101.py" $v > "$W/res/$v/sweep.log" 2>&1 ;;
    devf) python3 -I "$M/sweepf102.py" $v > "$W/res/swf_$v.txt" 2>&1 ;;
    hf*) s=SET_$what; env ${!s} OUT=$what python3 -I "$M/sweepf112.py" $v > "$W/res/${what}_$v.txt" 2>&1 ;;
    h*) s=SET_$what; mkdir -p "$W/res/${v}_$what"; cp "$W/res/$v/lab101" "$W/res/${v}_$what/lab101"; rm -f "$W/res/${v}_$what/sweep.json"
      env ${!s} python3 -I "$M/sweeph111.py" ${v}_$what > "$W/res/${v}_$what/sweep.log" 2>&1
      [ -n "$ONLY" ] || env ${!s} SET=strafe OUT=t$what python3 -I "$M/toes112.py" $v > "$W/res/t${what}_$v.txt" 2>&1 ;;
    *) echo "unknown: $what"; exit 1 ;;
  esac
  echo "done $v $what"
done
