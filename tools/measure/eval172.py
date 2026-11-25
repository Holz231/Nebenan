# eval172.py old new group: the rule of round 172, docs/Regeln/RULE172.txt, on two states measured with check170.sh at
# four nudges each. Z1: the runs at a walk in which the pelvis jumps more than 20 mm in an update, on the courses and in
# the single changes, summed over the nudges, at most half as many as before, and no more where there were fewer than
# two. Z2: the hardest pelvis jump on the courses at a walk, in the median of the nudges, 5 mm less where it was over
# 20 mm and no more than 2 mm more else. S1: compare170.py calls no quantity worse.
import statistics, sys
from compare170 import HIGHER, SHOWN, floor, more, nudged
from eval167 import numbers

if __name__ == '__main__':
    old, new, group = sys.argv[1], sys.argv[2], sys.argv[3]
    A = [dict(numbers(v, group), **more(v, group)) for v in nudged(old, group)]
    B = [dict(numbers(v, group), **more(v, group)) for v in nudged(new, group)]
    if len(A) < 4 or len(B) < 4:
        sys.exit('fewer than four nudges of %s or %s on %s' % (old, new, group))
    worse = []
    for key in SHOWN:
        a, b = [n[key] for n in A], [n[key] for n in B]
        apart = max(b) < min(a) if key in HIGHER else min(b) > max(a)
        if apart and abs(statistics.median(b) - statistics.median(a)) >= floor(key, statistics.median(a)):
            worse.append('%s %.2f to %.2f against %.2f to %.2f' % (key, min(b), max(b), min(a), max(a)))
    was, now = sum(n['pelvis over 20 walk'] for n in A), sum(n['pelvis over 20 walk'] for n in B)
    z1 = now <= 0.5 * was if was >= 2 else now <= was
    print('Z1 runs at a walk with the pelvis over 20 mm, over the nudges: %d, before %d: %s' % (
        now, was, 'held' if z1 else 'MISSED'))
    was, now = statistics.median(n['pelvis walk'] for n in A), statistics.median(n['pelvis walk'] for n in B)
    z2 = now <= was - 5.0 if was > 20.0 else now <= was + 2.0
    print('Z2 hardest pelvis jump on the courses at a walk, median: %.1f mm, before %.1f: %s' % (
        now, was, 'held' if z2 else 'MISSED'))
    print('S1 quantities compare170.py calls worse: %s: %s' % (
        '; '.join(worse) if worse else 'none', 'MISSED' if worse else 'held'))
    print('all held' if z1 and z2 and not worse else 'missed')
