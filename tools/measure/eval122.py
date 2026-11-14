# eval122.py base variant: the conditions A, B, C, D1, D2 and D6 of the rule of round 122 (docs/Regeln/RULE122.txt) from
# the outputs of check122.sh, with the tolerances of tolerance122.py (docs/Regeln/tol122.json)
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, filecmp, json, sys
F = _W + '/'
import gait100 as g

base, var = sys.argv[1], sys.argv[2]
ok = {}
TOL = json.load(open(_os.path.join(_M, '..', '..', 'docs', 'Regeln', 'tol122.json')))
T, TF = TOL['fixed'], TOL['facing']
LIM = {'feet': 37.0, 'shins': 37.0, 'hands': 15.0, 'pelvis': 10.0}
PARTS = ('pelvis', 'shins', 'feet', 'hands')


def hset(v):
    d = {}
    for suf in ('h21', 'h22'):
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
    q['hard shins'] = sum(r[pre + 'shins'] > 50.0 for r in p)
    q['excess shins'] = sum(max(r[pre + 'shins'] - LIM['shins'], 0.0) for r in p)
    q['low'] = sum(r['low'] > 0.15 for r in p)
    q['lowest'] = 100 * max(r['low'] for r in p)
    q['flights'] = sum(r['flight'] > 0.2 for r in p)
    q['flight'] = 1000 * sum(r['flight'] for r in p) / len(p)
    return q


def check(name, label, a, b, tol, unit, fmt='%.2f'):
    good = b <= a + tol
    print(('  %s %-28s ' + fmt + ' -> ' + fmt + ' (allowed ' + fmt + ' %s)%s') % (name, label, a, b, a + tol, unit, '' if good else '   MISSED'))
    return good


def group(name, qa, qb, tol, keys, unit, fmt):
    return all([check(name, k, qa[k], qb[k], tol[k], unit, fmt) for k in keys])


A, B = hset(base), hset(var)
assert len(A) == len(B) == 1008 and set(A) == set(B)
qa, qb = measures(A, 'j_'), measures(B, 'j_')
def fset(v):
    d = {}
    for s in ('hf25', 'hf27'):
        for n, r in json.load(open(F + 'res/%s/%s.json' % (v, s))).items():
            d[s + ':' + n] = r
    return d


FA, FB = fset(base), fset(var)
assert len(FA) == len(FB) == 624 and set(FA) == set(FB)
# two sets facing where it moves: counts get twice the tolerance of one set
TF = {k: (2 * t if k.startswith('over ') or k in ('hard shins', 'excess shins', 'low', 'flights') else t) for k, t in TF.items()}
fa, fb = measures(FA, ''), measures(FB, '')
OVER = ['over ' + k for k in ('feet', 'shins', 'hands', 'pelvis')]
SIZE = [m + ' ' + k for k in PARTS for m in ('mean', 'tenth')]
HARD = ['worst ' + k for k in ('feet', 'shins', 'hands', 'pelvis')]

