"""Where the threads of the solver wait, from a Tracy capture of Nebenan, step by step.

usage: tracy_team.py capture.csv [first_step last_step] [--per N]

The CSV comes from tracy-csvexport of Tracy 0.13.1 with -u, one row per zone: tracy-csvexport -u abriss.tracy >
abriss.csv. The tool is in windows-0.13.1.zip of Tracy, or builds from .fetchcontent-cache/tracy-src/csvexport after a
build with NEBENAN_PROFILE. Python 3, nothing else. first_step and last_step pick a slice of the steps, --per N prints a
line for every N steps as well.

For every "Solve Constraints" zone of the calling thread, the solver of one step, it splits the time of every thread in
that window into work and waiting. Work are the blocks of the team, with domains those of the domains and of the merges
after them, the packs of islands, what the calling thread computes alone (overflow, merging the knots) and other zones.
Waiting goes by what the other threads do at the moment:

  late        a worker before its solver zone starts
  stage       another thread still solves a block of the team, at the end of a stage, outliers apart
  outlier     a block another thread solves takes over 4 times the median of its stage and over 20 us
  packs       no block runs, another thread solves a pack
  alone       no block or pack runs, the calling thread computes alone
  transition  nobody works, between two stages and around them
  pause       nobody works for over 20 us at a stretch

The loss of a cause is its waiting summed over the threads and divided by their number: how much shorter the window
would be if that waiting were work. A step has a team when a worker solves blocks outside of packs. The means are over
the steps with a team. Step 61 of docs/Roadmap.md decides by them. The setup of the solver before each window, which builds
the knots and the domains, comes with it, for step 62.
"""
import bisect
import collections
import csv
import statistics
import sys

BLOCKS = {'Prepare Joints', 'Prepare Wide Contacts', 'Prepare Contacts', 'Integrate Velocities', 'Warm Start', 'Solve',
          'Integrate Positions', 'Relax', 'Restitution', 'Store Wide Impulses', 'Store Impulses', 'Knots', 'Domain',
          'Merge Domains'}
ALONE = {'Overflow', 'Merge Knots'}
# Zones that hold others or a whole step, no work of their own
CONTAINERS = {'Solve Constraints', 'Solver Worker', 'Step', 'Nebenan Step', 'Nebenan Update', 'Nebenan Task'}
CAUSES = ['late', 'stage', 'outlier', 'packs', 'alone', 'transition', 'pause']
OUTLIER_FACTOR = 4.0
OUTLIER_MIN_NS = 20000
PAUSE_MIN_NS = 20000


def load(path):
    """The zones of every thread as (start, end, name), sorted by start, times in ns."""
    zones = collections.defaultdict(list)
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            try:
                start = int(row['ns_since_start'])
                end = start + int(row['exec_time_ns'])
                thread = int(row['thread'])
            except (KeyError, TypeError, ValueError):
                continue
            zones[thread].append((start, end, row['name']))
    for thread_zones in zones.values():
        thread_zones.sort()
    return zones


def covering(cuts, intervals):
    """For every segment between two cuts the intervals that cover it."""
    cover = [[] for _ in range(len(cuts) - 1)]
    for item in intervals:
        i = bisect.bisect_left(cuts, item[0])
        while i < len(cuts) - 1 and cuts[i] < item[1]:
            cover[i].append(item)
            i += 1
    return cover


