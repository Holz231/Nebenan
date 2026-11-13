# tolerance119.py base: the tolerances of the rule of round 119, measured on base before the rule is frozen: how much
# each quantity differs between two sets of similar speeds, from three pairs each. With a fixed heading the slow pairs
# h6 h1, h1 h10, h10 h12 and the fast pairs h11 h9, h9 h2, h2 h5: for the total of a fast and a slow set the largest
# spread of the slow pairs plus the largest of the fast pairs, for means half of that, for the hardest values the
# largest spread of all six. Facing where it moves the pairs hf7 hf9, hf9 hf11, hf7 hf11: the largest spread. Counts get
# at least 2 points, the hardest jumps at least 2 mm. Prints the table and writes out/tol119.json.
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
import json
base = _sys.argv[1]
PARTS = ('feet', 'shins', 'hands', 'pelvis')
LIM = {'feet': 37.0, 'shins': 37.0, 'hands': 15.0, 'pelvis': 10.0}


def measures(p, pre):
    q = {}
    for k in PARTS:
        q['over ' + k] = sum(r[pre + k] > LIM[k] for r in p)
        q['worst ' + k] = max(r[pre + k] for r in p)
        q['mean ' + k] = sum(r['m_' + k] for r in p) / len(p)
        q['tenth ' + k] = sum(r['t_' + k] for r in p) / len(p)
    q['low'] = sum(r['low'] > 0.15 for r in p)
    q['lowest'] = 100 * max(r['low'] for r in p)
    q['flights'] = sum(r['flight'] > 0.2 for r in p)
    q['flight'] = 1000 * sum(r['flight'] for r in p) / len(p)
    return q


def fixed(s):
    p = list(json.load(open(_W + '/res/%s_%s/sweep.json' % (base, s))).values())
    q = measures(p, 'j_')
    q['crossed'] = sum(r['feetSep'] < 0 for r in p)
    rs = [r['resp'] for r in p if r['resp'] > 0]
    q['answer'] = 1000 * sum(rs) / len(rs)
    t = list(json.load(open(_W + '/res/%s/t%s.json' % (base, s))).values())
    q['toes'] = sum(x > 37 for x in t)
    q['toes70'] = sum(x > 70 for x in t)
    return q


def facing(s):
    return measures(list(json.load(open(_W + '/res/%s/%s.json' % (base, s))).values()), '')


SLOW, FAST = (('h6', 'h1'), ('h1', 'h10'), ('h10', 'h12')), (('h11', 'h9'), ('h9', 'h2'), ('h2', 'h5'))
FACE = (('hf7', 'hf9'), ('hf9', 'hf11'), ('hf7', 'hf11'))
QF = {s: fixed(s) for s in ('h1', 'h6', 'h10', 'h12', 'h2', 'h5', 'h9', 'h11')}
QA = {s: facing(s) for s in ('hf7', 'hf9', 'hf11')}
COUNTS = ['over ' + k for k in PARTS] + ['low', 'flights', 'crossed', 'toes', 'toes70']
MEANS = ['mean ' + k for k in PARTS] + ['tenth ' + k for k in PARTS] + ['flight', 'answer']
WORST = ['worst ' + k for k in PARTS] + ['lowest']
tol = {'fixed': {}, 'facing': {}}
print('with a fixed heading          ' + ''.join('%8s' % s for s in ('h6', 'h1', 'h10', 'h12', 'h11', 'h9', 'h2', 'h5')) + '   slow    fast   tolerance')
for k in QF['h1']:
    slow = max(abs(QF[a][k] - QF[b][k]) for a, b in SLOW)
    fast = max(abs(QF[a][k] - QF[b][k]) for a, b in FAST)
    if k in COUNTS:
        t = max(slow + fast, 2)
    elif k in MEANS:
        t = (slow + fast) / 2
    else:
        t = max(slow, fast, 2.0 if k != 'lowest' else 0.0)
    tol['fixed'][k] = t
    print('%-30s' % k + ''.join('%8.2f' % QF[s][k] for s in ('h6', 'h1', 'h10', 'h12', 'h11', 'h9', 'h2', 'h5')) + '%7.2f %7.2f %9.2f' % (slow, fast, t))
print('facing where it moves         ' + ''.join('%8s' % s for s in ('hf7', 'hf9', 'hf11')) + '   tolerance')
for k in QA['hf7']:
    s = max(abs(QA[a][k] - QA[b][k]) for a, b in FACE)
    t = max(s, 2) if k in COUNTS else (max(s, 2.0) if k in WORST and k != 'lowest' else s)
    tol['facing'][k] = t
    print('%-30s' % k + ''.join('%8.2f' % QA[x][k] for x in ('hf7', 'hf9', 'hf11')) + '%9.2f' % t)
json.dump(tol, open(_W + '/tol119.json', 'w'), indent=1, sort_keys=True)
