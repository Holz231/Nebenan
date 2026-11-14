# land124.py csv [t0]: where the hip is as a foot swings and lands, in a run written by lab101 from t0 on: for every landing
# the time, the foot, its roll as it lands (rad, negative on its heel), how far the hip is ahead of its ankle along its
# toes then (m, from the bodies) and at the middle of its swing, how far the root goes down for the legs over the next
# eight updates (cm) and the largest jump of the pelvis in them (mm). With "sum" as third argument only the range of each
# column over all landings.
import csv, sys
import numpy as np
rows = list(csv.DictReader(open(sys.argv[1])))
t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
brief = len(sys.argv) > 3 and sys.argv[3] == 'sum'


def col(n):
    return np.array([float(r[n]) for r in rows])


def pos(b):
    return np.stack([col('b%d%s' % (b, c)) for c in 'xyz'], 1)


t = col('t')
pel = pos(0)
jp = np.zeros(len(t))
jp[1:-1] = 1000 * np.linalg.norm(pel[2:] - 2 * pel[1:-1] + pel[:-2], axis=1)
rdrop = col('rdrop')
out = []
for s, thigh in (('L', 13), ('R', 16)):
    hip, ank = pos(thigh), pos(thigh + 2)
    toe = np.stack([col('toe%s%s' % (s, c)) for c in 'xz'], 1)
    heel = np.stack([col('heel%s%s' % (s, c)) for c in 'xz'], 1)
    d = toe - heel
    d /= np.linalg.norm(d, axis=1)[:, None]
    ahead = np.sum(d * (hip[:, [0, 2]] - ank[:, [0, 2]]), axis=1)
    sw, prog, roll = col('swing' + s), col('along' + s), col('roll' + s)
    for i in range(1, len(t) - 9):
        if sw[i - 1] > 0.5 and sw[i] < 0.5 and t[i] >= t0:
            j = i - 1
            while j > 0 and sw[j] > 0.5 and prog[j] > 0.5:
                j -= 1
            out.append((t[i], s, roll[i], ahead[i], ahead[j], 100 * (np.max(rdrop[i:i + 9]) - rdrop[i]), np.max(jp[i:i + 9])))
out.sort()
if brief:
    a = np.array([[o[2], o[3], o[4], o[5], o[6]] for o in out])
    print('%3d landings | roll %5.2f to %5.2f | hip ahead as it lands %6.3f to %6.3f m, mid swing %6.3f to %6.3f | root down %4.1f cm, pelvis %4.1f mm at most' % (
        len(out), a[:, 0].min(), a[:, 0].max(), a[:, 1].min(), a[:, 1].max(), a[:, 2].min(), a[:, 2].max(), a[:, 3].max(), a[:, 4].max()))
else:
    print('     t  foot  roll  ahead  mid-swing  root down  pelvis')
    for o in out:
        print('%6.3f   %s  %5.2f %6.3f   %6.3f     %4.1f cm  %4.1f mm' % o)
