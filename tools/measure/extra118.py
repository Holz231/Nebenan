# extra118.py: more measures of a run of the lab from a time on: the longest time both feet swing (s), and the mean and
# the 90th percentile of the jump per update (second difference of a bone origin, mm) of pelvis, shins, feet and hands
import csv
import numpy as np
BONES = {'pelvis': [0], 'shins': [14, 17], 'feet': [15, 18], 'hands': [9, 12]}


def extra(path, t0):
    rows = list(csv.DictReader(open(path)))
    t = np.array([float(r['t']) for r in rows])
    both = np.array([float(r['swingL']) > 0 and float(r['swingR']) > 0 for r in rows])
    m = t >= t0
    longest = run = 0
    for b, ok in zip(both, m):
        run = run + 1 if (b and ok) else 0
        longest = max(longest, run)
    out = {'flight': longest / 60.0}
    for name, ids in BONES.items():
        js = []
        for i in ids:
            p = np.array([[float(r['b%d%s' % (i, c)]) for c in 'xyz'] for r in rows])
            j = np.zeros(len(p))
            j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
            js.append(j)
        j = np.maximum.reduce(js)[m]
        out['m_' + name] = float(j.mean())
        out['t_' + name] = float(np.percentile(j, 90))
    return out
