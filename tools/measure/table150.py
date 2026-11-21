# table150.py base var [show]: two built variants on the development table of devtable.sh, scene by scene: with a fixed
# heading the 19 speed sets with toes and heels, 9 576 points, and facing where it moves five sets, 1 560 points. For
# feet, shins, pelvis, hands and toes and heels at how many points the hardest jump lies over its limit (37, 37, 10, 15
# and 37 mm) and what those sum to beyond it, as points/mm base -> var, and at how many points the pelvis goes low.
# With "show" every scene that differs, then the total, the part of it that is starts from standing, and the total
# without them: all 24 points of a start are equal, so a start that moves by half a millimetre moves a sum 24-fold and
# looks like a broad loss or gain, see round 150 in docs/Roadmap.md. The starts themselves: starts150.py. SETS and
# FACING name other sets, as the fresh ones of a check.
import collections, json, os, sys

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
FIXED = os.environ.get('SETS', 'h1 h2 h5 h6 h9 h10 h11 h12 h13 h14 h15 h29 h30 h31 h32 h33 h34 h35 h36').split()
FACING = os.environ.get('FACING', 'hf9 hf11 hf15 hf41 hf53').split()
LIMIT = {'feet': 37.0, 'shins': 37.0, 'pelvis': 10.0, 'hands': 15.0, 'toes': 37.0}


def load(variant):
    fixed, facing = collections.defaultdict(list), collections.defaultdict(list)
    for s in FIXED:
        sweep = json.load(open(os.path.join(W, 'res', '%s_%s' % (variant, s), 'sweep.json')))
        toes = json.load(open(os.path.join(W, 'res', variant, 't%s.json' % s)))
        for name, r in sweep.items():
            r = dict(r)
            r['j_toes'] = toes[name]
            fixed[name.rsplit('_', 1)[0]].append(r)
    for s in FACING:
        for name, r in json.load(open(os.path.join(W, 'res', variant, '%s.json' % s))).items():
            facing[name.rsplit('_', 1)[0]].append(r)
    return fixed, facing


def measure(rows, pre):
    out = {}
    for k, limit in LIMIT.items():
        if pre + k in rows[0]:
            out[k] = (sum(r[pre + k] > limit for r in rows), sum(max(r[pre + k] - limit, 0.0) for r in rows))
    out['low'] = (sum(r['low'] > 0.15 for r in rows), 0.0)
    return out


def line(head, sums):
    return head + ' '.join('%s %d/%.0f->%d/%.0f' % (k, t[0], t[2], t[1], t[3]) for k, t in sums.items())


if __name__ == '__main__':
    base, var = sys.argv[1], sys.argv[2]
    show = len(sys.argv) > 3
    A, B = load(base), load(var)
    for title, a, b, pre in (('fixed', A[0], B[0], 'j_'), ('facing', A[1], B[1], '')):
        assert set(a) == set(b)
        total = collections.defaultdict(lambda: [0, 0, 0.0, 0.0])
        starts = collections.defaultdict(lambda: [0, 0, 0.0, 0.0])
        for scene in a:
            qa, qb = measure(a[scene], pre), measure(b[scene], pre)
            for k in qa:
                for t in (total, starts) if scene.startswith('start') else (total,):
                    t[k][0] += qa[k][0]
                    t[k][1] += qb[k][0]
                    t[k][2] += qa[k][1]
                    t[k][3] += qb[k][1]
            differ = [k for k in qa if abs(qa[k][1] - qb[k][1]) > 0.5 or qa[k][0] != qb[k][0]]
            if differ and show:
                print(line('%-6s %-12s ' % (title, scene), {k: (qa[k][0], qb[k][0], qa[k][1], qb[k][1]) for k in differ}))
        rest = {k: [t[i] - starts[k][i] for i in range(4)] for k, t in total.items()}
        print(line('%-6s TOTAL        ' % title, total))
        print(line('%-6s the starts   ' % title, starts))
        print(line('%-6s without them ' % title, rest))
