# spread118.py base variant [sets]: per quantity, its value per set of points on base, the spread between the two fast
# sets and between the two slow sets on base, the tolerance for the total of a fast and a slow set (the two spreads in
# quadrature), and the change of the variant on the totals of all sets against twice that tolerance (four sets)
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import json, sys
F = _W + '/res/'
base, var = sys.argv[1], sys.argv[2]
SETS = ['h1', 'h6', 'h2', 'h5']   # slow pair, fast pair


def load(v, suf):
    return json.load(open(F + '%s_%s/sweep.json' % (v, suf)))


def quantities(d, keep=None):
    pts = [r for n, r in d.items() if keep is None or n in keep]
    q = {}
    for k, lim in (('j_feet', 37.0), ('j_shins', 37.0), ('j_hands', 15.0), ('j_pelvis', 10.0)):
        q['over ' + k[2:]] = sum(r[k] > lim for r in pts)
    q['low over 15 cm'] = sum(r['low'] > 0.15 for r in pts)
    q['crossed'] = sum(r['feetSep'] < 0 for r in pts)
    q['flight over 0.2 s'] = sum(r['flight'] > 0.2 for r in pts)
    q['flight mean (ms)'] = 1000 * sum(r['flight'] for r in pts) / len(pts)
    for k in ('pelvis', 'shins', 'feet', 'hands'):
        q['mean ' + k] = sum(r['m_' + k] for r in pts) / len(pts)
        q['tenth ' + k] = sum(r['t_' + k] for r in pts) / len(pts)
    rs = [r['resp'] for r in pts if r['resp'] > 0]
    q['answer (ms)'] = 1000 * sum(rs) / max(len(rs), 1)
    return q


B = {s: load(base, s) for s in SETS}
V = {s: load(var, s) for s in SETS}
QB = {s: quantities(B[s], set(V[s])) for s in SETS}
QV = {s: quantities(V[s]) for s in SETS}
print('%-18s %8s %8s %8s %8s | %7s %7s %7s | %9s %9s %9s  %s' % ('quantity', 'h1', 'h6', 'h2', 'h5', 'slow', 'fast', 'tol', 'base', 'variant', 'change', ''))
for k in QB['h1']:
    b = [QB[s][k] for s in SETS]
    slow, fast = abs(b[0] - b[1]), abs(b[2] - b[3])
    tol = (slow * slow + fast * fast) ** 0.5
    tb, tv = sum(b), sum(QV[s][k] for s in SETS)
    if k.startswith(('mean', 'tenth', 'flight mean', 'answer')):
        tb, tv = tb / 4, tv / 4
        lim = tol / 2 * 2 ** 0.5   # the mean of four sets: two pairs
    else:
        lim = tol * 2 ** 0.5       # the total of four sets: two pairs
    mark = 'better' if tv < tb - lim else ('WORSE' if tv > tb + lim else 'within')
    print('%-18s %8.2f %8.2f %8.2f %8.2f | %7.2f %7.2f %7.2f | %9.2f %9.2f %+9.2f  %s (limit %.2f)' % (k, b[0], b[1], b[2], b[3], slow, fast, tol, tb, tv, tv - tb, mark, lim))
