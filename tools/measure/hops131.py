# hops131.py variant scene|run set [set...]: where a foot lifts again at once after it landed, in the changes of the gait
# with a fixed heading of where129.py (one scene, or "run" for the eight changes at a run), at 24 points for every set.
# A hop is a foot that stands for at most two updates between two swings, from 0.1 s before the change on, and lifts
# again while the other foot is on the ground. For every hop the hardest jump of the shin of its leg in the update it
# lifts again and the two after (second difference of its origin, mm). Prints for every scene and in all at how many
# points a foot hops, how many hops there are, how many of them with the shin over 37 mm, and what those sum to
# beyond 37 mm. Writes out/res/<variant>/hops_<scene>_<set>.json. See round 131 in docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'hops131_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    hops = []
    for leg, b in w.SHIN.items():
        other = 'R' if leg == 'L' else 'L'
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        so = np.array([float(r['swing' + other]) > 0.5 for r in rows])
        p = np.array([[float(r['b%d%s' % (b, c)]) for c in 'xyz'] for r in rows])
        j = np.zeros(len(t))
        j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
        k = 1
        while k < len(t):
            if sw[k - 1] and not sw[k]:
                end = k
                while end < len(t) and not sw[end]:
                    end += 1
                if end < len(t) and end - k <= 2 and t[k] >= a - 0.1 and not so[end]:
                    hops.append({'leg': leg, 'after': float(t[end] - a), 'stood': int(end - k), 'jump': float(j[end:end + 3].max())})
                k = end
            k += 1
    return job, hops


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
    for (variant, name, scene, speed, a), hops in results:
        rows = written.setdefault((name, scene), {})
        rows['%s_%02d' % (scene, len(rows))] = hops
        for key in (scene, 'all'):
            q = total.setdefault(key, [0, 0, 0, 0.0, 0])
            hard = [h for h in hops if h['jump'] > LIMIT]
            q[0] += 1 if hops else 0
            q[1] += len(hops)
            q[2] += len(hard)
            q[3] += sum(h['jump'] - LIMIT for h in hard)
            q[4] += 1
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'hops_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a foot lifts again at once after it landed' % (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s at %3d of %d points, %3d hops, %3d with the shin over 37 mm, %6.1f mm beyond it' % (key, q[0], q[4], q[1], q[2], q[3]))
