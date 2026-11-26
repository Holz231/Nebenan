# eval182.py old new group [group...]: the rule of round 182, docs/Regeln/RULE182.txt, for the third foot of a stop out
# of a walk, on two states that check177.sh (walk176.py at four nudges) and check170.sh have measured on the groups.
# Over all stops of the groups: Z1 two feet land in more of them, Z2 the feet stand no later; S1 no bone jumps harder
# in a stop, S2 the capsule stands as before, S3 compare170.py names no quantity worse.
import json, os, re, subprocess, sys
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
    oldNudged = os.environ.get('OLD170', old)
    a, b = stops(old, groups), stops(new, groups)
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-86s %s' % (name, text, 'held' if passed else 'MISSED'))

    def med(rows, k):
        return float(np.median([r[k] for r in rows]))

    print('%d and %d stops out of a walk on %s' % (len(a), len(b), ' '.join(groups)))
    two = [100.0 * sum(1 for r in rows if r['steps'] <= 2) / len(rows) for rows in (a, b)]
    most = [max(r['steps'] for r in rows) for rows in (a, b)]
    line('Z1', 'two feet land in %.0f %% of the stops, old %.0f %%, at least 5 points more; at most %d, old %d' % (
        two[1], two[0], most[1], most[0]), two[1] >= two[0] + 5.0 and most[1] <= most[0])
    line('Z2', 'feet stand after %.2f s, old %.2f s, no later; the latest after %.2f s, old %.2f s, at most 0.05 later' % (
        med(b, 'rest'), med(a, 'rest'), max(r['rest'] for r in b), max(r['rest'] for r in a)),
        med(b, 'rest') <= med(a, 'rest') and max(r['rest'] for r in b) <= max(r['rest'] for r in a) + 0.05)
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x = [float(np.median([max(r[bone] for r in rows if r['nudge'] == n) for n in '0123'])) for rows in (a, b)]
        line('S1', 'hardest jump of a %s in a stop %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, x[1], x[0], room),
             x[1] <= x[0] + room)
    line('S2', 'capsule stands after %.2f s and %.2f m, old %.2f s and %.2f m, within 0.02 s and 0.02 m' % (
        med(b, 'body'), med(b, 'way'), med(a, 'body'), med(a, 'way')),
        abs(med(b, 'body') - med(a, 'body')) <= 0.02 and abs(med(b, 'way') - med(a, 'way')) <= 0.02)
    worse = []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), oldNudged, new, g], capture_output=True,
                             text=True, check=True).stdout
        worse += [re.split(r'\s{2,}', l)[0] for l in out.split('\n') if l.rstrip().endswith('| worse')]
    line('S3', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(worse) == 0)
    print('all held' if ok else 'NOT all held')


main()
