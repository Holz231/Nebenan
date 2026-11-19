# walk144.py variant set [set...]: walking from sideways to backward with a fixed heading (LToBack16 of where129.py) at
# 24 points of a cycle for every speed set of common.sh: per point the hardest jump of a toe tip or a heel and of a shin
# from 0.1 s before the change on (second difference per update, mm). Prints for every set and in total at how many
# points each is over 37 mm, what that sums to beyond 37 mm and the hardest, and writes
# out/res/<variant>/walk144_<set>.json with every point.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
sys.path.insert(0, M)
from where129 import SCENES, speeds

LIMIT = 37.0


def jumps(d, keys, t, a):
    best = 0.0
    for k in keys:
        p = np.stack([d[k + 'x'], d[k + 'y'], d[k + 'z']], 1)
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        j[t < a - 0.1] = 0.0
        best = max(best, float(np.max(j)))
    return best


def one(job):
    variant, name, speed, a = job
    path = os.path.join(W, 'tmp', 'walk144_%s_%s_%.4f.csv' % (variant, name, a))
    script = SCENES['LToBack16'][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    return job, jumps(d, ('toeL', 'heelL', 'toeR', 'heelR'), t, a), jumps(d, ('b14', 'b17'), t, a)


def summary(values):
    over = [v for v in values if v > LIMIT]
    return len(over), sum(v - LIMIT for v in over), max(values)


if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    _, speed, spread, first = SCENES['LToBack16']
    jobs = []
    for name in sets:
        s = speeds(name)
        jobs += [(variant, name, s[speed], first + i * float(s[spread]) / 24) for i in range(24)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(one, jobs))
    toes, shins = [], []
    for name in sets:
        mine = [(job, toe, shin) for job, toe, shin in results if job[1] == name]
        out = {'LToBack16_%02d' % i: {'toes': toe, 'shins': shin} for i, (job, toe, shin) in enumerate(mine)}
        json.dump(out, open(os.path.join(W, 'res', variant, 'walk144_%s.json' % name), 'w'), indent=1)
        t, s = [x[1] for x in mine], [x[2] for x in mine]
        toes += t
        shins += s
        print('%-4s toes and heels over 37 mm at %2d, %5.1f mm beyond, hardest %5.1f | shins at %2d, %5.1f mm beyond, hardest %5.1f'
              % ((name,) + summary(t) + summary(s)))
    print('%s walking from sideways to backward on %s, %d points: toes and heels over 37 mm at %d, %.1f mm beyond, hardest %.1f'
          ' | shins over 37 mm at %d, %.1f mm beyond, hardest %.1f' % ((variant, ' '.join(sets), len(toes)) + summary(toes) +
                                                                       summary(shins)))
