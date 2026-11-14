# eval129.py base variant: the conditions A, B, C, D1 and D2 of the rule of round 129 (docs/Regeln/RULE129.txt) from the
# outputs of check129.sh, with the tolerances of tolerance129.py (docs/Regeln/tol129.json)
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import filecmp, json, sys
F = _W + '/'

base, var = sys.argv[1], sys.argv[2]
ok = {}
TOL = json.load(open(_os.path.join(_M, '..', '..', 'docs', 'Regeln', 'tol129.json')))
T, TF, TR = TOL['fixed'], TOL['facing'], TOL['run']
LIM = {'feet': 37.0, 'shins': 37.0, 'hands': 15.0, 'pelvis': 10.0}
PARTS = ('pelvis', 'shins', 'feet', 'hands')


def hset(v):
    d = {}
    for suf in ('h31', 'h32'):
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
    q['excess pelvis'] = sum(max(r[pre + 'pelvis'] - LIM['pelvis'], 0.0) for r in p)
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
    for s in ('hf45', 'hf47'):
        for n, r in json.load(open(F + 'res/%s/%s.json' % (v, s))).items():
            d[s + ':' + n] = r
    return d


FA, FB = fset(base), fset(var)
assert len(FA) == len(FB) == 624 and set(FA) == set(FB)
# two sets facing where it moves: counts get twice the tolerance of one set
TF = {k: (2 * t if k.startswith('over ') or k in ('hard shins', 'excess shins', 'excess pelvis', 'low', 'flights') else t) for k, t in TF.items()}
fa, fb = measures(FA, ''), measures(FB, '')
OVER = ['over ' + k for k in ('feet', 'shins', 'hands', 'pelvis')]
SIZE = [m + ' ' + k for k in PARTS for m in ('mean', 'tenth')]
HARD = ['worst ' + k for k in ('feet', 'shins', 'hands', 'pelvis')]


RUN = ['startL30', 'startB30', 'switchLR30', 'fwdToL30', 'LToBack30', 'backToFwd30', 'stopL30', 'stopB30']
BRAKED = ('swing', 'land 0', 'land 1', 'land 2')


def runs(v):
    # the eight changes at a run with a fixed heading of where129.py: what the hardest shin jumps beyond 37 mm where it
    # is at a leg that swings or lands, at how many points that is, at how many points a shin jumps over 37 mm at all,
    # and what those sum to beyond 37 mm
    rows = []
    for s in ('h31', 'h32'):
        for scene in RUN:
            rows += list(json.load(open(F + 'res/%s/where_%s_%s.json' % (v, scene, s))).values())
    assert len(rows) == 384
    over = [r for r in rows if r['jump'] > 37.0]
    braked = [r for r in over if r['does'] in BRAKED]
    return sum(r['jump'] - 37.0 for r in braked), len(braked), len(over), sum(r['jump'] - 37.0 for r in over)


ra, rb = runs(base), runs(var)
print('A. gain, the eight changes at a run with a fixed heading, 384 points: %s -> %s' % (base, var))
ok['A1'] = rb[0] < ra[0] - TR['braked excess']
print('  A1 shins beyond 37 mm at a leg that swings or lands, sum   %.0f -> %.0f mm (needed below %.0f), at %d -> %d points%s' % (
    ra[0], rb[0], ra[0] - TR['braked excess'], ra[1], rb[1], '' if ok['A1'] else '   MISSED'))
ok['A2'] = rb[3] < ra[3] and rb[2] <= ra[2]
print('  A2 shins beyond 37 mm there, sum   %.0f -> %.0f mm (needed below %.0f), over 37 mm %d -> %d points (allowed %d)%s' % (
    ra[3], rb[3], ra[3], ra[2], rb[2], ra[2], '' if ok['A2'] else '   MISSED'))


def rhythm(v):
    d = {}
    for st in ('hf45', 'hf47'):
        for n, r in json.load(open(F + 'res/%s/rhythm_%s.json' % (v, st))).items():
            d[st + ':' + n] = r
    return d


