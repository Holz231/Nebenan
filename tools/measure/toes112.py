# toes112.py variant [variant...]: as toes102.py for the independent sets of round 112: with SET=strafe the scenes of
# sweeph111.py (WALK, RUN, DIAG, CW, CR from the environment), with SET=face those of sweepf112.py (WALK, RUN, FAST, CW,
# CR, CF). Per scene the largest jump of a toe tip or a heel (second difference per update, mm) from 0.1 s before the
# change on, worst and 90th percentile, how many points lie over 37 mm, and res/<variant>/<OUT>.json with every point.
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
S = _W + '/'
NPTS = int(os.environ.get('NPTS', '24'))
SET = os.environ['SET']
OUT = os.environ['OUT']


def one(v, name, script, a, end):
    path = S + 'res/%s/%s_%s.csv' % (v, OUT, name)
    subprocess.run([S + 'res/%s/lab101' % v, script, path, '%.4f' % end], check=True, stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    m = t[1:-1] >= a - 0.1
    w = 0.0
    for side in 'LR':
        for part in ('toe', 'heel'):
            p = np.array([[float(r['%s%s%s' % (part, side, c)]) for c in 'xyz'] for r in rows])
            j = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
            w = max(w, float(np.max(np.where(m, j, 0))))
    return w


jobs = []
if SET == 'strafe':
    import sweeph111 as sw
    for name, script, a0, spread, after in sw.SC:
        for i in range(NPTS):
            a = a0 + spread * i / NPTS
            jobs.append((name, i, script.format(a='%.4f' % a), a, a + after))
else:
    import sweepf112 as sf
    for name, script, a0, da in sf.SC:
        for i in range(NPTS):
            a = a0 + i * da * 8 / NPTS
            jobs.append((name, i, script.format(a='%.4f' % a), a, a + 3.0))
vs = sys.argv[1:]
with ThreadPoolExecutor(8) as ex:
    fut = {(v, n, i): ex.submit(one, v, '%s_%02d' % (n, i), s, a, e) for v in vs for (n, i, s, a, e) in jobs}
    res = {k: f.result() for k, f in fut.items()}
names = []
for n, _, _, _, _ in jobs:
    if n not in names:
        names.append(n)
for n in names:
    print('%-16s' % n + ''.join(' %-6s %6.1f(%5.1f)' % (v[:6], max(res[(v, n, i)] for i in range(NPTS)),
                                                       np.percentile([res[(v, n, i)] for i in range(NPTS)], 90)) for v in vs))
for v in vs:
    json.dump({'%s_%02d' % (n, i): x for (vv, n, i), x in res.items() if vv == v}, open(S + 'res/%s/%s.json' % (v, OUT), 'w'))
    over = sum(1 for (vv, n, i), x in res.items() if vv == v and x > 37.0)
    p90 = np.mean([np.percentile([res[(v, n, i)] for i in range(NPTS)], 90) for n in names])
    print('%-8s toes/heels over 37 mm %4d of %d, mean p90 %.1f, worst %.1f' % (v, over, len(jobs), p90, max(x for (vv, n, i), x in res.items() if vv == v)))
