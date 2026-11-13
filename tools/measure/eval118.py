# eval118.py base variant: the conditions A, B, C, D1, D2 and D6 of RULE118.txt from the outputs of tools/h118.sh
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, filecmp, json, sys
F = _W + '/'
import gait100 as g

base, var = sys.argv[1], sys.argv[2]
ok = {}
LIM = {'feet': 37.0, 'shins': 37.0, 'hands': 15.0, 'pelvis': 10.0}
PARTS = ('pelvis', 'shins', 'feet', 'hands')


def hset(v):
    d = {}
    for suf in ('h11', 'h12'):
        for n, r in json.load(open(F + 'res/%s_%s/sweep.json' % (v, suf))).items():
            d[suf + ':' + n] = r
    return d


def measures(d, pre):
    p = list(d.values())
    q = {}
    for k in ('feet', 'shins', 'hands', 'pelvis'):
        q['over ' + k] = sum(r[pre + k] > LIM[k] for r in p)
        q['worst ' + k] = max(r[pre + k] for r in p)
        q['mean ' + k] = sum(r['m_' + k] for r in p) / len(p)
        q['tenth ' + k] = sum(r['t_' + k] for r in p) / len(p)
    q['low'] = sum(r['low'] > 0.15 for r in p)
    q['lowest'] = 100 * max(r['low'] for r in p)
    q['flights'] = sum(r['flight'] > 0.2 for r in p)
    q['flight'] = 1000 * sum(r['flight'] for r in p) / len(p)
    q['longest'] = max(r['flight'] for r in p)
    return q


def check(name, label, a, b, tol, unit, fmt='%.2f'):
    good = b <= a + tol
    print(('  %s %-28s ' + fmt + ' -> ' + fmt + ' (allowed ' + fmt + ' %s)%s') % (name, label, a, b, a + tol, unit, '' if good else '   MISSED'))
    return good


A, B = hset(base), hset(var)
assert len(A) == len(B) == 1008 and set(A) == set(B)
qa, qb = measures(A, 'j_'), measures(B, 'j_')
FA, FB = json.load(open(F + 'res/%s/hf11.json' % base)), json.load(open(F + 'res/%s/hf11.json' % var))
assert len(FA) == len(FB) == 312 and set(FA) == set(FB)
fa, fb = measures(FA, ''), measures(FB, '')

print('A. gain: %s -> %s' % (base, var))
ok['A1'] = qb['flights'] < qa['flights'] - 9
print('  A1 fixed heading, flights over 0.2 s      %d -> %d (needed below %d)%s' % (qa['flights'], qb['flights'], qa['flights'] - 9, '' if ok['A1'] else '   MISSED'))
ok['A2'] = qb['flight'] < qa['flight'] - 2.4
print('  A2 fixed heading, longest flight, mean    %.1f -> %.1f ms (needed below %.1f), the longest %.2f -> %.2f s%s' % (qa['flight'], qb['flight'], qa['flight'] - 2.4, qa['longest'], qb['longest'], '' if ok['A2'] else '   MISSED'))
ok['A3'] = fb['flights'] < fa['flights'] - 4
print('  A3 facing, flights over 0.2 s             %d -> %d (needed below %d), mean %.1f -> %.1f ms, the longest %.2f -> %.2f s%s' % (fa['flights'], fb['flights'], fa['flights'] - 4, fa['flight'], fb['flight'], fa['longest'], fb['longest'], '' if ok['A3'] else '   MISSED'))

print('B. protection with a fixed heading, H11 and H12, 1008 points')
T = {'over feet': 2, 'over shins': 23, 'over hands': 37, 'over pelvis': 33,
     'mean pelvis': 0.04, 'tenth pelvis': 0.10, 'mean shins': 0.24, 'tenth shins': 0.45, 'mean feet': 0.30, 'tenth feet': 0.53, 'mean hands': 0.12, 'tenth hands': 0.15,
     'worst feet': 2.0, 'worst shins': 8.2, 'worst hands': 3.3, 'worst pelvis': 3.2}
