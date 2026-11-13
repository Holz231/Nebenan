#!/bin/bash
# sanitize.sh: address and undefined behaviour sanitizers on the source tree (Debug, built into out/basan): scenes where
# the legs turn under a fixed heading, turning about, turning by a quarter, from the side to backward, from walking to
# running, stopping and turning the heading, changes within a fifth of a second of each other, caught hits of the
# balance table (a 48th of it, 186 hits), and the whole test program. Output in out/san.
. "$(dirname "$0")/common.sh"
F=$W; B=$W/basan
SAN="-fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -fno-omit-frame-pointer"
mkdir -p "$W/san"; cd "$W"
cmake -S "$T" -B basan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$SAN" -DCMAKE_CXX_FLAGS="$SAN" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DNEBENAN_BENCHMARK=ON -DNEBENAN_DEMO=OFF -DNEBENAN_TESTS=ON > san/cmake.log 2>&1 || { echo "cmake failed"; exit 1; }
nice cmake --build basan -j4 > san/build.log 2>&1 || { echo "build failed"; tail -5 san/build.log; exit 1; }
BOX=$(ls $B/extern/box3d/src/libbox3d*.a | head -1)
INC="-I$T/include -I$T/src -isystem $T/extern/box3d/include -isystem $B/extern/box3d/src -I$T/extern/box3d/src -I$T/test"
for tool in lab101 bal103; do
  gcc -O1 -g $SAN -std=gnu17 -o san/$tool "$M/$tool.c" $INC $B/src/libnebenan.a $BOX -lm -lpthread > san/link_$tool.log 2>&1 || { echo "link $tool failed"; head -5 san/link_$tool.log; exit 1; }
done
cd "$W/san"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
n=0; bad=0
run() { # script end
  n=$((n + 1))
  if ! ./lab101 "$1" out.csv "$2" > run.out 2> run.err; then bad=$((bad + 1)); echo "FAILED: $1"; head -20 run.err; fi
  if [ -s run.err ]; then echo "stderr for: $1"; head -5 run.err; fi
}
for w in 1.2 1.6; do
  run "0:0:0:0,1:$w:0:0,4.1:-$w:0:0,6.3:0:0:0" 8.0
  run "0:0:0:0,1:0:$w:0,4.2:$w:0:0,6.1:0:-$w:0" 8.5
  run "0:0:0:0,1:$w:0:0,4.3:$w:0:90,6.2:0:-$w:90" 8.0
done
for r in 3.0 3.4 4.3 4.8; do
  run "0:0:0:0,1:$r:0:0,4.15:-$r:0:0,6.4:$r:0:0" 9.0
  run "0:0:0:0,1:0:$r:0,4.25:$r:0:0,6.3:0:-$r:0" 8.5
  run "0:0:0:0,1:$r:0:0,4.35:0:-$r:0,6.2:-$r:0:0" 8.5
  run "0:0:0:0,1:1.2:0:0,4.3:$r:0:0,6.1:1.2:0:0,7.4:0:0:0" 9.0
  run "0:0:0:0,1:$r:0:0,4.2:$r:0:90,6.3:-$r:0:90" 8.5
done
run "0:0:0:0,1:4.3:0:0,3.0:-4.3:0:0,3.2:4.3:0:0,3.4:0:4.3:0,3.5:0:-4.3:0,3.7:4.3:0:0,3.8:0:0:0,4.0:-4.3:0:0" 7.0
run "0:0:0:0,1:3.0:3.0:0,3.1:-3.0:3.0:0,3.25:-3.0:-3.0:0,3.4:3.0:-3.0:0,3.6:0:0:0,3.7:3.0:0:0" 6.5
run "0:0:0,1:0:1.4,3.1:0:4.8,5.2:0:1.4,7.3:0:0" 9.0
echo "$n lab runs, $bad failed"
for hold in own none; do
  if HZ=60 SHIFT=0 HOLD=$hold PART=7 PARTS=48 ./bal103 bal_$hold.csv > bal.out 2> bal.err; then echo "balance table $hold: $(($(wc -l < bal_$hold.csv) - 1)) hits, fine"; else echo "FAILED: balance table $hold"; head -20 bal.err; fi
  if [ -s bal.err ]; then echo "stderr for the balance table $hold"; head -5 bal.err; fi
done
cd $B && ./bin/nebenan_test > $F/san/test.log 2> $F/san/test.err; echo "test program exit $?: $(tail -1 $F/san/test.log)"; head -5 $F/san/test.err
