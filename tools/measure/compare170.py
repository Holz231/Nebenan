# compare170.py old new group [quantity...]: two states of the library, each measured with check170.sh at several
# nudges, NUDGE in lab101.c: per quantity the range and median over the nudges of either, and whether new is better,
# worse or within the noise, which is where the ranges overlap. Since round 170: a single run of a state is one moment
# of the step against the updates, and its counts and maxima can lie anywhere in that range; a starting acceleration
# of 8 instead of 10 gave 316 shin jumps over 37 mm on the courses at a run in one run, 158 to 174 in four nudges, and
# 147 to 167 at 10. Quantities as eval167.py has them, more lower is better, but along, which is better higher.
import glob, os, re, statistics, sys
from eval167 import numbers
from where129 import W

HIGHER = ('along walk', 'along run')
SHOWN = ('steady run share', 'steady run deep', 'starts deep', 'stops deep', 'turns walk crossed', 'turns run crossed',
         'shuttle crossed', 'turns walk share', 'turns run share', 'shuttle share', 'steady walk deep', 'shin sum',
         'under sum', 'pelvis walk', 'pelvis run', 'shinOver walk', 'shinOver run', 'under walk', 'foot count',
         'foot max', 'pelvis gait', 'faced mean', 'faced max', 'along walk', 'along run')


def nudged(state, group):
    names = [os.path.basename(p) for p in glob.glob(os.path.join(W, 'res', state + '_n*'))]
    names = [n for n in names if re.fullmatch(re.escape(state) + r'_n-?\d+(\.\d+)?', n) and
             os.path.exists(os.path.join(W, 'res', n, 'steer162.%s.json' % group))]
    return sorted(names)


if __name__ == '__main__':
    old, new, group = sys.argv[1], sys.argv[2], sys.argv[3]
    keys = sys.argv[4:] or SHOWN
    A = [numbers(v, group) for v in nudged(old, group)]
    B = [numbers(v, group) for v in nudged(new, group)]
    if not A or not B:
        sys.exit('no nudged runs of %s or %s on %s, see check170.sh' % (old, new, group))
    print('%s against %s on %s, %d and %d nudges: range and median of each' % (new, old, group, len(B), len(A)))
    for key in keys:
        a, b = [n[key] for n in A], [n[key] for n in B]
        if key in HIGHER:
            better, worse = min(b) > max(a), max(b) < min(a)
        else:
            better, worse = max(b) < min(a), min(b) > max(a)
        verdict = 'better' if better else 'worse' if worse else 'within the noise'
        print('%-19s old %8.2f to %8.2f, median %8.2f | new %8.2f to %8.2f, median %8.2f | %s' % (
            key, min(a), max(a), statistics.median(a), min(b), max(b), statistics.median(b), verdict))
