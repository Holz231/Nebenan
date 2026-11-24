# eval163.py old_gait new_gait old_steer new_steer spot_steer: the rule of round 163, docs/Regeln/RULE163.txt, on the
# tables gait155.py and steer162.py wrote for the state before and the build on the same sets, and on the table of
# steer162.py with nbFigureSteering::walkBend at zero (spot). Prints every condition with its numbers and ends with 1
# if one is missed.
import json, math, sys
import numpy as np

FAST = ('circle180', 'circle360', 'zigzag90', 'shuttle')   # the courses of steer162.py steered fast
DEEP = 30.0          # the pelvis counts as sunk where it gets this far below its height in the steady gait, mm
FOLLOW = 0.12        # A1: steered fast at a walk it goes this much more of the steered speed where it is steered
FOLLOWS = 0.10       # A1: and this much at least
SHUTTLE = 0.15       # A1: and turning about every second this much at least
SPOT = 0.35          # A2: the runs with a sunk pelvis are at most this share of those turning on the spot
MORE = 0.07          # A2: and at most this share of the runs more than before
SUM = 10.0           # A3: single turns, the sum beyond DEEP may grow by this, mm: what one hard point makes
ONWARD = 0.30        # A4: turning about it gets no farther on the way it walked, m
SIDEWAYS = 0.40      # A4: and no farther to the side, m
AGAIN = {'turn90': 0.03, 'turn135': 0.03, 'turn180': 0.10}   # A5: it walks again no more than this later, mean, s
FACED = 0.25         # A5: turned about it faces the new way no more than this later in the mean, s
LATEST = 1.85        # A5: and after this at the latest, s
SWINGS = 0.40        # A6: turning about, a swing is shorter than 0.25 s at no more than this share of the points
OTHER = 2            # A6: the other turns, this many more points with such a swing
SHIN = 10.0          # A7: the sum of the shins beyond LIMIT may grow by this, mm
FOOT = 2             # A7: this many more points with a foot over LIMIT
PELVIS = 2.0         # A7: the hardest jump of the pelvis may grow by this, mm
TWIST = 46.0         # A7: a planted foot turns on the ground no farther, degrees, as since round 156
RUNS = 2             # A7: steered on and on, this many runs with a pelvis jump more than PELVIS over the hardest before
OVER = 8             # A7: steered on and on, this many more updates with a shin over LIMIT
LIMIT = 37.0
TURNS = ('turn45', 'turn90', 'turn90R', 'turn135', 'turn180')
og, ng, os_, ns, sp = [json.load(open(p)) for p in sys.argv[1:6]]
ok = True


def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)


def sel(rows, scenes, gait):
    return [r for r in rows if r['scene'] in scenes and r['gait'] == gait]


def walk(rows, fast=None):
    return [r for r in rows if r['gait'] == 'WALK' and (fast is None or r['course'].startswith(FAST) == fast)]


def mean(rows, key):
    return float(np.mean([r[key] for r in rows]))


def sunk(rows):
    return sum(r['under'] > DEEP for r in rows)


a, b, c = mean(walk(os_, True), 'along'), mean(walk(ns, True), 'along'), mean(walk(sp, True), 'along')
say('A1', b >= a + FOLLOW and b >= FOLLOWS, 'steered fast at a walk it goes %.2f of the steered speed where it is steered '
    'before, %.2f now, %.2f turning on the spot; at least %.2f more and %.2f' % (a, b, c, FOLLOW, FOLLOWS))
a, b = [mean([r for r in walk(x) if r['course'] == 'shuttle_1.0'], 'along') for x in (os_, ns)]
say('A1', b >= SHUTTLE, 'turning about every second at a walk: %.2f before, %.2f now, at least %.2f' % (a, b, SHUTTLE))
n = len(walk(ns, True))
a, b, c = sunk(walk(os_, True)), sunk(walk(ns, True)), sunk(walk(sp, True))
most = math.ceil(MORE * n)
say('A2', b <= SPOT * c and b <= a + most and c >= 0.2 * n, 'steered fast at a walk the pelvis sinks beyond %.0f mm in %d of '
    '%d runs before, %d now, %d turning on the spot; at most %.0f %% of those and %d more than before' % (
        DEEP, a, n, b, c, 100 * SPOT, most))
to, tn = sel(og, TURNS, 'WALK'), sel(ng, TURNS, 'WALK')
a, b = [sum(max(r['under'] - DEEP, 0.0) for r in x) for x in (to, tn)]
say('A3', b <= a + SUM, 'single turns at a walk, the pelvis sinks beyond %.0f mm by %.0f mm in sum before, %.0f mm now, at %d '
    'and %d of %d points; at most %.0f mm more' % (DEEP, a, b, sunk(to), sunk(tn), len(tn), SUM))
ao, an = sel(og, ('turn180',), 'WALK'), sel(ng, ('turn180',), 'WALK')
a, b = max(r['onward'] for r in ao), max(r['onward'] for r in an)
say('A4', b <= ONWARD and a >= 0.35, 'turning about at a walk it gets %.2f m onward at most before, %.2f m now, at most '
    '%.2f m' % (a, b, ONWARD))
