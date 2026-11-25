# legs167.py variant set [set...]: do the legs of a steered humanoid go through each other? Measured for round 167, see
# docs/Roadmap.md. The rules so far asked whether feet and knees cross across the pelvis; this asks for the shapes: per
# update, how far the capsules of the thighs and shins and the feet of the two legs are apart or overlap, from the poses
# of the bones lab101 writes. A thigh is a capsule of 7 cm radius, a shin one of 6 cm, a foot a box of 10 by 27 by 6 cm,
# taken here as a capsule of 4 cm radius along its length. Scenes, facing where the figure moves, walking and running at
# the speeds of the sets (common.sh): the steady gait, starting from standing, stopping, turning by 90 and 180 degrees
# and steering back and forth every second, each but the steady gait from four moments of the cycle. Per scene and pair
# of parts: the deepest overlap in mm (negative: the closest they come), the share of the updates with more than 5 and
# more than 20 mm of overlap, and the longest time one overlap of more than 5 mm lasts. Writes
# out/res/<variant>/legs167.json. For the starts also how long the capsule takes to 95 % of the steered speed and how
# many feet landed by then. Needs lab101 of the variant: ./build.sh <variant> "-DLAB_BEND" lab101.c From round 170 on a
# change out of the steady gait waits until the left foot lifts, see SYNC in lab101.c.
import csv, json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, lab_env, shifted, W

# bone: index left, index right, the ends of its capsule along the bone and its radius, m, for the humanoid of 1.8 m
PARTS = {'thigh': (13, 16, 0.07, 0.38, 0.07), 'shin': (14, 17, 0.06, 0.38, 0.06), 'foot': (15, 18, -0.035, 0.175, 0.04)}
PAIRS = (('thigh', 'thigh'), ('shin', 'shin'), ('thigh', 'shin'), ('foot', 'shin'), ('foot', 'foot'), ('foot', 'thigh'))
SCENES = ('steady', 'start', 'stop', 'turn90', 'turn180', 'shuttle')
MOMENTS = 4


def rotate(q, v):
    """the vectors v rotated by the quaternions q (x, y, z, w), row by row"""
    u, w = q[:, :3], q[:, 3:4]
    t = 2.0 * np.cross(u, v)
    return v + w * t + np.cross(u, t)


def ends(d, bone, low, high):
    p = np.stack([d['b%d%s' % (bone, a)] for a in 'xyz'], 1)
    q = np.stack([d['b%dq%s' % (bone, a)] for a in 'xyzw'], 1)
    axis = rotate(q, np.tile(np.array([0.0, 1.0, 0.0]), (len(p), 1)))
    return p + low * axis, p + high * axis


def apart(p1, q1, p2, q2):
    """the distance between the segments p1 q1 and p2 q2, row by row"""
    d1, d2, r = q1 - p1, q2 - p2, p1 - p2
    a, e, f = np.sum(d1 * d1, 1), np.sum(d2 * d2, 1), np.sum(d2 * r, 1)
    b, c = np.sum(d1 * d2, 1), np.sum(d1 * r, 1)
    den = a * e - b * b
    s = np.where(den > 1e-12, np.clip((b * f - c * e) / np.maximum(den, 1e-12), 0.0, 1.0), 0.0)
    t = (b * s + f) / e
    s = np.where(t < 0.0, np.clip(-c / a, 0.0, 1.0), np.where(t > 1.0, np.clip((b - c) / a, 0.0, 1.0), s))
    t = np.clip(t, 0.0, 1.0)
    return np.linalg.norm(p1 + s[:, None] * d1 - p2 - t[:, None] * d2, axis=1)


