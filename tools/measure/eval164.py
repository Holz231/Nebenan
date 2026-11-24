# eval164.py old_gait new_gait old_steer new_steer [old_where.txt new_where.txt]: the rule of round 164,
# docs/Regeln/RULE164.txt, on the tables gait155.py and steer162.py wrote for the state before and the build on the same
# sets, and with the two last arguments on what where129.py printed for both. Prints every condition with its numbers
# and ends with 1 if one is missed.
import json, re, sys
import numpy as np

HALF = 0.5           # A1: letting go at a run, the hardest jump of the pelvis is at most this share of before
MOST = 20.0          # A1: and at most this, mm
SHOWN = 30.0         # A1 counts as shown where it is at least this before, mm
HARD = 30.0          # A2: a run counts as hard where the pelvis jumps more than this in an update, mm
THIRD = 1.0 / 3.0    # A2: steered on and on at a run, the hard runs are at most this share of before and FLOOR
FLOOR = 2
HARDS = 6            # A2 counts as shown from this many hard runs before
OVER = 8             # A3, A4: this many more updates with a shin over LIMIT running, A4: 2 walking
SHORTS = 8           # A3: this many more swings shorter than 0.25 s running, A4: 6 walking
SUNK = 2             # A3, A4: this many more runs with the pelvis more than 30 mm under the steady gait
JUMP = 2.0           # A3 to A5: the hardest jump of a shin, a foot or the pelvis may grow by this, mm
SHIN = 10.0          # A5: single changes, the sum of the shins beyond LIMIT may grow by this, mm
FOOT = 2             # A5: this many more points with a foot over LIMIT
AGAIN = 0.03         # A5: changing from a walk to a run it is at its new speed no more than this later, mean, s
POINTS = 2           # D2: the table with a fixed heading, this many more points over LIMIT
BEYOND = 10          # D2: and this much more beyond it in sum, mm
LIMIT = 37.0
og, ng, os_, ns = [json.load(open(p)) for p in sys.argv[1:5]]
ok = True


def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)


def part(rows, gait, course=None):
    return [r for r in rows if r['gait'] == gait and (course is None or r['course'] == course)]


def most(rows, key):
    return max(r[key] for r in rows)


a, b = most(part(os_, 'RUN', 'stopgo'), 'pelvis'), most(part(ns, 'RUN', 'stopgo'), 'pelvis')
say('A1', b <= HALF * a and b <= MOST and a >= SHOWN, 'let go for 0.2 s at a run, the pelvis jumps %.1f mm at most before, '
    '%.1f mm now, at most %.0f %% of before and %.0f mm' % (a, b, 100 * HALF, MOST))
ro, rn = part(os_, 'RUN'), part(ns, 'RUN')
a, b = sum(r['pelvis'] > HARD for r in ro), sum(r['pelvis'] > HARD for r in rn)
say('A2', b <= THIRD * a + FLOOR and a >= HARDS, 'steered on and on at a run the pelvis jumps more than %.0f mm in %d of %d runs '
    'before, %d now, at most a third of before and %d; %.1f and %.1f mm at most' % (
        HARD, a, len(ro), b, FLOOR, most(ro, 'pelvis'), most(rn, 'pelvis')))
for name, old, new, over, shorts in (('A3', ro, rn, OVER, SHORTS), ('A4', part(os_, 'WALK'), part(ns, 'WALK'), 2, 6)):
    gait = 'running' if name == 'A3' else 'walking'
    a, b = sum(r['shinOver'] for r in old), sum(r['shinOver'] for r in new)
    say(name, b <= a + over, 'steered on and on %s a shin jumps more than %.0f mm in %d updates before, %d now, at most %d '
        'more' % (gait, LIMIT, a, b, over))
    a, b = sum(r['shorts'] for r in old), sum(r['shorts'] for r in new)
    say(name, b <= a + shorts, '%s, swings shorter than 0.25 s: %d before, %d now, at most %d more' % (gait, a, b, shorts))
    a, b = sum(r['under'] > 30.0 for r in old), sum(r['under'] > 30.0 for r in new)
    say(name, b <= a + SUNK, '%s, runs with the pelvis more than 30 mm under the steady gait: %d before, %d now' % (gait, a, b))
    for key in ('shin', 'foot') + (('pelvis',) if name == 'A4' else ()):
        a, b = most(old, key), most(new, key)
        say(name, b <= a + JUMP, '%s, hardest jump of a %s: %.1f mm before, %.1f mm now' % (gait, key, a, b))
fell = sum(not r['animated'] for r in ns)
say('A3', fell == 0, '%d of %d figures leave the animated state' % (fell, len(ns)))
a, b = sum(max(r['shin'] - LIMIT, 0.0) for r in og), sum(max(r['shin'] - LIMIT, 0.0) for r in ng)
say('A5', b <= a + SHIN, 'single changes, shins beyond %.0f mm in sum: %.0f mm before, %.0f mm now' % (LIMIT, a, b))
a, b = sum(r['foot'] > LIMIT for r in og), sum(r['foot'] > LIMIT for r in ng)
say('A5', b <= a + FOOT, 'points with a foot over %.0f mm: %d before, %d now' % (LIMIT, a, b))
a, b = most(og, 'pelvis'), most(ng, 'pelvis')
say('A5', b <= a + JUMP, 'hardest jump of the pelvis: %.1f mm before, %.1f mm now' % (a, b))
co, cn = [[r for r in x if r['scene'] == 'change' and r['gait'] == 'WALK'] for x in (og, ng)]
a, b = float(np.mean([r['again'] for r in co])), float(np.mean([r['again'] for r in cn]))
say('A5', b <= a + AGAIN, 'changing from a walk to a run, at its new speed after %.2f s before, %.2f s now, at most %.2f s '
    'more' % (a, b, AGAIN))
ko, kn = [[r for r in x if r['course'].startswith('circle')] for x in (os_, ns)]
same = sum(x == y for x, y in zip(ko, kn))
say('D1', same == len(ko) == len(kn), 'steered on and on with the stick going round: %d of %d runs the same' % (same, len(ko)))
so, sn = [[r for r in x if not (r['scene'] == 'change' and r['gait'] == 'WALK')] for x in (og, ng)]
same = sum(x == y for x, y in zip(so, sn))
say('D1', same == len(so) == len(sn), 'single changes but for changing from a walk to a run: %d of %d points the same' % (
    same, len(so)))
if len(sys.argv) > 6:
    sums = []
    for path in sys.argv[5:7]:
        found = [re.search(r'over at\s+(\d+) points,\s+(\d+) mm beyond', l) for l in open(path)]
        sums.append((sum(int(m.group(1)) for m in found if m), sum(int(m.group(2)) for m in found if m)))
    say('D2', sums[1][0] <= sums[0][0] + POINTS and sums[1][1] <= sums[0][1] + BEYOND, 'changes at a run with a fixed heading, a '
        'shin over %.0f mm at %d points and %d mm beyond before, %d points and %d mm now' % ((LIMIT,) + sums[0] + sums[1]))
sys.exit(0 if ok else 1)
