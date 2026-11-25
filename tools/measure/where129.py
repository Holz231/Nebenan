# where129.py variant scene set [set...]: what a shin does where it jumps hardest in a change of the gait with a fixed
# heading, as turn128.py does for the turn-abouts facing where the figure moves: one of the scenes below, as
# sweeph111.py runs them, or "run" for the eight changes at a run among them, at 24 points for every set (h2, h29,
# ...; speeds from common.sh). For every point the hardest
# jump of a shin from 0.1 s before the change on (second difference of its origin, mm), which leg, how long after the
# change, and what that leg does: "lift" with the updates since it lifted (0 to 2), "swing" with how far along, "land"
# with the updates since it landed (0 to 2), or "stand". The bodies follow the pose of the update before, so "land 1"
# is the pose the update of the landing gave. Then how often each of these is the hardest jump over 37 mm, and what it
# sums to beyond 37 mm, and for every set the points where it is a leg that swings or lands ("braked", see round 129
# in docs/Roadmap.md) and their sum. Writes out/res/<variant>/where_<scene>_<set>.json. With "all" as last argument
# every point is printed, else only the sums.
import csv, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
SHIN = {'L': 14, 'R': 17}
LIMIT = 37.0


# From round 170 on a change out of the steady gait comes at a moment of the step, not at a time: lab101 holds the
# keys after SYNC_AT back until the left foot lifts next, see SYNC in lab101.c, and the tools move their windows by how
# long that took. Until then a change came a fixed time after the start, and a build that starts differently met its
# turns in other moments of the step: starting at 9.5 instead of 10 m/s^2 moved the sum of the shin jumps from 93 to
# 134 mm. NB_SYNC=0 gives the old timing, to run a check of an earlier round as it ran then.
SYNC_AT = 3.5


def lab_env(steady):
    """the environment for lab101: with the keys of a change out of the steady gait waiting for the left foot, and
    NUDGE passed on as it is set, see lab101.c"""
    env = {k: v for k, v in os.environ.items() if k != 'SYNC'}
    if steady and os.environ.get('NB_SYNC', '1') != '0':
        env['SYNC'] = '%.1f' % SYNC_AT
    return env


def shifted(d):
    """how long the keys waited, 0 for a lab101 from before round 170"""
    return float(d['shift'][-1]) if 'shift' in d else 0.0


def beyond(d, bones, a, win, margin=5.0):
    """How hard bones jump beyond what they do in the steady gait before a change at a, from round 170 on: the hardest
    jump of one of them in an update (second difference of its origin, mm) in the 1.5 s before the change, the base,
    how far the hardest in win is beyond that, at least 0, and in how many updates of win one jumps more than the base
    and margin. A fixed limit of 37 mm lies where a shin jumps in every stride of a steady run, 30 to 38 mm depending on
    how the updates meet the step, so counts over it flip with nothing changed: 147 against 316 on one state."""
    t = d['t']
    before = (t >= a - 1.6) & (t < a - 0.1)
    base, most, jumps = 0.0, 0.0, []
    for b in bones:
        p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        base = max(base, float(j[before].max()))
        most = max(most, float(j[win].max()))
        jumps.append(j)
    over = sum(int(np.sum(j[win] > base + margin)) for j in jumps)
    return base, max(most - base, 0.0), over


def speeds(name):
    for line in open(os.path.join(M, 'common.sh')):
        m = re.match(r'SET_%s="([^"]*)"' % name, line)
        if m:
            return dict(kv.split('=') for kv in m.group(1).split())
    raise SystemExit('no set ' + name)


def what(d, leg, k):
    """what the leg does at update k"""
    sw = d['swing' + leg]
    if sw[k] > 0.5:
        since = 0
        while since < 3 and k - since - 1 >= 0 and sw[k - since - 1] > 0.5:
            since += 1
        return ('lift', since) if since < 3 else ('swing', float(d['along' + leg][k]))
    since = 0
    while since < 3 and k - since - 1 >= 0 and sw[k - since - 1] < 0.5:
        since += 1
    return ('land', since) if since < 3 else ('stand', 0)