a, b = max(r['sideways'] for r in ao), max(r['sideways'] for r in an)
say('A4', b <= SIDEWAYS, 'and %.2f m to the side at most before, %.2f m now, at most %.2f m' % (a, b, SIDEWAYS))
never = sum(r['again'] < 0 or r['faced'] < 0 for r in tn)
say('A5', never == 0, 'turning at a walk it walks and faces the new way again in every run: %d of %d do not' % (never, len(tn)))
for scene, most in AGAIN.items():
    a, b = mean(sel(og, (scene,), 'WALK'), 'again'), mean(sel(ng, (scene,), 'WALK'), 'again')
    say('A5', b <= a + most, '%s at a walk, at its new speed after %.2f s before, %.2f s now, at most %.2f s more' % (
        scene, a, b, most))
a, b = mean(ao, 'faced'), mean(an, 'faced')
worst = max(r['faced'] for r in an)
say('A5', b <= a + FACED and worst <= LATEST, 'turned about at a walk it faces the new way after %.2f s before, %.2f s now, at '
    'most %.2f s more; %.2f s at the latest before, %.2f s now, at most %.2f s' % (
        a, b, FACED, max(r['faced'] for r in ao), worst, LATEST))
a, b = sum(r['short'] < 0.25 for r in ao), sum(r['short'] < 0.25 for r in an)
say('A6', b <= SWINGS * len(an), 'turning about at a walk, points with a swing shorter than 0.25 s: %d before, %d of %d now, at '
    'most %.0f %%' % (a, b, len(an), 100 * SWINGS))
rest = tuple(s for s in TURNS if s != 'turn180')
a, b = [sum(r['short'] < 0.25 for r in sel(x, rest, 'WALK')) for x in (og, ng)]
say('A6', b <= a + OTHER, 'the other turns at a walk, points with such a swing: %d before, %d now' % (a, b))
a, b = sum(max(r['shin'] - LIMIT, 0.0) for r in og), sum(max(r['shin'] - LIMIT, 0.0) for r in ng)
say('A7', b <= a + SHIN, 'single changes, shins beyond %.0f mm in sum: %.0f mm before, %.0f mm now' % (LIMIT, a, b))
a, b = sum(r['foot'] > LIMIT for r in og), sum(r['foot'] > LIMIT for r in ng)
say('A7', b <= a + FOOT, 'points with a foot over %.0f mm: %d before, %d now' % (LIMIT, a, b))
a, b = max(r['pelvis'] for r in og), max(r['pelvis'] for r in ng)
say('A7', b <= a + PELVIS, 'hardest jump of the pelvis: %.1f mm before, %.1f mm now' % (a, b))
b = max(r['twist'] for r in ng if r['scene'] in TURNS)
say('A7', b <= TWIST, 'a planted foot turns %.1f degrees on the ground at most' % b)
a = max(r['pelvis'] for r in walk(os_))
hard = [r for r in walk(ns) if r['pelvis'] > a + PELVIS]
say('A7', len(hard) <= RUNS, 'steered on and on at a walk the pelvis jumps %.1f mm at most before, %.1f mm now, more than '
    '%.1f mm in %d of %d runs, at most %d' % (
        a, max(r['pelvis'] for r in walk(ns)), a + PELVIS, len(hard), len(walk(ns)), RUNS))
a, b = sum(r['shinOver'] for r in walk(os_)), sum(r['shinOver'] for r in walk(ns))
say('A7', b <= a + OVER, 'and a shin jumps more than %.0f mm in %d updates before, %d now, at most %d more' % (LIMIT, a, b, OVER))
fell = sum(not r['animated'] for r in ns)
say('A7', fell == 0, 'and %d of %d figures leave the animated state' % (fell, len(ns)))
ro, rn = [r for r in og if r['gait'] == 'RUN'], [r for r in ng if r['gait'] == 'RUN']
same = sum(x == y for x, y in zip(ro, rn))
say('D1', same == len(ro) == len(rn), 'single changes running: %d of %d points the same' % (same, len(ro)))
wo = [r for r in og if r['gait'] == 'WALK' and r['scene'] not in TURNS]
wn = [r for r in ng if r['gait'] == 'WALK' and r['scene'] not in TURNS]
same = sum(x == y for x, y in zip(wo, wn))
say('D1', same == len(wo) == len(wn), 'at a walk starting, also to the side and back, the steady gait, stopping and changing to '
    'a run: %d of %d points the same' % (same, len(wo)))
ro, rn = [r for r in os_ if r['gait'] == 'RUN'], [r for r in ns if r['gait'] == 'RUN']
same = sum(x == y for x, y in zip(ro, rn))
say('D1', same == len(ro) == len(rn), 'steered on and on running: %d of %d runs the same' % (same, len(ro)))
sys.exit(0 if ok else 1)
