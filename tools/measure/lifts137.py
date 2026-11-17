# lifts137.py variant scene|run set [set...]: how a foot lifts while the figure brakes from a run, in the changes of the
# gait with a fixed heading of where129.py (one scene, or "run" for the eight changes at a run), at 24 points for every
# set. Counted is every swing that lifts from the change on with the run share of the spring of the gait (runTurn of
# lab101) more than GAP above the run share of the gait (gaitRun) and the figure more than BEHIND m/s slower than its
# gait (gaitSpeed): braking from a run through standing, the gait falls to walking within a few updates, its spring
# follows slowly, and the speed of the gait lags behind the figure. For every swing from the change on the run shares of
# both at lift, how far the figure lags behind its gait, how high its toes or its heel come over where they lifted, how
# far they rise in an update at most, how far the foot turns toward its heel in an update at most, and the hardest jump
# of the shin of its leg from the update after it lifts to the fourth after (second difference of its origin, mm; the
# bodies follow the pose of the update before). Prints for every scene and in all how many counted lifts there are, how
# many of them with the shin over 37 mm, what those sum to beyond 37 mm, the hardest, and how high their feet came at
# most. Writes out/res/<variant>/lifts137_<scene>_<set>.json. Needs a lab101 that writes gaitRun. See round 137 in
# docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0
GAP = 0.5
BEHIND = 0.8


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'lifts137_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    run = np.array([float(r['gaitRun']) for r in rows])
    swings = []
    spring = np.array([float(r['runTurn']) for r in rows])
    gait = np.array([float(r['gaitSpeed']) for r in rows])
    fx = np.array([float(r['fx']) for r in rows])
    fz = np.array([float(r['fz']) for r in rows])
    moves = np.zeros(len(t))
    moves[1:] = np.hypot(np.diff(fx), np.diff(fz)) / np.diff(t)
    for leg, b in w.SHIN.items():
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        roll = np.array([float(r['roll' + leg]) for r in rows])
        up = np.array([max(float(r['toe%sy' % leg]), float(r['heel%sy' % leg])) for r in rows])
        p = np.array([[float(r['b%d%s' % (b, c)]) for c in 'xyz'] for r in rows])
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        k = 1
        while k < len(t):
            if sw[k] and not sw[k - 1]:
                end = k
                while end < len(t) and sw[end]:
                    end += 1
                if t[k] >= a and end < len(t) - 1:
                    rise = np.diff(up[k - 1:end + 1])
                    turn = np.diff(roll[k - 1:end + 1])
                    swings.append({'leg': leg, 'after': float(t[k] - a), 'lifted': float(run[k]), 'spring': float(spring[k]),
                                   'top': float(1000.0 * (up[k:end].max() - up[k - 1])), 'rise': float(1000.0 * rise.max()),
                                   'heelward': float(max(0.0, -turn.min())), 'jump': float(j[k + 1:min(k + 5, len(t))].max()),
                                   'behind': float(gait[k] - moves[k]),
                                   'counted': bool(spring[k] - run[k] > GAP and gait[k] - moves[k] > BEHIND)})
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
            q[4] = max([q[4]] + [h['top'] for h in some])
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'lifts137_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a foot that lifts while the figure brakes from a run' % (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s %3d lifts, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %5.1f mm, the highest %5.0f mm'
              % (key, *q))
