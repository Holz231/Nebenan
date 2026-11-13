# sweepf102.py variant [variant...]: the scenes of sweep99.py of round 99 with res/<variant>/lab101, figures that face where
# they move (starting and stopping, walk and run changes, turns of 90 and 180 degrees), each change at NPTS points of one
# gait cycle. Writes res/<variant>/sweepf.json: per point the largest jump (mm per update, second difference) of feet,
# shins, hands and pelvis from 0.1 s before the change on, and how far the pelvis goes below standing (m).
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
S = _W + '/'
import gait100 as g
import extra118
# sweepf112.py: as sweepf102.py with the speeds from the environment: WALK, RUN and FAST in m/s (1.6, 3.0 and 4.8 in
# sweepf102.py), CW, CR and CF the length of a stride at each in seconds, over which the points of a change spread, and
# OUT the name of the json in res/<variant>/
W, R_, Q = os.environ['WALK'], os.environ['RUN'], os.environ['FAST']
C16, C30, C48 = float(os.environ['CW']), float(os.environ['CR']), float(os.environ['CF'])
OUT = os.environ.get('OUT', 'sweepf')
SC = [('startwalk', '0:0:0,{a}:0:%s' % W, 1.0, 0.1), ('startrun48', '0:0:0,{a}:0:%s' % Q, 1.0, 0.1),
      ('startrun30', '0:0:0,{a}:0:%s' % R_, 1.0, 0.1),
      ('stopwalk', '0:0:0,1:0:%s,{a}:0:0' % W, 4.0, C16 / 8), ('stoprun48', '0:0:0,1:0:%s,{a}:0:0' % Q, 4.0, C48 / 8),
      ('stoprun30', '0:0:0,1:0:%s,{a}:0:0' % R_, 4.0, C30 / 8),
      ('walkrun', '0:0:0,1:0:%s,{a}:0:%s' % (W, Q), 4.0, C16 / 8), ('runwalk', '0:0:0,1:0:%s,{a}:0:%s' % (Q, W), 4.0, C48 / 8),
      ('turnwalk90', '0:0:0,1:0:%s,{a}:%s:0' % (W, W), 4.0, C16 / 8),
      ('turnwalk180', '0:0:0,1:0:%s,{a}:0:-%s' % (W, W), 4.0, C16 / 8),
      ('turnrun90', '0:0:0,1:0:%s,{a}:%s:0' % (Q, Q), 4.0, C48 / 8), ('turnrun180', '0:0:0,1:0:%s,{a}:0:-%s' % (Q, Q), 4.0, C48 / 8),
      ('turnrun180b', '0:0:0,1:0:%s,{a}:0:-%s' % (R_, R_), 4.0, C30 / 8)]
NPTS = int(os.environ.get('NPTS', '24'))
BONES = {'feet': [15, 18], 'shins': [14, 17], 'hands': [9, 12], 'pelvis': [0]}


def one(v, sc, i):
    name, script, a0, da = sc
    a = a0 + i * da * 8 / NPTS
    path = S + 'res/%s/%s_%s_%02d.csv' % (v, OUT, name, i)
    subprocess.run([S + 'res/%s/lab101' % v, script.format(a='%.4f' % a), path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    d = {k: np.array([float(r[k]) for r in rows]) for k in ('t', 'b0y') + tuple('b%d%s' % (b, c) for bs in BONES.values() for b in bs for c in 'xyz')}
    more = extra118.extra(path, a - 0.1)
    os.remove(path)
    t = d['t']
    m = t[1:-1] >= a - 0.1
    r = {}
    for cls, bs in BONES.items():
        w = 0.0
        for b in bs:
            p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
            j = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
            w = max(w, float(np.max(np.where(m, j, 0))))
        r[cls] = w
    r['low'] = float(np.max(np.where(t >= a - 0.1, g.STAND - d['b0y'], -1)))
    r.update(more)
    return r


if __name__ == '__main__':
    vs = sys.argv[1:]
    jobs = {}
    with ThreadPoolExecutor(8) as ex:
        for v in vs:
            for sc in SC:
                for i in range(NPTS):
                    jobs[(v, sc[0], i)] = ex.submit(one, v, sc, i)
        res = {k: f.result() for k, f in jobs.items()}
    LIM = [('feet', 37.0), ('shins', 37.0), ('hands', 15.0), ('pelvis', 10.0), ('low', 0.15)]
    for v in vs:
        json.dump({'%s_%02d' % (n, i): r for (vv, n, i), r in res.items() if vv == v}, open(S + 'res/%s/%s.json' % (v, OUT), 'w'))
    print('%-12s %-6s' % ('scene', 'var') + ''.join('%14s' % k for k, _ in LIM))
    for sc in SC:
        for v in vs:
            row = '%-12s %-6s' % (sc[0], v[:6])
            for k, _ in LIM:
                vals = np.array([res[(v, sc[0], i)][k] for i in range(NPTS)])
                row += '%7.3g(%5.3g)' % (vals.max(), np.percentile(vals, 90))
            print(row)
    print('points over limit (feet/shins 37, hands 15, pelvis 10 mm, low 15 cm) and mean p90:')
    for v in vs:
        row = '%-8s' % v
        for k, lim in LIM:
            row += ' %s %4d' % (k, sum(1 for (vv, n, i), r in res.items() if vv == v and r[k] > lim))
        row += ' |'
        for k, lim in LIM:
            row += ' %.3g' % np.mean([np.percentile([res[(v, sc[0], i)][k] for i in range(NPTS)], 90) for sc in SC])
        print(row)
