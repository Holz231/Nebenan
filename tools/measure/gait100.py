# gait100.py run.csv [t0] [t1]: how a lab100 run looks between t0 and t1 (default 3 s to the end): how far apart the feet
# and the knees are across the heading (negative: crossed), the angle between the thighs, how deep the pelvis goes below
# standing, the stance knee, how far a planted foot slides per update, how far the feet turn from the heading, the step
# length and cadence, and the jumps (second differences of bone origins per update) of feet, shins, hands and pelvis
import csv, math, sys
import numpy as np

STAND = 0.96


def load(path):
    rows = list(csv.DictReader(open(path)))
    return {k: np.array([float(r[k]) for r in rows]) for k in rows[0].keys()}


def P(d, i):
    return np.stack([d['b%dx' % i], d['b%dy' % i], d['b%dz' % i]], axis=1)


def jump(p):
    j = np.zeros(len(p))
    j[1:-1] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
    return j


def angle(a, b):
    c = np.sum(a * b, axis=1) / np.maximum(np.linalg.norm(a, axis=1) * np.linalg.norm(b, axis=1), 1e-9)
    return np.degrees(np.arccos(np.clip(c, -1.0, 1.0)))


def analyze(path, t0=3.0, t1=1e9):
    d = load(path)
    t = d['t']
    m = (t >= t0) & (t <= t1)
    h = d['heading']
    fwd = np.stack([np.sin(h), np.zeros_like(h), np.cos(h)], axis=1)
    # across the pelvis: its local x axis, flattened, so a gait whose legs turn from the heading counts against the hips
    q = np.stack([d['b0qx'], d['b0qy'], d['b0qz'], d['b0qw']], axis=1)
    u, s = q[:, :3], q[:, 3:4]
    vx = np.array([1.0, 0.0, 0.0])
    left = 2.0 * (u @ vx)[:, None] * u + (s * s - np.sum(u * u, axis=1, keepdims=True)) * vx + 2.0 * s * np.cross(u, vx)
    left[:, 1] = 0.0
    left /= np.maximum(np.linalg.norm(left, axis=1, keepdims=True), 1e-9)
    footL, footR = P(d, 15), P(d, 18)
    kneeL, kneeR = P(d, 14), P(d, 17)
    hipL, hipR = P(d, 13), P(d, 16)
    feetSep = np.sum((footL - footR) * left, axis=1)
    kneeSep = np.sum((kneeL - kneeR) * left, axis=1)
    split = angle(kneeL - hipL, kneeR - hipR)
    low = STAND - d['b0y']
    # stance knee: the knee of a foot that does not swing, the angle between thigh and shin
    kneeAngL = angle(kneeL - hipL, footL - kneeL)
    kneeAngR = angle(kneeR - hipR, footR - kneeR)
    stanceKnee = np.where(d['swingL'] < 0.5, kneeAngL, 0.0)
    stanceKnee = np.maximum(stanceKnee, np.where(d['swingR'] < 0.5, kneeAngR, 0.0))
    # a planted foot slides: its sole center moves while it does not swing, from its second update on the ground
    slide = np.zeros(len(t))
    for side, sw in (('L', d['swingL']), ('R', d['swingR'])):
        c = 0.5 * (np.stack([d['toe%sx' % side], d['toe%sy' % side], d['toe%sz' % side]], axis=1) +
                   np.stack([d['heel%sx' % side], d['heel%sy' % side], d['heel%sz' % side]], axis=1))
        mv = np.zeros(len(t))
        mv[1:] = 1000.0 * np.linalg.norm((c[1:] - c[:-1])[:, [0, 2]], axis=1)
        planted = (sw < 0.5)
        steady = np.zeros(len(t), bool)
        steady[2:] = planted[2:] & planted[1:-1] & planted[:-2]
        slide = np.maximum(slide, np.where(steady, mv, 0.0))
    # foot yaw against the heading: the toe direction of each foot
    yaw = np.zeros(len(t))
    for side in 'LR':
        dir_ = np.stack([d['toe%sx' % side] - d['heel%sx' % side], np.zeros(len(t)), d['toe%sz' % side] - d['heel%sz' % side]], axis=1)
        hipFwd = np.stack([-left[:, 2], np.zeros(len(t)), left[:, 0]], axis=1)
        a = np.degrees(np.arctan2(np.sum(dir_ * left, axis=1), np.sum(dir_ * hipFwd, axis=1)))
        yaw = np.where(np.abs(a) > np.abs(yaw), a, yaw)
    res = {
        'feetSep': feetSep[m].min(), 'feetSep10': np.percentile(feetSep[m], 10),
        'kneeSep': kneeSep[m].min(), 'split': split[m].max(), 'low': low[m].max(), 'lowMean': low[m].mean(),
        'stanceKnee': stanceKnee[m].max(), 'slide': slide[m].max(), 'footYaw': np.abs(yaw[m]).max(),
    }
    cls = {'feet': [15, 18], 'shins': [14, 17], 'hands': [9, 12], 'pelvis': [0]}
    for k, idx in cls.items():
        res['j_' + k] = max(jump(P(d, i))[m].max() for i in idx)
    # cadence: landings per second
    lands = 0
    for sw in (d['swingL'], d['swingR']):
        s = sw[m]
        lands += int(np.sum((s[:-1] > 0.5) & (s[1:] < 0.5)))
    dur = t[m][-1] - t[m][0]
    res['steps/s'] = lands / dur if dur > 0 else 0.0
    sp = np.linalg.norm(np.stack([np.gradient(d['fx'], t), np.gradient(d['fz'], t)], axis=1), axis=1)
    res['speed'] = sp[m].mean()
    res['stepLen'] = res['speed'] / res['steps/s'] if res['steps/s'] > 0 else 0.0
    flight = (d['swingL'] > 0.5) & (d['swingR'] > 0.5)
    res['flight'] = flight[m].mean()
    return res


if __name__ == '__main__':
    path = sys.argv[1]
    t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
    t1 = float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
    r = analyze(path, t0, t1)
    print(' '.join('%s %.3g' % (k, v) for k, v in r.items()))
