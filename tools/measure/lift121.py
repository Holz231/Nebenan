# lift121.py csv [t0]: every lift of a foot in a run written by lab101, from t0 on: when, which foot, how far its leg is
# stretched as it lifts (hip to ankle over thigh plus shin, from the bodies), whether it lifts rolled over its toes, how
# much its swing hurries and how far the gait runs then, and the largest jump of its foot and its shin over the first
# five updates of the swing (second differences of the bone origins, mm)
import csv, sys
import numpy as np
rows = list(csv.DictReader(open(sys.argv[1])))
t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k}
t = d['t']


def pos(b):
    return np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)


def jump(p):
    j = np.zeros(len(p))
    j[1:-1] = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
    return j


for side, thigh in (('L', 13), ('R', 16)):
    hip, knee, ankle = pos(thigh), pos(thigh + 1), pos(thigh + 2)
    length = np.linalg.norm(hip - knee, axis=1) + np.linalg.norm(knee - ankle, axis=1)
    stretch = np.linalg.norm(hip - ankle, axis=1) / length
    swing = d['swing' + side]
    js, jf = jump(knee), jump(ankle)
    for i in range(1, len(t) - 6):
        if swing[i] > 0.5 and swing[i - 1] < 0.5 and t[i] >= t0:
            print('%7.3f %s stretch %.3f roll %5.2f hurry %.2f run %.2f speed %.2f | foot %5.1f shin %5.1f mm' % (
                t[i], side, stretch[i - 1], d['roll' + side][i - 1], d['hurry' + side][i], d['runTurn'][i],
                (d['wishX'][i] ** 2 + d['wishZ'][i] ** 2) ** 0.5, jf[i - 1:i + 5].max(), js[i - 1:i + 5].max()))
