# eval177.py old new group [group...]: the rule of round 177, docs/Regeln/RULE177.txt, for stopping out of a walk over
# the last step, on two states that check177.sh (walk176.py at four nudges) and check170.sh have measured on the groups.
# Over all stops of the groups: Z1 the feet stand sooner (median of when the last foot lands), Z2 fewer feet land, Z3
# the capsule takes about a step to stand; S1 no bone jumps harder in a stop (the hardest jump of a foot, a shin and
# the pelvis in an update per nudge, the median of the nudges); S2 compare170.py names no quantity worse on any group;
# P the price, how far the capsule goes on after the stick let go.
import json, os, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))


def stops(state, groups):
    rows = []
    for g in groups:
        rows += [r for r in json.load(open(os.path.join(W, 'res', state, 'walk176.%s.json' % g))) if r['scene'] == 'stop']
    return rows


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    a, b = stops(old, groups), stops(new, groups)
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-62s %s' % (name, text, 'held' if passed else 'MISSED'))

    def med(rows, k):
        return float(np.median([r[k] for r in rows]))

    print('%d and %d stops out of a walk on %s' % (len(a), len(b), ' '.join(groups)))
    line('Z1', 'feet stand after %.2f s, old %.2f s, at most 0.60 of it' % (med(b, 'rest'), med(a, 'rest')),
         med(b, 'rest') <= 0.60 * med(a, 'rest'))
    two = sum(1 for r in b if r['steps'] <= 2) / len(b)
    most = max(r['steps'] for r in b)
    line('Z2', 'two feet land in %.0f %%, old %.0f %%, at least 60; at most %d, old %d, no more than old' % (
        100 * two, 100 * sum(1 for r in a if r['steps'] <= 2) / len(a), most, max(r['steps'] for r in a)),
        two >= 0.60 and most <= max(r['steps'] for r in a))
    body = [r['body'] for r in b]
    line('Z3', 'capsule stands after %.2f s, 0.30 to 0.60; at most %.2f s, 0.80' % (np.median(body), max(body)),
         0.30 <= np.median(body) <= 0.60 and max(body) <= 0.80 and min(body) >= 0.0)
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x = [float(np.median([max(r[bone] for r in rows if r['nudge'] == n) for n in '0123'])) for rows in (a, b)]
        line('S1', 'hardest jump of a %s in a stop %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, x[1], x[0], room),
             x[1] <= x[0] + room)
    worse = 0
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), old.replace('c176', 'c175'), new, g],
                             capture_output=True, text=True, check=True).stdout
        worse += sum(1 for l in out.split('\n') if l.rstrip().endswith('| worse'))
    line('S2', 'compare170.py names %d quantities worse' % worse, worse == 0)
    way = [r['way'] for r in b]
    line('P', 'capsule goes on %.2f m, old %.2f m, at most 0.45; at most %.2f m, 0.60' % (
        np.median(way), med(a, 'way'), max(way)), np.median(way) <= 0.45 and max(way) <= 0.60)
    print('all held' if ok else 'NOT all held')


main()
