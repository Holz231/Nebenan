# sweep101.py variant [variant...]: the strafe scenes of round 100 with f200/res/<variant>/lab100, each change shifted over
# NPTS points of one gait cycle, the figure facing the heading 0 while it moves. Per scene and variant: the largest jump
# (second difference of a bone origin per update, mm) of feet, shins, hands and pelvis from 0.1 s before the change on, the
# deepest the pelvis goes below standing, the feet and knees closest across the pelvis (negative: crossed), the stance knee,
# and the slide of a planted foot; the worst over all points, and in brackets the 90th percentile.
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
S = _W
import gait100 as g
import extra118

C16, C30 = 0.90, 0.77
# name: script with {a} for the time of the change, the first change, the spread of the points, the seconds after the change
SC = [
    ('startL16', '0:0:0:0,{a}:1.6:0:0', 1.0, 0.1, 3.0),
    ('startL30', '0:0:0:0,{a}:3.0:0:0', 1.0, 0.1, 3.0),
    ('startB16', '0:0:0:0,{a}:0:-1.6:0', 1.0, 0.1, 3.0),
    ('startB30', '0:0:0:0,{a}:0:-3.0:0', 1.0, 0.1, 3.0),
    ('startD16', '0:0:0:0,{a}:1.13:1.13:0', 1.0, 0.1, 3.0),
    ('switchLR16', '0:0:0:0,1:1.6:0:0,{a}:-1.6:0:0', 4.0, C16, 3.0),
    ('switchLR30', '0:0:0:0,1:3.0:0:0,{a}:-3.0:0:0', 4.0, C30, 3.0),
    ('fwdToL16', '0:0:0:0,1:0:1.6:0,{a}:1.6:0:0', 4.0, C16, 3.0),
    ('fwdToL30', '0:0:0:0,1:0:3.0:0,{a}:3.0:0:0', 4.0, C30, 3.0),
    ('LToBack16', '0:0:0:0,1:1.6:0:0,{a}:0:-1.6:0', 4.0, C16, 3.0),
    ('LToBack30', '0:0:0:0,1:3.0:0:0,{a}:0:-3.0:0', 4.0, C30, 3.0),
    ('backToFwd16', '0:0:0:0,1:0:-1.6:0,{a}:0:1.6:0', 4.0, C16, 3.0),
    ('backToFwd30', '0:0:0:0,1:0:-3.0:0,{a}:0:3.0:0', 4.0, C30, 3.0),
    ('stopL16', '0:0:0:0,1:1.6:0:0,{a}:0:0:0', 4.0, C16, 3.0),
    ('stopL30', '0:0:0:0,1:3.0:0:0,{a}:0:0:0', 4.0, C30, 3.0),
    ('stopB30', '0:0:0:0,1:0:-3.0:0,{a}:0:0:0', 4.0, C30, 3.0),
    ('walkRunL', '0:0:0:0,1:1.6:0:0,{a}:3.0:0:0', 4.0, C16, 3.0),
    ('runWalkL', '0:0:0:0,1:3.0:0:0,{a}:1.6:0:0', 4.0, C30, 3.0),
    ('lookL16', '0:0:0:0,1:1.6:0:0,{a}:1.6:0:90', 4.0, C16, 3.0),
    ('lookL30', '0:0:0:0,1:3.0:0:0,{a}:3.0:0:90', 4.0, C30, 3.0),
    ('lookB16', '0:0:0:0,1:0:-1.6:0,{a}:0:-1.6:90', 4.0, C16, 3.0),
]
CLS = ['j_feet', 'j_shins', 'j_hands', 'j_pelvis']
KEYS = CLS + ['low', 'feetSep', 'kneeSep', 'stanceKnee', 'split', 'slide', 'resp']
NPTS = int(os.environ.get('NPTS', '24'))
ONLY = os.environ.get('ONLY')


def one(variant, sc, i):
    name, script, a0, spread, after = sc
    a = a0 + spread * i / NPTS
    out = os.path.join(S, 'res', variant, 'sweep')
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, '%s_%02d.csv' % (name, i))
    subprocess.run([os.path.join(S, 'res', variant, 'lab101'), script.format(a='%.4f' % a), path, '%.4f' % (a + after)],
                   check=True, stdout=subprocess.DEVNULL)
    r = g.analyze(path, a - 0.1)
    r['resp'] = response(path, script.format(a='%.4f' % a), a)
    r.update(extra118.extra(path, a - 0.1))
    os.remove(path)
    return r


def response(path, script, a):
    # the time from the change until the capsule goes 90 percent of the speed it wants in that direction, or stopping,
    # until it goes slower than 0.1 m/s; 0 where only the heading changes
    import csv
    keys = [k.split(':') for k in script.split(',')]
    last = [float(x) for x in keys[-1]]
    prev = [float(x) for x in keys[-2]]
    vx, vz = last[1], last[2]
    if (vx, vz) == (prev[1], prev[2]):
        return 0.0
    rows = list(csv.DictReader(open(path)))
    t = np.array([float(r['t']) for r in rows])
    x = np.array([float(r['fx']) for r in rows])
    z = np.array([float(r['fz']) for r in rows])
    v = np.stack([np.gradient(x, t), np.gradient(z, t)], 1)
    sp = (vx * vx + vz * vz) ** 0.5
    if sp == 0.0:
        ok = np.where((t >= a) & (np.linalg.norm(v, axis=1) < 0.1))[0]
    else:
        along = v @ np.array([vx / sp, vz / sp])
        ok = np.where((t >= a) & (along >= 0.9 * sp))[0]
    return float(t[ok[0]] - a) if len(ok) else 9.0


if __name__ == '__main__':
    variants = sys.argv[1:]
    scenes = [s for s in SC if ONLY is None or s[0] in ONLY.split(',')]
    jobs = {}
    with ThreadPoolExecutor(8) as ex:
        for v in variants:
            for sc in scenes:
                for i in range(NPTS):
                    jobs[(v, sc[0], i)] = ex.submit(one, v, sc, i)
        res = {k: f.result() for k, f in jobs.items()}
    for v in variants:
        dump = {}
        path = os.path.join(S, 'res', v, 'sweep.json')
        if os.path.exists(path):
            dump = json.load(open(path))
        for (vv, name, i), r in res.items():
            if vv == v:
                dump['%s_%02d' % (name, i)] = {k: float(x) for k, x in r.items()}
        json.dump(dump, open(path, 'w'))
    print('%-11s %-6s' % ('scene', 'var') + ''.join('%13s' % k[:10] for k in KEYS))
    total = {v: {k: [] for k in KEYS} for v in variants}
    for sc in scenes:
        for v in variants:
            row = '%-11s %-6s' % (sc[0], v[:6])
            for k in KEYS:
                vals = np.array([res[(v, sc[0], i)][k] for i in range(NPTS)])
                worst = vals.min() if k in ('feetSep', 'kneeSep') else vals.max()
                p90 = np.percentile(vals, 10) if k in ('feetSep', 'kneeSep') else np.percentile(vals, 90)
                row += '%7.3g(%4.3g)' % (worst, p90)
                total[v][k].append(worst)
            print(row)
    print('worst over all scenes:')
    for v in variants:
        print('%-18s' % v + ''.join('%13.3g' % (min(total[v][k]) if k in ('feetSep', 'kneeSep') else max(total[v][k])) for k in KEYS))
