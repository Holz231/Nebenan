# chain171.py group state [state...]: the median and range over the nudges of each state, measured with check170.sh, for
# the quantities the rounds from 155 on were about, side by side, to see where a quantity changed along the rounds.
# Written for round 171, which measured every built round from 155 to 167 this way, see docs/Roadmap.md.
import statistics, sys
from compare170 import nudged, more
from eval167 import numbers
states = sys.argv[2:]; g = sys.argv[1]
keys = ('turns walk crossed', 'turns run crossed', 'shuttle crossed', 'pelvis walk', 'pelvis run', 'under walk',
        'under sum', 'foot max', 'faced mean', 'faced 180 walk', 'faced 180 run', 'along walk', 'along run', 'twist walk',
        'letgo pelvis run', 'shinOver run', 'foot extra', 'pelvis extra', 'fallen')
data = {}
for s in states:
    runs = [dict(numbers(v, g), **more(v, g)) for v in nudged(s, g)]
    data[s] = runs
print('%-18s ' % 'median (range)' + ' '.join('%17s' % s for s in states))
for k in keys:
    cells = []
    for s in states:
        x = [r[k] for r in data[s]]
        form = '%7.2f (%6.1f-%-6.1f)' if max(x) < 1000 else '%7.0f (%5.0f-%-5.0f)'
        cells.append(form % (statistics.median(x), min(x), max(x)))
    print('%-18s ' % k + ' '.join('%17s' % c for c in cells))
