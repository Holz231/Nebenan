# falls126.py old new [tag...]: the falls of two builds on their balance tables (table.sh, default the four tables of
# check126.sh: up down slow free), for each table and in sum: all falls, the falls after a push backward (directions 9
# to 23 of 32, direction 16 pushes straight back) and after a push forward, the falls after light hits up to 60 N s and
# the hits that end differently. Round 126 found with it that shoulder joints further back make the figure fall more
# often when it is pushed backward: falls126.py r124 n126, and falls126.py r124 <variant> same for a prototype.
import csv, os, sys

here = os.path.dirname(os.path.abspath(__file__))
tab = os.path.join(os.environ.get('NB_MEASURE_OUT', os.path.join(here, 'out')), 'res', 'tab')


def load(variant, tag):
    with open(os.path.join(tab, '%s_%s.csv' % (variant, tag)), newline='') as f:
        return list(csv.DictReader(f))


def count(rows):
    fell = [r for r in rows if r['outcome'] == 'fell']
    back = sum(1 for r in fell if 9 <= int(r['dir']) <= 23)
    light = sum(1 for r in fell if float(r['impulse']) <= 60.0)
    return [len(fell), back, len(fell) - back, light]


old, new = sys.argv[1], sys.argv[2]
tags = sys.argv[3:] or ['up', 'down', 'slow', 'free']
print('%-6s %6s  %-14s %-14s %-14s %-9s %s' % ('table', 'hits', 'falls', 'pushed back', 'pushed forward', 'light', 'end differently'))
total = [[0] * 4, [0] * 4, 0, 0]
for tag in tags + ['sum']:
    if tag == 'sum':
        if len(tags) == 1:
            break
        a, b, differ, hits = total
    else:
        ra, rb = load(old, tag), load(new, tag)
        assert len(ra) == len(rb) and all((x['bone'], x['dir'], x['impulse']) == (y['bone'], y['dir'], y['impulse']) for x, y in zip(ra, rb))
        a, b = count(ra), count(rb)
        differ = sum(1 for x, y in zip(ra, rb) if x['outcome'] != y['outcome'])
        hits = len(ra)
        total = [[p + q for p, q in zip(total[0], a)], [p + q for p, q in zip(total[1], b)], total[2] + differ, total[3] + hits]
    cells = ['%d -> %d' % (p, q) for p, q in zip(a, b)]
    print('%-6s %6d  %-14s %-14s %-14s %-9s %d' % (tag, hits, cells[0], cells[1], cells[2], cells[3], differ))
