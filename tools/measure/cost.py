# cost.py [file]: the pairs of out/cost/<file> (default pairs.txt) written by cost_pairs.sh: per mode the median cost of a
# figure per update in microseconds on the old and the new build, the median of the differences of the pairs with their
# quartiles, and the best run of each
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import statistics, sys
P = _W + '/cost/' + (sys.argv[1] if len(sys.argv) > 1 else 'pairs.txt')
rows = {}
for line in open(P):
    p = line.split()
    if len(p) < 6 or p[0] not in '2340':
        continue
    mode, r, b = p[0], int(p[1]), p[2]
    ms = float(p[-2])
    count = int(p[5])
    rows.setdefault(mode, {}).setdefault(r, {})[b] = 1000.0 * ms / count
names = {'2': 'direction and gait change every 2 s (cost test of round 107)', '4': 'steady sideways at 1.6 m/s',
         '3': 'forward walking and backward running facing ahead (extra)', '0': 'standing (extra)'}
for mode in rows:
    pairs = [(v['old'], v['new']) for r, v in sorted(rows[mode].items()) if len(v) == 2]
    a = [x for x, _ in pairs]; b = [y for _, y in pairs]; d = [y - x for x, y in pairs]
    first = [y - x for i, (x, y) in enumerate(pairs) if i % 2 == 0]; second = [y - x for i, (x, y) in enumerate(pairs) if i % 2 == 1]
    print('mode %s, %s: %d pairs' % (mode, names[mode], len(pairs)))
    print('  median old %.2f us, new %.2f us; median of differences %+.2f us (mean %+.2f, quartiles %+.2f to %+.2f)' % (
        statistics.median(a), statistics.median(b), statistics.median(d), statistics.mean(d),
        statistics.quantiles(d, n=4)[0], statistics.quantiles(d, n=4)[2]))
    print('  old first: %+.2f us, new first: %+.2f us; min old %.2f, min new %.2f' % (
        statistics.median(first), statistics.median(second), min(a), min(b)))
