# eval179.py old new group [group...]: the rule of round 179, docs/Regeln/RULE179.txt, for rolling out into a turn about
# at a walk, on two states that check177.sh (walk176.py at four nudges) and check170.sh have measured on the groups. Z1
# the capsule takes about a step to come to rest before it turns about, Z2 the pelvis sinks on fewer walking courses, Z3
# the legs are no more inside each other in turns; S1 no bone jumps harder in a turn about; S2 compare170.py names no
# quantity worse apart from the three prices; S3 stopping and starting stay as they are; P the prices, when it faces
# back, the latest of all turns, and how much of the speed of a stick it follows on the courses.
import json, os, re, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))
PRICES = ('faced max', 'faced 180 walk', 'along walk')


def rows(state, groups, scene, nudge=None):
    out = []
    for g in groups:
        out += [r for r in json.load(open(os.path.join(W, 'res', state, 'walk176.%s.json' % g)))
                if r['scene'] == scene and (nudge is None or r['nudge'] == nudge)]
    return out


def compared(old, new, group):
    """what compare170.py says of a group: per quantity the medians of old and new and its verdict"""
    out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), old, new, group], capture_output=True, text=True,
                         check=True).stdout
    res = {}
    for line in out.split('\n'):
        m = re.match(r'(.+?)\s+old .* median\s+([-0-9.]+) \| new .* median\s+([-0-9.]+) \| (.*)$', line)
        if m:
            res[m.group(1).strip()] = (float(m.group(2)), float(m.group(3)), m.group(4).strip())
    return res


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-92s %s' % (name, text, 'held' if passed else 'MISSED'))

    def med(r, k):
        return float(np.median([x[k] for x in r]))

    a, b = rows(old, groups, 'turn180'), rows(new, groups, 'turn180')
    print('%d and %d turns about at a walk on %s' % (len(a), len(b), ' '.join(groups)))
    line('Z1', 'capsule goes on for %.2f s, old %.2f s: 0.25 to 0.50 s' % (med(b, 'halt'), med(a, 'halt')),
         0.25 <= med(b, 'halt') <= 0.50)
    cmp = [compared(old, new, g) for g in groups]

    def mean(name, k):
        return float(np.mean([c[name][k] for c in cmp]))

    line('Z2', 'walking courses with the pelvis more than 30 mm low: %.1f, old %.1f, at least 3 fewer' % (
        mean('under walk', 1), mean('under walk', 0)), mean('under walk', 1) <= mean('under walk', 0) - 3.0)
    line('Z3', 'legs inside each other in turns at a walk in %.2f %% of the updates, old %.2f %%, no more' % (
        mean('turns walk share', 1), mean('turns walk share', 0)), mean('turns walk share', 1) <= mean('turns walk share', 0))
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x = [float(np.median([max(r[bone] for r in rs if r['nudge'] == n) for n in '0123'])) for rs in (a, b)]
        line('S1', 'hardest jump of a %s in a turn about %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, x[1], x[0], room),
             x[1] <= x[0] + room)
    worse = sorted(set(k for c in cmp for k, v in c.items() if v[2] == 'worse'))
    line('S2', 'compare170.py names worse: %s' % (', '.join(worse) or 'nothing'), all(w in PRICES for w in worse))
    sa, sb = rows(old, groups, 'stop'), rows(new, groups, 'stop')
    ta, tb = rows(old, groups, 'start', '0'), rows(new, groups, 'start', '0')
    line('S3', 'stopping: feet stand after %.2f s, old %.2f s; starting: at its speed after %.2f s, old %.2f s' % (
        med(sb, 'rest'), med(sa, 'rest'), med(tb, 'body'), med(ta, 'body')),
        med(sb, 'rest') <= med(sa, 'rest') + 0.05 and abs(med(tb, 'body') - med(ta, 'body')) <= 0.02)
    line('P1', 'faces back after %.2f s, old %.2f s, at most 0.15 s later' % (
        mean('faced 180 walk', 1), mean('faced 180 walk', 0)), mean('faced 180 walk', 1) <= mean('faced 180 walk', 0) + 0.15)
    line('P2', 'the latest turn faces the new way after %.2f s, old %.2f s, at most 0.35 s later' % (
        mean('faced max', 1), mean('faced max', 0)), mean('faced max', 1) <= mean('faced max', 0) + 0.35)
    line('P3', 'follows %.2f of the speed of the stick on the courses, old %.2f, at most 0.03 less' % (
        mean('along walk', 1), mean('along walk', 0)), mean('along walk', 1) >= mean('along walk', 0) - 0.03)
    print('all held' if ok else 'NOT all held')


main()
