# gait155.py variant set [set...]: the gait of a steered humanoid that faces where it moves, as a player walks a figure
# around, measured for round 155, see docs/Roadmap.md: starting from standing, the steady gait, stopping, turning by 45,
# 90, 135 and 180 degrees to its left and by 90 to its right, and changing between walking and running, each walking and
# running at the speeds of the sets (common.sh). A change comes at 12 moments spread over a cycle of the gait, starting
# and the steady gait once. From 0.1 s before the change for 3 s, per point: the hardest jump of a shin, a foot, the
# pelvis, a hand and the head in an update (second difference of the origin, mm) and what the leg of that shin does
# there (see where129.py); how far a standing foot slides over the ground in one stance (the point between heel and
# toes while both are within 5 mm of the same height, mm) and how far it turns on the ground (degrees), both from the
# third update after it landed to the last before it lifts, and how far a foot points away from the heading of the
# figure as it lands (degrees); how far the pelvis gets below and above its height before the change (mm), and, since
# round 159, how far it gets below and above the heights it has in the steady gait at the new speed in the first 1.5 s
# (under and beyond, mm; -1 where the figure stops): walking and running have their own heights, the pelvis is 7 cm
# lower running at 4.3 m/s than walking at 1.05 m/s, and against the height before a change of the gait that
# difference looked like a pelvis that sinks; when the
# figure moves at nine tenths of the new speed along the new direction (stopping: at a tenth of the old speed) and when
# it faces within 10 degrees of it; the shortest swing of a foot (s) and how many feet land in the first 1.5 s against
# the steady gait of the new speed. Since round 161 also starting from standing to where the figure does not face, 90
# and 180 degrees to its left (startTurn90, startTurn180), and the way of the frame: how far it goes on the way it
# went before the change (onward, m) and how far to the side of it (sideways, m; turning about that is how wide its
# bend is). Writes out/res/<variant>/gait155.json and prints one line per scene. Needs lab101 of the variant:
# ./build.sh <variant> "" lab101.c
import csv, json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, what, W

LIMIT = 37.0
BANDS = {}
GROUPS = {'shin': (14, 17), 'foot': (15, 18), 'pelvis': (0,), 'hand': (9, 12), 'head': (6,)}
# scene: the gait before, the gait after (None: standing), the turn to the left in degrees, moments
SCENES = {'start': (None, 'same', 0, 1), 'steady': ('same', 'same', 0, 1), 'stop': ('same', None, 0, 12),
          'turn45': ('same', 'same', 45, 12), 'turn90': ('same', 'same', 90, 12), 'turn135': ('same', 'same', 135, 12),
          'turn180': ('same', 'same', 180, 12), 'turn90R': ('same', 'same', -90, 12), 'change': ('same', 'other', 0, 12),
          'startTurn90': (None, 'same', 90, 1), 'startTurn180': (None, 'same', 180, 1)}