# the scenes: the script of velocities with the heading held, the speed it runs at, the cycle its points spread over,
# or the seconds, and the time of the first point
SCENES = {
    'startL30': ('0:0:0:0,{a}:{v}:0:0', 'RUN', 0.1, 1.0),
    'startB30': ('0:0:0:0,{a}:0:-{v}:0', 'RUN', 0.1, 1.0),
    'switchLR30': ('0:0:0:0,1:{v}:0:0,{a}:-{v}:0:0', 'RUN', 'CR', 4.0),
    'fwdToL30': ('0:0:0:0,1:0:{v}:0,{a}:{v}:0:0', 'RUN', 'CR', 4.0),
    'LToBack30': ('0:0:0:0,1:{v}:0:0,{a}:0:-{v}:0', 'RUN', 'CR', 4.0),
    'backToFwd30': ('0:0:0:0,1:0:-{v}:0,{a}:0:{v}:0', 'RUN', 'CR', 4.0),
    'stopL30': ('0:0:0:0,1:{v}:0:0,{a}:0:0:0', 'RUN', 'CR', 4.0),
    'stopB30': ('0:0:0:0,1:0:-{v}:0,{a}:0:0:0', 'RUN', 'CR', 4.0),
    'LToBack16': ('0:0:0:0,1:{v}:0:0,{a}:0:-{v}:0', 'WALK', 'CW', 4.0),
}
RUN = ['startL30', 'startB30', 'switchLR30', 'fwdToL30', 'LToBack30', 'backToFwd30', 'stopL30', 'stopB30']
BRAKED = ('swing', 'land 0', 'land 1', 'land 2')


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'where129_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    best = None
    for leg, b in SHIN.items():
        p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        j[t < a - 0.1] = 0.0
        k = int(np.argmax(j))
        if best is None or j[k] > best[0]:
            other = 'R' if leg == 'L' else 'L'
            speed_now = float(np.hypot(d['fx'][k] - d['fx'][k - 1], d['fz'][k] - d['fz'][k - 1]) / (t[k] - t[k - 1]))
            best = (float(j[k]), leg, float(t[k] - a), what(d, leg, k), what(d, other, k), float(d['hurry' + leg][k]), speed_now,
                    float(np.degrees(d['heading'][k])))
    return job, best


if __name__ == '__main__':
    args = sys.argv[1:]
    show = args[-1] == 'all'
    if show:
        args = args[:-1]
    variant, scenes, sets = args[0], RUN if args[1] == 'run' else [args[1]], args[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = []
    for name in sets:
        s = speeds(name)
        for scene in scenes:
            script, speed, spread, first = SCENES[scene]
            spread = spread if isinstance(spread, float) else float(s[spread])
            jobs += [(variant, name, scene, s[speed], first + i * spread / 24) for i in range(24)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(one, jobs))
    count, excess = {}, {}
    over = 0
    written = {}
    for (variant, name, scene, speed, a), (jump, leg, after, does, other, hurry, v, heading) in results:
        key = does[0] + (' %d' % does[1] if does[0] in ('lift', 'land') else '')
        rows = written.setdefault((name, scene), {})
        rows['%s_%02d' % (scene, len(rows))] = {'jump': jump, 'leg': leg, 'after': after, 'does': key, 'other': other[0],
                                               'hurry': hurry, 'speed': v, 'heading': heading}
        if show:
            print('%-5s %-12s %7.4f  %5.1f mm  shin %s  %.2f s after  %-8s other %-6s hurry %.2f  speed %.2f  heading %5.1f' % (
                name, scene, a, jump, leg, after, key if does[0] != 'swing' else 'swing %.2f' % does[1], other[0], hurry, v, heading))
        if jump > LIMIT:
            over += 1
            count[key] = count.get(key, 0) + 1
            excess[key] = excess.get(key, 0.0) + jump - LIMIT
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'where_%s_%s.json' % (scene, name)), 'w'), indent=1, sort_keys=True)
    print('%s %s on %s: %d of %d points over %.0f mm, %.0f mm beyond it' % (
        variant, args[1], ' '.join(sets), over, len(results), LIMIT, sum(excess.values())))
    for key in sorted(count, key=lambda k: -excess[k]):
        print('  %-8s hardest at %2d points, %5.0f mm beyond' % (key, count[key], excess[key]))
    for name in sets:
        hard = [r for scene in scenes for r in written[(name, scene)].values() if r['jump'] > LIMIT]
        braked = [r for r in hard if r['does'] in BRAKED]
        print('  %-5s over at %3d points, %5.0f mm beyond; braked: %3d points, %5.0f mm' % (
            name, len(hard), sum(r['jump'] - LIMIT for r in hard), len(braked), sum(r['jump'] - LIMIT for r in braked)))
