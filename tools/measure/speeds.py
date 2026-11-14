# speeds.py [v ...]: every speed that has run before, as far as the speed sets of common.sh and docs/Roadmap.md name
# them, and for each speed given in m/s whether it is among them, the time of one cycle of the gait at it from the
# cadence formula of the library (nbCadenceModel in src/figure.c, read from the source, with the height of the stance
# fitted to the sets of common.sh), and the speed per axis going diagonally. Nothing is run: a fresh speed set for an
# independent check is defined with this before any build has run at it.
import math, os, re, sys
M = os.path.dirname(os.path.abspath(__file__))
T = os.path.join(M, '..', '..')
src = open(os.path.join(T, 'src', 'figure.c')).read()
C = {k: float(v) for k, v in re.findall(r'#define (NB_GAIT_(?:RUN_FROM|RUN_TO|CADENCE_MAX|CADENCE_SPAN|CADENCE_SPEED|CADENCE_WALK|CADENCE_MIN)) ([0-9.]+)f', src)}
sets = [dict(p.split('=') for p in line.split('"')[1].split()) for line in open(os.path.join(M, 'common.sh')) if line.startswith('SET_')]
used = {float(s[k]) for s in sets for k in ('WALK', 'RUN', 'FAST', 'DIAG') if k in s}
known = {float(s[a]): float(s[b]) for s in sets for a, b in (('WALK', 'CW'), ('RUN', 'CR'), ('FAST', 'CF')) if a in s}
road = open(os.path.join(T, 'docs', 'Roadmap.md'), encoding='utf-8').read()
for group in re.findall(r'((?:\d,\d+(?:, | und ))*\d,\d+) m/s', road):
    used.update(float(x.replace(',', '.')) for x in re.findall(r'\d,\d+', group))


def cycle(v, height, g=9.81):
    omega = math.sqrt(g / height)
    u = v / (omega * height)
    model = max(C['NB_GAIT_CADENCE_MAX'] - C['NB_GAIT_CADENCE_SPAN'] * math.exp(-u / C['NB_GAIT_CADENCE_SPEED']), C['NB_GAIT_CADENCE_MIN'])
    x = min(max((u - C['NB_GAIT_RUN_FROM']) / (C['NB_GAIT_RUN_TO'] - C['NB_GAIT_RUN_FROM']), 0.0), 1.0)
    walk = max(C['NB_GAIT_CADENCE_WALK'] * math.sqrt(u), C['NB_GAIT_CADENCE_MIN'])
    return 2.0 / (omega * (walk + (model - walk) * x * x * (3.0 - 2.0 * x)))


error, height = min((max(abs(cycle(v, h / 1000.0) - c) for v, c in known.items()), h / 1000.0) for h in range(850, 1100))
print('used before, m/s: ' + ' '.join('%g' % v for v in sorted(used)))
print('height of the stance fitted to %d speeds of common.sh: %.3f m, worst error of a cycle %.4f s' % (len(known), height, error))
for a in sys.argv[1:]:
    v = float(a)
    print('%g m/s: %s, cycle %.3f s, diagonal %.2f m/s per axis' % (v, 'USED BEFORE' if any(abs(v - w) < 0.0005 for w in used) else 'fresh', cycle(v, height), v / math.sqrt(2.0)))
