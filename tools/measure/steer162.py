# steer162.py variant set [set...]: the gait of a steered humanoid that faces where it moves while a player steers it
# on and on, measured for round 162, see docs/Roadmap.md. gait155.py measures one change out of the steady gait; here
# the direction keeps changing: the stick goes round at 45, 90, 180 and 360 degrees per second (circle), swings between
# 45 or 90 degrees to the left and to the right every 0.3, 0.6 or 1.0 s (zigzag), turns about every 0.3, 0.6 or 1.0 s
# (shuttle), goes 90 degrees to the left for 0.1 or 0.2 s and back (tap), lets go for 0.2 s every 0.7 s (stopgo), and
# changes between the walking and the running speed of the set every 0.5 s (gears). Each course lasts 4 s out of the
# steady gait, from 4 moments spread over a cycle, walking and running at the speeds of the sets (common.sh), and the
# figure goes on for 1.5 s the way it was steered last. Per run, from 0.1 s before the course to its end: the hardest
# jump of a shin, a foot, the pelvis, a hand and the head in an update (second difference of the origin, mm) and in
# how many updates a shin jumps more than 37 mm; how far the pelvis gets below the lowest it has in the steady gait at
# that speed (under, mm; gears: at either speed) and for how long more than 30 mm (s); the swings of the feet, how many
# are shorter than 0.25 s and the shortest (s); how far a standing foot turns on the ground (degrees) and slides (mm);
# and how the figure follows: the share of the steered speed it has in the mean (kept), the share of it that goes the
# way it is steered (along), how far its heading is off the steered direction in the mean (off, degrees), and whether
# it is still animated at the end. Writes out/res/<variant>/steer162.json and prints one line per course and gait.
# Needs lab101 of the variant: ./build.sh <variant> "-DLAB_BEND" lab101.c
import csv, json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import speeds, W
from gait155 import band, GROUPS, LIMIT

LAST = 4.0
MOMENTS = 4
DT = 1.0 / 60.0
COURSES = ([('circle%d' % r, ('circle', r)) for r in (45, 90, 180, 360)] +
           [('zigzag%d_%.1f' % (g, p), ('zigzag', g, p)) for g in (45, 90) for p in (0.3, 0.6, 1.0)] +
           [('shuttle_%.1f' % p, ('zigzag', 90, p, 'about')) for p in (0.3, 0.6, 1.0)] +
           [('tap_%.1f' % p, ('tap', p)) for p in (0.1, 0.2)] + [('stopgo', ('stopgo',)), ('gears', ('gears',))])
BANDS = {}


def steered(course, v, other, u):
    """the velocity steered at u seconds into the course: x to the left, z ahead"""
    kind = course[0]
    if kind == 'circle':
        rad = math.radians(course[1]) * u
        return v * math.sin(rad), v * math.cos(rad)
    if kind == 'zigzag':
        leg = int(u / course[2] + 1e-6) % 2
        if len(course) > 3:
            return 0.0, -v if leg == 0 else v
        rad = math.radians(course[1]) * (1.0 if leg == 0 else -1.0)
        return v * math.sin(rad), v * math.cos(rad)
    if kind == 'tap':
        return (v, 0.0) if u < course[1] - 1e-6 else (0.0, v)
    if kind == 'stopgo':
        return (0.0, 0.0) if (u % 0.7) < 0.2 - 1e-6 else (0.0, v)
    leg = int(u / 0.5 + 1e-6) % 2
    return 0.0, other if leg == 0 else v


