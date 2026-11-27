# eval186.py old new group [group...]: the rule of round 186, docs/Regeln/RULE186.txt, for a foot that stays where it
# stands when the figure has stopped out of a walk, on two states that check177.sh (walk176.py), check186.sh
# (rest186.py) and check170.sh have measured on the groups at four nudges; OLD170 in the environment names the old
# state of compare170.py where it differs. Over all stops of the groups: Z1 no more than two feet land, Z2 the feet
# stand sooner, Z3 standing they do not move and stand no narrower than the bound; S1 no bone jumps harder in a stop,
# S2 the capsule stands as before, S3 walking on, ahead and back, no bone jumps harder and it gets as far and lifts a
# foot as soon, S4 compare170.py names no quantity worse.
import json, os, re, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))
NARROW = 135.0


def load(state, tool, groups, keep=lambda r: True):
    rows = []
    for g in groups:
        rows += [r for r in json.load(open(os.path.join(W, 'res', state, '%s.%s.json' % (tool, g)))) if keep(r)]
    return rows


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    oldNudged = os.environ.get('OLD170', old)
    a, b = (load(s, 'walk176', groups, lambda r: r['scene'] == 'stop') for s in (old, new))
    ra, rb = (load(s, 'rest186', groups) for s in (old, new))
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-104s %s' % (name, text, 'held' if passed else 'MISSED'))

    def med(rows, k):
        return float(np.median([r[k] for r in rows]))

    def worst(rows, k):
        """per nudge the most of the runs, of those the median"""
        return float(np.median([max(r[k] for r in rows if str(r['nudge']) == n) for n in '0123']))

    print('%d and %d stops out of a walk, %d and %d runs that walk on, on %s' % (
        len(a), len(b), len(ra), len(rb), ' '.join(groups)))
    two = [100.0 * sum(1 for r in rows if r['steps'] <= 2) / len(rows) for rows in (a, b)]
    most = [max(r['steps'] for r in rows) for rows in (a, b)]
    line('Z1', 'no more than two feet land in %.0f %% of the stops, old %.0f %%, at least 5 points more; at most %d, old %d, no '
         'more than 2' % (two[1], two[0], most[1], most[0]), two[1] >= two[0] + 5.0 and most[1] <= 2)
    line('Z2', 'feet stand after %.2f s, old %.2f s, no later; the latest after %.2f s, old %.2f s, no later' % (
        med(b, 'rest'), med(a, 'rest'), max(r['rest'] for r in b), max(r['rest'] for r in a)),
        med(b, 'rest') <= med(a, 'rest') and max(r['rest'] for r in b) <= max(r['rest'] for r in a))
    moved = [max(r['moved'] for r in rows) for rows in (ra, rb)]
    wide = [min(r['wide'] for r in rows) for rows in (ra, rb)]
    line('Z3', 'standing, a foot moves %.1f mm at most, old %.1f mm, at most 1 mm; the feet stand %.0f mm apart at the least, '
         'old %.0f mm, at least %.0f mm' % (moved[1], moved[0], wide[1], wide[0], NARROW), moved[1] <= 1.0 and wide[1] >= NARROW)
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x = [worst(rows, bone) for rows in (a, b)]
        line('S1', 'hardest jump of a %s in a stop %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, x[1], x[0], room),
             x[1] <= x[0] + room)
    line('S2', 'capsule stands after %.2f s and %.2f m, old %.2f s and %.2f m, within 0.02 s and 0.02 m' % (
        med(b, 'body'), med(b, 'way'), med(a, 'body'), med(a, 'way')),
        abs(med(b, 'body') - med(a, 'body')) <= 0.02 and abs(med(b, 'way') - med(a, 'way')) <= 0.02)
    for back, name in ((0, 'ahead'), (1, 'back')):
        qa, qb = ([r for r in rows if r['back'] == back] for rows in (ra, rb))
        for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
            x = [worst(rows, bone) for rows in (qa, qb)]
            line('S3', 'walking on %-5s hardest jump of a %s %.1f mm, old %.1f mm, at most %.0f mm more' % (
                name, bone, x[1], x[0], room), x[1] <= x[0] + room)
        way = [float(np.mean([r['way1'] for r in rows])) for rows in (qa, qb)]
        lift = [max(r['lift'] for r in rows) for rows in (qa, qb)]
        line('S3', 'walking on %-5s it gets %.2f m in the first second, old %.2f m, within 0.02 m; the first foot lifts after '
             '%.2f s at the latest, old %.2f s, at most 0.05 s later' % (name, way[1], way[0], lift[1], lift[0]),
             abs(way[1] - way[0]) <= 0.02 and lift[1] <= lift[0] + 0.05)
    worse = []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), oldNudged, new, g], capture_output=True,
                             text=True, check=True).stdout
        worse += [re.split(r'\s{2,}', l)[0] for l in out.split('\n') if l.rstrip().endswith('| worse')]
    line('S4', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(worse) == 0)
    print('all held' if ok else 'NOT all held')


main()
