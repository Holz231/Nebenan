# land139.py variant [set...]: from running sideways to running backward with a fixed heading, as where129.py runs
# LToBack30 at 24 points for every set (default the 19 sets of the development), every point whose hardest shin jump
# over 37 mm is a leg landing 0.6 s or more after the change (in the update it lands or the two after): how long its
# swing took, how far its foot went, the gait speed as it lifted, the figure speed as it lifted and as it landed, the
# root height as it landed, how far the foot braked in the update it landed (second difference, mm), how fast the
# knee went then and the hurry of the swing. Writes out/res/<variant>/land139.json. Round 139: with the root over
# 0.88 m the leg straightens as the gait changes from walking to running, below it the swing lifted walking.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
sys.path.insert(0, M)
import where129 as WH
variant = sys.argv[1]
sets = sys.argv[2:] or 'h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29 h30 h31 h32 h33 h34 h35 h36'.split()
SHIN = {'L': (14, 'swingL'), 'R': (17, 'swingR')}
FOOT = {'L': 15, 'R': 18}
def one(job):
    name, i, speed, cr = job
    a = 4.0 + i * cr / 24
    path = os.path.join(W, 'tmp', 'land139_%s_%s_%d.csv' % (variant, name, i))
    script = '0:0:0:0,1:%s:0:0,%.4f:0:-%s:0' % (speed, a, speed)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path))); os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    best = None
    for leg, (b, sw) in SHIN.items():
        p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
        j = np.zeros(len(t)); j[2:] = 1000 * np.linalg.norm(p[2:] - 2 * p[1:-1] + p[:-2], axis=1)
        j[t < a - 0.1] = 0
        k = int(np.argmax(j))
        if best is None or j[k] > best[0]:
            best = (j[k], leg, k)
    jump, leg, k = best
    if jump <= 37.0:
        return None
    s = d['swing' + leg]
    if s[k] > 0.5 or t[k] - a < 0.6:
        return None
    kl = k
    while kl > 0 and s[kl - 1] < 0.5:
        kl -= 1
    if k - kl > 2:
        return None
    ks = kl - 1
    while ks > 0 and s[ks - 1] > 0.5:
        ks -= 1
    f = FOOT[leg]
    fp = np.stack([d['b%dx' % f], d['b%dz' % f]], 1)
    step = float(np.linalg.norm(fp[kl] - fp[ks]))
    dt = t[1] - t[0]
    v = lambda kk: float(np.hypot(d['fx'][kk] - d['fx'][kk - 1], d['fz'][kk] - d['fz'][kk - 1]) / dt)
    brake = float(1000 * np.linalg.norm(fp[kl + 1] - 2 * fp[kl] + fp[kl - 1]))
    kp = np.stack([d['b%dx' % (f - 1)], d['b%dz' % (f - 1)]], 1)
    knee = float(np.linalg.norm(kp[kl] - kp[kl - 1]) / dt)
    return dict(set=name, i=i, jump=float(jump), after=float(t[k] - a), land=k - kl, swing=float((kl - ks) * dt),
                step=step,
                gaitLift=float(d['gaitSpeed'][ks]), speedLift=v(ks), speedLand=v(kl), rootY=float(d['rootY'][kl]),
                brake=brake, knee=knee, hurry=float(d['hurry' + leg][kl - 1]))
jobs = []
for name in sets:
    s = WH.speeds(name)
    jobs += [(name, i, s['RUN'], float(s['CR'])) for i in range(24)]
os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
with ThreadPoolExecutor(4) as pool:
    res = [r for r in pool.map(one, jobs) if r]
json.dump(res, open(os.path.join(W, 'res', variant, 'land139.json'), 'w'))
print(len(res), 'late landings over 37 mm, sum %.0f mm beyond' % sum(r['jump'] - 37 for r in res))
for r in sorted(res, key=lambda r: -r['jump'])[:20]:
    print('%-4s %2d %4.1f mm %.2f s land %d swing %.3f s step %.2f m gait %.2f fig %.2f/%.2f root %.3f brake %4.1f '
          'knee %.2f hurry %.2f' % (r['set'], r['i'], r['jump'], r['after'], r['land'], r['swing'], r['step'], r['gaitLift'],
                                    r['speedLift'], r['speedLand'], r['rootY'], r['brake'], r['knee'], r['hurry']))
