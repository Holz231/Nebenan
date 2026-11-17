# eval137.py base variant: the conditions A, B, C, D1 and D2 of the rule of round 137 (docs/Regeln/RULE137.txt)
# from the outputs of check137.sh, with the tolerances of tolerance137.py (docs/Regeln/tol137.json)
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import filecmp, json, sys
F = _W + '/'

base, var = sys.argv[1], sys.argv[2]
ok = {}
TOL = json.load(open(_os.path.join(_M, '..', '..', 'docs', 'Regeln', 'tol137.json')))
T, TF, TR = TOL['fixed'], TOL['facing'], TOL['run']
LIM = {'feet': 37.0, 'shins': 37.0, 'hands': 15.0, 'pelvis': 10.0}
PARTS = ('pelvis', 'shins', 'feet', 'hands')


def hset(v):
    d = {}
    for suf in ('h59', 'h60'):
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
    for s in ('hf77', 'hf79'):
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


EIGHT = ['startL30', 'startB30', 'switchLR30', 'fwdToL30', 'LToBack30', 'backToFwd30', 'stopL30', 'stopB30']
TH = TOL['hops']


SIX = ('h59', 'h60', 'h61', 'h62', 'h63', 'h64')


def hops(v):
    # the feet that lift again at once after they landed in the eight changes at a run with a fixed heading of
    # hops131.py: how many, how many of them with the shin over 37 mm, and what those sum to beyond 37 mm
    found = []
    for s in SIX:
        for scene in EIGHT:
            for at in json.load(open(F + 'res/%s/hops_%s_%s.json' % (v, scene, s))).values():
                found += at
    hard = [h for h in found if h['jump'] > 37.0]
    return len(found), len(hard), sum(h['jump'] - 37.0 for h in hard)


def runs(v):
    # the eight changes at a run with a fixed heading of where129.py: at how many points the hardest shin jumps over
    # 37 mm and what those sum to beyond 37 mm
    rows = []
    for s in SIX:
        for scene in EIGHT:
            rows += list(json.load(open(F + 'res/%s/where_%s_%s.json' % (v, scene, s))).values())
    assert len(rows) == 1152
    over = [r for r in rows if r['jump'] > 37.0]
    return len(over), sum(r['jump'] - 37.0 for r in over)


ha, hb = hops(base), hops(var)
ra, rb = runs(base), runs(var)
def flips(v, scenes):
    # the feet of flips135.py on the ground as the legs change between going forward and going backward: how many with
    # the shin over 37 mm, what those sum to beyond 37 mm, and the hardest
    found = []
    for s in SIX:
        for scene in scenes:
            for at in json.load(open(F + 'res/%s/flips_%s_%s.json' % (v, scene, s))).values():
                found += at
    hard = [h for h in found if h['jump'] > 37.0]
    return len(hard), sum(h['jump'] - 37.0 for h in hard), max([0.0] + [h['jump'] for h in found]), len(found)


TP = TOL['flips']
qa8, qb8 = flips(base, EIGHT), flips(var, EIGHT)


def pitch(v, scenes=EIGHT):
    # the swings of pitch136.py in the eight changes at a run with a fixed heading during which the gait falls toward
    # walking: how many turn their foot more than 0.17 rad toward its heel in one update, how many of those have the
    # shin over 37 mm and what those sum to beyond it, the fastest turn toward the heel, and over all such swings how
    # many with the shin over 37 mm, what those sum to beyond it, and how many swings there are
    found = []
    for s in SIX:
        for scene in scenes:
            for at in json.load(open(F + 'res/%s/pitch_%s_%s.json' % (v, scene, s))).values():
                found += [h for h in at if h['counted']]
    fast = [h for h in found if h['heelward'] > 0.17]
    hard = [h for h in found if h['jump'] > 37.0]
    return (len(fast), sum(h['jump'] > 37.0 for h in fast), sum(max(h['jump'] - 37.0, 0.0) for h in fast),
            max([0.0] + [h['heelward'] for h in found]), len(hard), sum(h['jump'] - 37.0 for h in hard), len(found))


