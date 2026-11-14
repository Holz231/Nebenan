# turn128.py variant set [set...]: what a shin does where it jumps hardest in the turn-abouts at a run, facing where the
# figure moves: the two scenes turnrun180 and turnrun180b of sweepf112.py at 24 points each, for every set (hf7, hf37,
# ...; speeds from common.sh). For every point the hardest jump of a shin from 0.1 s before the turn on (second
# difference of its origin, mm), which leg, how long after the turn, and what that leg does: "lift" with the updates
# since it lifted (0 to 2), "swing" with how far along, "land" with the updates since it landed (0 to 2), or "stand".
# The bodies follow the pose of the update before, so "lift 0" is the pose the last update on the ground gave. Then how
# often each of these is the hardest jump over 37 mm, and what it sums to beyond 37 mm. Writes
# out/res/<variant>/turn_<set>.json. With "all" as last argument every point is printed, else only the sums.
import csv, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
SHIN = {'L': 14, 'R': 17}
LIMIT = 37.0


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


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'turn128_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), '0:0:0,1:0:%s,%.4f:0:-%s' % (speed, a, speed), path, '%.4f' % (a + 3.0)],
                   check=True, stdout=subprocess.DEVNULL)
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
    variant, sets = args[0], args[1:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = []
    for name in sets:
        s = speeds(name)
        for scene, speed, cycle in (('turnrun180', s['FAST'], float(s['CF'])), ('turnrun180b', s['RUN'], float(s['CR']))):
            jobs += [(variant, name, scene, speed, 4.0 + i * cycle / 24) for i in range(24)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(one, jobs))
    count, excess = {}, {}
    over = 0
    written = {name: {} for name in sets}
    index = {}
    for (variant, name, scene, speed, a), (jump, leg, after, does, other, hurry, v, heading) in results:
        key = does[0] + (' %d' % does[1] if does[0] in ('lift', 'land') else '')
        i = index.get((name, scene), 0)
        index[(name, scene)] = i + 1
        written[name]['%s_%02d' % (scene, i)] = {'jump': jump, 'leg': leg, 'after': after, 'does': key, 'other': other[0], 'hurry': hurry,
                                                 'speed': v, 'heading': heading}
        if show:
            print('%-5s %-12s %7.4f  %5.1f mm  shin %s  %.2f s after  %-8s other %-6s hurry %.2f  speed %.2f  heading %5.1f' % (
                name, scene, a, jump, leg, after, key if does[0] != 'swing' else 'swing %.2f' % does[1], other[0], hurry, v, heading))
        if jump > LIMIT:
            over += 1
            count[key] = count.get(key, 0) + 1
            excess[key] = excess.get(key, 0.0) + jump - LIMIT
    for name in sets:
        json.dump(written[name], open(os.path.join(W, 'res', variant, 'turn_%s.json' % name), 'w'), indent=1, sort_keys=True)
    print('%s on %s: %d of %d points over %.0f mm, %.0f mm beyond it' % (variant, ' '.join(sets), over, len(results), LIMIT, sum(excess.values())))
    for key in sorted(count, key=lambda k: -excess[k]):
        print('  %-8s hardest at %2d points, %5.0f mm beyond' % (key, count[key], excess[key]))
