# cross118.py base variant...: the scenes of tools/cross118.sh on H1, H2, H5 and H6 together, per scene of 96 points: how many
# cross their feet, how many deeper than 3 cm and how deep, the points over the limits of feet, shins, hands and pelvis,
# the worst pelvis, and the time to answer the change, mean and latest; then the totals. SCENES limits the scenes shown.
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import json, os, sys
F = _W + '/res/'
LIM = [('j_feet', 37.0), ('j_shins', 37.0), ('j_hands', 15.0), ('j_pelvis', 10.0)]


def load(v):
    d = {}
    for suf in ('h1', 'h2', 'h5', 'h6'):
        for n, r in json.load(open(F + '%s_%s/sweep.json' % (v, suf))).items():
            d[suf + ':' + n] = r
    return d


def row(pts):
    sep = [r['feetSep'] for r in pts]
    resp = [r['resp'] for r in pts]
    out = [sum(1 for x in sep if x < 0), sum(1 for x in sep if x < -0.03), 100 * min(sep)]
    out += [sum(1 for r in pts if r[k] > lim) for k, lim in LIM]
    out += [max(r['j_shins'] for r in pts), max(r['j_pelvis'] for r in pts), sum(1 for r in pts if r['low'] > 0.15), 100 * max(r['low'] for r in pts), sum(resp) / len(resp), max(resp)]
    fl = [r.get('flight', 0.0) for r in pts]
    out += [sum(1 for x in fl if x > 0.2), sum(fl) / len(fl), max(fl)]
    for k in ('pelvis', 'shins', 'feet', 'hands'):
        out += [sum(r.get('m_' + k, 0.0) for r in pts) / len(pts), sum(r.get('t_' + k, 0.0) for r in pts) / len(pts)]
    return out


V = [load(v) for v in sys.argv[1:]]
keep = set(V[-1])
V = [{n: r for n, r in d.items() if n in keep} for d in V]
scenes = sorted({n.split(':')[1].rsplit('_', 1)[0] for n in keep})
show = os.environ.get('SCENES')
fmt = '%-7s crossed %2d, %2d deep (%6.1f cm)  feet %2d shins %3d hands %3d pelvis %3d  worst shin %5.1f pelvis %5.1f  low %2d (%4.1f cm)  answer %.2f s (%.2f)'
fmt += '\n          flight over 0.2 s %3d, mean %.3f, longest %.2f s  jumps mean and upper tenth (mm): pelvis %.2f %.2f shins %.2f %.2f feet %.2f %.2f hands %.2f %.2f'
for sc in scenes + ['all']:
    if show and sc not in show.split(',') and sc != 'all':
        continue
    print(sc)
    for name, d in zip(sys.argv[1:], V):
        pts = [r for n, r in d.items() if sc == 'all' or n.split(':')[1].rsplit('_', 1)[0] == sc]
        print('  ' + fmt % tuple([name] + row(pts)))
