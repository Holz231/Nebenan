# first118.py csv t0 t1 [step]: the start of a run, per update: phase, which feet swing and how far along, how soon a foot
# on the ground gets lost (out), hurry, the root below standing (cm), what the gait crouches and the legs drop, the speed
# of the frame, and the ankles along the way of the frame from the pelvis (cm)
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, sys
import numpy as np
path, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
step = int(sys.argv[4]) if len(sys.argv) > 4 else 1
rows = list(csv.DictReader(open(path)))
d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
names = [l.strip() for l in open(_M + '/bones.txt')]
I = {n: i for i, n in enumerate(names)}


def pos(n):
    i = I[n]
    return np.stack([d['b%dx' % i], d['b%dy' % i], d['b%dz' % i]], 1)


pel, fl, fr = pos('pelvis'), pos('footL'), pos('footR')
vx = np.gradient(d['fx'], d['t']); vz = np.gradient(d['fz'], d['t'])
sp = np.hypot(vx, vz)
wl = np.hypot(d['wishX'], d['wishZ'])
dirx = np.where(wl > 0, d['wishX'] / np.maximum(wl, 1e-9), 0.0); dirz = np.where(wl > 0, d['wishZ'] / np.maximum(wl, 1e-9), 1.0)
stand = pel[5:25, 1].mean()
print('    t  phase sw  alongL alongR  outL  outR  hurL hurR  pelvis  crouch  rdrop  speed   L along  y    R along  y   gs')
for i in range(0, len(rows), step):
    t = d['t'][i]
    if t < t0 or t > t1:
        continue
    al = (fl[i, 0] - pel[i, 0]) * dirx[i] + (fl[i, 2] - pel[i, 2]) * dirz[i]
    ar = (fr[i, 0] - pel[i, 0]) * dirx[i] + (fr[i, 2] - pel[i, 2]) * dirz[i]
    print('%6.3f %5.3f %d%d  %5.2f  %5.2f  %5.2f %5.2f  %4.2f %4.2f  %6.1f  %5.1f  %5.1f  %5.2f  %6.1f %5.1f  %6.1f %5.1f  %4.2f' % (
        t, d['phase'][i], d['swingL'][i], d['swingR'][i], d['alongL'][i], d['alongR'][i], min(d['shortL'][i], 9), min(d['shortR'][i], 9),
        d['hurryL'][i], d['hurryR'][i], 100 * (stand - pel[i, 1]), 100 * d['crouch'][i], 100 * d['rdrop'][i], sp[i],
        100 * al, 100 * fl[i, 1], 100 * ar, 100 * fr[i, 1], d['gaitSpeed'][i]))
