#!/bin/bash
# devtable.sh variant...: the development table for each built variant (out/res/<variant>/lab101), four sets at a time
# with sweeps.sh: the 19 speed sets with a fixed heading, with toes and heels, and the five sets facing where it moves
# that the rounds compare their variants on. None of them is fresh. Then: python3 -I table150.py old new [show]
. "$(dirname "$0")/common.sh"
SETS="h1 h2 h5 h6 h9 h10 h11 h12 h13 h14 h15 h29 h30 h31 h32 h33 h34 h35 h36 hf9 hf11 hf15 hf41 hf53"
for v in "$@"; do
  [ -x "$W/res/$v/lab101" ] || { echo "build $v first"; exit 1; }
done
for v in "$@"; do for s in $SETS; do echo "$v $s"; done; done | xargs -P 4 -n 2 sh -c '"$0/sweeps.sh" "$1" "$2"' "$M"
echo "development table done: $*"
