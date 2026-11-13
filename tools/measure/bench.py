# bench.py: the pairs of out/bench written by bench_pairs.sh, TAGS the pairs to read (default r1 to r6): frame times of
# the five town scenes as new over old per pair, the update of the figures benchmark per phase, and whether hashes and
# end counters agree
import os as _os, sys as _sys
_M = _os.path.dirname(_os.path.abspath(__file__))
_W = _os.environ.get('NB_MEASURE_OUT', _os.path.join(_M, 'out'))
_sys.path.insert(0, _M)
import re, statistics, glob, os
TAGS = os.environ.get('TAGS', 'r1 r2 r3 r4 r5 r6').split()
B = _W + '/bench/'
def towns(path):
    t = open(path).read()
    return [float(x) for x in re.findall(r'\n  frame\s+avg\s+([\d.]+) ms', t)]
def ends(path):
    t = open(path).read()
    keep = re.findall(r'figures hash 0x[0-9a-f]+|\d+ grenades, .*|chunks\s+\d+ bodies\s+\d+ overloaded\s+\d+|at the end .*|\d+ figures: .*', t)
    return keep
def phases(path):
    t = open(path).read()
    out = {}
    blocks = re.split(r'\n  (?=\d+ (?:figures?|fighters|ragdolls) )', t)
    for b in blocks[1:]:
        name = b.split('\n')[0][:30]
        for m in re.finditer(r'\n    (stand|flinch|knockdown|get up|cover breaks|follow|hits|apart)\s+(\d+) \|\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+ \|\s+([\d.]+)', b):
            out[(name, m.group(1))] = float(m.group(3))
    return out
names = ['16 houses x1', '16 houses x2', '64 houses x4', 'town with figures', 'streamed city']
ratios = [[] for _ in names]
for r, tag in enumerate(TAGS, 1):
    a = towns(B + 'old_%s.txt' % tag); b = towns(B + 'new_%s.txt' % tag)
    assert len(a) == 5 and len(b) == 5, (len(a), len(b))
    for i in range(5):
        ratios[i].append(b[i] / a[i])
    print('pair %d' % r, ' '.join('%.3f' % (b[i] / a[i]) for i in range(5)), '(new first)' if r % 2 == 0 else '(old first)')
for i, n in enumerate(names):
    v = ratios[i]
    print('%-18s min %.2f max %.2f mean %.3f median %.3f' % (n, min(v), max(v), statistics.mean(v), statistics.median(v)))
allr = [x for v in ratios for x in v]
print('all: %.2f to %.2f' % (min(allr), max(allr)))
# second measurement of each pair slower?
second_slower = 0; total = 0
for r, tag in enumerate(TAGS, 1):
    a = towns(B + 'old_%s.txt' % tag); b = towns(B + 'new_%s.txt' % tag)
    for i in range(5):
        first, second = (a[i], b[i]) if r % 2 == 1 else (b[i], a[i])
        second_slower += second > first; total += 1
print('second of the pair slower in %d of %d' % (second_slower, total))
# end states
e = set()
for r, tag in enumerate(TAGS, 1):
    for v in ('old', 'new'):
        e.add((v, tuple(ends(B + '%s_%s.txt' % (v, tag)))))
by = {}
for v, x in e:
    by.setdefault(v, set()).add(x)
print('distinct end states: old %d, new %d, same between them: %s' % (len(by['old']), len(by['new']), by['old'] == by['new']))
if by['old'] != by['new']:
    a = list(by['old'])[0]; b = list(by['new'])[0]
    for x, y in zip(a, b):
        if x != y: print('  old:', x, '\n  new:', y)
fe = set()
for r, tag in enumerate(TAGS, 1):
    for v in ('old', 'new'):
        fe.add((v, tuple(ends(B + 'fig%s_%s.txt' % (v, tag)))))
fby = {}
for v, x in fe:
    fby.setdefault(v, set()).add(x)
print('figures benchmark distinct end states: old %d, new %d, same: %s' % (len(fby['old']), len(fby['new']), fby['old'] == fby['new']))
# figures benchmark update per phase
ph = {}
for r, tag in enumerate(TAGS, 1):
    a = phases(B + 'figold_%s.txt' % tag); b = phases(B + 'fignew_%s.txt' % tag)
    for k in a:
        if a[k] > 0:
            ph.setdefault(k, []).append(b[k] / a[k])
meds = []
for k, v in ph.items():
    meds.append(statistics.median(v))
allp = [x for v in ph.values() for x in v]
print('figures update: median over all phases and pairs %.3f, per phase medians %.2f to %.2f' % (statistics.median(allp), min(meds), max(meds)))
