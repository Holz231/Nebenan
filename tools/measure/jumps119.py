# jumps119.py csv t0 [t1] [count]: the largest jumps (second differences of the bone origins per update, mm) of feet,
# shins and pelvis between t0 and t1, with when, which feet swing and how far along, the speed of the frame and the
# heading, to see what a figure does where it jumps
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
import csv
import numpy as np
path, t0 = _sys.argv[1], float(_sys.argv[2])
t1 = float(_sys.argv[3]) if len(_sys.argv) > 3 else 1e9
count = int(_sys.argv[4]) if len(_sys.argv) > 4 else 6
rows = list(csv.DictReader(open(path)))
d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
names = [l.strip() for l in open(_M + '/bones.txt')]
t = d['t']
m = (t >= t0) & (t <= t1)
sp = np.hypot(np.gradient(d['fx'], t), np.gradient(d['fz'], t))
for n in ('footL', 'footR', 'shinL', 'shinR', 'pelvis'):
    i = names.index(n)
    p = np.stack([d['b%dx' % i], d['b%dy' % i], d['b%dz' % i]], 1)
    a = np.zeros_like(p)
    a[2:] = 1000.0 * (p[2:] - 2.0 * p[1:-1] + p[:-2])
    j = np.where(m, np.linalg.norm(a, axis=1), 0.0)
    top = sorted(np.argsort(-j)[:count])
    print(n)
    for k in top:
        print('  %.3f  %5.1f mm (x %6.1f y %6.1f z %6.1f)  swings %d%d  along %.2f %.2f  hurry %.2f %.2f  speed %.2f  heading %6.1f' % (
            t[k], j[k], a[k, 0], a[k, 1], a[k, 2], d['swingL'][k], d['swingR'][k], d['alongL'][k], d['alongR'][k], d['hurryL'][k], d['hurryR'][k], sp[k], np.degrees(d['heading'][k])))
