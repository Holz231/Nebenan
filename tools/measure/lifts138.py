# lifts138.py variant scene|run set [set...]: how a foot lifts with the legs going backward while the figure is steered
# against the way it moves, in the changes of the gait with a fixed heading of where129.py (one scene, or "run" for the
# eight changes at a run), at 24 points for every set. Counted is every swing that lifts from the change on with the
# legs going backward (legBack of lab101) and the velocity the figure is steered to (wishX, wishZ) more than AGAINST
# against the way it moves (from fx, fz): turning from running backward to running forward, its hip runs on away from a
# foot ahead of it while the gait aims that foot forward. For every swing from the change on whether it is counted, how
# far the figure is steered against its way, how fast its leg (thigh to foot) got longer as it lifted, how much that
# growth fell at most from one update to the next in the four after it lifted, as where the leg comes to its reach and
# stops (mm per update), and the hardest jump of the shin of its leg from the update after it lifts to the fourth after
# (second difference of its origin, mm; the bodies follow the pose of the update before). Prints for every scene and in
# all how many counted lifts there are, how many of them with the shin over 37 mm, what those sum to beyond 37 mm, the
# hardest, and how much a leg stopped at most. Writes out/res/<variant>/lifts138_<scene>_<set>.json. See round 138 in
# docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0
AGAINST = 0.5
LEG = {'L': (13, 15), 'R': (16, 18)}


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'lifts138_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    run = np.array([float(r['gaitRun']) for r in rows])
    swings = []
    back = np.array([float(r['legBack']) > 0.5 for r in rows])
    fx = np.array([float(r['fx']) for r in rows])
    fz = np.array([float(r['fz']) for r in rows])
    vx = np.zeros(len(t))
    vz = np.zeros(len(t))
    vx[1:] = np.diff(fx) / np.diff(t)
    vz[1:] = np.diff(fz) / np.diff(t)
    wx = np.array([float(r['wishX']) for r in rows])
    wz = np.array([float(r['wishZ']) for r in rows])
    moves = np.hypot(vx, vz)
    wish = np.hypot(wx, wz)
    against = np.where((moves > 1e-3) & (wish > 1e-3), -(vx * wx + vz * wz) / np.maximum(moves * wish, 1e-9),
                       0.0)
    for leg, b in w.SHIN.items():
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        th, ft = LEG[leg]
        legs = [[float(r['b%d%s' % (th, c)]) - float(r['b%d%s' % (ft, c)]) for c in 'xyz'] for r in rows]
        length = np.linalg.norm(np.array(legs), axis=1)
        grow = np.zeros(len(t))
        grow[1:] = 1000.0 * np.diff(length)
        p = np.array([[float(r['b%d%s' % (b, c)]) for c in 'xyz'] for r in rows])
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        k = 1
        while k < len(t):
            if sw[k] and not sw[k - 1]:
                end = k
                while end < len(t) and sw[end]:
                    end += 1
                if t[k] >= a and end < len(t) - 1 and k + 5 < len(t):
                    stops = grow[k:k + 4] - grow[k + 1:k + 5]
                    swings.append({'leg': leg, 'after': float(t[k] - a), 'back': bool(back[k]), 'against': float(against[k]),
                                   'grow': float(grow[k]), 'stop': float(stops.max()), 'jump': float(j[k + 1:k + 5].max()),
                                   'counted': bool(back[k] and against[k] > AGAINST)})
                k = end
            k += 1
    return job, swings


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
    for (variant, name, scene, speed, a), swings in results:
        rows = written.setdefault((name, scene), {})
        rows['%s_%02d' % (scene, len(rows))] = swings
        for key in (scene, 'all'):
            q = total.setdefault(key, [0, 0, 0.0, 0.0, 0.0])
            some = [h for h in swings if h['counted']]
            hard = [h for h in some if h['jump'] > LIMIT]
            q[0] += len(some)
            q[1] += len(hard)
            q[2] += sum(h['jump'] - LIMIT for h in hard)
            q[3] = max([q[3]] + [h['jump'] for h in some])
            q[4] = max([q[4]] + [h['stop'] for h in some])
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'lifts138_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a foot that lifts with the legs going backward, steered against the way' % (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s %3d lifts, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %5.1f mm, a leg stopped %4.1f mm'
              % (key, *q))
