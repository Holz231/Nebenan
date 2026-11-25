# eval174.py old new group: the rule of round 174, docs/Regeln/RULE174.txt, on two states measured with check170.sh at
# four nudges each. Z1: turning about at a walk the figure faces the other way at least 0.10 s sooner, in the median of
# the nudges. Z2: the latest it faces the new way in a single turn at a walk is no later than before. P1: the courses at
# a walk on which the pelvis gets more than 30 mm below the steady gait are at most 6 more. S1: compare170.py calls no
# other quantity worse.
import statistics, sys
from compare170 import HIGHER, SHOWN, floor, more, nudged
from eval167 import numbers

if __name__ == '__main__':
    old, new, group = sys.argv[1], sys.argv[2], sys.argv[3]
    A = [dict(numbers(v, group), **more(v, group)) for v in nudged(old, group)]
    B = [dict(numbers(v, group), **more(v, group)) for v in nudged(new, group)]
    if len(A) < 4 or len(B) < 4:
        sys.exit('fewer than four nudges of %s or %s on %s' % (old, new, group))
    med = lambda rows, key: statistics.median(n[key] for n in rows)
    worse = []
    for key in SHOWN:
        a, b = [n[key] for n in A], [n[key] for n in B]
        apart = max(b) < min(a) if key in HIGHER else min(b) > max(a)
        if key != 'under walk' and apart and abs(med(B, key) - med(A, key)) >= floor(key, med(A, key)):
            worse.append('%s %.2f to %.2f against %.2f to %.2f' % (key, min(b), max(b), min(a), max(a)))
    z1 = med(B, 'faced 180 walk') <= med(A, 'faced 180 walk') - 0.10
    z2 = med(B, 'faced max') <= med(A, 'faced max')
    p1 = med(B, 'under walk') <= med(A, 'under walk') + 6.0
    print('Z1 turning about at a walk, faces back after %.2f s, before %.2f: %s' % (
        med(B, 'faced 180 walk'), med(A, 'faced 180 walk'), 'held' if z1 else 'MISSED'))
    print('Z2 the latest it faces the new way in a turn at a walk %.2f s, before %.2f: %s' % (
        med(B, 'faced max'), med(A, 'faced max'), 'held' if z2 else 'MISSED'))
    print('P1 courses at a walk with the pelvis over 30 mm below the steady gait %.1f, before %.1f, 6 more allowed: %s' % (
        med(B, 'under walk'), med(A, 'under walk'), 'held' if p1 else 'MISSED'))
    print('S1 other quantities compare170.py calls worse: %s: %s' % (
        '; '.join(worse) if worse else 'none', 'MISSED' if worse else 'held'))
    print('all held' if z1 and z2 and p1 and not worse else 'missed')
