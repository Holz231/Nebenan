# rhythm123.py variant set [set...]: how the run finds its rhythm after a change. For every point of the six scenes of the
# facing sets that end at a run (starting to run at both speeds, from a walk into a run, a turn of 90 degrees and the two
# turn-abouts at a run) it runs res/<variant>/lab101 and looks at the lifts of the last second (two to three seconds after
# the change): a run is uneven where two lifts there follow each other within a quarter of the time between the two before
# or after them, that is, where both feet lift almost together instead of in turn. Prints per variant how many points are
# uneven, by scene, and the largest jump of a shin (mm) and what the shins jump beyond 37 mm, in sum, for the even and the
# uneven points, and the highest hurry of a lift in that second. Writes res/<variant>/rhythm_<set>.json.
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
import csv, json, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np


def sets():
    out = {}
    for line in open(_os.path.join(_M, 'common.sh')):
        if line.startswith('SET_'):
            name, rest = line.strip().split('=', 1)
            out[name[4:]] = dict(kv.split('=') for kv in rest.strip('"').split())
    return out


def scenes(e):
    w, r, q = e['WALK'], e['RUN'], e['FAST']
    cw, cr, cf = float(e['CW']), float(e['CR']), float(e['CF'])
    return {'startrun48': ('0:0:0,{a}:0:%s' % q, 1.0, 0.8), 'startrun30': ('0:0:0,{a}:0:%s' % r, 1.0, 0.8),
            'walkrun': ('0:0:0,1:0:%s,{a}:0:%s' % (w, q), 4.0, cw), 'turnrun90': ('0:0:0,1:0:%s,{a}:%s:0' % (q, q), 4.0, cf),
            'turnrun180': ('0:0:0,1:0:%s,{a}:0:-%s' % (q, q), 4.0, cf), 'turnrun180b': ('0:0:0,1:0:%s,{a}:0:-%s' % (r, r), 4.0, cr)}


def one(v, s, e, scene, i):
    script, a0, spread = scenes(e)[scene]
    a = a0 + i * spread / 24
    path = '%s/res/%s/rh_%s_%s_%02d.csv' % (_W, v, s, scene, i)
    subprocess.run(['%s/res/%s/lab101' % (_W, v), script.format(a='%.4f' % a), path, '%.4f' % (a + 3.0)], check=True, stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    _os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    hurry, shin, lifts = 0.0, 0.0, []
    for side, b in (('L', 14), ('R', 17)):
        sw = np.array([float(r['swing' + side]) for r in rows])
        hu = np.array([float(r['hurry' + side]) for r in rows])
        for n in range(1, len(t)):
            if sw[n] > 0.5 and sw[n - 1] < 0.5 and t[n] >= a + 2.0:
                hurry = max(hurry, hu[n])
                lifts.append(t[n])
        p = np.array([[float(r['b%d%s' % (b, x)]) for x in 'xyz'] for r in rows])
        j = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
        shin = max(shin, float(np.max(np.where(t[1:-1] >= a - 0.1, j, 0))))
    gaps = np.diff(sorted(lifts))
    uneven = any(min(gaps[n], gaps[n + 1]) < 0.25 * max(gaps[n], gaps[n + 1]) for n in range(len(gaps) - 1))
    return {'hurry': hurry, 'shin': shin, 'uneven': bool(uneven)}


if __name__ == '__main__':
    v, names = sys.argv[1], sys.argv[2:]
    env = sets()
    jobs = {}
    with ThreadPoolExecutor(8) as ex:
        for s in names:
            for scene in scenes(env[s]):
                for i in range(24):
                    jobs[(s, scene, i)] = ex.submit(one, v, s, env[s], scene, i)
        res = {k: f.result() for k, f in jobs.items()}
    for s in names:
        json.dump({'%s_%02d' % (sc, i): r for (ss, sc, i), r in res.items() if ss == s}, open('%s/res/%s/rhythm_%s.json' % (_W, v, s), 'w'))
    names = list(scenes(env[names[0]]))
    print('%-6s uneven by scene: ' % v + ', '.join('%s %d' % (n, sum(r['uneven'] for (ss, sc, i), r in res.items() if sc == n)) for n in names))
    for label, pick in (('even', lambda r: not r['uneven']), ('uneven', lambda r: r['uneven'])):
        rs = [r for r in res.values() if pick(r)]
        print('%-6s %-7s %3d of %3d points | shins: hardest %5.1f mm, over 37 mm at %3d, beyond 37 mm in sum %5.0f mm | hurry %.2f' % (
            v, label, len(rs), len(res), max([r['shin'] for r in rs] or [0]), sum(r['shin'] > 37 for r in rs),
            sum(max(r['shin'] - 37, 0) for r in rs), max([r['hurry'] for r in rs] or [0])))
