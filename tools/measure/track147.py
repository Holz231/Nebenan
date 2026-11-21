# track147.py variant spec [spec...]: steady runs of 9 s and how wide the track is from 5 s on. A spec is a kind and a speed
# in m/s: f forward facing where it moves, x forward with a fixed heading, s to the side and b backward with a fixed
# heading, e.g. f:3.9. The track is how far apart across the way of the capsule consecutive left and right feet land (the
# foot bodies at each landing, mean, in cm); with it the step along the way and what gait100.py says of the run (feet
# apart across the pelvis, how low the pelvis goes, the jumps of feet, shins, hands and pelvis, how far a planted foot
# slides and how far the feet turn from the heading). Keeps the runs as out/tmp/track147_<variant>_<spec>.csv and writes
# out/res/<variant>/track147.json.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
sys.path.insert(0, M)
import gait100

SCRIPT = {'f': '0:0:0,1:0:{v}', 'x': '0:0:0:0,1:0:{v}:0', 's': '0:0:0:0,1:{v}:0:0', 'b': '0:0:0:0,1:0:-{v}:0'}


def track(path, t0):
    rows = list(csv.DictReader(open(path)))
    t = np.array([float(r['t']) for r in rows])
    fx = np.array([float(r['fx']) for r in rows])
    fz = np.array([float(r['fz']) for r in rows])
    lands = []
    for k, (sw, b) in enumerate((('swingL', 15), ('swingR', 18))):
        s = [int(float(r[sw])) for r in rows]
        for i in range(1, len(rows)):
            if s[i - 1] == 1 and s[i] == 0 and t[i] >= t0:
                lands.append((t[i], k, float(rows[i]['b%dx' % b]), float(rows[i]['b%dz' % b]), i))
    lands.sort()
    widths, steps = [], []
    for a, b in zip(lands, lands[1:]):
        if a[1] == b[1]:
            continue
        i = b[4]
        j = max(i - 6, 0)
        way = np.array([fx[i] - fx[j], fz[i] - fz[j]])
        n = np.linalg.norm(way)
        if n < 1e-6:
            continue
        way /= n
        d = np.array([b[2] - a[2], b[3] - a[3]])
        widths.append(abs(-d[0] * way[1] + d[1] * way[0]))
        steps.append(abs(d @ way))
    return 100.0 * float(np.mean(widths)), float(np.mean(steps)), len(lands)


def one(variant, spec):
    kind, v = spec.split(':')
    path = os.path.join(W, 'tmp', 'track147_%s_%s%s.csv' % (variant, kind, v))
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), SCRIPT[kind].format(v=v), path, '9'], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    width, step, lands = track(path, 5.0)
    r = gait100.analyze(path, 5.0)
    out = {'track': width, 'step': step, 'landings': lands}
    for k in ('feetSep', 'low', 'j_feet', 'j_shins', 'j_hands', 'j_pelvis', 'slide', 'footYaw'):
        out[k] = float(r[k])
    return spec, out


if __name__ == '__main__':
    variant, specs = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    with ThreadPoolExecutor(4) as ex:
        res = dict(ex.map(lambda s: one(variant, s), specs))
    for spec, r in res.items():
        print('%-8s track %5.1f cm, step %.2f m, feet apart %.1f cm, low %.1f cm, jumps feet %.1f shins %.1f hands %.1f '
              'pelvis %.1f mm, slide %.1f, feet turned %.1f deg' % (spec, r['track'], r['step'], 100 * r['feetSep'],
                                                                100 * r['low'], r['j_feet'], r['j_shins'], r['j_hands'],
                                                                r['j_pelvis'], r['slide'], r['footYaw']))
    path = os.path.join(W, 'res', variant, 'track147.json')
    old = json.load(open(path)) if os.path.exists(path) else {}
    old.update(res)
    json.dump(old, open(path, 'w'), indent=1, sort_keys=True)
