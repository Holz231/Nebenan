# eval185.py old new group [group...]: the rule of round 185, docs/Regeln/RULE185.txt, for a standing foot that turns on
# its heel with its toes up while the hip is behind its ankle, and the heading that stays free once it has waited for
# the feet as long as it does, on two states that check177.sh (walk176.py at four nudges) and check170.sh have measured
# on the groups; OLD170 in the environment names the old state of compare170.py and of the single changes where it
# differs. The conditions of eval184.py, and S4: of the single changes of gait155.py no more face the new way more than
# 0.05 s later than face it that much earlier, and no more than one in a hundred.
import json, os, re, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))
SCENES = ('turn45', 'turn90', 'turn180', 'start90', 'start180')
# the most of the old slide on what it stands on that the new one may have, per scene, and how far past it in mm
SHARE = {'turn45': 1.0, 'turn90': 1.0, 'turn180': 0.85, 'start90': 1.0, 'start180': 0.6}
ROOM = 0.5


def runs(state, groups):
    rows = []
    for g in groups:
        rows += [r for r in json.load(open(os.path.join(W, 'res', state, 'walk176.%s.json' % g))) if r['scene'] in SCENES]
    return rows


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    oldNudged = os.environ.get('OLD170', old)
    a, b = runs(old, groups), runs(new, groups)
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-100s %s' % (name, text, 'held' if passed else 'MISSED'))

    def mean(rows, scene, k):
        return float(np.mean([r[k] for r in rows if r['scene'] == scene]))

    print('%d and %d runs on %s' % (len(a), len(b), ' '.join(groups)))
    for sc in SCENES:
        x, y = mean(a, sc, 'soleSlide'), mean(b, sc, 'soleSlide')
        line('Z1', '%-8s a foot slides %.1f mm on what it stands on, old %.1f mm, at most %.2f of it' % (sc, y, x, SHARE[sc]),
             y <= SHARE[sc] * x + (ROOM if SHARE[sc] == 1.0 else 0.0))
    for sc in SCENES:
        x, y = mean(a, sc, 'toeDrag'), mean(b, sc, 'toeDrag')
        line('Z2', '%-8s its toes move %.1f mm over the ground, old %.1f mm, at most 2 mm farther' % (sc, y, x), y <= x + 2.0)
    for sc in SCENES:
        x, y = mean(a, sc, 'faced'), mean(b, sc, 'faced')
        line('S1', '%-8s faces the new way after %.2f s, old %.2f s, at most 0.05 s later' % (sc, y, x), y <= x + 0.05)
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x = [float(np.median([max(r[bone] for r in rows if r['nudge'] == n) for n in '0123'])) for rows in (a, b)]
        line('S2', 'hardest jump of a %s %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, x[1], x[0], room),
             x[1] <= x[0] + room)
    for sc in SCENES:
        x, y = mean(a, sc, 'shin'), mean(b, sc, 'shin')
        line('S2', '%-8s a shin jumps %.1f mm in the mean of the runs, old %.1f mm, at most 5 mm more' % (sc, y, x), y <= x + 5.0)
    worse = []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), oldNudged, new, g], capture_output=True,
                             text=True, check=True).stdout
        for l in out.split('\n'):
            if l.rstrip().endswith('| worse'):
                worse.append(re.split(r'\s{2,}', l)[0])
    line('S3', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(worse) == 0)
    late, early, count = 0, 0, 0
    for g in groups:
        for n in range(4):
            x = json.load(open(os.path.join(W, 'res', '%s_n%d' % (oldNudged, n), 'gait155.%s.json' % g)))
            y = json.load(open(os.path.join(W, 'res', '%s_n%d' % (new, n), 'gait155.%s.json' % g)))
            for p, q in zip(x, y):
                assert (p['set'], p['scene'], p['gait'], p['moment']) == (q['set'], q['scene'], q['gait'], q['moment'])
                if p.get('faced') is not None:
                    count += 1
                    late += q['faced'] - p['faced'] > 0.05
                    early += q['faced'] - p['faced'] < -0.05
    line('S4', 'of %d single changes %d face the new way more than 0.05 s later and %d earlier; no more later, at most 1 %%' % (
        count, late, early), late <= early and late <= 0.01 * count)
    print('all held' if ok else 'NOT all held')


main()