print('A. gain: %s -> %s' % (base, var))
ok['A1'] = fb['excess shins'] < fa['excess shins'] - TF['excess shins']
print('  A1 facing, shins beyond 37 mm, sum  %.0f -> %.0f mm (needed below %.0f)%s' % (fa['excess shins'], fb['excess shins'], fa['excess shins'] - TF['excess shins'], '' if ok['A1'] else '   MISSED'))
print('     shins over 50 mm                %d -> %d, for the record' % (fa['hard shins'], fb['hard shins']))
print('B. protection with a fixed heading, H21 and H22, 1008 points')
ok['B1'] = group('B1', qa, qb, T, OVER, 'points', '%d')
ok['B2'] = group('B2', qa, qb, T, SIZE, 'mm', '%.3f')
ok['B3'] = group('B3', qa, qb, T, HARD, 'mm', '%.1f')
ok['B4'] = all([check('B4', 'pelvis low', qa['low'], qb['low'], T['low'], 'points', '%d'), check('B4', 'lowest', qa['lowest'], qb['lowest'], T['lowest'], 'cm', '%.1f')])
cr = [(sum(r['feetSep'] < 0 for r in d.values()), 100 * min(r['feetSep'] for r in d.values())) for d in (A, B)]
ok['B5'] = cr[1][0] <= cr[0][0] + T['crossed'] and cr[1][1] >= -3.0
print('  B5 crossed feet                 %d -> %d (allowed %d), deepest %.1f -> %.1f cm (allowed -3.0)%s' % (cr[0][0], cr[1][0], cr[0][0] + T['crossed'], cr[0][1], cr[1][1], '' if ok['B5'] else '   MISSED'))
rs = [[r['resp'] for r in d.values() if r['resp'] > 0] for d in (A, B)]
an = [1000 * sum(x) / len(x) for x in rs]
ok['B6'] = an[1] <= an[0] + T['answer'] and max(rs[1]) <= 1.7
print('  B6 answer, mean                 %.1f -> %.1f ms (allowed %.1f), latest %.2f -> %.2f s (allowed 1.70), %d points%s' % (an[0], an[1], an[0] + T['answer'], max(rs[0]), max(rs[1]), len(rs[1]), '' if ok['B6'] else '   MISSED'))
toes = []
for v in (base, var):
    vals = []
    for s in ('th21', 'th22'):
        vals += list(json.load(open(F + 'res/%s/%s.json' % (v, s))).values())
    toes.append((sum(x > 37 for x in vals), sum(x > 70 for x in vals), max(vals), len(vals)))
ok['B7'] = toes[1][0] <= toes[0][0] + T['toes'] and toes[1][1] <= toes[0][1] + T['toes70']
print('  B7 toes and heels over 37 mm    %d -> %d (allowed %d), over 70 mm %d -> %d (allowed %d), worst %.1f -> %.1f mm, %d points%s' % (
    toes[0][0], toes[1][0], toes[0][0] + T['toes'], toes[0][1], toes[1][1], toes[0][1] + T['toes70'], toes[0][2], toes[1][2], toes[1][3], '' if ok['B7'] else '   MISSED'))
ok['B8'] = all([check('B8', 'flights over 0.2 s', qa['flights'], qb['flights'], T['flights'], 'points', '%d'), check('B8', 'longest flight, mean', qa['flight'], qb['flight'], T['flight'], 'ms', '%.1f')])

print('C. protection facing where it moves, HF25 and HF27, 624 points')
ok['C1'] = group('C1', fa, fb, TF, OVER, 'points', '%d')
ok['C2'] = group('C2', fa, fb, TF, SIZE, 'mm', '%.3f')
ok['C3'] = group('C3', fa, fb, TF, HARD, 'mm', '%.1f')
ok['C4'] = all([check('C4', 'pelvis low', fa['low'], fb['low'], TF['low'], 'points', '%d'), check('C4', 'lowest', fa['lowest'], fb['lowest'], TF['lowest'], 'cm', '%.1f')])
ok['C5'] = all([check('C5', 'flights over 0.2 s', fa['flights'], fb['flights'], TF['flights'], 'points', '%d'), check('C5', 'longest flight, mean', fa['flight'], fb['flight'], TF['flight'], 'ms', '%.1f')])

print('  by scene facing where it moves: over the limits feet, shins, hands, pelvis, the hardest shin')
for sc in sorted({n.split(':')[1].rsplit('_', 1)[0] for n in FA}):
    def row(d):
        p = [r for n, r in d.items() if n.split(':')[1].rsplit('_', 1)[0] == sc]
        return tuple(sum(r[k] > LIM[k] for r in p) for k in ('feet', 'shins', 'hands', 'pelvis')) + (max(r['shins'] for r in p),)
    a, b = row(FA), row(FB)
    if a != b:
        print('    %-12s %2d %2d %2d %2d (%5.1f mm) -> %2d %2d %2d %2d (%5.1f mm)' % ((sc,) + a + b))

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
for name in ('side1.28', 'side1.38', 'side3.72', 'side4.28', 'back1.28', 'back3.72'):
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