ok['B1'] = all([check('B1', 'over ' + k, qa['over ' + k], qb['over ' + k], T['over ' + k], 'points', '%d') for k in ('feet', 'shins', 'hands', 'pelvis')])
ok['B2'] = all([check('B2', m + ' ' + k, qa[m + ' ' + k], qb[m + ' ' + k], T[m + ' ' + k], 'mm', '%.3f') for k in PARTS for m in ('mean', 'tenth')])
ok['B3'] = all([check('B3', 'worst ' + k, qa['worst ' + k], qb['worst ' + k], T['worst ' + k], 'mm', '%.1f') for k in ('feet', 'shins', 'hands', 'pelvis')])
ok['B4'] = all([check('B4', 'pelvis low', qa['low'], qb['low'], 2, 'points', '%d'), check('B4', 'lowest', qa['lowest'], qb['lowest'], 1.8, 'cm', '%.1f')])
cr = [(sum(r['feetSep'] < 0 for r in d.values()), 100 * min(r['feetSep'] for r in d.values())) for d in (A, B)]
ok['B5'] = cr[1][0] <= cr[0][0] + 2 and cr[1][1] >= -3.0
print('  B5 crossed feet                 %d -> %d (allowed %d), deepest %.1f -> %.1f cm (allowed -3.0)%s' % (cr[0][0], cr[1][0], cr[0][0] + 2, cr[0][1], cr[1][1], '' if ok['B5'] else '   MISSED'))
rs = [[r['resp'] for r in d.values() if r['resp'] > 0] for d in (A, B)]
an = [1000 * sum(x) / len(x) for x in rs]
ok['B6'] = an[1] <= an[0] + 7.0 and max(rs[1]) <= 1.7
print('  B6 answer, mean                 %.1f -> %.1f ms (allowed %.1f), latest %.2f -> %.2f s (allowed 1.70), %d points%s' % (an[0], an[1], an[0] + 7.0, max(rs[0]), max(rs[1]), len(rs[1]), '' if ok['B6'] else '   MISSED'))
toes = []
for v in (base, var):
    vals = []
    for s in ('th11', 'th12'):
        vals += list(json.load(open(F + 'res/%s/%s.json' % (v, s))).values())
    toes.append((sum(x > 37 for x in vals), sum(x > 70 for x in vals), max(vals), len(vals)))
ok['B7'] = toes[1][0] <= toes[0][0] + 16 and toes[1][1] <= toes[0][1] + 2
print('  B7 toes and heels over 37 mm    %d -> %d (allowed %d), over 70 mm %d -> %d (allowed %d), worst %.1f -> %.1f mm, %d points%s' % (
    toes[0][0], toes[1][0], toes[0][0] + 16, toes[0][1], toes[1][1], toes[0][1] + 2, toes[0][2], toes[1][2], toes[1][3], '' if ok['B7'] else '   MISSED'))

print('C. protection facing where it moves, HF11, 312 points')
TF = {'over feet': 2, 'over shins': 10, 'over hands': 20, 'over pelvis': 4,
      'mean pelvis': 0.04, 'tenth pelvis': 0.16, 'mean shins': 0.27, 'tenth shins': 0.55, 'mean feet': 0.34, 'tenth feet': 0.57, 'mean hands': 0.17, 'tenth hands': 0.26,
      'worst feet': 9.9, 'worst shins': 3.2, 'worst hands': 2.4, 'worst pelvis': 2.5}
ok['C1'] = all([check('C1', 'over ' + k, fa['over ' + k], fb['over ' + k], TF['over ' + k], 'points', '%d') for k in ('feet', 'shins', 'hands', 'pelvis')])
ok['C2'] = all([check('C2', m + ' ' + k, fa[m + ' ' + k], fb[m + ' ' + k], TF[m + ' ' + k], 'mm', '%.3f') for k in PARTS for m in ('mean', 'tenth')])
ok['C3'] = all([check('C3', 'worst ' + k, fa['worst ' + k], fb['worst ' + k], TF['worst ' + k], 'mm', '%.1f') for k in ('feet', 'shins', 'hands', 'pelvis')])
ok['C4'] = all([check('C4', 'pelvis low', fa['low'], fb['low'], 2, 'points', '%d'), check('C4', 'lowest', fa['lowest'], fb['lowest'], 0.7, 'cm', '%.1f')])

print('  by scene with a fixed heading: flights over 0.2 s, feet + shins + pelvis over the limits, low')
for sc in sorted({n.split(':')[1].rsplit('_', 1)[0] for n in A}):
    def row(d):
        p = [r for n, r in d.items() if n.split(':')[1].rsplit('_', 1)[0] == sc]
        return (sum(r['flight'] > 0.2 for r in p), max(r['flight'] for r in p), sum((r['j_feet'] > 37) + (r['j_shins'] > 37) + (r['j_pelvis'] > 10) for r in p), sum(r['low'] > 0.15 for r in p), 100 * max(r['low'] for r in p))
    a, b = row(A), row(B)
    if a != b:
        print('    %-12s flights %2d -> %2d (longest %.2f -> %.2f s)  over %3d -> %3d  low %2d -> %2d (%.1f -> %.1f cm)' % (sc, a[0], b[0], a[1], b[1], a[2], b[2], a[3], b[3], a[4], b[4]))

