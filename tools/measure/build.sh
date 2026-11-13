#!/bin/bash
# build.sh name ["defines"] [tool.c...]: the library of the source tree (NB_SRC, default this repository) with the
# defines into out/vb/<name>, and against it the tools named, by default lab101.c and bal103.c, into out/res/<name>/.
# Box3D comes from a cmake build of the source tree in out/box3d, made once. To compare two rounds, check the other one
# out as a git worktree and build it with NB_SRC pointing there under another name.
. "$(dirname "$0")/common.sh"
N=$1; D=$2; shift; shift
B=$W/box3d; O=$W/vb/$N; R=$W/res/$N
mkdir -p "$O" "$R"
if [ ! -f "$B/extern/box3d/src/libbox3d.a" ]; then
  cmake -S "$T" -B "$B" -DCMAKE_BUILD_TYPE=Release -DNEBENAN_DEMO=OFF -DNEBENAN_BENCHMARK=OFF -DNEBENAN_TESTS=OFF > "$W/box3d.cmake.log" 2>&1 &&
    cmake --build "$B" -j4 > "$W/box3d.build.log" 2>&1 || { echo "box3d failed, see $W/box3d.build.log"; exit 1; }
fi
INC="-I$T/include -I$T/src -isystem $T/extern/box3d/include -isystem $B/extern/box3d/src"
FLAGS="-O3 -DNDEBUG -std=gnu17 -Wall -Wextra -pedantic -Wmissing-prototypes -ffp-contract=off $D"
ls "$T"/src/*.c | xargs -P 4 -I{} sh -c "gcc $FLAGS $INC -c {} -o $O/\$(basename {} .c).o" 2> "$O/build.log" || { head -20 "$O/build.log"; exit 1; }
grep -E "warning|error" "$O/build.log" | head -5
rm -f "$O/libnebenan.a" && ar rcs "$O/libnebenan.a" "$O"/*.o
TINC="$INC -I$T/extern/box3d/src -I$T/test"
LINK="$O/libnebenan.a $B/extern/box3d/src/libbox3d.a -lm -lpthread"
TOOLS="$@"; [ -z "$TOOLS" ] && TOOLS="lab101.c bal103.c"
rm -f "$O/link.log"
for src in $TOOLS; do gcc -O2 -DNDEBUG -std=gnu17 $D -o "$R/$(basename $src .c)" "$M/$src" $TINC $LINK 2>> "$O/link.log" & done; wait
grep -E "error" "$O/link.log" | head -5
for src in $TOOLS; do [ -x "$R/$(basename $src .c)" ] || { echo "MISSING $src"; exit 1; }; done
echo "built $N"
