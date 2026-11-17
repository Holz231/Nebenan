# flips135.py variant scene|run set [set...]: how hard a foot on the ground takes it when the legs change between going
# forward and going backward (legBack), in the changes of the gait with a fixed heading of where129.py (one scene, or
# "run" for the eight changes at a run), at 24 points for every set. For every such change of the legs from the change
# of the gait on, and for each foot that stands in the update before it: the hardest jump of the shin of its leg in the
# update of the change and the two after (second difference of its origin, mm), and how far its roll came down in the
# update of the change (rad, positive where a foot rolled over its toes rolls back). Prints for every scene and in all
# how many such feet there are, how many of them with the shin over 37 mm, what those sum to beyond 37 mm and the
# hardest. Writes out/res/<variant>/flips_<scene>_<set>.json. See round 135 in docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'flips135_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    back = np.array([float(r['legBack']) > 0.5 for r in rows])
    flips = []
    for leg, b in w.SHIN.items():
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        roll = np.array([float(r['roll' + leg]) for r in rows])
        p = np.array([[float(r['b%d%s' % (b, c)]) for c in 'xyz'] for r in rows])
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        for k in range(1, len(t) - 3):
            if back[k] != back[k - 1] and t[k] >= a and not sw[k - 1]:
                flips.append({'leg': leg, 'after': float(t[k] - a), 'to': 'back' if back[k] else 'forward',
                              'down': float(roll[k - 1] - roll[k]), 'rolled': float(roll[k - 1]),
                              'jump': float(j[k + 1:k + 4].max())})
    return job, flips


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
    for (variant, name, scene, speed, a), flips in results:
        rows = written.setdefault((name, scene), {})
        rows['%s_%02d' % (scene, len(rows))] = flips
        for key in (scene, 'all'):
            q = total.setdefault(key, [0, 0, 0.0, 0.0])
            hard = [h for h in flips if h['jump'] > LIMIT]
            q[0] += len(flips)
            q[1] += len(hard)
            q[2] += sum(h['jump'] - LIMIT for h in hard)
            q[3] = max([q[3]] + [h['jump'] for h in flips])
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'flips_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a foot on the ground as the legs change between going forward and going backward' %
          (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s %3d feet, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %5.1f mm' % (key, *q))