def band(variant, name, v):
    """the lowest and highest pelvis of the steady gait at a speed, from 4 s on"""
    path = os.path.join(W, 'tmp', 'gait155_band_%s_%s_%.4f.csv' % (variant, name, v))
    lab = os.path.join(W, 'res', variant, 'lab101')
    subprocess.run([lab, '0:0:0,1:0:%.4f' % v, path, '8.0'], check=True, stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    y = np.array([float(r['b0y']) for r in rows if float(r['t']) > 4.0])
    return float(y.min()), float(y.max())


def one(job):
    variant, name, scene, gait, i = job
    s = speeds(name)
    before, after, turn, moments = SCENES[scene]
    v = float(s[gait])
    other = float(s['RUN' if gait == 'WALK' else 'WALK'])
    v0 = 0.0 if before is None else v
    v1 = 0.0 if after is None else (v if after == 'same' else other)
    cycle = float(s['CW' if gait == 'WALK' else 'CR'])
    a = 1.0 if before is None else 4.0 + i * cycle / moments
    rad = math.radians(turn)
    script = '0:0:0' + (',1:0:%.4f' % v0 if before is not None else '')
    script += ',%.4f:%.5f:%.5f' % (a, v1 * math.sin(rad), v1 * math.cos(rad))
    path = os.path.join(W, 'tmp', 'gait155_%s_%s_%s_%s_%d.csv' % (variant, name, scene, gait, i))
    lab = os.path.join(W, 'res', variant, 'lab101')
    subprocess.run([lab, script, path, '%.4f' % (a + 3.0)], check=True, stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    dt = float(t[1] - t[0])
    win = t >= a - 0.1
    out = {'set': name, 'scene': scene, 'gait': gait, 'moment': i, 'from': v0, 'to': v1}
    for group, bones in GROUPS.items():
        best = (0.0, 0, 0)
        for b in bones:
            p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
            j = np.zeros(len(t))
            j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
            j[~win] = 0.0
            k = int(np.argmax(j))
            best = max(best, (float(j[k]), b, k))
        out[group] = best[0]
        if group == 'shin':
            does = what(d, 'L' if best[1] == 14 else 'R', best[2])
            out['does'] = does[0] + (' %d' % does[1] if does[0] in ('lift', 'land') else '')
            out['after'] = float(t[best[2]] - a)
    slide, twist, aside, short, lands = 0.0, 0.0, 0.0, 9.0, 0
    for leg in 'LR':
        sw = d['swing' + leg] > 0.5
        toe = np.stack([d['toe%sx' % leg], d['toe%sz' % leg]], 1)
        heel = np.stack([d['heel%sx' % leg], d['heel%sz' % leg]], 1)
        mid = 0.5 * (toe + heel)
        flat = np.abs(d['toe%sy' % leg] - d['heel%sy' % leg]) < 0.005
        yaw = np.unwrap(np.arctan2(toe[:, 0] - heel[:, 0], toe[:, 1] - heel[:, 1]))
        k = int(np.argmax(win))
        while k < len(t):
            e = k
            while e + 1 < len(t) and sw[e + 1] == sw[k]:
                e += 1
            whole = k > int(np.argmax(win)) and e + 1 < len(t)
            if sw[k] and whole:
                short = min(short, (e - k + 1) * dt)
            if not sw[k] and e - k >= 4:
                step = np.linalg.norm(mid[k + 3:e] - mid[k + 2:e - 1], axis=1) * (flat[k + 3:e] & flat[k + 2:e - 1])
                slide = max(slide, 1000.0 * float(np.sum(step)))
                twist = max(twist, math.degrees(float(np.max(yaw[k + 2:e]) - np.min(yaw[k + 2:e]))))
            if not sw[k] and k > 0 and sw[k - 1] and t[k] >= a:
                away = (yaw[k] - d['heading'][k] + math.pi) % (2.0 * math.pi) - math.pi
                aside = max(aside, abs(math.degrees(away)))
            if not sw[k] and k > 0 and sw[k - 1] and a <= t[k] < a + 1.5:
                lands += 1
            k = e + 1
    out['slide'], out['twist'], out['aside'], out['short'], out['lands'] = slide, twist, aside, short, lands
    ref = float(np.mean(d['b0y'][(t >= a - 1.0) & (t < a - 0.1)]))
    out['low'] = 1000.0 * (ref - float(np.min(d['b0y'][win])))
    out['high'] = 1000.0 * (float(np.max(d['b0y'][win])) - ref)
    out['under'], out['beyond'] = -1.0, -1.0
    if after is not None:
        lowest, highest = BANDS[(name, v1)]
        first = (t >= a) & (t < a + 1.5)
        out['under'] = max(1000.0 * (lowest - float(np.min(d['b0y'][first]))), 0.0)
        out['beyond'] = max(1000.0 * (float(np.max(d['b0y'][first])) - highest), 0.0)
    at = int(np.argmax(t >= a))
    out['onward'] = float(np.max(d['fz'][at:] - d['fz'][at]))
    out['sideways'] = float(np.max(np.abs(d['fx'][at:] - d['fx'][at])))
    vx, vz = np.gradient(d['fx'], t), np.gradient(d['fz'], t)
    if after is None:
        ok = np.hypot(vx, vz) <= 0.1 * v0
    else:
        ok = vx * math.sin(rad) + vz * math.cos(rad) >= 0.9 * v1
    ok &= t >= a
    out['again'] = float(t[int(np.argmax(ok))] - a) if ok.any() else -1.0
    off = np.abs((d['heading'] - rad + math.pi) % (2.0 * math.pi) - math.pi)
    faced = (off <= math.radians(10.0)) & (t >= a)
    out['faced'] = float(t[int(np.argmax(faced))] - a) if faced.any() else -1.0
    return out


if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = [(variant, name, scene, gait, i) for name in sets for scene in SCENES for gait in ('WALK', 'RUN')
            for i in range(SCENES[scene][3])]
    for name in sets:
        for key in ('WALK', 'RUN'):
            BANDS[(name, float(speeds(name)[key]))] = band(variant, name, float(speeds(name)[key]))
    with ThreadPoolExecutor(4) as pool:
        rows = list(pool.map(one, jobs))
    json.dump(rows, open(os.path.join(W, 'res', variant, 'gait155.json'), 'w'), indent=1)
    steady = {(r['set'], r['gait']): r for r in rows if r['scene'] == 'steady'}
    print('%s on %s: %d runs. Jumps: median / hardest in mm; over: points with a shin over %.0f mm and their sum '
          'beyond it' % (variant, ' '.join(sets), len(rows), LIMIT))
    print('scene    gait   n | shin        over      | foot        | pelvis      | hand        | head        | '
          'slide mm twist aside deg | low  high mm | under mm | again s     | faced s     | short s | lands | onward m')
    for scene in SCENES:
        for gait in ('WALK', 'RUN'):
            q = [r for r in rows if r['scene'] == scene and r['gait'] == gait]
            def mm(key): return '%5.1f %5.1f' % (float(np.median([r[key] for r in q])), max(r[key] for r in q))
            def tt(key):
                x = [r[key] for r in q]
                return ' never     ' if min(x) < 0 else '%5.2f %5.2f' % (float(np.mean(x)), max(x))
            over = [r['shin'] - LIMIT for r in q if r['shin'] > LIMIT]
            target = 'WALK' if (gait == 'WALK') == (scene != 'change') else 'RUN'
            ratio = np.mean([r['lands'] / max(steady[(r['set'], target)]['lands'], 1) for r in q]) if SCENES[scene][1] else 0.0
            print('%-8s %-4s %3d | %s %3d %5.0f | %s | %s | %s | %s | %7.1f  %5.1f %5.1f     | %4.0f %4.0f    | %8.1f | '
                  '%s | %s | %7.3f | %.2f | %.2f' % (
                scene, gait.lower(), len(q), mm('shin'), len(over), sum(over), mm('foot'), mm('pelvis'), mm('hand'), mm('head'),
                max(r['slide'] for r in q), max(r['twist'] for r in q), max(r['aside'] for r in q), max(r['low'] for r in q),
                max(r['high'] for r in q), max(r['under'] for r in q), tt('again'), tt('faced'),
                min(r['short'] for r in q), ratio, max(r['onward'] for r in q)))
