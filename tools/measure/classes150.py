# classes150.py variant [variant...]: what is worth a round, counted before one is proposed or chosen. Over the eight
# changes at a run with a fixed heading on the sets of SETS: the points of where129.py at which the hardest shin jumps
# over 37 mm, by what its leg does there (land, lift, swing, stand), with how many they are and what they sum to beyond
# 37 mm, the largest class first, and the same for every scene. The two starts from standing among the eight are a
# class of their own, "start": all 24 points of a start are equal, one event counted 24 times. Reads
# out/res/<variant>/where_<scene>_<set>.json, so first: python3 -I where129.py <variant> run <sets>. SETS defaults to
# the twelve sets h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29, SCENES names other scenes than the eight.
import collections, json, os, sys

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
SETS = os.environ.get('SETS', 'h1 h14 h15 h6 h10 h12 h5 h13 h9 h11 h2 h29').split()
EIGHT = 'startL30 startB30 switchLR30 fwdToL30 LToBack30 backToFwd30 stopL30 stopB30'
SCENES = os.environ.get('SCENES', EIGHT).split()

if __name__ == '__main__':
    for variant in sys.argv[1:]:
        count, excess = collections.Counter(), collections.Counter()
        scenes = collections.defaultdict(collections.Counter)
        points = 0
        for scene in SCENES:
            for s in SETS:
                path = os.path.join(W, 'res', variant, 'where_%s_%s.json' % (scene, s))
                if not os.path.exists(path):
                    raise SystemExit('missing %s: python3 -I where129.py %s %s %s' % (path, variant, scene, s))
                for e in json.load(open(path)).values():
                    points += 1
                    if e['jump'] > 37.0:
                        kind = 'start' if scene.startswith('start') else e['does'].split()[0]
                        count[kind] += 1
                        excess[kind] += e['jump'] - 37.0
                        scenes[scene][kind] += e['jump'] - 37.0
        print('%s: %d points, the hardest shin over 37 mm at %d, %.0f mm beyond it' % (
            variant, points, sum(count.values()), sum(excess.values())))
        for kind, mm in excess.most_common():
            print('  %-6s %4d points %5.0f mm' % (kind, count[kind], mm))
        for scene in SCENES:
            if scenes[scene]:
                print('  %-12s %s' % (scene, ', '.join('%s %.0f' % (k, mm) for k, mm in scenes[scene].most_common())))