def analyze(zones, first=None, last=None):
    callers = [t for t, zs in zones.items() if any(z[2] == 'Solve Constraints' for z in zs)]
    if len(callers) != 1:
        sys.exit('expected one thread with Solve Constraints, found %s' % callers)
    caller = callers[0]
    workers = sorted(t for t, zs in zones.items() if t != caller and any(z[2] == 'Solver Worker' for z in zs))
    threads = [caller] + workers
    starts = {t: [z[0] for z in zones[t]] for t in threads}

    # The median length of the blocks of each stage over the whole capture, for the outliers
    lengths = collections.defaultdict(list)
    for t in threads:
        for start, end, name in zones[t]:
            if name in BLOCKS:
                lengths[name].append(end - start)
    medians = {name: statistics.median(values) for name, values in lengths.items()}

    windows = [z for z in zones[caller] if z[2] == 'Solve Constraints'][first:last]
    setups = [z for z in zones[caller] if z[2] == 'Solver Setup']
    setup_starts = [z[0] for z in setups]
    steps = []
    others = collections.Counter()
    for index, (lo, hi, _) in enumerate(windows):
        busy = {}
        blocks = []
        packs = []
        alone = []
        joined = {caller: lo}
        team = False
        for t in threads:
            window_zones = zones[t][bisect.bisect_left(starts[t], lo):bisect.bisect_left(starts[t], hi)]
            thread_packs = [(a, min(b, hi)) for a, b, name in window_zones if name == 'Pack']
            intervals = list(thread_packs)
            for a, b, name in window_zones:
                b = min(b, hi)
                in_pack = any(p[0] <= a and b <= p[1] for p in thread_packs)
                if name in BLOCKS:
                    if in_pack:
                        continue
                    outlier = b - a > max(OUTLIER_FACTOR * medians[name], OUTLIER_MIN_NS)
                    blocks.append((a, b, t, outlier))
                    intervals.append((a, b))
                    team = team or t != caller
                elif name in ALONE:
                    alone.append((a, b))
                    intervals.append((a, b))
                elif name == 'Solver Worker':
                    joined.setdefault(t, a)
                elif name not in CONTAINERS and name != 'Pack' and not in_pack:
                    others[name] += 1
                    intervals.append((a, b))
            packs += [(a, b, t) for a, b in thread_packs]
            joined.setdefault(t, hi)
            busy[t] = intervals
        if not team:
            continue

        # Sweep the segments between all beginnings and ends
        cuts = {lo, hi}
        for t in threads:
            cuts.add(joined[t])
            for a, b in busy[t]:
                cuts.add(a)
                cuts.add(b)
        cuts = sorted(c for c in cuts if lo <= c <= hi)
        segment_count = len(cuts) - 1
        block_cover = covering(cuts, blocks)
        pack_cover = covering(cuts, packs)
        alone_cover = covering(cuts, alone)
        busy_cover = {t: covering(cuts, busy[t]) for t in threads}

        # Runs of segments in which no thread works, the long ones are pauses
        idle = [not any(busy_cover[t][i] for t in threads) for i in range(segment_count)]
        paused = [False] * segment_count
        i = 0
        while i < segment_count:
            j = i
            while j < segment_count and idle[j]:
                j += 1
            if j > i and cuts[j] - cuts[i] > PAUSE_MIN_NS:
                paused[i:j] = [True] * (j - i)
            i = max(j, i + 1)

        waits = {t: collections.Counter() for t in threads}
        work = dict.fromkeys(threads, 0)
        for i in range(segment_count):
            a, b = cuts[i], cuts[i + 1]
            for t in threads:
                if busy_cover[t][i]:
                    work[t] += b - a
                    continue
                if a < joined[t]:
                    cause = 'late'
                elif block_cover[i]:
                    cause = 'outlier' if any(x[3] for x in block_cover[i]) else 'stage'
                elif pack_cover[i]:
                    cause = 'packs'
                elif alone_cover[i]:
                    cause = 'alone'
                else:
                    cause = 'pause' if paused[i] else 'transition'
                waits[t][cause] += b - a

        # The setup of the solver right before the window, which builds the knots and the domains
        k = bisect.bisect_left(setup_starts, lo) - 1
        setup = setups[k][1] - setups[k][0] if k >= 0 and setups[k][1] <= lo else 0

        runs = sum(1 for i in range(segment_count) if block_cover[i] and (i == 0 or not block_cover[i - 1]))
        steps.append({'index': index, 'span': hi - lo, 'setup': setup, 'waits': waits, 'work': work, 'runs': runs,
                      'blocks': len(blocks), 'worker_blocks': sum(1 for x in blocks if x[2] != caller),
                      'outliers': sum(1 for x in blocks if x[3])})
    return caller, workers, steps, len(windows), others