print('D. what must not change')
tab = filecmp.cmp(F + 'res/tab/%s_same.csv' % base, F + 'res/tab/%s_same.csv' % var, shallow=False)
print('  D1 the balance table %s' % ('identical' if tab else 'DIFFERENT'))
ok['D1'] = tab


def cadence(path):
    # Steps a minute from the times between landings from 4 s on: the counter of whole steps is too coarse
    rows = list(csv.DictReader(open(path)))
    lands = []
    for k in ('swingL', 'swingR'):
        for i in range(1, len(rows)):
            if float(rows[i - 1][k]) > 0.5 and float(rows[i][k]) < 0.5 and float(rows[i]['t']) >= 4.0:
                lands.append(float(rows[i]['t']))
    lands.sort()
    return 60.0 * (len(lands) - 1) / (lands[-1] - lands[0])


d2 = True
for name in ('side1.08', 'side1.68', 'side3.45', 'side4.15', 'back1.08', 'back3.45'):
    pa, pb = F + 'tmp/h_%s_%s.csv' % (base, name), F + 'tmp/h_%s_%s.csv' % (var, name)
    a = g.analyze(pa, 4.0); b = g.analyze(pb, 4.0)
    ca, cb = cadence(pa), cadence(pb)
    good = abs(cb / ca - 1.0) <= 0.005 and b['feetSep'] >= a['feetSep'] - 0.01
    d2 = d2 and good
    print('  D2 %-9s steps a minute %6.2f -> %6.2f, feet closest %5.1f -> %5.1f cm, shins %4.1f -> %4.1f mm, pelvis %4.1f -> %4.1f mm%s' % (
        name, ca, cb, 100 * a['feetSep'], 100 * b['feetSep'], a['j_shins'], b['j_shins'], a['j_pelvis'], b['j_pelvis'], '' if good else '   MISSED'))
ok['D2'] = d2
import numpy as np
names = [l.strip() for l in open(_M + '/bones.txt')]


def gait(path):
    # How far the pelvis goes up and down per step (cm, mean) and how far a knee is bent as its foot lands (degrees, mean
    # and most), from 4 s on, as round 91 measured them
    rows = list(csv.DictReader(open(path)))
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
    t = d['t']

    def P(n):
        i = names.index(n)
        return np.stack([d['b%dx' % i], d['b%dy' % i], d['b%dz' % i]], 1)
    pel = P('pelvis')[:, 1]
    lands, knees = [], []
    for side, sw in (('L', 'swingL'), ('R', 'swingR')):
        th, sh, fo = P('thigh' + side), P('shin' + side), P('foot' + side)
        a, b = sh - th, fo - sh
        bend = np.degrees(np.arccos(np.clip(np.sum(a * b, 1) / (np.linalg.norm(a, axis=1) * np.linalg.norm(b, axis=1)), -1, 1)))
        for i in range(1, len(t)):
            if d[sw][i - 1] > 0.5 and d[sw][i] < 0.5 and t[i] >= 4.0:
                lands.append(i); knees.append(bend[i])
    lands.sort()
    bobs = [100 * (np.max(pel[a:b]) - np.min(pel[a:b])) for a, b in zip(lands[:-1], lands[1:])]
    return sum(bobs) / len(bobs), sum(knees) / len(knees), max(knees)


d6 = True
for sp in ('3.0', '4.8'):
    a = gait(F + 'tmp/k_%s_%s.csv' % (base, sp)); b = gait(F + 'tmp/k_%s_%s.csv' % (var, sp))
    good = b[0] >= a[0] - 0.2 and b[1] <= a[1] + 1.0
    d6 = d6 and good
    print('  D6 running forward at %s m/s: pelvis up and down per step %.2f -> %.2f cm (allowed %.2f), knee as its foot lands %.1f -> %.1f degrees in the mean (allowed %.1f), most %.1f -> %.1f%s' % (
        sp, a[0], b[0], a[0] - 0.2, a[1], b[1], a[1] + 1.0, a[2], b[2], '' if good else '   MISSED'))
ok['D6'] = d6
print('conditions: ' + ', '.join('%s %s' % (c, 'met' if v else 'MISSED') for c, v in ok.items()))
