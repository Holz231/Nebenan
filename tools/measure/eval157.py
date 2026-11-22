# eval157.py old.json new.json: the rule of round 157, docs/Regeln/RULE157.txt, on the tables gait155.py wrote for the
# state before and the build on the same sets. Prints every condition with its numbers and ends with 1 if one is missed.
import json, sys
import numpy as np

FASTER = {'WALK': 0.12, 'RUN': 0.06}  # A1: a turn-about takes at least this much less time in the mean, s
TWIST = 46.0         # A2: a planted foot turns on the ground no farther in a stance, degrees, as since round 156
SHIN = 2.0           # A3: the sum of the shins beyond LIMIT may grow by this, mm
FOOT = 2             # A4: this many more points with a foot over LIMIT
PELVIS = 2.0         # A5: the hardest jump of the pelvis and its lowest point may grow by this, mm
SHORT = 6            # A6: this many more points with a swing shorter than 0.25 s
LIMIT = 37.0
TURNS = ('turn90', 'turn90R', 'turn135', 'turn180')
STILL = ('start', 'steady', 'stop', 'change')
old, new = json.load(open(sys.argv[1])), json.load(open(sys.argv[2]))
ok = True


def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)


def sel(rows, scenes, gait=None):
    return [r for r in rows if r['scene'] in scenes and (gait is None or r['gait'] == gait)]


for gait in ('WALK', 'RUN'):
    for key in ('faced', 'again'):
        a = float(np.mean([r[key] for r in sel(old, ('turn180',), gait)]))
        b = float(np.mean([r[key] for r in sel(new, ('turn180',), gait)]))
        never = min(r[key] for r in sel(new, ('turn180',), gait)) < 0.0
        say('A1', b <= a - FASTER[gait] and not never, 'turn-about %s, %s after %.2f s before, %.2f s now, at least %.2f s '
            'less' % (gait.lower(), key, a, b, FASTER[gait]))
to, tn = sel(old, TURNS), sel(new, TURNS)
on = sum(r['twist'] > TWIST for r in tn)
say('A2', on == 0, 'a planted foot turns over %.0f degrees at %d of %d points, at most %.1f before and %.1f now' % (
    TWIST, on, len(tn), max(r['twist'] for r in to), max(r['twist'] for r in tn)))
so, sn = sum(max(r['shin'] - LIMIT, 0.0) for r in old), sum(max(r['shin'] - LIMIT, 0.0) for r in new)
say('A3', sn <= so + SHIN, 'shins beyond %.0f mm in sum: %.0f mm before, %.0f mm now' % (LIMIT, so, sn))
fo, fn = sum(r['foot'] > LIMIT for r in old), sum(r['foot'] > LIMIT for r in new)
say('A4', fn <= fo + FOOT, 'points with a foot over %.0f mm: %d before, %d now, at most %d more' % (LIMIT, fo, fn, FOOT))
for key, what in (('pelvis', 'hardest jump of the pelvis'), ('low', 'lowest pelvis')):
    a, b = max(r[key] for r in old), max(r[key] for r in new)
    say('A5', b <= a + PELVIS, '%s: %.1f mm before, %.1f mm now' % (what, a, b))
ho, hn = sum(r['short'] < 0.25 for r in old), sum(r['short'] < 0.25 for r in new)
say('A6', hn <= ho + SHORT, 'points with a swing shorter than 0.25 s: %d before, %d now, at most %d more' % (ho, hn, SHORT))
same = sum(a == b for a, b in zip(sel(old, STILL), sel(new, STILL)))
say('D1', same == len(sel(old, STILL)) and len(sel(old, STILL)) == len(sel(new, STILL)), 'starting, steady gait, stopping and '
    'changing the gait: %d of %d points the same' % (same, len(sel(old, STILL))))
sys.exit(0 if ok else 1)