RA, RB = rhythm(base), rhythm(var)
assert len(RA) == len(RB) == 288 and set(RA) == set(RB)
ua, ub = sum(r['uneven'] for r in RA.values()), sum(r['uneven'] for r in RB.values())
print('B. protection with a fixed heading, H31 and H32, 1008 points')
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
    for s in ('th31', 'th32'):
        vals += list(json.load(open(F + 'res/%s/%s.json' % (v, s))).values())
    toes.append((sum(x > 37 for x in vals), sum(x > 70 for x in vals), max(vals), len(vals)))
ok['B7'] = toes[1][0] <= toes[0][0] + T['toes'] and toes[1][1] <= toes[0][1] + T['toes70']
print('  B7 toes and heels over 37 mm    %d -> %d (allowed %d), over 70 mm %d -> %d (allowed %d), worst %.1f -> %.1f mm, %d points%s' % (
    toes[0][0], toes[1][0], toes[0][0] + T['toes'], toes[0][1], toes[1][1], toes[0][1] + T['toes70'], toes[0][2], toes[1][2], toes[1][3], '' if ok['B7'] else '   MISSED'))
ok['B8'] = all([check('B8', 'flights over 0.2 s', qa['flights'], qb['flights'], T['flights'], 'points', '%d'), check('B8', 'longest flight, mean', qa['flight'], qb['flight'], T['flight'], 'ms', '%.1f')])
ok['B9'] = group('B9', qa, qb, T, ['excess shins', 'excess pelvis'], 'mm', '%.0f')

print('C. protection facing where it moves, HF45 and HF47, 624 points')
ok['C1'] = group('C1', fa, fb, TF, OVER, 'points', '%d')
ok['C2'] = group('C2', fa, fb, TF, SIZE, 'mm', '%.3f')
ok['C3'] = group('C3', fa, fb, TF, HARD, 'mm', '%.1f')
ok['C4'] = all([check('C4', 'pelvis low', fa['low'], fb['low'], TF['low'], 'points', '%d'), check('C4', 'lowest', fa['lowest'], fb['lowest'], TF['lowest'], 'cm', '%.1f')])
ok['C5'] = all([check('C5', 'flights over 0.2 s', fa['flights'], fb['flights'], TF['flights'], 'points', '%d'), check('C5', 'longest flight, mean', fa['flight'], fb['flight'], TF['flight'], 'ms', '%.1f')])
ok['C6'] = group('C6', fa, fb, TF, ['excess shins', 'excess pelvis'], 'mm', '%.0f')
ok['C7'] = ub <= ua
print('  C7 changes that end at a run after which the feet lift together  %d -> %d of %d (allowed %d)%s' % (ua, ub, len(RA), ua, '' if ok['C7'] else '   MISSED'))

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


def steady(path):
    # the rows of a steady run from 5 s on
    return [line for line in open(path).read().split('\n')[1:] if line and float(line.split(',')[0]) >= 5.0]


STEADY = ['f1.2', 'f1.6', 'f2.2', 'f3.0', 'f3.9', 'f4.5', 'f4.8', 'f4.51', 'f4.66', 's1.31', 's1.36', 's1.6', 's3.61', 's4.33',
          's4.5', 'b1.31', 'b1.36', 'b1.6', 'b3.61', 'b4.33', 'b4.5', 'd2.0', 'd4.0']
same = [name for name in STEADY if steady(F + 'tmp/s_%s_%s.csv' % (base, name)) == steady(F + 'tmp/s_%s_%s.csv' % (var, name))]
ok['D2'] = len(same) == len(STEADY)
print('  D2 steady runs the same to the byte from 5 s on: %d of %d%s' % (
    len(same), len(STEADY), '' if ok['D2'] else ', not ' + ' '.join(n for n in STEADY if n not in same) + '   MISSED'))
print('conditions: ' + ', '.join('%s %s' % (c, 'met' if v else 'MISSED') for c, v in ok.items()))