sa, sb = pitch(base), pitch(var)
TS = TOL['pitch']
print('A. gain: %s -> %s' % (base, var))
def lifts137(v):
    # the lifts of lifts137.py in the eight changes at a run with a fixed heading: of those while the figure brakes, how
    # many, how many with the shin over 37 mm, what those sum to beyond it and the hardest; of all with the gait walking
    # while its spring runs, how many with the shin over 37 mm and what those sum to beyond it; and how high the feet of
    # the lifts while braking came at most
    found = []
    for s in SIX:
        for scene in EIGHT:
            for at in json.load(open(F + 'res/%s/lifts137_%s_%s.json' % (v, scene, s))).values():
                found += [h for h in at if h['spring'] - h['lifted'] > 0.5]
    braking = [h for h in found if h['counted']]
    hard = [h for h in braking if h['jump'] > 37.0]
    gap = [h for h in found if h['jump'] > 37.0]
    return (len(braking), len(hard), sum(h['jump'] - 37.0 for h in hard), max([0.0] + [h['jump'] for h in braking]),
            len(gap), sum(h['jump'] - 37.0 for h in gap), max([0.0] + [h['top'] for h in braking]))


la7, lb7 = lifts137(base), lifts137(var)
ok['A1'] = lb7[1] == 0 and la7[1] >= 2
print('  A1 the eight changes at a run on three pairs of sets, 1152 points: feet that lift while the figure brakes, the gait '
      'walking and its spring running, with the shin over 37 mm   %d -> %d (needed none, and before at least 2), beyond it '
      '%.0f -> %.0f mm, the hardest %.1f -> %.1f mm, the highest foot %.0f -> %.0f mm, %d lifts%s' % (
          la7[1], lb7[1], la7[2], lb7[2], la7[3], lb7[3], la7[6], lb7[6], lb7[0], '' if ok['A1'] else '   MISSED'))
ok['A2'] = lb7[5] < la7[5]
print('  A2 there all feet that lift with the gait walking and its spring running: with the shin over 37 mm   %d -> %d, their '
      'sum beyond it %.1f -> %.1f mm (needed smaller)%s' % (la7[4], lb7[4], la7[5], lb7[5], '' if ok['A2'] else '   MISSED'))
ok['A3'] = rb[1] <= ra[1] + 3.0 * TH['excess eight']
print('  A3 the eight changes at a run with a fixed heading on three pairs of sets, 1152 points: the hardest shin beyond 37 mm, sum   %.0f -> %.0f mm (allowed %.0f)%s' % (
    ra[1], rb[1], ra[1] + 3.0 * TH['excess eight'], '' if ok['A3'] else '   MISSED'))
ok['A4'] = rb[0] <= ra[0] + 3 * TH['over eight']
print('  A4 the hardest shin over 37 mm there at   %d -> %d points (allowed %d)%s' % (
    ra[0], rb[0], ra[0] + 3 * TH['over eight'], '' if ok['A4'] else '   MISSED'))
ok['A5'] = fb['over shins'] <= fa['over shins'] + TF['over shins']
print('  A5 facing where it moves, 624 points: shins over 37 mm at   %d -> %d points (allowed %d)%s' % (
    fa['over shins'], fb['over shins'], fa['over shins'] + TF['over shins'], '' if ok['A5'] else '   MISSED'))
ok['A6'] = hb[0] <= ha[0] + 3 * TH['hops'] and hb[1] <= ha[1] + 3 * TH['hard hops']
print('  A6 feet that lift again at once after they landed   %d -> %d (allowed %d), of them with the shin over 37 mm %d -> %d (allowed %d)%s' % (
    ha[0], hb[0], ha[0] + 3 * TH['hops'], ha[1], hb[1], ha[1] + 3 * TH['hard hops'], '' if ok['A6'] else '   MISSED'))
ok['A7'] = qb8[0] <= qa8[0] + 3 * TP['hard flips'] and qb8[1] <= qa8[1] + 3.0 * TP['flip excess']
print('  A7 feet on the ground as the legs change, all eight changes: with the shin over 37 mm   %d -> %d (allowed %d), beyond it %.0f -> %.0f mm (allowed %.0f)%s' % (
    qa8[0], qb8[0], qa8[0] + 3 * TP['hard flips'], qa8[1], qb8[1], qa8[1] + 3.0 * TP['flip excess'], '' if ok['A7'] else '   MISSED'))


def lifts(v):
    # the lifts of lifts134.py in the eight changes at a run with a fixed heading, a foot not over its toes that lifts
    # while the figure brakes: how many with the shin over 37 mm and what those sum to beyond 37 mm, and of them those
    # with the legs going forward: how many, what they sum to beyond 37 mm and the hardest
    found = []
    for s in SIX:
        for scene in EIGHT:
            for at in json.load(open(F + 'res/%s/lifts_%s_%s.json' % (v, scene, s))).values():
                found += [h for h in at if h['counted']]
    hard = [h for h in found if h['jump'] > 37.0]
    fwd = [h for h in found if h['forward']]
    return (len(hard), sum(h['jump'] - 37.0 for h in hard), len(fwd), sum(max(h['jump'] - 37.0, 0.0) for h in fwd),
            max([0.0] + [h['jump'] for h in fwd]))


