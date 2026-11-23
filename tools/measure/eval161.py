# eval161.py old.json new.json: the rule of round 161, docs/Regeln/RULE161.txt, on the tables gait155.py wrote for the
# state before and the build on the same sets. Prints every condition with its numbers and ends with 1 if one is missed.
import json, sys
import numpy as np

DEEP = 30.0          # the pelvis counts as sunk where it gets this far below its height in the steady gait, mm
SHARE = 0.4          # A1: walking turns, the sum beyond DEEP is at most this share of the sum before
SHOWN = 200.0        # A1 counts as shown where the sum before is at least this, mm
SWINGS = 0.25        # A2: walking turns, the points with a short swing are at most this share of before and FLOOR
FLOOR = 2
SWUNG = 20           # A2 counts as shown from this many points with a short swing before
AGAIN = {'turn90': 0.10, 'turn135': 0.17, 'turn180': 0.04}   # A3: it walks again no more than this later, mean, s
FACED = 0.03         # A3: turned about it faces the new way no more than this later in the mean, s
ONWARD = 0.50        # A4: turning about or by 90 degrees it gets no farther onward the way it walked, m
SIDEWAYS = 0.85      # A4: turning about it gets no farther to the side, m
SHIN = 10.0          # A5: the sum of the shins beyond LIMIT may grow by this, mm: what one hard point makes
FOOT = 2             # A5: this many more points with a foot over LIMIT
PELVIS = 2.0         # A5: the hardest jump of the pelvis may grow by this, mm
TWIST = 46.0         # A5: a planted foot turns on the ground no farther, degrees, as since round 156
LIMIT = 37.0
TURNS = ('turn45', 'turn90', 'turn90R', 'turn135', 'turn180')
SAME = ('start', 'steady', 'stop', 'change', 'startTurn90', 'startTurn180')
old, new = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
ok = True


def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)


def sel(rows, scenes, gait):
    return [r for r in rows if r['scene'] in scenes and r['gait'] == gait]


def beyond(rows):
    return sum(max(r['under'] - DEEP, 0.0) for r in rows)


def mean(rows, key):
    return float(np.mean([r[key] for r in rows]))


to, tn = sel(old, TURNS, 'WALK'), sel(new, TURNS, 'WALK')
say('A1', beyond(tn) <= SHARE * beyond(to) and beyond(to) >= SHOWN, 'turning at a walk the pelvis sinks beyond %.0f mm under '
    'its steady gait by %.0f mm in sum before, %.0f mm now, at %d and %d of %d points, at most %.0f %% of before' % (
        DEEP, beyond(to), beyond(tn), sum(r['under'] > DEEP for r in to), sum(r['under'] > DEEP for r in tn), len(tn),
        100 * SHARE))
a, b = sum(r['short'] < 0.25 for r in to), sum(r['short'] < 0.25 for r in tn)
say('A2', b <= SWINGS * a + FLOOR and a >= SWUNG, 'turning at a walk, points with a swing shorter than 0.25 s: %d before, '
    '%d now, at most %.0f %% of before and %d' % (a, b, 100 * SWINGS, FLOOR))
never = sum(r['again'] < 0 or r['faced'] < 0 for r in tn)
say('A3', never == 0, 'turning at a walk it walks and faces the new way again in every run: %d of %d do not' % (never, len(tn)))
for scene, most in AGAIN.items():
    a, b = mean(sel(old, (scene,), 'WALK'), 'again'), mean(sel(new, (scene,), 'WALK'), 'again')
    say('A3', b <= a + most, '%s at a walk, at its new speed after %.2f s before, %.2f s now, at most %.2f s more' % (
        scene, a, b, most))
a, b = mean(sel(old, ('turn180',), 'WALK'), 'faced'), mean(sel(new, ('turn180',), 'WALK'), 'faced')
say('A3', b <= a + FACED, 'turned about at a walk it faces the new way after %.2f s before, %.2f s now, at most %.2f s more' % (
    a, b, FACED))
for scene in ('turn180', 'turn90'):
    a = max(r['onward'] for r in sel(old, (scene,), 'WALK'))
    b = max(r['onward'] for r in sel(new, (scene,), 'WALK'))
    say('A4', b <= ONWARD, '%s at a walk it gets %.2f m onward at most before, %.2f m now, at most %.2f m' % (
        scene, a, b, ONWARD))
a = max(r['sideways'] for r in sel(old, ('turn180',), 'WALK'))
b = max(r['sideways'] for r in sel(new, ('turn180',), 'WALK'))
say('A4', b <= SIDEWAYS, 'turn180 at a walk it gets %.2f m to the side at most before, %.2f m now, at most %.2f m' % (
    a, b, SIDEWAYS))
a, b = sum(max(r['shin'] - LIMIT, 0.0) for r in old), sum(max(r['shin'] - LIMIT, 0.0) for r in new)
say('A5', b <= a + SHIN, 'shins beyond %.0f mm in sum: %.0f mm before, %.0f mm now' % (LIMIT, a, b))
a, b = sum(r['foot'] > LIMIT for r in old), sum(r['foot'] > LIMIT for r in new)
say('A5', b <= a + FOOT, 'points with a foot over %.0f mm: %d before, %d now' % (LIMIT, a, b))
a, b = max(r['pelvis'] for r in old), max(r['pelvis'] for r in new)
say('A5', b <= a + PELVIS, 'hardest jump of the pelvis: %.1f mm before, %.1f mm now' % (a, b))
b = max(r['twist'] for r in new if r['scene'] in TURNS)
say('A5', b <= TWIST, 'a planted foot turns %.1f degrees on the ground at most' % b)
ro, rn = [r for r in old if r['gait'] == 'RUN'], [r for r in new if r['gait'] == 'RUN']
same = sum(x == y for x, y in zip(ro, rn))
say('D1', same == len(ro) == len(rn), 'running: %d of %d points the same' % (same, len(ro)))
wo, wn = sel(old, SAME, 'WALK'), sel(new, SAME, 'WALK')
same = sum(x == y for x, y in zip(wo, wn))
say('D1', same == len(wo) == len(wn), 'at a walk starting, also to the side and back, the steady gait, stopping and '
    'changing to a run: %d of %d points the same' % (same, len(wo)))
sys.exit(0 if ok else 1)
