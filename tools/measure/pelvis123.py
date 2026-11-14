# pelvis123.py csv t0 [updates]: the hardest jump of the pelvis in a run written by lab101 from t0 on, and the updates
# around it: the jump of the pelvis with its part up and down, the height of the root, how far the root is down for the
# legs and how fast, the crouch of the gait, and for each foot whether it swings, how far along, whether it is behind
# the hip, its roll and how far its leg is stretched
import csv, sys
import numpy as np
rows = list(csv.DictReader(open(sys.argv[1])))
t0 = float(sys.argv[2])
n = int(sys.argv[3]) if len(sys.argv) > 3 else 5
d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
t = d['t']
P = lambda b: np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
pel = P(0)
acc = np.zeros((len(t), 3)); acc[1:-1] = 1000 * (pel[2:] - 2 * pel[1:-1] + pel[:-2])
j = np.linalg.norm(acc, axis=1)
st = {}
for s, b in (('L', 13), ('R', 16)):
    hip, knee, ank = P(b), P(b + 1), P(b + 2)
    st[s] = np.linalg.norm(hip - ank, axis=1) / (np.linalg.norm(hip - knee, axis=1) + np.linalg.norm(knee - ank, axis=1))
i0 = int(np.argmax(j * (t >= t0)))
print('hardest pelvis jump %.1f mm at %.3f s' % (j[i0], t[i0]))
print('t      jump (up)    rootY  rdrop  rrate  crouch | L: sw along beh roll stretch | R: sw along beh roll stretch | speed')
for i in range(max(i0 - n, 1), min(i0 + n, len(t) - 1)):
    print('%.3f %5.1f (%6.1f) %.3f %.3f %6.2f %.3f | %d %.2f %d %5.2f %.3f | %d %.2f %d %5.2f %.3f | %.2f' % (
        t[i], j[i], acc[i][1], d['rootY'][i], d['rdrop'][i], d['rrate'][i], d['crouch'][i],
        d['swingL'][i], d['alongL'][i], d['behindL'][i], d['rollL'][i], st['L'][i],
        d['swingR'][i], d['alongR'][i], d['behindR'][i], d['rollR'][i], st['R'][i], (d['wishX'][i] ** 2 + d['wishZ'][i] ** 2) ** 0.5))
