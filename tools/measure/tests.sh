#!/bin/bash
# tests.sh name: the tests of the source tree in a scratch copy whose RUN_TEST goes on after a failed test and prints the
# hashes, linked against the library of out/vb/<name> (build.sh): out/res/tests_<name>.log. One run shows every test
# that fails and every hash that changed.
. "$(dirname "$0")/common.sh"
V=$1; O=$W/tests; B=$W/box3d
mkdir -p "$O" && cp "$T"/test/*.c "$T"/test/*.h "$O"/
python3 -I - "$O/" <<'PY'
import re, sys
O = sys.argv[1]
s = open(O + 'test_macros.h').read()
a = s.index('#define RUN_TEST( T )'); b = s.index('while ( false )', a)
blk = re.sub(r'\t\t\treturn 1;( *)\\', lambda m: '\t\t\t(void)0;' + m.group(1) + '\\', s[a:b], count=1)
open(O + 'test_macros.h', 'w').write(s[:a] + blk + s[b:])
s = open(O + 'test_figure.c', encoding='utf-8').read()
s = re.sub(r'(\t+)ENSURE\( one == (NB_EXPECTED_\w+_HASH) \);', lambda m: m.group(1) + 'printf( "HASH %s 0x%08x\\n", "' + m.group(2) + '", one );\n' + m.group(0), s)
old = '\tENSURE( one.hash == NB_EXPECTED_WALL_HASH );'
assert s.count(old) == 1
s = s.replace(old, '\tprintf( "wall hash: 0x%08x\\n", one.hash );\n' + old)
open(O + 'test_figure.c', 'w', encoding='utf-8').write(s)
PY
cd "$O" && for f in main test_figure test_poly test_world; do
  gcc -I"$T/src" -I"$T/include" -isystem "$T/extern/box3d/include" -isystem "$B/extern/box3d/src" -O3 -DNDEBUG -std=gnu17 -ffp-contract=off -c $f.c -o $f.o 2> $f.err &
done; wait
grep -h "error" *.err | head -5
gcc -O3 -DNDEBUG main.o test_figure.o test_poly.o test_world.o -o "$W/res/tests_$V" "$W/vb/$V/libnebenan.a" "$B/extern/box3d/src/libbox3d.a" -lm -lpthread || exit 1
cd "$B" && "$W/res/tests_$V" > "$W/res/tests_$V.log" 2>&1
grep -E "test failed|condition false" "$W/res/tests_$V.log"
grep -E "^HASH|wall hash" "$W/res/tests_$V.log" | sort -u | head -12
echo "tests $(grep -c 'test passed' "$W/res/tests_$V.log"), failed $(grep -c 'test failed' "$W/res/tests_$V.log")"
