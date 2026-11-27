# eval188.py old new group [group...]: the rule of round 188, docs/Regeln/RULE188.txt, for legs that walk backward while
# a figure that faces where it moves turns about at a walk, on two states that check170.sh, check177.sh (walk176.py)
# and check186.sh (rest186.py) have measured on the groups at four nudges; OLD170 in the environment names the old
# state of compare170.py where it differs. On the courses of steer162.py at a walk that turn about: Z1 the pelvis sinks
# on a quarter of the runs at most, Z2 a shin jumps on no more runs; S1 compare170.py names nothing worse but the
# slide of a flat sole within its bound, S2 the single turns and starts of walk176.py, S3 the single changes of
# gait155.py face the new way no later, S4 stopping and walking on as before.
import json, os, re, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))
ABOUT = ('shuttle_0.3', 'shuttle_0.6', 'shuttle_1.0', 'zigzag90_0.3', 'zigzag90_0.6', 'zigzag90_1.0')
SCENES = ('turn45', 'turn90', 'turn180', 'start90', 'start180')
# how much farther a foot may slide on what it stands on and its toes may move over the ground, per scene, in mm
SLIDE = {'turn45': 0.5, 'turn90': 0.5, 'turn180': 8.0, 'start90': 0.5, 'start180': 0.5}
DRAG = {'turn45': 2.0, 'turn90': 2.0, 'turn180': 10.0, 'start90': 2.0, 'start180': 2.0}
SLIDE_FLAT = 5.0


def load(state, tool, groups, keep=lambda r: True):
    rows = []
    for g in groups:
        rows += [r for r in json.load(open(os.path.join(W, 'res', state, '%s.%s.json' % (tool, g)))) if keep(r)]
    return rows


def nudged(state, tool, groups, keep=lambda r: True):
    rows = []
    for g in groups:
        for n in range(4):
            path = os.path.join(W, 'res', '%s_n%d' % (state, n), '%s.%s.json' % (tool, g))
            rows += [dict(r, nudge=str(n)) for r in json.load(open(path)) if keep(r)]
    return rows


