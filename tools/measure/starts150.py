# starts150.py variant set [set...]: the four starts from standing with a fixed heading of sweeph111.py, walking and
# running to the side and backward (startL16, startL30, startB16, startB30), once for each speed set of common.sh: all
# 24 points of a start are equal, so one run stands for them. For each start the hardest jump of a shin, a foot and the
# pelvis from the start on (second difference of the origin of the body in an update, mm). Writes
# out/res/<variant>/starts150.json and prints the hardest values and what the shins sum to beyond 37 mm.
import csv, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
COMMON = open(os.path.join(M, 'common.sh')).read()
SC = {'startL16': '0:0:0:0,1.0:{WALK}:0:0', 'startL30': '0:0:0:0,1.0:{RUN}:0:0', 'startB16': '0:0:0:0,1.0:0:-{WALK}:0',
      'startB30': '0:0:0:0,1.0:0:-{RUN}:0'}
PARTS = (('shins', (14, 17)), ('feet', (15, 18)), ('pelvis', (0,)))


def speeds(s):
    m = re.search(r'^SET_%s="([^"]*)"' % s, COMMON, re.M)
    return dict(kv.split('=') for kv in m.group(1).split())


def one(arg):
    variant, s, scene = arg
    path = os.path.join(W, 'tmp', 'starts150_%s_%s_%s.csv' % (variant, s, scene))
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), SC[scene].format(**speeds(s)), path, '4'], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    out = {}
    for name, bodies in PARTS:
        hardest = 0.0
        for b in bodies:
            p = np.array([[float(r['b%d%s' % (b, a)]) for a in 'xyz'] for r in rows])
            j = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
            hardest = max(hardest, float(np.max(j[t[1:-1] > 1.0])))
        out[name] = hardest
    return '%s:%s' % (s, scene), out


if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    with ThreadPoolExecutor(4) as ex:
        res = dict(ex.map(one, [(variant, s, scene) for s in sets for scene in SC]))
    for scene in SC:
        rows = [res['%s:%s' % (s, scene)] for s in sets]
        print('%-9s shins at most %.1f mm, beyond 37 mm in sum %.1f mm, feet at most %.1f mm, pelvis at most %.1f mm' % (
            scene, max(r['shins'] for r in rows), sum(max(r['shins'] - 37.0, 0.0) for r in rows), max(r['feet'] for r in rows),
            max(r['pelvis'] for r in rows)))
    json.dump(res, open(os.path.join(W, 'res', variant, 'starts150.json'), 'w'), indent=1, sort_keys=True)
