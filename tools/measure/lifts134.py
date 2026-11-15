# lifts134.py variant scene|run set [set...]: how hard a foot lifts while the figure brakes at a run, in the changes of
# the gait with a fixed heading of where129.py (one scene, or "run" for the eight changes at a run), at 24 points for
# every set. Counted is every lift from the change on at which the gait is more than LAG m/s faster than the capsule
# (its speed follows the capsule's and lags as the capsule brakes) and the foot is not rolled over its toes: it lifts
# flat or over its heel, as a foot ahead of the hip does. For every such lift the hardest jump of the shin of its leg
# in the update it lifts and the two after (second difference of its origin, mm). Prints for every scene and in all how
# many such lifts there are, how many of them with the shin over 37 mm, what those sum to beyond 37 mm and the hardest,
# and the same for those of them at which the legs no longer go backward (legBack): there the foot lifts on the arc of
# running forward.
# Writes out/res/<variant>/lifts_<scene>_<set>.json with every lift from the change on, also those not counted. See
# round 134 in docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0
LAG = 0.7


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'lifts134_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    gait = np.array([float(r['gaitSpeed']) for r in rows])
    c = np.array([[float(r['fx']), float(r['fz'])] for r in rows])
    capsule = np.zeros(len(t))
    capsule[1:] = 60.0 * np.linalg.norm(c[1:] - c[:-1], axis=1)
    back = np.array([float(r['legBack']) > 0.5 for r in rows])
    lifts = []
    for leg, b in w.SHIN.items():
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        roll = np.array([float(r['roll' + leg]) for r in rows])
        p = np.array([[float(r['b%d%s' % (b, c)]) for c in 'xyz'] for r in rows])
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        for k in range(1, len(t) - 2):
            if sw[k] and not sw[k - 1] and t[k] >= a:
                lag = float(gait[k] - capsule[k])
                counted = bool(lag > LAG and roll[k - 1] <= 0.0)
                lifts.append({'leg': leg, 'after': float(t[k] - a), 'lag': lag, 'roll': float(roll[k - 1]),
                              'speed': float(capsule[k]), 'back': bool(back[k]), 'jump': float(j[k:k + 3].max()),
                              'counted': counted, 'forward': counted and not back[k]})
    return job, lifts


if __name__ == '__main__':
    variant, scenes, sets = sys.argv[1], w.RUN if sys.argv[2] == 'run' else [sys.argv[2]], sys.argv[3:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = []
    for name in sets:
        s = w.speeds(name)
        for scene in scenes:
            script, speed, spread, first = w.SCENES[scene]
            spread = spread if isinstance(spread, float) else float(s[spread])
            jobs += [(variant, name, scene, s[speed], first + i * spread / 24) for i in range(24)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(one, jobs))
    written, total = {}, {}
    for (variant, name, scene, speed, a), lifts in results:
        rows = written.setdefault((name, scene), {})
        rows['%s_%02d' % (scene, len(rows))] = lifts
        for key in (scene, 'all'):
            q = total.setdefault(key, [0, 0, 0.0, 0.0, 0, 0, 0.0, 0.0])
            for at, which in ((0, 'counted'), (4, 'forward')):
                some = [h for h in lifts if h[which]]
                hard = [h for h in some if h['jump'] > LIMIT]
                q[at] += len(some)
                q[at + 1] += len(hard)
                q[at + 2] += sum(h['jump'] - LIMIT for h in hard)
                q[at + 3] = max([q[at + 3]] + [h['jump'] for h in some])
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'lifts_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a foot not over its toes lifts while the figure brakes at a run, all and with the legs going forward' %
          (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s %3d lifts, %3d over 37 mm, %6.1f mm beyond, hardest %5.1f | forward %3d lifts, %3d over, %6.1f mm, %5.1f' %
              (key, *q))
