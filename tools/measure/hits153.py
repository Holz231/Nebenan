# hits153.py hits.csv meet.csv: the tables of round 153 from the output of hit153 (out/res/<variant>/hit153, built with
# build.sh <variant> "" hit153.c). Hits while walking and running, by impulse: how many stay animated, catch
# themselves, fall, the median time until the figure moves at nine tenths of its speed again, and the hardest jump of
# a bone at the hit, at a change of state and later (median and most, mm); the falls of the hits that make it catch
# itself by direction; and the meetings of two figures line by line. See round 153 in docs/Roadmap.md.
import csv, statistics, sys


def med(xs):
    return statistics.median(xs) if xs else float('nan')


hits = list(csv.DictReader(open(sys.argv[1])))
print('%d hits, landed %d' % (len(hits), sum(r['landed'] == '1' for r in hits)))
for speed in sorted({r['speed'] for r in hits}, key=float):
    for bone in ('chest', 'thigh'):
        print('%s m/s, hit on the %s: N s | of | animated | caught | fell | s until at speed | jump at hit | at change | '
              'later (median/most mm)' % (speed, bone))
        for m in sorted({int(r['impulse']) for r in hits}):
            g = [r for r in hits if r['speed'] == speed and r['bone'] == bone and int(r['impulse']) == m]
            again = [float(r['again']) for r in g if float(r['again']) >= 0.0]
            cols = [[float(r[k]) for r in g] for k in ('jumpHit', 'jumpChange', 'jumpLate')]
            print('  %3d | %2d | %2d | %2d | %2d | %4.2f | %s' % (
                m, len(g), sum(r['balanced'] == '0' and r['fell'] == '0' for r in g),
                sum(r['balanced'] == '1' and r['fell'] == '0' for r in g), sum(r['fell'] == '1' for r in g), med(again),
                ' | '.join('%5.1f/%5.1f' % (med(c), max(c)) for c in cols)))
hard = [r for r in hits if r['balanced'] == '1' or r['fell'] == '1']
for speed in sorted({r['speed'] for r in hits}, key=float):
    print('%s m/s, falls of the %d hits that end the gait, by direction (0 from behind, 90 from the right, 180 from '
          'ahead): %s' % (speed, sum(r['speed'] == speed for r in hard), ', '.join('%d: %d of %d' % (
            d, sum(r['fell'] == '1' for r in hard if r['speed'] == speed and int(r['direction']) == d),
            sum(1 for r in hard if r['speed'] == speed and int(r['direction']) == d)) for d in range(0, 360, 45))))
print('meetings of two steered figures: engage radius, first m/s, second m/s, to the side | closest frames m | first '
      'slowest m/s | went past | fell first, second | pushes, hits, let go | hardest jump first, second mm')
for r in csv.DictReader(open(sys.argv[2])):
    print('  %s %s %4s %s | %s | %s | %s | %s %s | %s %s %s | %5s %5s' % (
        r['engage'], r['first'], r['second'], r['side'], r['closestFrame'], r['slowest'], r['through'], r['fellA'], r['fellC'],
        r['pushes'], r['hits'], r['letGo'], r['jumpA'], r['jumpC']))
