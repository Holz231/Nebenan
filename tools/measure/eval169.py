# eval169.py plain gentle group: the rule of round 169, docs/Regeln/RULE169.txt, on what legs167.py, gait155.py and
# steer162.py wrote for one state of the library run as it is and run with nbFigureSteering::startAcceleration at 4, see
# check169.sh. Z are the targets, S the protections, P the price the rule allows. The sums over the turns are printed
# without a condition: they move with the phase of the steps at the turn, which a gentler start shifts, see the rule.
import sys
from eval167 import numbers, load

if __name__ == '__main__':
    plain, gentle, group = sys.argv[1], sys.argv[2], sys.argv[3]
    old, new = numbers(plain, group), numbers(gentle, group)
    for v, n in ((plain, old), (gentle, new)):
        legs, gait, steer = load(v, group)
        for g in ('WALK', 'RUN'):
            q = [r for r in legs if r['scene'] == 'start' and r['gait'] == g]
            n['full least ' + g], n['full most ' + g] = min(r['full'] for r in q), max(r['full'] for r in q)
            n['landed least ' + g] = min(r['landed'] for r in q)
        starts = [x for x in gait if x['scene'].startswith('start')]
        for k in ('shin', 'foot', 'pelvis'):
            n['start ' + k] = max(x[k] for x in starts)
    checks = (
        ('Z1', 'walking, 95 %% of the speed after %.2f to %.2f s, 0.2 to 0.5 allowed' % (
            new['full least WALK'], new['full most WALK']), new['full least WALK'] >= 0.2 and new['full most WALK'] <= 0.5),
        ('Z2', 'running, %d feet landed by then at least, 2 asked, after %.2f s at the latest, 1.2 allowed' % (
            new['landed least RUN'], new['full most RUN']), new['landed least RUN'] >= 2 and new['full most RUN'] <= 1.2),
        ('S1a', 'starts, hardest shin jump %.1f, plain %.1f mm, 3 more allowed' % (new['start shin'], old['start shin']),
         new['start shin'] <= old['start shin'] + 3.0),
        ('S1b', 'starts, hardest foot jump %.1f, plain %.1f mm, 3 more allowed' % (new['start foot'], old['start foot']),
         new['start foot'] <= old['start foot'] + 3.0),
        ('S1c', 'starts, hardest pelvis jump %.1f, plain %.1f mm, 3 more allowed' % (new['start pelvis'], old['start pelvis']),
         new['start pelvis'] <= old['start pelvis'] + 3.0),
        ('P1', 'starts, legs overlap %.1f, plain %.1f mm, 10 more and 30 allowed' % (new['starts deep'], old['starts deep']),
         new['starts deep'] <= old['starts deep'] + 10.0 and new['starts deep'] <= 30.0),
        ('S4a', 'follows the stick walking %.2f, plain %.2f, 0.01 less allowed' % (new['along walk'], old['along walk']),
         new['along walk'] >= old['along walk'] - 0.01),
        ('S4b', 'follows the stick running %.2f, plain %.2f, 0.01 less allowed' % (new['along run'], old['along run']),
         new['along run'] >= old['along run'] - 0.01),
    )
    missed = [name for name, text, ok in checks if not ok]
    for name, text, ok in checks:
        print('%-4s %s: %s' % (name, text, 'held' if ok else 'MISSED'))
    print('plain: 95 %% of the speed walking after %.2f to %.2f s with %d feet landed at least, '
          'running %.2f to %.2f s with %d' % (old['full least WALK'], old['full most WALK'], old['landed least WALK'],
                                              old['full least RUN'], old['full most RUN'], old['landed least RUN']))
    for key in ('turns walk crossed', 'turns run crossed', 'shuttle crossed', 'shin sum', 'foot count', 'pelvis gait',
                'pelvis walk', 'pelvis run', 'shinOver walk', 'shinOver run', 'under walk'):
        print('     %-18s plain %8.2f gentle %8.2f, no condition' % (key, old[key], new[key]))
    print('all held' if not missed else 'missed: ' + ' '.join(missed))
