# hops140.py variant set [set...]: the feet that lift again at once after they landed, as hops131.py finds them, from
# running sideways to running backward with a fixed heading (LToBack30 of where129.py) at POINTS points spread over a
# cycle of the run for every set (default 96, four times as many as where129.py): such a hop comes in a narrow window
# of the cycle, see round 140 in docs/Roadmap.md. Prints for every set and in all at how many points a foot hops, how
# many hops there are, how many of them with the shin over 37 mm, the sum beyond 37 mm and the hardest. Writes
# out/res/<variant>/hops140_<set>.json.
import json, os, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import where129 as w
import hops131 as h

if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    points = int(os.environ.get('POINTS', '96'))
    scene = 'LToBack30'
    script, speed, spread, first = w.SCENES[scene]
    os.makedirs(os.path.join(w.W, 'tmp'), exist_ok=True)
    jobs = []
    for name in sets:
        s = w.speeds(name)
        jobs += [(variant, name, scene, s[speed], first + i * float(s[spread]) / points) for i in range(points)]
    with ThreadPoolExecutor(4) as pool:
        results = list(pool.map(h.one, jobs))
    per, total = {}, [0, 0, 0, 0.0, 0.0]
    for (variant, name, scene, speed, a), hops in results:
        rows = per.setdefault(name, {})
        rows['%s_%02d' % (scene, len(rows))] = hops
    print('%s, %s at %d points per set: a foot lifts again at once after it landed' % (variant, scene, points))
    for name in sets:
        rows = per[name]
        json.dump(rows, open(os.path.join(w.W, 'res', variant, 'hops140_%s.json' % name), 'w'), indent=1)
        hops = [x for r in rows.values() for x in r]
        hard = [x for x in hops if x['jump'] > h.LIMIT]
        q = (sum(1 for r in rows.values() if r), len(hops), len(hard), sum(x['jump'] - h.LIMIT for x in hard),
             max([x['jump'] for x in hops] or [0.0]))
        total = [total[0] + q[0], total[1] + q[1], total[2] + q[2], total[3] + q[3], max(total[4], q[4])]
        print('  %-5s at %3d points, %3d hops, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %.1f mm' % (name, *q))
    print('  all   at %3d points, %3d hops, %3d with the shin over 37 mm, %6.1f mm beyond it, the hardest %.1f mm' % tuple(total))
