# start118.py csv [t0] [t1]: a start in numbers: the flights (both feet swinging) with when and how long, the deepest
# the pelvis goes below standing (cm), and the largest second difference per update of pelvis, shins, feet and hands (mm)
# between t0 and t1
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, sys
import numpy as np
path = sys.argv[1]
t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
t1 = float(sys.argv[3]) if len(sys.argv) > 3 else 2.5
rows = list(csv.DictReader(open(path)))
d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
names = [l.strip() for l in open(_M + '/bones.txt')]
I = {n: i for i, n in enumerate(names)}


def pos(n):
    i = I[n]
    return np.stack([d['b%dx' % i], d['b%dy' % i], d['b%dz' % i]], 1)


def jump(n):
    p = pos(n)
    j = np.zeros(len(p))
    j[2:] = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
    return j


t = d['t']
m = (t >= t0) & (t <= t1)
both = (d['swingL'] > 0) & (d['swingR'] > 0)
fl = []
i = 0
while i < len(t):
    if both[i] and t[i] >= t0 - 0.05:
        j = i
        while j + 1 < len(t) and both[j + 1]:
            j += 1
        fl.append((t[i], (j - i + 1) / 60.0))
        i = j + 1
    else:
        i += 1
pel = pos('pelvis')
stand = pel[5:25, 1].mean()
low = 100 * (stand - pel[:, 1])
J = {k: np.maximum.reduce([jump(n) for n in ns]) for k, ns in (('pelvis', ['pelvis']), ('shins', ['shinL', 'shinR']), ('feet', ['footL', 'footR']), ('hands', ['handL', 'handR']))}
print('%s: flights %s | low %.1f cm at %.2f | jumps pelvis %.1f shins %.1f feet %.1f hands %.1f mm | mean pelvis %.2f shins %.2f' % (
    path.split('/')[-1], ' '.join('%.2f:%.2f' % f for f in fl[:5]), low[m].max(), t[m][low[m].argmax()],
    J['pelvis'][m].max(), J['shins'][m].max(), J['feet'][m].max(), J['hands'][m].max(), J['pelvis'][m].mean(), J['shins'][m].mean()))
