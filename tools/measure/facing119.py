# facing119.py base variant [set...]: two builds on sets of points facing where they move (default sweepf hf7 hf9, the
# development sets of round 119): points over the limits of feet, shins, hands and pelvis, the hardest jumps, mean and
# upper tenth of the jumps, pelvis low, flights over 0.2 s; per set, in total, and by scene where they differ
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
import json
names = _sys.argv[1:3]
sets = _sys.argv[3:] or ['sweepf', 'hf7', 'hf9']
LIM = (('feet', 37.0), ('shins', 37.0), ('hands', 15.0), ('pelvis', 10.0))


def load(v, s):
    return json.load(open(_W + '/res/%s/%s.json' % (v, s)))


def row(p):
    out = []
    for k, lim in LIM:
        out += [sum(r[k] > lim for r in p), max(r[k] for r in p)]
    for k, _ in LIM:
        out += [sum(r['m_' + k] for r in p) / len(p), sum(r['t_' + k] for r in p) / len(p)]
    out += [sum(r['low'] > 0.15 for r in p), sum(r['flight'] > 0.2 for r in p)]
    return out


FMT = '%-7s %4d: feet %3d (%5.1f) shins %3d (%5.1f) hands %3d (%5.1f) pelvis %3d (%5.1f) | mean, tenth: feet %.2f %.2f shins %.2f %.2f hands %.2f %.2f pelvis %.2f %.2f | low %2d flights %3d'
D = {v: {s: load(v, s) for s in sets} for v in names}
for s in sets + ['all']:
    print(s)
    for v in names:
        p = [r for t in sets if s in (t, 'all') for r in D[v][t].values()]
        x = row(p)
        print('  ' + FMT % tuple([v, len(p)] + x))
print('by scene, over the limits feet shins hands pelvis, hardest foot:')
a, b = names
for sc in sorted({n.rsplit('_', 1)[0] for t in sets for n in D[a][t]}):
    def one(v):
        p = [r for t in sets for n, r in D[v][t].items() if n.rsplit('_', 1)[0] == sc]
        return [sum(r[k] > lim for r in p) for k, lim in LIM] + [max(r['feet'] for r in p)]
    x, y = one(a), one(b)
    if x != y:
        print('  %-12s %3d %3d %3d %3d (%5.1f) -> %3d %3d %3d %3d (%5.1f)' % tuple([sc] + x + y))
