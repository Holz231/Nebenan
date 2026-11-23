# eval158.py old.json new.json: the rule of round 158, docs/Regeln/RULE158.txt, on the tables gait155.py wrote for the
# state before and the build on the same sets. Prints every condition with its numbers and ends with 1 if one is missed.
import json, sys
import numpy as np

DEEP = 60.0          # the pelvis counts as sunk where it gets this far below its height before the change, mm
SHARE = 0.7          # A1: walking turns, the sum beyond DEEP is at most this share of the sum before
SHOWN = 200.0        # A1 counts as shown where the sum before is at least this, mm
STOP = 2.0           # A2: stopping at a walk, no point more than this beyond DEEP, mm
STOPPED = 4          # A2 counts as shown from this many points beyond DEEP before
AGAIN = 0.03         # A3: turning at a walk, the figure walks again no more than this later in the mean, s
HALT = 0.05          # A3: stopping at a walk takes no more than this longer in the mean, s
SHIN = 10.0          # A4: the sum of the shins beyond LIMIT may grow by this, mm: what one hard point makes
FOOT = 2             # A5: this many more points with a foot over LIMIT
PELVIS = 2.0         # A6: the hardest jump of the pelvis may grow by this, mm
SHORT = 6            # A7: this many more points with a swing shorter than 0.25 s
TWIST = 46.0         # A8: a planted foot turns on the ground no farther, degrees, as since round 156
LIMIT = 37.0
TURNS = ('turn45', 'turn90', 'turn90R', 'turn135', 'turn180')
old, new = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
ok = True


def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)


def sel(rows, scenes, gait):
    return [r for r in rows if r['scene'] in scenes and r['gait'] == gait]


def beyond(rows):
    return sum(max(r['low'] - DEEP, 0.0) for r in rows)


to, tn = sel(old, TURNS, 'WALK'), sel(new, TURNS, 'WALK')
say('A1', beyond(tn) <= SHARE * beyond(to) and beyond(to) >= SHOWN, 'turning at a walk the pelvis sinks beyond %.0f mm by '
    '%.0f mm in sum before, %.0f mm now, at %d and %d of %d points, at most %.0f %% of before' % (
        DEEP, beyond(to), beyond(tn), sum(r['low'] > DEEP for r in to), sum(r['low'] > DEEP for r in tn), len(tn), 100 * SHARE))
so, sn = sel(old, ('stop',), 'WALK'), sel(new, ('stop',), 'WALK')
shown = sum(r['low'] > DEEP for r in so) >= STOPPED
say('A2', max(r['low'] for r in sn) <= DEEP + STOP and shown, 'stopping at a walk the pelvis sinks %.0f mm at most before, '
    '%.0f mm now, beyond %.0f mm at %d of %d points before' % (
        max(r['low'] for r in so), max(r['low'] for r in sn), DEEP, sum(r['low'] > DEEP for r in so), len(so)))
for scene in ('turn90', 'turn135', 'turn180', 'stop'):
    a = float(np.mean([r['again'] for r in sel(old, (scene,), 'WALK')]))
    b = float(np.mean([r['again'] for r in sel(new, (scene,), 'WALK')]))
    most = HALT if scene == 'stop' else AGAIN
    say('A3', b <= a + most, '%s at a walk, at its new speed after %.2f s before, %.2f s now, at most %.2f s more' % (
        scene, a, b, most))
a, b = sum(max(r['shin'] - LIMIT, 0.0) for r in old), sum(max(r['shin'] - LIMIT, 0.0) for r in new)
say('A4', b <= a + SHIN, 'shins beyond %.0f mm in sum: %.0f mm before, %.0f mm now' % (LIMIT, a, b))
a, b = sum(r['foot'] > LIMIT for r in old), sum(r['foot'] > LIMIT for r in new)
say('A5', b <= a + FOOT, 'points with a foot over %.0f mm: %d before, %d now' % (LIMIT, a, b))
a, b = max(r['pelvis'] for r in old), max(r['pelvis'] for r in new)
say('A6', b <= a + PELVIS, 'hardest jump of the pelvis: %.1f mm before, %.1f mm now' % (a, b))
a, b = sum(r['short'] < 0.25 for r in to), sum(r['short'] < 0.25 for r in tn)
say('A7', b <= a + SHORT, 'turning at a walk, points with a swing shorter than 0.25 s: %d before, %d now' % (a, b))
b = max(r['twist'] for r in new if r['scene'] in TURNS)
say('A8', b <= TWIST, 'a planted foot turns %.1f degrees on the ground at most' % b)
ro, rn = [r for r in old if r['gait'] == 'RUN'], [r for r in new if r['gait'] == 'RUN']
same = sum(x == y for x, y in zip(ro, rn))
say('D1', same == len(ro) == len(rn), 'running: %d of %d points the same' % (same, len(ro)))
wo, wn = sel(old, ('start', 'steady', 'change'), 'WALK'), sel(new, ('start', 'steady', 'change'), 'WALK')
same = sum(x == y for x, y in zip(wo, wn))
say('D1', same == len(wo) == len(wn), 'starting, the steady gait and changing to a run at a walk: %d of %d points the same' % (
    same, len(wo)))
sys.exit(0 if ok else 1)