def one(job):
    variant, name, scene, gait, i = job
    s = speeds(name)
    v = float(s[gait])
    cycle = float(s['CW' if gait == 'WALK' else 'CR'])
    a = 4.0 + i * cycle / MOMENTS
    script = '0:0:0'
    if scene == 'start':
        a = 1.0
        script += ',1:0:%.4f' % v
    else:
        script += ',1:0:%.4f' % v
        if scene == 'stop':
            script += ',%.4f:0:0' % a
        elif scene == 'turn90':
            script += ',%.4f:%.4f:0' % (a, v)
        elif scene == 'turn180':
            script += ',%.4f:0:%.4f' % (a, -v)
        elif scene == 'shuttle':
            script += ''.join(',%.4f:0:%.4f' % (a + k, -v if k % 2 == 0 else v) for k in range(4))
    last = 8.0 if scene == 'steady' else a + (4.5 if scene == 'shuttle' else 2.5)
    path = os.path.join(W, 'tmp', 'legs167_%s_%s_%s_%s_%d.csv' % (variant, name, scene, gait, i))
    lab = os.path.join(W, 'res', variant, 'lab101')
    subprocess.run([lab, script, path, '%.4f' % last], check=True, stdout=subprocess.DEVNULL,
                   env=lab_env(scene not in ('steady', 'start')))
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    a += shifted(d)
    t = d['t']
    dt = float(t[1] - t[0])
    win = t >= (4.0 if scene == 'steady' else a - 0.1)
    out = {'set': name, 'scene': scene, 'gait': gait, 'moment': i, 'speed': v, 'updates': int(win.sum())}
    if scene == 'start':
        # how long the capsule takes to 95 % of the steered speed, and how many feet landed by then
        fast = np.hypot(np.gradient(d['fx'], t), np.gradient(d['fz'], t))
        full = float(t[np.argmax((fast >= 0.95 * v) & (t >= a))])
        took = (t >= a) & (t <= full)
        out['full'] = full - a
        out['landed'] = int(sum(np.sum((np.diff(d[s]) < -0.5) & took[1:]) for s in ('swingL', 'swingR')))
    for x, y in PAIRS:
        deep = np.full(len(t), -1e9)
        for left, right in ((x, y), (y, x)) if x != y else ((x, y),):
            l, r = PARTS[left], PARTS[right]
            gap = apart(*ends(d, l[0], l[2], l[3]), *ends(d, r[1], r[2], r[3])) - l[4] - r[4]
            deep = np.maximum(deep, -1000.0 * gap)
        deep = deep[win]
        over = deep > 5.0
        longest, run = 0, 0
        for o in over:
            run = run + 1 if o else 0
            longest = max(longest, run)
        key = x + '-' + y
        out[key] = float(deep.max())
        out[key + ' over5'] = int(over.sum())
        out[key + ' over20'] = int((deep > 20.0).sum())
        out[key + ' longest'] = longest * dt
    return out


if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = [(variant, name, scene, gait, i) for name in sets for scene in SCENES for gait in ('WALK', 'RUN')
            for i in range(1 if scene in ('steady', 'start') else MOMENTS)]
    with ThreadPoolExecutor(4) as pool:
        rows = list(pool.map(one, jobs))
    json.dump(rows, open(os.path.join(W, 'res', variant, 'legs167.json'), 'w'), indent=1)
    print('%s on %s: %d runs. Per pair of parts of the two legs: deepest overlap in mm (negative: closest), share of the '
          'updates over 5 mm and over 20 mm in %%, longest overlap over 5 mm in s' % (variant, ' '.join(sets), len(rows)))
    print('scene    gait   n | ' + ' | '.join('%-22s' % ('%s-%s' % p) for p in PAIRS))
    for scene in SCENES:
        for gait in ('WALK', 'RUN'):
            q = [r for r in rows if r['scene'] == scene and r['gait'] == gait]
            n = sum(r['updates'] for r in q)
            cells = []
            for x, y in PAIRS:
                k = x + '-' + y
                cells.append('%5.0f %4.0f%% %4.0f%% %4.2f' % (max(r[k] for r in q), 100.0 * sum(r[k + ' over5'] for r in q) / n,
                                                           100.0 * sum(r[k + ' over20'] for r in q) / n,
                                                           max(r[k + ' longest'] for r in q)))
            print('%-8s %-4s %3d | %s' % (scene, gait.lower(), len(q), ' | '.join('%-22s' % c for c in cells)))
    for gait in ('WALK', 'RUN'):
        q = [r for r in rows if r['scene'] == 'start' and r['gait'] == gait]
        print('starting to %s: the capsule has 95 %% of its speed after %.2f to %.2f s, %d to %d feet landed by then' % (
            gait.lower(), min(r['full'] for r in q), max(r['full'] for r in q), min(r['landed'] for r in q),
            max(r['landed'] for r in q)))