def mean(steps, value):
    return sum(value(s) for s in steps) / len(steps)


def report(caller, workers, steps, window_count, others):
    threads = [caller] + workers
    print('threads: calling %d, workers %s' % (caller, workers))
    print('%d steps, %d with a team' % (window_count, len(steps)))
    if not steps:
        return
    if others:
        print('other zones in the windows: %s' % ', '.join('%s %d' % kv for kv in others.most_common(8)))
    span = mean(steps, lambda s: s['span']) / 1000
    print('solver window %.1f us per step with a team, %.1f blocks of the team in %.1f runs, %.1f of them on workers, '
          '%.2f outliers' % (span, mean(steps, lambda s: s['blocks']), mean(steps, lambda s: s['runs']),
                             mean(steps, lambda s: s['worker_blocks']), mean(steps, lambda s: s['outliers'])))
    print('%-8s %9s ' % ('thread', 'work') + ' '.join('%10s' % c for c in CAUSES) + '   (us per step)')
    totals = collections.Counter()
    for t in threads:
        work = mean(steps, lambda s: s['work'][t]) / 1000
        waits = {c: mean(steps, lambda s: s['waits'][t][c]) / 1000 for c in CAUSES}
        totals['work'] += work
        totals.update(waits)
        print('%-8s %9.1f ' % ('caller' if t == caller else 'w%d' % t, work) + ' '.join('%10.1f' % waits[c] for c in CAUSES))
    count = len(threads)
    loss = {c: totals[c] / count for c in CAUSES}
    print('loss of the window per cause, the waiting divided by %d threads, us per step:' % count)
    for c in CAUSES:
        print('  %-10s %8.1f   %5.1f %% of the window' % (c, loss[c], 100 * loss[c] / span))
    print('  %-10s %8.1f   the window minus the work spread evenly' % ('all', span - totals['work'] / count))
    setup = mean(steps, lambda s: s['setup']) / 1000
    print('solver setup %.1f us per step with a team, setup and window %.1f us' % (setup, setup + span))
    print('for step 61 of docs/Roadmap.md, ms per step with a team:')
    print('  outlier and pause     %.3f   disturbed from 0.05' % ((loss['outlier'] + loss['pause']) / 1000))
    print('  stage and transition  %.3f   Zerlegen from 0.10' % ((loss['stage'] + loss['transition']) / 1000))
    print('  late                  %.3f   Anlaufen from 0.03' % (loss['late'] / 1000))


def report_groups(workers, steps, window_count, per):
    count = 1 + len(workers)
    print('%-11s %5s %10s %10s %10s %10s %10s' % ('steps', 'team', 'window us', 'stage', 'transition', 'outlier', 'pause'))
    for start in range(0, window_count, per):
        group = [s for s in steps if start <= s['index'] < start + per]
        if not group:
            print('%5d-%-5d %5d' % (start, start + per - 1, 0))
            continue

        def loss(cause):
            return mean(group, lambda s: sum(w[cause] for w in s['waits'].values())) / count / 1000

        print('%5d-%-5d %5d %10.1f %10.1f %10.1f %10.1f %10.1f' % (
            start, start + per - 1, len(group), mean(group, lambda s: s['span']) / 1000, loss('stage'),
            loss('transition'), loss('outlier'), loss('pause')))


def main():
    args = sys.argv[1:]
    per = None
    if '--per' in args:
        i = args.index('--per')
        per = int(args[i + 1])
        del args[i:i + 2]
    if not args:
        sys.exit(__doc__)
    first = int(args[1]) if len(args) > 1 else None
    last = int(args[2]) if len(args) > 2 else None
    caller, workers, steps, window_count, others = analyze(load(args[0]), first, last)
    report(caller, workers, steps, window_count, others)
    if per:
        report_groups(workers, steps, window_count, per)


if __name__ == '__main__':
    main()
