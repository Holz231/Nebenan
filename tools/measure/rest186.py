# rest186.py variant nudges set [set...]: a steered humanoid that faces where it moves stops out of a walk, stands for
# 2.5 s and walks on, ahead or back the way it came, for round 186, see docs/Roadmap.md. Walking at the speed of the
# sets (common.sh), from 6 moments of the cycle, at the nudges given (NUDGE in lab101.c, "0,1,2,3"). Per run: how many
# feet land between letting go and walking on (steps), how far a foot is from the other across the heading while it
# stands (wide, mm) and how far a foot moves in the last 1.2 s of standing (moved, mm), and walking on: the hardest
# jump of a foot, a shin and the pelvis in an update in the 1.2 s after (second difference of the origin, mm), how
# far the capsule goes in the first second (way1, m) and when the first foot lifts (lift, s). Writes
# out/res/<variant>/rest186.json and prints the medians and ranges per direction. Needs lab101 of the variant.
import csv, json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, lab_env, shifted, W

MOMENTS, STAND = 6, 2.5


def one(job):
    variant, name, back, i, nudge = job
    v = float(speeds(name)['WALK'])
    a = 4.0 + i * float(speeds(name)['CW']) / MOMENTS
    script = '0:0:0,1:0:%.4f,%.4f:0.00000:0.00000,%.4f:0.00000:%.5f' % (v, a, a + STAND, -v if back else v)
    path = os.path.join(W, 'tmp', 'rest186_%s_%s_%d_%d_%s.csv' % (variant, name, back, i, nudge))
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + STAND + 2.5)], check=True,
                   stdout=subprocess.DEVNULL, env=dict(lab_env(True), NUDGE=nudge))
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    a += shifted(d)
    t = d['t']
    go = a + STAND

    def jump(b):
        p = np.stack([d['b%d%s' % (b, c)] for c in 'xyz'], 1)
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        return j

    res = {'set': name, 'back': back, 'moment': i, 'nudge': nudge, 'speed': v}
    still = (t > go - 1.2) & (t < go - 0.02)
    after = (t > go - 0.02) & (t < go + 1.2)
    lands = lifts = 0
    moved = 0.0
    first = 9.0
    for k, b in (('L', 15), ('R', 18)):
        sw = d['swing' + k]
        change = np.nonzero(sw[1:] != sw[:-1])[0] + 1
        lands += sum(1 for j in change if sw[j] < 0.5 and a < t[j] < go)
        ups = [t[j] - go for j in change if sw[j] > 0.5 and t[j] > go - 0.02]
        first = min(first, ups[0] if ups else 9.0)
        x, z = d['b%dx' % b][still], d['b%dz' % b][still]
        moved = max(moved, 1000.0 * float(np.hypot(x.max() - x.min(), z.max() - z.min())))
    j = np.nonzero(still)[0][-1]
    s, c = math.sin(d['heading'][j]), math.cos(d['heading'][j])
    res['steps'], res['moved'], res['lift'] = lands, moved, first
    res['wide'] = 1000.0 * abs((d['b15x'][j] - d['b18x'][j]) * c - (d['b15z'][j] - d['b18z'][j]) * s)
    res['foot'] = float(max(jump(15)[after].max(), jump(18)[after].max()))
    res['shin'] = float(max(jump(14)[after].max(), jump(17)[after].max()))
    res['pelvis'] = float(jump(0)[after].max())
    w = (t > go) & (t <= go + 1.0)
    res['way1'] = float(np.sum(np.hypot(np.diff(d['fx']), np.diff(d['fz']))[w[1:]]))
    return res


def main():
    variant, nudges, sets = sys.argv[1], sys.argv[2].split(','), sys.argv[3:]
    jobs = [(variant, name, back, i, n) for name in sets for back in (0, 1) for i in range(MOMENTS) for n in nudges]
    with ThreadPoolExecutor(max_workers=4) as pool:
        res = list(pool.map(one, jobs))
    json.dump(res, open(os.path.join(W, 'res', variant, 'rest186.json'), 'w'))
    keys = ('steps', 'wide', 'moved', 'lift', 'foot', 'shin', 'pelvis', 'way1')
    print('walks on  runs | ' + ' | '.join(keys) + '   (median low..high)')
    for back in (0, 1):
        q = [r for r in res if r['back'] == back]
        cells = ['%.2f %.2f..%.2f' % (float(np.median([r[k] for r in q])), min(r[k] for r in q), max(r[k] for r in q))
                 for k in keys]
        print('%-9s %4d | ' % ('back' if back else 'ahead', len(q)) + ' | '.join(cells))


if __name__ == '__main__':
    main()
