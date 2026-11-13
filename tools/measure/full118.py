# full118.py base variant...: all transitions with a fixed heading, the development set and H1, H2, H5 and H6, 2 520
# points: points over the limits, pelvis low, crossed feet, the worst values, the time to answer; toes and heels on the
# four H sets; and the 312 points facing where it moves
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import json, sys
F = _W + '/res/'
LIM = [('j_feet', 37.0), ('j_shins', 37.0), ('j_hands', 15.0), ('j_pelvis', 10.0)]
SETS = (('d', '%s/sweep.json'), ('h1', '%s_h1/sweep.json'), ('h2', '%s_h2/sweep.json'), ('h5', '%s_h5/sweep.json'), ('h6', '%s_h6/sweep.json'))


def load(v, which):
    d = {}
    for suf, path in SETS:
        if which in ('all', suf):
            for n, r in json.load(open(F + path % v)).items():
                d[suf + ':' + n] = r
    return d


for which in ('d', 'h1', 'h2', 'h5', 'h6', 'all'):
    print(which)
    for v in sys.argv[1:]:
        d = load(v, which)
        c = [sum(1 for r in d.values() if r[k] > lim) for k, lim in LIM]
        w = [max(r[k] for r in d.values()) for k, lim in LIM]
        sep = [r['feetSep'] for r in d.values()]
        resp = sorted(r['resp'] for r in d.values() if r['resp'] > 0.05)
        print('  %-6s %4d: feet %3d (%5.1f) shins %3d (%5.1f) hands %3d (%5.1f) pelvis %3d (%5.1f) sum %4d  low %3d (%4.1f cm)  crossed %2d, %d deep (%5.1f cm)'
              '  answer %.3f max %.2f s' % (v, len(d), c[0], w[0], c[1], w[1], c[2], w[2], c[3], w[3], c[0] + c[1] + c[3], sum(1 for r in d.values() if r['low'] > 0.15),
                                          100 * max(r['low'] for r in d.values()), sum(1 for x in sep if x < 0), sum(1 for x in sep if x < -0.03), 100 * min(sep),
                                          sum(resp) / len(resp), resp[-1]))
print('toes and heels, H1, H2, H5, H6')
for v in sys.argv[1:]:
    vals = []
    for o in ('th1', 'th2', 'th5', 'th6'):
        vals += list(json.load(open(F + '%s/%s.json' % (v, o))).values())
    print('  %-6s %4d: over 37 mm %3d, over 70 mm %2d, worst %5.1f' % (v, len(vals), sum(1 for x in vals if x > 37), sum(1 for x in vals if x > 70), max(vals)))
print('facing where it moves, development set')
for v in sys.argv[1:]:
    d = json.load(open(F + '%s/sweepf.json' % v))
    keys = sorted({k for r in d.values() for k in r})
    c = [sum(1 for r in d.values() if r.get(k, r.get(k[2:], 0)) > lim) for k, lim in LIM]
    w = [max(r.get(k, r.get(k[2:], 0)) for r in d.values()) for k, lim in LIM]
    low = [r.get('low', 0) for r in d.values()]
    print('  %-6s %4d: feet %3d (%5.1f) shins %3d (%5.1f) hands %3d (%5.1f) pelvis %3d (%5.1f) sum %4d  low %3d (%4.1f cm)' % (
        v, len(d), c[0], w[0], c[1], w[1], c[2], w[2], c[3], w[3], c[0] + c[1] + c[3], sum(1 for x in low if x > 0.15), 100 * max(low)))
    fl = [r.get('flight', 0.0) for r in d.values()]
    print('         flight over 0.2 s %3d, mean %.3f, longest %.2f s  jumps mean and upper tenth (mm): ' % (sum(1 for x in fl if x > 0.2), sum(fl) / len(fl), max(fl)) + ' '.join('%s %.2f %.2f' % (k, sum(r.get('m_' + k, 0) for r in d.values()) / len(d), sum(r.get('t_' + k, 0) for r in d.values()) / len(d)) for k in ('pelvis', 'shins', 'feet', 'hands')))
