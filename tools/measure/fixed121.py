# fixed121.py base variant [set...]: two builds on speed sets with a fixed heading (default h17 h18 h2): points over the
# limits of feet, shins, hands and pelvis with the hardest jump, mean and upper tenth of the jumps, pelvis low, crossed
# feet, flights over 0.2 s and toes and heels over 37 mm; per variant in total, and by scene where the shins differ
import json, os, sys
M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
names = sys.argv[1:3]
sets = sys.argv[3:] or ['h17', 'h18', 'h2']
LIM = (('feet', 37.0), ('shins', 37.0), ('hands', 15.0), ('pelvis', 10.0))
data = {}
for v in names:
    pts = {}
    toes = 0
    for s in sets:
        d = json.load(open('%s/res/%s_%s/sweep.json' % (W, v, s)))
        pts.update({s + ':' + k: x for k, x in d.items()})
        t = json.load(open('%s/res/%s/t%s.json' % (W, v, s)))
        toes += sum(1 for x in t.values() if x > 37.0)
    data[v] = pts
    line = '  %-6s %4d:' % (v, len(pts))
    for c, lim in LIM:
        line += ' %s %3d (%5.1f)' % (c, sum(1 for x in pts.values() if x['j_' + c] > lim), max(x['j_' + c] for x in pts.values()))
    line += ' | mean, tenth:' + ''.join(' %s %.2f %.2f' % (c, sum(x['m_' + c] for x in pts.values()) / len(pts), sum(x['t_' + c] for x in pts.values()) / len(pts)) for c, _ in LIM)
    line += ' | low %d crossed %d flights %d toes %d' % (sum(1 for x in pts.values() if x['low'] > 0.15), sum(1 for x in pts.values() if x['feetSep'] < 0),
                                                       sum(1 for x in pts.values() if x['flight'] > 0.2), toes)
    print(line)
a, b = (data[v] for v in names)
scenes = sorted({k.split(':')[1].rsplit('_', 1)[0] for k in a})
print('by scene, shins over 37 mm and the hardest shin:')
for sc in scenes:
    ka = [k for k in a if k.split(':')[1].rsplit('_', 1)[0] == sc]
    ca, cb = (sum(1 for k in ka if x[k]['j_shins'] > 37.0) for x in (a, b))
    if ca != cb:
        print('  %-14s %3d (%5.1f) -> %3d (%5.1f) of %d' % (sc, ca, max(a[k]['j_shins'] for k in ka), cb, max(b[k]['j_shins'] for k in ka), len(ka)))
