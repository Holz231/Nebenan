# walk176.py variant nudges set [set...]: walking seen as body against legs, for round 176, see docs/Roadmap.md: how
# long the capsule takes for a change and how long the legs take for it. A steered humanoid that faces where it moves,
# at the walking speed of each set (common.sh): starting from standing, stopping, and turning by 45, 90 and 180
# degrees to its left, a change out of walking at 6 moments of the step (SYNC as in gait155.py) and at each nudge of
# the list (for example 0,1,2,3, see NUDGE in lab101.c). Per run, in seconds after the change: when the capsule has
# nine tenths of its new velocity (stopping: a tenth of the old speed left), when the first foot lifts and lands, when
# the last foot lands in the 3 s after the change, and when it faces within 10 degrees of the new way; how many feet
# land after the capsule has its new velocity and before 1.5 s, and how many in the 3 s (steps); the slowest the
# capsule gets as a share of its speed and for how long it is slower than half of it; and against the 1.5 s of walking
# before the change (starting: against second 4 to 7 of the same run): how far a foot is behind the pelvis as it
# lifts, how far the feet are apart across the heading and how far along it as one lands, and how far the trunk leans
# (pelvis to chest against upright, degrees), each the most after the change and the most before. And the hardest jump
# of a foot, a shin and the pelvis in an update in the 3 s after the change (second difference of the origin, mm) and
# how far the capsule goes in them (way, m). Writes out/res/<variant>/walk176.json and prints the medians and ranges
# per scene.
import csv, json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, lab_env, shifted, W

SCENES = {'start': (None, 0, 1), 'stop': (0.0, 0, 6), 'turn45': (1.0, 45, 6), 'turn90': (1.0, 90, 6),
          'turn180': (1.0, 180, 6)}
FEET = {'L': 15, 'R': 18}


def shape(d, win):
    """the most a foot is behind the pelvis as it lifts, the feet are apart across and along as one lands, and the
    trunk leans, over the updates of win"""
    s, c = np.sin(d['heading']), np.cos(d['heading'])
    behind, across, along = 0.0, 0.0, 0.0
    for k, o in (('L', 'R'), ('R', 'L')):
        sw = d['swing' + k]
        for i in np.nonzero(win[1:] & (sw[1:] != sw[:-1]))[0] + 1:
            fx, fz = d['b%dx' % FEET[k]][i], d['b%dz' % FEET[k]][i]
            if sw[i] > 0.5:
                behind = max(behind, (d['b0x'][i] - fx) * s[i] + (d['b0z'][i] - fz) * c[i])
            else:
                x, z = fx - d['b%dx' % FEET[o]][i], fz - d['b%dz' % FEET[o]][i]
                across, along = max(across, abs(x * c[i] - z * s[i])), max(along, abs(x * s[i] + z * c[i]))
    up = d['b4y'] - d['b0y']
    lean = np.degrees(np.arctan2(np.hypot(d['b4x'] - d['b0x'], d['b4z'] - d['b0z']), up))
    return behind, across, along, float(lean[win].max())


def one(job):
    variant, name, scene, i, nudge = job
    keep, turn, moments = SCENES[scene]
    v = float(speeds(name)['WALK'])
    cycle = float(speeds(name)['CW'])
    a = 1.0 if keep is None else 4.0 + i * cycle / moments
    rad = math.radians(turn)
    v1 = v if keep is None else keep * v
    script = '0:0:0' + ('' if keep is None else ',1:0:%.4f' % v)
    script += ',%.4f:%.5f:%.5f' % (a, v1 * math.sin(rad), v1 * math.cos(rad))
    path = os.path.join(W, 'tmp', 'walk176_%s_%s_%s_%d_%s.csv' % (variant, name, scene, i, nudge))
    env = dict(lab_env(keep is not None), NUDGE=nudge)
    span = 7.0 if keep is None else a + 3.0
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % span], check=True,
                   stdout=subprocess.DEVNULL, env=env)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    a += shifted(d)
    t = d['t']
    dt = float(t[1] - t[0])
    vx, vz = np.gradient(d['fx'], dt), np.gradient(d['fz'], dt)
    speed = np.hypot(vx, vz)
    after = (t > a) & (t <= a + 3.0)
    if keep == 0.0:
        there = after & (speed <= 0.1 * v)
    else:
        there = after & (vx * math.sin(rad) + vz * math.cos(rad) >= 0.9 * v)
    body = float(t[there][0] - a) if there.any() else -1.0
    lifts, lands = [], []
    for k in 'LR':
        sw = d['swing' + k]
        for j in np.nonzero(after[1:] & (sw[1:] != sw[:-1]))[0] + 1:
            (lifts if sw[j] > 0.5 else lands).append(float(t[j] - a))
    off = np.abs((d['heading'] - (d['heading'][t <= a][-1] + rad) + math.pi) % (2.0 * math.pi) - math.pi)
    late = after & (off > math.radians(10.0))
    faced = float(t[late][-1] - a + dt) if late.any() else 0.0
    first = (t > a) & (t <= a + 1.5)
    base = (t > 4.0) & (t <= 7.0) if keep is None else (t >= a - 1.6) & (t < a - 0.1)
    now, was = shape(d, first), shape(d, base)
    res = {'set': name, 'scene': scene, 'moment': i, 'nudge': nudge, 'speed': v, 'body': body,
           'lift': min(lifts) if lifts else -1.0, 'land': min(lands) if lands else -1.0,
           'rest': max(lands) if lands else 0.0, 'faced': faced,
           'late': sum(1 for x in lands if body >= 0.0 and body < x <= 1.5),
           'steps': len(lands),
           'slowest': float(speed[first].min() / v), 'slow': float(np.sum(first & (speed < 0.5 * v)) * dt)}
    for n, bones in (('foot', (15, 18)), ('shin', (14, 17)), ('pelvis', (0,))):
        res[n] = 0.0
        for b in bones:
            p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
            j = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
            res[n] = max(res[n], float(j[after[2:]].max()))
    res['way'] = float(np.sum(speed[after]) * dt)
    for n, x, y in zip(('behind', 'across', 'along', 'lean'), now, was):
        res[n], res[n + 'Base'] = float(x), float(y)
    return res


def main():
    variant, nudges, sets = sys.argv[1], sys.argv[2].split(','), sys.argv[3:]
    jobs = [(variant, s, sc, i, n) for n in nudges for s in sets for sc in SCENES for i in range(SCENES[sc][2])]
    with ThreadPoolExecutor(os.cpu_count()) as pool:
        res = list(pool.map(one, jobs))
    json.dump(res, open(os.path.join(W, 'res', variant, 'walk176.json'), 'w'))
    keys = ('body', 'lift', 'land', 'rest', 'faced', 'late', 'steps', 'foot', 'shin', 'pelvis', 'way', 'slowest', 'slow',
            'behind', 'behindBase', 'across', 'acrossBase', 'along', 'alongBase', 'lean', 'leanBase')
    print('scene     runs | ' + ' | '.join(keys) + '   (median low..high)')
    for sc in SCENES:
        q = [r for r in res if r['scene'] == sc]
        cells = []
        for k in keys:
            x = np.array([r[k] for r in q])
            cells.append('%.2f %.2f..%.2f' % (np.median(x), x.min(), x.max()))
        print('%-9s %4d | ' % (sc, len(q)) + ' | '.join(cells))


main()
