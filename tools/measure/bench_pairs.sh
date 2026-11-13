#!/bin/bash
# bench_pairs.sh old_build new_build [tag]: the benchmark with 4 workers and the figures benchmark of two cmake builds
# (Release, with NEBENAN_BENCHMARK) in turn, six pairs, one after the other, the order changing every pair, into
# out/bench/old_<tag><n>.txt and so on (tag default r). Nothing else may run on the machine. Two sets, r and s, then:
# TAGS="r1 r2 r3 r4 r5 r6 s1 s2 s3 s4 s5 s6" python3 -I bench.py
. "$(dirname "$0")/common.sh"
A=$1; B=$2; tag=${3:-r}
mkdir -p "$W/bench"; cd "$W/bench"
rm -f old_$tag?.txt new_$tag?.txt figold_$tag?.txt fignew_$tag?.txt
for r in 1 2 3 4 5 6; do
  if [ $((r % 2)) = 1 ]; then
    "$A/bin/nebenan_benchmark" 4 > old_$tag$r.txt 2>&1; "$B/bin/nebenan_benchmark" 4 > new_$tag$r.txt 2>&1
    "$A/bin/nebenan_benchmark" figures 4 1 > figold_$tag$r.txt 2>&1; "$B/bin/nebenan_benchmark" figures 4 1 > fignew_$tag$r.txt 2>&1
  else
    "$B/bin/nebenan_benchmark" 4 > new_$tag$r.txt 2>&1; "$A/bin/nebenan_benchmark" 4 > old_$tag$r.txt 2>&1
    "$B/bin/nebenan_benchmark" figures 4 1 > fignew_$tag$r.txt 2>&1; "$A/bin/nebenan_benchmark" figures 4 1 > figold_$tag$r.txt 2>&1
  fi
  echo "pair $r done"
done