def main():
    old, new, groups = sys.argv[1], sys.argv[2], sys.argv[3:]
    oldNudged = os.environ.get('OLD170', old)
    ok = True

    def line(name, text, passed):
        nonlocal ok
        ok = ok and passed
        print('%-3s %-108s %s' % (name, text, 'held' if passed else 'MISSED'))

    def worst(rows, k):
        """per nudge the most of the runs, of those the median"""
        return float(np.median([max(r[k] for r in rows if str(r['nudge']) == n) for n in '0123']))

    about = lambda r: r['gait'] == 'WALK' and r['course'] in ABOUT
    a, b = nudged(oldNudged, 'steer162', groups, about), nudged(new, 'steer162', groups, about)
    print('%d and %d runs of courses at a walk that turn about, on %s' % (len(a), len(b), ' '.join(groups)))
    x, y = (sum(r['under'] > 30.0 for r in rows) for rows in (a, b))
    line('Z1', 'the pelvis sinks more than 30 mm below the steady walk on %d runs, old %d, at most a quarter of that or 1' % (
        y, x), y <= max(0.25 * x, 1.0))
    x, y = (sum(r['shin'] > 37.0 for r in rows) for rows in (a, b))
    line('Z2', 'a shin jumps more than 37 mm on %d runs, old %d, at most one in a hundred runs more' % (y, x),
         y <= x + 0.01 * len(b))
    x, y = (max(r['hand'] for r in rows) for rows in (a, b))
    line('Z2', 'a hand jumps %.1f mm at most, old %.1f mm, at most 5 mm more' % (y, x), y <= x + 5.0)

    worse, slide = [], []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), oldNudged, new, g], capture_output=True,
                             text=True, check=True).stdout
        for l in out.split('\n'):
            if l.startswith('slide walk'):
                slide.append([float(v) for v in re.findall(r'median\s+([-0-9.]+)', l)])
            if l.rstrip().endswith('| worse') and not l.startswith('slide walk'):
                worse.append(re.split(r'\s{2,}', l)[0])
    line('S1', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(worse) == 0)
    for g, (x, y) in zip(groups, slide):
        line('S1', '%-8s the sole of a flat foot slides %.1f mm in one stance at most, old %.1f mm, at most %.0f mm more' % (
            g, y, x, SLIDE_FLAT), y <= x + SLIDE_FLAT)

    a, b = (load(s, 'walk176', groups, lambda r: r['scene'] in SCENES) for s in (old, new))

    def mean(rows, scene, k):
        return float(np.mean([r[k] for r in rows if r['scene'] == scene]))

    for sc in SCENES:
        x, y = mean(a, sc, 'faced'), mean(b, sc, 'faced')
        line('S2', '%-8s faces the new way after %.2f s, old %.2f s, at most 0.05 s later' % (sc, y, x), y <= x + 0.05)
    for sc in SCENES:
        x, y = mean(a, sc, 'soleSlide'), mean(b, sc, 'soleSlide')
        line('S2', '%-8s a foot slides %.1f mm on what it stands on, old %.1f mm, at most %.1f mm more' % (sc, y, x, SLIDE[sc]),
             y <= x + SLIDE[sc])
    for sc in SCENES:
        x, y = mean(a, sc, 'toeDrag'), mean(b, sc, 'toeDrag')
        line('S2', '%-8s its toes move %.1f mm over the ground, old %.1f mm, at most %.0f mm farther' % (sc, y, x, DRAG[sc]),
             y <= x + DRAG[sc])
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x, y = worst(a, bone), worst(b, bone)
        line('S2', 'hardest jump of a %s %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, y, x, room), y <= x + room)
    for sc in SCENES:
        x, y = mean(a, sc, 'shin'), mean(b, sc, 'shin')
        line('S2', '%-8s a shin jumps %.1f mm in the mean of the runs, old %.1f mm, at most 5 mm more' % (sc, y, x), y <= x + 5.0)

    late, early, count = 0, 0, 0
    for g in groups:
        for n in range(4):
            p = json.load(open(os.path.join(W, 'res', '%s_n%d' % (oldNudged, n), 'gait155.%s.json' % g)))
            q = json.load(open(os.path.join(W, 'res', '%s_n%d' % (new, n), 'gait155.%s.json' % g)))
            for u, v in zip(p, q):
                assert (u['set'], u['scene'], u['gait'], u['moment']) == (v['set'], v['scene'], v['gait'], v['moment'])
                if u.get('faced') is not None:
                    count += 1
                    late += v['faced'] - u['faced'] > 0.05
                    early += v['faced'] - u['faced'] < -0.05
    line('S3', 'of %d single changes %d face the new way more than 0.05 s later and %d earlier; no more later, at most 2 %%' % (
        count, late, early), late <= early and late <= 0.02 * count)

    a, b = (load(s, 'walk176', groups, lambda r: r['scene'] == 'stop') for s in (old, new))
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x, y = worst(a, bone), worst(b, bone)
        line('S4', 'hardest jump of a %s in a stop %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, y, x, room),
             y <= x + room)
    ra, rb = (load(s, 'rest186', groups) for s in (old, new))
    for back, name in ((0, 'ahead'), (1, 'back')):
        qa, qb = ([r for r in rows if r['back'] == back] for rows in (ra, rb))
        for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
            x, y = worst(qa, bone), worst(qb, bone)
            line('S4', 'walking on %-5s hardest jump of a %s %.1f mm, old %.1f mm, at most %.0f mm more' % (
                name, bone, y, x, room), y <= x + room)
        way = [float(np.mean([r['way1'] for r in rows])) for rows in (qa, qb)]
        lift = [max(r['lift'] for r in rows) for rows in (qa, qb)]
        line('S4', 'walking on %-5s it gets %.2f m in the first second, old %.2f m, within 0.02 m; the first foot lifts after '
             '%.2f s at the latest, old %.2f s, at most 0.05 s later' % (name, way[1], way[0], lift[1], lift[0]),
             abs(way[1] - way[0]) <= 0.02 and lift[1] <= lift[0] + 0.05)
    print('all held' if ok else 'NOT all held')


main()
