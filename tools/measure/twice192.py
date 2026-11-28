# twice192.py variant nudges set [set...]: the foot that lifts again as it has landed while the other one is left
# behind, for round 192, see docs/Roadmap.md. On every course of steer162.py at a walk, at the speeds of the sets
# (common.sh), from its 4 moments of the cycle, at the nudges given (NUDGE in lab101.c, "0,1,2,3"). Per run: how often a
# foot lifts less than SOON seconds after it landed while the other foot stands more than BEHIND meters behind its hip
# along the heading: after it stood for TOUCH seconds or more (twice), as the step a waiting heading asks for, see
# NB_FOOT_STEP_STOOD in figure.c, and after less (touch), as where the clock of the gait has it due as it lands; how
# far a planted foot gets behind its hip (far, m) and how far the pelvis gets below its lowest of the steady walk
# (under, mm). Writes out/res/<variant>/twice192.json and prints the counts. Needs lab101 of the variant.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, lab_env, shifted, W
import steer162 as S
from gait155 import band

SOON, TOUCH, BEHIND = 0.2, 0.08, 0.3
BANDS = {}


def one(job):
    variant, name, label, course, i, nudge = job
    s = speeds(name); v = float(s['WALK']); other = float(s['RUN'])
    a = 4.0 + i * float(s['CW']) / S.MOMENTS
    keys, last = ['0:0:0', '1:0:%.4f' % v], (0.0, v)
    for k in range(int(round(S.LAST / S.DT))):
        now = S.steered(course, v, other, k * S.DT)
        if now != last:
            keys.append('%.4f:%.5f:%.5f' % (a + k * S.DT, now[0], now[1])); last = now
    path = os.path.join(W, 'tmp', 'twice192_%s_%s_%s_%d_%s.csv' % (variant, name, label, i, nudge))
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), ','.join(keys), path, '%.4f' % (a + S.LAST + 1.5)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=dict(lab_env(True), NUDGE=nudge))
    rows = list(csv.DictReader(open(path))); os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    a += shifted(d); t = d['t']; win = (t >= a - 0.1) & (t < a + S.LAST + 1.0)
    sn, cs = np.sin(d['heading']), np.cos(d['heading'])
    along, up = {}, {}
    for leg, thigh, foot in (('L', 13, 15), ('R', 16, 18)):
        along[leg] = (d['b%dx' % foot] - d['b%dx' % thigh]) * sn + (d['b%dz' % foot] - d['b%dz' % thigh]) * cs
        up[leg] = d['swing' + leg] > 0.5
    far = max(float(np.max(-along[leg][win & ~up[leg]])) for leg in 'LR')
    twice, touch = 0, 0
    for leg, mate in (('L', 'R'), ('R', 'L')):
        landed = -1.0
        for k in range(1, len(t)):
            if up[leg][k - 1] and not up[leg][k]: landed = t[k]
            if up[leg][k] and not up[leg][k - 1] and win[k] and landed >= 0.0 and t[k] - landed < SOON:
                left = int(not up[mate][k] and along[mate][k] < -BEHIND)
                if t[k] - landed >= TOUCH: twice += left
                else: touch += left
    low = BANDS[(variant, name, v)]
    if course[0] == 'gears': low = min(low, BANDS[(variant, name, other)])
    under = 1000.0 * float(np.max(low - d['b0y'][win]))
    return dict(set=name, course=label, moment=i, nudge=nudge, twice=twice, touch=touch, far=far, under=under)


def main():
    variant, nudges, sets = sys.argv[1], sys.argv[2].split(','), sys.argv[3:]
    for name in sets:
        for key in ('WALK', 'RUN'):
            v = float(speeds(name)[key]); BANDS[(variant, name, v)] = band(variant, name, v)[0]
    jobs = [(variant, name, label, course, i, n) for name in sets for label, course in S.COURSES for i in range(S.MOMENTS)
            for n in nudges]
    with ThreadPoolExecutor(max_workers=4) as pool:
        res = list(pool.map(one, jobs))
    json.dump(res, open(os.path.join(W, 'res', variant, 'twice192.json'), 'w'))
    count = lambda k, most: sum(r[k] > most for r in res)
    print('%d runs: a foot lifts again as it has landed while the other is left behind %d times on %d runs, and after it '
          'only touched the ground %d times; a planted foot gets more than 0.55 m behind its hip on %d runs, %.2f m at '
          'most; the pelvis more than 30, 40 and 60 mm low on %d, %d and %d runs, %.0f mm at most' % (
              len(res), sum(r['twice'] for r in res), count('twice', 0), sum(r['touch'] for r in res), count('far', 0.55),
              max(r['far'] for r in res), count('under', 30.0), count('under', 40.0), count('under', 60.0),
              max(r['under'] for r in res)))


main()
