# eval178.py old new group [group...]: the rule of round 178, docs/Regeln/RULE178.txt, for the gentle start as the
# default of the library, on two states that check177.sh (walk176.py at four nudges) and check170.sh have measured on
# the groups. A start out of standing comes at no moment of a step, so the nudges give it no band: its quantities are
# taken at nudge 0 over the sets. Z1 starting to walk, the capsule has its speed as the first foot lands, Z2 and is fast
# enough then, Z3 the first step is shorter than a step of the steady gait, Z4 no shin jumps harder; S1 stopping out of
# a walk stays as it is; S2 compare170.py names no quantity worse, apart from the one start without a band; S3 for that
# one, how deep the shins get into each other starting to run, over the sets; P the price, how far the capsule has gone
# a second after it was steered and when it has its running speed.
import json, os, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))


def rows(state, groups, scene, nudge=None):
    out = []
    for g in groups:
        out += [r for r in json.load(open(os.path.join(W, 'res', state, 'walk176.%s.json' % g)))
                if r['scene'] == scene and (nudge is None or r['nudge'] == nudge)]
    return out


def shins(state, groups):
    """how deep the shins get into each other starting to run, per set, mm"""
    out = []
    for g in groups:
        legs = json.load(open(os.path.join(W, 'res', state + '_n0', 'legs167.%s.json' % g)))
        out += [r['shin-shin'] for r in legs if r['scene'] == 'start' and r['gait'] == 'RUN']
    return out


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-88s %s' % (name, text, 'held' if passed else 'MISSED'))

    def med(r, k):
        return float(np.median([x[k] for x in r]))

    a, b = rows(old, groups, 'start', '0'), rows(new, groups, 'start', '0')
    print('%d and %d starts to a walk on %s' % (len(a), len(b), ' '.join(groups)))
    gap = float(np.median([abs(x['body'] - x['land']) for x in b]))
    line('Z1', 'capsule at its speed after %.2f s, old %.2f s, the first foot lands after %.2f s: within 0.08 s' % (
        med(b, 'body'), med(a, 'body'), med(b, 'land')), gap <= 0.08)
    slow = min(x['landSpeed'] for x in b)
    line('Z2', 'as it lands the capsule goes %.2f of its speed, at least 0.90; the slowest %.2f, at least 0.75' % (
        med(b, 'landSpeed'), slow), med(b, 'landSpeed') >= 0.90 and slow >= 0.75)
    share = float(np.median([x['first'] / x['alongBase'] for x in b]))
    line('Z3', 'first step %.2f of a steady step, old %.2f, at most 0.80' % (
        share, np.median([x['first'] / x['alongBase'] for x in a])), share <= 0.80)
    line('Z4', 'hardest jump of a shin starting to walk %.1f mm, old %.1f mm, no more' % (
        max(x['shin'] for x in b), max(x['shin'] for x in a)), max(x['shin'] for x in b) <= max(x['shin'] for x in a))
    sa, sb = rows(old, groups, 'stop'), rows(new, groups, 'stop')
    two = [sum(1 for r in s if r['steps'] <= 2) / len(s) for s in (sa, sb)]
    line('S1', 'stopping: feet stand after %.2f s, old %.2f s, at most 0.05 s later; two feet land in %.0f %%, old %.0f %%' % (
        med(sb, 'rest'), med(sa, 'rest'), 100 * two[1], 100 * two[0]),
        med(sb, 'rest') <= med(sa, 'rest') + 0.05 and two[1] >= two[0] - 0.05)
    worse = []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), old, new, g], capture_output=True, text=True,
                             check=True).stdout
        worse += [l.split('  ')[0] for l in out.split('\n') if l.rstrip().endswith('| worse')]
    other = [w for w in worse if w.strip() != 'starts deep']
    line('S2', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(other) == 0)
    da, db = shins(old, groups), shins(new, groups)
    line('S3', 'shins starting to run: median %.1f mm, old %.1f, at most 2 more; deepest %.1f mm, old %.1f, at most 8 more' % (
        np.median(db), np.median(da), max(db), max(da)), np.median(db) <= np.median(da) + 2.0 and max(db) <= max(da) + 8.0)
    ra, rb = rows(old, groups, 'startRun', '0'), rows(new, groups, 'startRun', '0')
    line('P', 'a second after it was steered it has walked %.2f m, old %.2f m, at most 0.20 less' % (
        med(b, 'way1'), med(a, 'way1')), med(b, 'way1') >= med(a, 'way1') - 0.20)
    line('P', 'and run %.2f m, old %.2f m, at most 1.20 less; at its running speed after %.2f s, old %.2f s, at most 1.00' % (
        med(rb, 'way1'), med(ra, 'way1'), med(rb, 'body'), med(ra, 'body')),
        med(rb, 'way1') >= med(ra, 'way1') - 1.20 and med(rb, 'body') <= 1.00)
    print('all held' if ok else 'NOT all held')


main()
