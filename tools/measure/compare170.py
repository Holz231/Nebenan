# compare170.py old new group [quantity...]: two states of the library, each measured with check170.sh at several
# nudges, NUDGE in lab101.c: per quantity the range and median over the nudges of either, and whether new is better,
# worse or within the noise, which is where the ranges overlap. Since round 170: a single run of a state is one moment
# of the step against the updates, and its counts and maxima can lie anywhere in that range; a starting acceleration
# of 8 instead of 10 gave 316 shin jumps over 37 mm on the courses at a run in one run, 158 to 174 in four nudges, and
# 147 to 167 at 10. Quantities as eval167.py has them and more() below, lower is better but where HIGHER says, and a
# difference counts where the ranges do not overlap and the medians lie farther apart than the floor of the quantity.
import glob, os, re, statistics, sys
from eval167 import numbers, load
from where129 import W

HIGHER = ('along walk', 'along run', 'onward 90 walk', 'slowest 90 walk')
SHOWN = ('steady run share', 'steady run deep', 'starts deep', 'stops deep', 'turns walk crossed', 'turns run crossed',
         'shuttle crossed', 'turns walk share', 'turns run share', 'shuttle share', 'steady walk deep', 'shin sum',
         'under sum', 'pelvis walk', 'pelvis run', 'shinOver walk', 'shinOver run', 'under walk', 'foot count',
         'foot max', 'pelvis gait', 'faced mean', 'faced max', 'along walk', 'along run',
         'fallen', 'twist walk', 'slide walk', 'twist run', 'slide run', 'faced 180 walk', 'faced 180 run', 'sink walk',
         'sink run', 'letgo pelvis run', 'shin extra', 'foot extra', 'pelvis extra')


def more(v, group):
    """what the rounds from 155 on were about, from the same runs: falls on the courses, how far a standing foot turns
    and slides in the single changes, how long turning about takes, how far the pelvis sinks, the pelvis where the stick
    is let go at a run, and the jumps beyond the steady gait summed over every run"""
    legs, gait, steer = load(v, group)
    n = {'fallen': sum(not x['animated'] for x in steer)}
    for g, name in (('WALK', 'walk'), ('RUN', 'run')):
        q = [x for x in gait if x['gait'] == g and x['scene'] not in ('start', 'steady')]
        n['twist ' + name] = max(x['twist'] for x in q)
        n['slide ' + name] = max(x['slide'] for x in q)
        n['sink ' + name] = max(x['low'] for x in q)
        about = [x['faced'] for x in q if x['scene'] == 'turn180' and x['faced'] is not None]
        n['faced 180 ' + name] = sum(about) / len(about)
    n['letgo pelvis run'] = max(x['pelvis'] for x in steer if x['gait'] == 'RUN' and x['course'] == 'stopgo')
    for part in ('shin', 'foot', 'pelvis'):
        n[part + ' extra'] = sum(x.get(part + 'Extra', 0.0) for x in gait + steer)
    return n


def floor(key, old):
    """the least difference of the medians that counts, as the rules have floors: 2 mm, 2 runs or updates, 2 degrees,
    0.05 s, 0.01 of the speed, half a percent of the updates, and 3 % for the sums of the jumps beyond the steady gait"""
    if key.endswith(' extra'):
        return 0.03 * abs(old)
    if key.startswith('faced'):
        return 0.05
    if key.startswith('along') or key.startswith('onward') or key.startswith('slowest'):
        return 0.01
    if key.endswith('share'):
        return 0.5
    return 2.0


def nudged(state, group):
    names = [os.path.basename(p) for p in glob.glob(os.path.join(W, 'res', state + '_n*'))]
    names = [n for n in names if re.fullmatch(re.escape(state) + r'_n-?\d+(\.\d+)?', n) and
             os.path.exists(os.path.join(W, 'res', n, 'steer162.%s.json' % group))]
    return sorted(names)


if __name__ == '__main__':
    old, new, group = sys.argv[1], sys.argv[2], sys.argv[3]
    keys = sys.argv[4:] or SHOWN
    A = [dict(numbers(v, group), **more(v, group)) for v in nudged(old, group)]
    B = [dict(numbers(v, group), **more(v, group)) for v in nudged(new, group)]
    if not A or not B:
        sys.exit('no nudged runs of %s or %s on %s, see check170.sh' % (old, new, group))
    print('%s against %s on %s, %d and %d nudges: range and median of each' % (new, old, group, len(B), len(A)))
    for key in keys:
        a, b = [n[key] for n in A], [n[key] for n in B]
        if key in HIGHER:
            better, worse = min(b) > max(a), max(b) < min(a)
        else:
            better, worse = max(b) < min(a), min(b) > max(a)
        small = abs(statistics.median(b) - statistics.median(a)) < floor(key, statistics.median(a))
        verdict = 'within the noise' if not (better or worse) else 'apart, under the floor' if small else \
            'better' if better else 'worse'
        print('%-19s old %8.2f to %8.2f, median %8.2f | new %8.2f to %8.2f, median %8.2f | %s' % (
            key, min(a), max(a), statistics.median(a), min(b), max(b), statistics.median(b), verdict))
