# eval167.py old new group: the rule of round 167, docs/Regeln/RULE167.txt, on what legs167.py, gait155.py and
# steer162.py wrote for two variants on the same sets, see check167.sh. Prints each quantity for the old and the new
# state and whether its condition holds: Z are the targets, S the protections and P the prices the rule allows. Crossed
# is the number of runs in which the thighs or the shins of the two legs overlap by more than 60 mm, half of what a shin
# is thick.
import json, os, sys
from where129 import W

PAIRS = ('thigh-thigh', 'shin-shin', 'thigh-shin', 'foot-shin', 'foot-foot', 'foot-thigh')


def load(v, group):
    d = os.path.join(W, 'res', v)
    return [json.load(open(os.path.join(d, '%s.%s.json' % (n, group)))) for n in ('legs167', 'gait155', 'steer162')]


def numbers(v, group):
    legs, gait, steer = load(v, group)
    n = {}
    groups = {'steady walk': lambda r: r['scene'] == 'steady' and r['gait'] == 'WALK',
              'steady run': lambda r: r['scene'] == 'steady' and r['gait'] == 'RUN',
              'starts': lambda r: r['scene'] == 'start', 'stops': lambda r: r['scene'] == 'stop',
              'turns walk': lambda r: r['scene'].startswith('turn') and r['gait'] == 'WALK',
              'turns run': lambda r: r['scene'].startswith('turn') and r['gait'] == 'RUN',
              'shuttle': lambda r: r['scene'] == 'shuttle'}
    for g, f in groups.items():
        q = [r for r in legs if f(r)]
        updates = sum(r['updates'] for r in q)
        n[g + ' deep'] = max(r[p] for r in q for p in PAIRS)
        n[g + ' legs'] = max(r[p] for r in q for p in ('thigh-thigh', 'shin-shin'))
        n[g + ' crossed'] = sum(max(r['thigh-thigh'], r['shin-shin']) > 60.0 for r in q)
        n[g + ' share'] = 100.0 * sum(max(r[p + ' over20'] for p in PAIRS) for r in q) / updates
    turns = [x for x in gait if x['gait'] == 'WALK' and x['scene'].startswith('turn')]
    n['shin sum'] = sum(max(x['shin'] - 37.0, 0.0) for x in gait)
    n['foot count'] = sum(x['foot'] > 37.0 for x in gait)
    n['foot max'] = max(x['foot'] for x in gait)
    n['pelvis gait'] = max(x['pelvis'] for x in gait)
    n['under sum'] = sum(max(x['under'] - 30.0, 0.0) for x in turns)
    faced = [x['faced'] for x in turns if x['faced'] is not None]
    n['faced mean'] = sum(faced) / len(faced)
    n['faced max'] = max(faced)
    walk = [x for x in steer if x['gait'] == 'WALK']
    run = [x for x in steer if x['gait'] == 'RUN']
    n['pelvis walk'] = max(x['pelvis'] for x in walk)
    n['pelvis run'] = max(x['pelvis'] for x in run)
    n['shinOver walk'] = sum(x['shinOver'] for x in walk)
    n['shinOver run'] = sum(x['shinOver'] for x in run)
    n['under walk'] = sum(x['under'] > 30.0 for x in walk)
    n['along walk'] = sum(x['along'] for x in walk) / len(walk)
    n['along run'] = sum(x['along'] for x in run) / len(run)
    return n


# name, quantity, unit, and the most the new state may have for what the old one has
RULE = (
    ('Z1', 'steady run share', '%', lambda o: 0.25 * o),
    ('Z2a', 'steady run deep', 'mm', lambda o: o - 20.0),
    ('Z2b', 'starts deep', 'mm', lambda o: o - 20.0),
    ('Z2c', 'stops deep', 'mm', lambda o: o - 20.0),
    ('Z3a', 'turns walk crossed', '', lambda o: max(0.2 * o, 1.0)),
    ('Z3b', 'turns run crossed', '', lambda o: max(0.2 * o, 1.0)),
    ('Z3c', 'shuttle crossed', '', lambda o: max(0.2 * o, 1.0)),
    ('Z4a', 'turns walk share', '%', lambda o: 0.75 * o),
    ('Z4b', 'turns run share', '%', lambda o: 0.75 * o),
    ('Z4c', 'shuttle share', '%', lambda o: 0.75 * o),
    ('S1', 'steady walk deep', 'mm', lambda o: o + 2.0),
    ('S2', 'shin sum', 'mm', lambda o: o + 10.0),
    ('S3', 'under sum', 'mm', lambda o: o + 10.0),
    ('S4', 'pelvis walk', 'mm', lambda o: o + 2.0),
    ('S5', 'pelvis run', 'mm', lambda o: o + 2.0),
    ('S6', 'shinOver walk', '', lambda o: o + 2.0),
    ('S7', 'under walk', '', lambda o: o + 2.0),
    ('P1', 'foot count', '', lambda o: 1.4 * o + 2.0),
    ('P2', 'foot max', 'mm', lambda o: o + 6.0),
    ('P3', 'pelvis gait', 'mm', lambda o: o + 4.0),
    ('P4', 'shinOver run', '', lambda o: 1.15 * o + 2.0),
    ('P5', 'faced mean', 's', lambda o: o + 0.15),
    ('P6', 'faced max', 's', lambda o: o + 0.45),
)
# the least the new state may have
LEAST = (('S8', 'along walk', '', lambda o: o - 0.03), ('S9', 'along run', '', lambda o: o - 0.03))

if __name__ == '__main__':
    old, new = numbers(sys.argv[1], sys.argv[3]), numbers(sys.argv[2], sys.argv[3])
    missed = []
    for name, key, unit, most in RULE:
        ok = new[key] <= most(old[key])
        missed += [] if ok else [name]
        print('%-4s %-18s old %8.2f new %8.2f at most %8.2f %-2s %s' % (name, key, old[key], new[key], most(old[key]), unit,
                                                                     'held' if ok else 'MISSED'))
    for name, key, unit, least in LEAST:
        ok = new[key] >= least(old[key])
        missed += [] if ok else [name]
        print('%-4s %-18s old %8.2f new %8.2f at least %7.2f %-2s %s' % (name, key, old[key], new[key], least(old[key]), unit,
                                                                     'held' if ok else 'MISSED'))
    for key in ('turns walk legs', 'turns run legs', 'shuttle legs'):
        print('     %-18s old %8.2f new %8.2f mm, the deepest of thighs and shins, no condition' % (key, old[key], new[key]))
    print('all held' if not missed else 'missed: ' + ' '.join(missed))