def one(job):
    variant, name, label, course, gait, i = job
    s = speeds(name)
    v = float(s[gait])
    other = float(s['RUN' if gait == 'WALK' else 'WALK'])
    cycle = float(s['CW' if gait == 'WALK' else 'CR'])
    a = 4.0 + i * cycle / MOMENTS
    n = int(round(LAST / DT))
    keys, last = ['0:0:0', '1:0:%.4f' % v], (0.0, v)
    for k in range(n):
        now = steered(course, v, other, k * DT)
        if now != last:
            keys.append('%.4f:%.5f:%.5f' % (a + k * DT, now[0], now[1]))
            last = now
    path = os.path.join(W, 'tmp', 'steer162_%s_%s_%s_%s_%d.csv' % (variant, name, label, gait, i))
    lab = os.path.join(W, 'res', variant, 'lab101')
    done = subprocess.run([lab, ','.join(keys), path, '%.4f' % (a + LAST + 1.5)], check=True, stdout=subprocess.PIPE)
    rows = list(csv.DictReader(open(path)))
    os.remove(path)
    d = {k: np.array([float(r[k]) for r in rows]) for k in rows[0]}
    t = d['t']
    dt = float(t[1] - t[0])
    win = (t >= a - 0.1) & (t < a + LAST + 1.0)
    out = {'set': name, 'course': label, 'gait': gait, 'moment': i, 'speed': v,
           'animated': done.stdout.decode().strip().endswith('state 0')}
    for group, bones in GROUPS.items():
        best, over = 0.0, 0
        for b in bones:
            p = np.stack([d['b%dx' % b], d['b%dy' % b], d['b%dz' % b]], 1)
            j = np.zeros(len(t))
            j[2:] = 1000.0 * np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1)
            j[~win] = 0.0
            best = max(best, float(j.max()))
            over += int(np.sum(j > LIMIT))
        out[group] = best
        if group == 'shin':
            out['shinOver'] = over
    slide, twist, short, swings, shorts = 0.0, 0.0, 9.0, 0, 0
    for leg in 'LR':
        sw = d['swing' + leg] > 0.5
        toe = np.stack([d['toe%sx' % leg], d['toe%sz' % leg]], 1)
        heel = np.stack([d['heel%sx' % leg], d['heel%sz' % leg]], 1)
        mid = 0.5 * (toe + heel)
        flat = np.abs(d['toe%sy' % leg] - d['heel%sy' % leg]) < 0.005
        yaw = np.unwrap(np.arctan2(toe[:, 0] - heel[:, 0], toe[:, 1] - heel[:, 1]))
        first = int(np.argmax(win))
        end = len(t) - 1 - int(np.argmax(win[::-1]))
        k = first
        while k <= end:
            e = k
            while e + 1 < len(t) and sw[e + 1] == sw[k]:
                e += 1
            whole = k > first and e < end
            if sw[k] and whole:
                length = (e - k + 1) * dt
                short = min(short, length)
                swings += 1
                shorts += length < 0.25
            if not sw[k] and e - k >= 4:
                step = np.linalg.norm(mid[k + 3:e] - mid[k + 2:e - 1], axis=1) * (flat[k + 3:e] & flat[k + 2:e - 1])
                slide = max(slide, 1000.0 * float(np.sum(step)))
                twist = max(twist, math.degrees(float(np.max(yaw[k + 2:e]) - np.min(yaw[k + 2:e]))))
            k = e + 1
    out['slide'], out['twist'], out['short'], out['swings'], out['shorts'] = slide, twist, short, swings, int(shorts)
    lowest = BANDS[(name, v)][0]
    if course[0] == 'gears':
        lowest = min(lowest, BANDS[(name, other)][0])
    below = 1000.0 * (lowest - d['b0y'][win])
    out['under'] = max(float(below.max()), 0.0)
    out['underTime'] = float(np.sum(below > 30.0)) * dt
    on = (t >= a) & (t < a + LAST)
    sx = np.array([steered(course, v, other, u - a)[0] for u in t[on]])
    sz = np.array([steered(course, v, other, u - a)[1] for u in t[on]])
    vx, vz = np.gradient(d['fx'], t)[on], np.gradient(d['fz'], t)[on]
    size = np.hypot(sx, sz)
    go = size > 0.0
    out['kept'] = float(np.mean(np.hypot(vx, vz)) / np.mean(size))
    out['along'] = float(np.mean((vx[go] * sx[go] + vz[go] * sz[go]) / size[go]) / np.mean(size[go]))
    off = np.abs((d['heading'][on][go] - np.arctan2(sx[go], sz[go]) + math.pi) % (2.0 * math.pi) - math.pi)
    out['off'] = math.degrees(float(np.mean(off)))
    return out


if __name__ == '__main__':
    variant, sets = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    jobs = [(variant, name, label, course, gait, i) for name in sets for label, course in COURSES
            for gait in ('WALK', 'RUN') for i in range(MOMENTS)]
    for name in sets:
        for key in ('WALK', 'RUN'):
            BANDS[(name, float(speeds(name)[key]))] = band(variant, name, float(speeds(name)[key]))
    with ThreadPoolExecutor(4) as pool:
        rows = list(pool.map(one, jobs))
    json.dump(rows, open(os.path.join(W, 'res', variant, 'steer162.json'), 'w'), indent=1)
    print('%s on %s: %d runs. Jumps: median / hardest in mm; over: updates with a shin over %.0f mm; under: the pelvis '
          'below the steady gait, hardest in mm, runs over 30 mm and seconds over 30 mm in the mean' % (
              variant, ' '.join(sets), len(rows), LIMIT))
    print('course      gait   n | shin        over | foot  | pelvis      | under mm runs  s    | swings short <0.25 s | '
          'twist slide | kept along off deg | fell')
    for label, course in COURSES:
        for gait in ('WALK', 'RUN'):
            q = [r for r in rows if r['course'] == label and r['gait'] == gait]
            def mm(key): return '%5.1f %5.1f' % (float(np.median([r[key] for r in q])), max(r[key] for r in q))
            print('%-11s %-4s %3d | %s %4d | %5.1f | %s | %5.0f   %3d  %4.2f  | %4d   %4d   %.3f    | %5.1f %5.1f | '
                  '%.2f %5.2f  %5.1f  | %d' % (
                      label, gait.lower(), len(q), mm('shin'), sum(r['shinOver'] for r in q), max(r['foot'] for r in q),
                      mm('pelvis'), max(r['under'] for r in q), sum(r['under'] > 30.0 for r in q),
                      float(np.mean([r['underTime'] for r in q])), sum(r['swings'] for r in q),
                      sum(r['shorts'] for r in q), min(r['short'] for r in q), max(r['twist'] for r in q),
                      max(r['slide'] for r in q), float(np.mean([r['kept'] for r in q])),
                      float(np.mean([r['along'] for r in q])), float(np.mean([r['off'] for r in q])),
                      sum(not r['animated'] for r in q)))
