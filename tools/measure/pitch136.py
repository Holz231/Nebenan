# pitch136.py variant scene|run set [set...]: how hard a shin jumps in a swing during which the gait falls from running
# toward walking, in the changes of the gait with a fixed heading of where129.py (one scene, or "run" for the eight
# changes at a run), at 24 points for every set. Counted is every swing that lifts from the change on and during which
# the run share of the gait (gaitRun of lab101) comes more than FALL below the share it had as the foot lifted: walking,
# a swing turns its foot over the toes or the heel early in its swing by as much as the gait walks, see
# NB_GAIT_SWING_PITCH, and with the gait falling toward walking that grew during the swing. For every such swing the
# hardest jump of the shin of its leg from the update after it lifts to the update after it lands (second difference of
# its origin, mm; the bodies follow the pose of the update before), and how far its foot turned at most in one update
# toward its heel and toward its toes during the swing (rad). Prints for every scene and in all how many such swings
# there are, how many of them with the shin over 37 mm, what those sum to beyond 37 mm and the hardest. Writes
# out/res/<variant>/pitch_<scene>_<set>.json. Needs a lab101 that writes gaitRun. See round 136 in docs/Roadmap.md.
import csv, json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w

W = w.W
LIMIT = 37.0
FALL = 0.1


def one(job):
    variant, name, scene, speed, a = job
    path = os.path.join(W, 'tmp', 'pitch136_%s_%s_%s_%.4f.csv' % (variant, name, scene, a))
    script = w.SCENES[scene][0].format(v=speed, a='%.4f' % a)
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (a + 3.0)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    t = np.array([float(r['t']) for r in rows])
    run = np.array([float(r['gaitRun']) for r in rows])
    swings = []
    for leg, b in w.SHIN.items():
        sw = np.array([float(r['swing' + leg]) > 0.5 for r in rows])
        roll = np.array([float(r['roll' + leg]) for r in rows])
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
                    fall = float(run[k] - run[k:end].min())
                    turn = np.diff(roll[k:end + 1])
                    swings.append({'leg': leg, 'after': float(t[k] - a), 'lifted': float(run[k]), 'fall': fall,
                                   'jump': float(j[k + 1:end + 2].max()), 'counted': bool(fall > FALL),
                                   'heelward': float(max(0.0, -turn.min())), 'toeward': float(max(0.0, turn.max()))})
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
            q = total.setdefault(key, [0, 0, 0.0, 0.0])
            some = [h for h in swings if h['counted']]
            hard = [h for h in some if h['jump'] > LIMIT]
            q[0] += len(some)
            q[1] += len(hard)
            q[2] += sum(h['jump'] - LIMIT for h in hard)
            q[3] = max([q[3]] + [h['jump'] for h in some])
    for (name, scene), rows in written.items():
        json.dump(rows, open(os.path.join(W, 'res', variant, 'pitch_%s_%s.json' % (scene, name)), 'w'), indent=1)
    print('%s on %s: a swing during which the gait falls toward walking' % (variant, ' '.join(sets)))
    for key in scenes + ['all']:
        q = total[key]
        print('  %-12s %3d swings, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %5.1f mm' % (key, *q))