la, lb = lifts(base), lifts(var)
TL = TOL['lifts']
ok['A8'] = lb[0] <= la[0] + 3 * TL['hard lifts'] and lb[1] <= la[1] + 3.0 * TL['lift excess']
print('  A8 a foot not over its toes that lifts while the figure brakes: with the shin over 37 mm   %d -> %d (allowed %d), their sum beyond it %.0f -> %.0f mm (allowed %.0f)%s' % (
    la[0], lb[0], la[0] + 3 * TL['hard lifts'], la[1], lb[1], la[1] + 3.0 * TL['lift excess'], '' if ok['A8'] else '   MISSED'))
print('     of them with the legs going forward   %d -> %d lifts, beyond 37 mm %.0f -> %.0f mm, the hardest %.1f -> %.1f mm' % (
    la[2], lb[2], la[3], lb[3], la[4], lb[4]))
ok['A9'] = sb[4] <= sa[4] + 3 * TS['hard swings'] and sb[5] <= sa[5] + 3.0 * TS['swing excess']
print('  A9 all swings during which the gait falls toward walking: with the shin over 37 mm   %d -> %d (allowed %d), their sum '
      'beyond it %.0f -> %.0f mm (allowed %.0f)%s' % (sa[4], sb[4], sa[4] + 3 * TS['hard swings'], sa[5], sb[5],
                                                      sa[5] + 3.0 * TS['swing excess'], '' if ok['A9'] else '   MISSED'))


def rhythm(v):
    d = {}
    for st in ('hf77', 'hf79'):
        for n, r in json.load(open(F + 'res/%s/rhythm_%s.json' % (v, st))).items():
            d[st + ':' + n] = r
    return d


RA, RB = rhythm(base), rhythm(var)
assert len(RA) == len(RB) == 288 and set(RA) == set(RB)
ua, ub = sum(r['uneven'] for r in RA.values()), sum(r['uneven'] for r in RB.values())
print('B. protection with a fixed heading, H59 and H60, 1008 points')
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
    for s in ('th59', 'th60'):
        vals += list(json.load(open(F + 'res/%s/%s.json' % (v, s))).values())
    toes.append((sum(x > 37 for x in vals), sum(x > 70 for x in vals), max(vals), len(vals)))
ok['B7'] = toes[1][0] <= toes[0][0] + T['toes'] and toes[1][1] <= toes[0][1] + T['toes70']
print('  B7 toes and heels over 37 mm    %d -> %d (allowed %d), over 70 mm %d -> %d (allowed %d), worst %.1f -> %.1f mm, %d points%s' % (
    toes[0][0], toes[1][0], toes[0][0] + T['toes'], toes[0][1], toes[1][1], toes[0][1] + T['toes70'], toes[0][2], toes[1][2], toes[1][3], '' if ok['B7'] else '   MISSED'))
ok['B8'] = all([check('B8', 'flights over 0.2 s', qa['flights'], qb['flights'], T['flights'], 'points', '%d'), check('B8', 'longest flight, mean', qa['flight'], qb['flight'], T['flight'], 'ms', '%.1f')])
ok['B9'] = group('B9', qa, qb, T, ['excess shins', 'excess pelvis'], 'mm', '%.0f')

print('C. protection facing where it moves, HF77 and HF79, 624 points')
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


STEADY = ['f1.2', 'f1.6', 'f2.2', 'f3.0', 'f3.9', 'f4.5', 'f4.8', 'f4.735', 'f4.555', 's1.225', 's1.485', 's1.6', 's3.495', 's4.365',
          's4.5', 's4.8', 'b1.225', 'b1.485', 'b1.6', 'b3.495', 'b4.365', 'b4.5', 'b4.8', 'd2.0', 'd4.0']
same = [name for name in STEADY if steady(F + 'tmp/s_%s_%s.csv' % (base, name)) == steady(F + 'tmp/s_%s_%s.csv' % (var, name))]
ok['D2'] = len(same) == len(STEADY)
print('  D2 steady runs the same to the byte from 5 s on: %d of %d%s' % (
    len(same), len(STEADY), '' if ok['D2'] else ', not ' + ' '.join(n for n in STEADY if n not in same) + '   MISSED'))
print('conditions: ' + ', '.join('%s %s' % (c, 'met' if v else 'MISSED') for c, v in ok.items()))
