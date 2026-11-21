# stand151.py variant scene set [set...]: the shin of a standing leg after a change of the gait with a fixed heading, the
# target of round 151. A scene of where129.py at its 24 points for every set (h2, h29, ...; speeds from common.sh): for
# every point the hardest jump of a shin while its leg stands, on the ground for more than two updates as where129.py
# has it, from the change on (second difference of the origin of the shin, mm), with the leg, how long after the change
# and how fast the figure goes then. A leg that trails as the figure speeds up faster than its gait follows gets longer
# update after update and stops at its reach in one: that is such a jump, see round 150 in docs/Roadmap.md. Prints for
# every set and for all how many points lie over 37 mm, what they sum to beyond 37 and beyond 30 mm and the hardest,
# and writes out/res/<variant>/stand_<scene>_<set>.json.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
sys.path.insert(0, M)
import where129


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'stand151_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = where129.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    best = (0.0, '-', 0.0, 0.0)
    for leg, b in where129.SHIN.items():
        p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        for k in range(2, len(t)):
            if t[k] >= a and j[k] > best[0] and where129.what(d, leg, k)[0] == 'stand':
                v = float(np.hypot(d['fx'][k] - d['fx'][k - 1], d['fz'][k] - d['fz'][k - 1]) / (t[k] - t[k - 1]))
                best = (float(j[k]), leg, float(t[k] - a), v)
    return job, best


def summary(jumps):
    return (sum(x > 37.0 for x in jumps), sum(max(x - 37.0, 0.0) for x in jumps), sum(max(x - 30.0, 0.0) for x in jumps),
            max(jumps))


if __name__ == '__main__':
    variant, scene, sets = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = []
    for name in sets:
        s = where129.speeds(name)
        script, speed, spread, first = where129.SCENES[scene]
        spread = spread if isinstance(spread, float) else float(s[spread])
        jobs += [(variant, name, scene, s[speed], first + i * spread / 24) for i in range(24)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(one, jobs))
    written = {}
    for (variant, name, scene, speed, a), (jump, leg, after, v) in results:
        rows = written.setdefault(name, {})
        rows['%s_%02d' % (scene, len(rows))] = {'jump': jump, 'leg': leg, 'after': after, 'speed': v}
    every = []
    for name, rows in written.items():
        path = os.path.join(W, 'res', variant, 'stand_%s_%s.json' % (scene, name))
        json.dump(rows, open(path, 'w'), indent=1, sort_keys=True)
        jumps = [r['jump'] for r in rows.values()]
        every += jumps
        print('  %-5s over 37 mm at %2d points, %5.1f mm beyond 37 mm, %5.1f mm beyond 30 mm, hardest %.1f mm' % (
            (name,) + summary(jumps)))
    print('%s %s on %d sets, %d points: over 37 mm at %d, %.1f mm beyond 37 mm, %.1f mm beyond 30 mm, hardest %.1f mm' % (
        (variant, scene, len(sets), len(every)) + summary(every)))
