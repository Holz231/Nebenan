# eval192.py old new group [group...]: the rule of round 192, docs/Regeln/RULE192.txt, for the step a waiting heading
# asks of a foot, on two states that check170.sh, check177.sh (walk176.py), check186.sh (rest186.py) and check192.sh
# (twice192.py) have measured on the groups at four nudges; OLD170 in the environment names the old state of
# compare170.py where it differs. Z1 on the courses of steer162.py at a walk a foot that stood for a moment lifts again
# while the other is left behind half as often at most, Z2 the pelvis gets more than 60 mm low and a planted foot
# more than 0.55 m behind its hip on no run; S1 compare170.py names nothing worse, S2 on the same courses a foot that
# only touched the ground lifts again no more often and shins, pelvis and hands jump and sink on few more runs, S3 the
# single turns and starts of walk176.py slide little more and the single turn-abouts face back no later, S4 the single
# changes face the new way no later, S5 stopping and walking on as before.
import json, os, re, subprocess, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

M = os.path.dirname(os.path.abspath(__file__))
SCENES = ('turn45', 'turn90', 'turn180', 'start90', 'start180')
# the share of the old slide of a foot on what it stands on, and of the way of its toes over the ground, the new one may
# have, per scene, and how far past it in mm
SHARE = {'turn45': 1.0, 'turn90': 1.0, 'turn180': 1.0, 'start90': 1.0, 'start180': 1.0}
ROOM = {'turn45': 0.5, 'turn90': 0.5, 'turn180': 2.0, 'start90': 0.5, 'start180': 0.5}
DRAG = {'turn45': 2.0, 'turn90': 2.0, 'turn180': 2.0, 'start90': 2.0, 'start180': 2.0}


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

    a, b = load(old, 'twice192', groups), load(new, 'twice192', groups)
    print('%d and %d runs of all courses at a walk, on %s' % (len(a), len(b), ' '.join(groups)))
    x, y = (sum(r['twice'] for r in rows) for rows in (a, b))
    line('Z1', 'a foot that stood for a moment lifts again while the other is left behind %d times, old %d, at most half '
         'of that or 1' % (y, x), y <= max(0.5 * x, 1.0))
    x, y = (sum(r['touch'] for r in rows) for rows in (a, b))
    line('S2', 'and after it only touched the ground %d times, old %d, at most one in two hundred runs more' % (y, x),
         y <= x + 0.005 * len(b))
    x, y = (sum(r['under'] > 60.0 for r in rows) for rows in (a, b))
    line('Z2', 'the pelvis gets more than 60 mm below the steady walk on %d runs, old %d, on none' % (y, x), y == 0)
    x, y = (sum(r['far'] > 0.55 for r in rows) for rows in (a, b))
    line('Z2', 'a planted foot gets more than 0.55 m behind its hip on %d runs, old %d, on none' % (y, x), y == 0)
    walk = lambda r: r['gait'] == 'WALK'
    a, b = nudged(oldNudged, 'steer162', groups, walk), nudged(new, 'steer162', groups, walk)
    x, y = (sum(r['shin'] > 37.0 for r in rows) for rows in (a, b))
    line('S2', 'of %d runs of all courses at a walk a shin jumps more than 37 mm on %d, old %d, at most one in a hundred runs '
         'more' % (len(b), y, x), y <= x + 0.01 * len(b))
    x, y = (sum(r['under'] > 30.0 for r in rows) for rows in (a, b))
    line('S2', 'the pelvis gets more than 30 mm below the steady walk on %d of them, old %d, at most one in a hundred runs '
         'more' % (y, x), y <= x + 0.01 * len(b))
    x, y = (sum(r['under'] > 35.0 for r in rows) for rows in (a, b))
    line('S2', 'and more than 35 mm on %d of them, old %d, at most one more' % (y, x), y <= x + 1)
    x, y = (sum(r['under'] > 60.0 for r in rows) for rows in (a, b))
    line('S2', 'and more than 60 mm on %d of them, old %d, on no more' % (y, x), y <= x)
    x, y = (sum(r['hand'] > 30.0 for r in rows) for rows in (a, b))
    line('S2', 'a hand jumps more than 30 mm on %d of them, old %d, at most one more' % (y, x), y <= x + 1)

    worse = []
    for g in groups:
        out = subprocess.run([sys.executable, os.path.join(M, 'compare170.py'), oldNudged, new, g], capture_output=True,
                             text=True, check=True).stdout
        for l in out.split('\n'):
            if l.rstrip().endswith('| worse'):
                worse.append(re.split(r'\s{2,}', l)[0])
    line('S1', 'compare170.py names worse: %s' % (', '.join(sorted(set(worse))) or 'nothing'), len(worse) == 0)

    a, b = (load(s, 'walk176', groups, lambda r: r['scene'] in SCENES) for s in (old, new))

    def mean(rows, scene, k):
        return float(np.mean([r[k] for r in rows if r['scene'] == scene]))

    for k, what in (('soleSlide', 'a foot slides %.1f mm on what it stands on'),
                    ('toeDrag', 'its toes move %.1f mm over the ground')):
        for sc in SCENES:
            x, y = mean(a, sc, k), mean(b, sc, k)
            room = ROOM[sc] if k == 'soleSlide' else DRAG[sc]
            line('S3', ('%-8s ' + what + ', old %.1f mm, at most %.1f mm more') % (sc, y, x, room), y <= x + room)
    for sc in SCENES:
        x, y = mean(a, sc, 'faced'), mean(b, sc, 'faced')
        line('S3', '%-8s faces the new way after %.2f s, old %.2f s, at most 0.05 s later' % (sc, y, x), y <= x + 0.05)
    for bone, limit in (('foot', 37.0), ('shin', 37.0), ('pelvis', 20.0)):
        x, y = (sum(r[bone] > limit for r in rows) for rows in (a, b))
        line('S3', 'a %s jumps more than %.0f mm on %d of %d runs, old %d, at most one more' % (bone, limit, y, len(b), x),
             y <= x + 1)
    for sc in SCENES:
        x, y = mean(a, sc, 'shin'), mean(b, sc, 'shin')
        line('S3', '%-8s a shin jumps %.1f mm in the mean of the runs, old %.1f mm, at most 5 mm more' % (sc, y, x), y <= x + 5.0)

    turn = lambda r: r['gait'] == 'WALK' and r['scene'] == 'turn180'
    a, b = nudged(oldNudged, 'gait155', groups, turn), nudged(new, 'gait155', groups, turn)
    x, y = (sum(r['faced'] > 1.25 for r in rows) for rows in (a, b))
    line('S3', 'of %d single turn-abouts the figure faces back later than 1.25 s on %d, old %d, at most one more' % (
        len(b), y, x), y <= x + 1)
    x, y = (max(r['faced'] for r in rows) for rows in (a, b))
    line('S3', 'at the latest after %.2f s, old %.2f s, at most 0.05 s later' % (y, x), y <= x + 0.05)
    x, y = (float(np.mean([r['faced'] for r in rows])) for rows in (a, b))
    line('S3', 'in their mean after %.2f s, old %.2f s, at most 0.01 s later' % (y, x), y <= x + 0.01)
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
    line('S4', 'of %d single changes %d face the new way more than 0.05 s later and %d earlier; at most two more later' % (
        count, late, early), late <= early + 2)

    a, b = (load(s, 'walk176', groups, lambda r: r['scene'] == 'stop') for s in (old, new))
    for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
        x, y = worst(a, bone), worst(b, bone)
        line('S5', 'hardest jump of a %s in a stop %.1f mm, old %.1f mm, at most %.0f mm more' % (bone, y, x, room),
             y <= x + room)
    ra, rb = (load(s, 'rest186', groups) for s in (old, new))
    for back, name in ((0, 'ahead'), (1, 'back')):
        qa, qb = ([r for r in rows if r['back'] == back] for rows in (ra, rb))
        for bone, room in (('foot', 3.0), ('shin', 3.0), ('pelvis', 2.0)):
            x, y = worst(qa, bone), worst(qb, bone)
            line('S5', 'walking on %-5s hardest jump of a %s %.1f mm, old %.1f mm, at most %.0f mm more' % (
                name, bone, y, x, room), y <= x + room)
        way = [float(np.mean([r['way1'] for r in rows])) for rows in (qa, qb)]
        lift = [max(r['lift'] for r in rows) for rows in (qa, qb)]
        line('S5', 'walking on %-5s it gets %.2f m in the first second, old %.2f m, within 0.02 m; the first foot lifts after '
             '%.2f s at the latest, old %.2f s, at most 0.05 s later' % (name, way[1], way[0], lift[1], lift[0]),
             abs(way[1] - way[0]) <= 0.02 and lift[1] <= lift[0] + 0.05)
    print('all held' if ok else 'NOT all held')


main()
