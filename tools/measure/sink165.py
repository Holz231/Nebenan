# sink165.py variant: for every walking run of the last table steer162.py wrote for the variant that was steered fast
# (the stick going round at 180 or 360 degrees per second, the zigzag by 90 degrees, back and forth) and whose pelvis
# got more than 30 mm below the steady walk, what went on at the deepest update, measured for round 165: how long after
# the stick last jumped, the speed of the capsule against the steered speed and how it changes, how many feet are in
# the air, how fast the heading turns, how far the root crouches for a landing and how far it is lowered in all. Each
# run is run again for it. Needs lab101 of the variant and its out/res/<variant>/steer162.json.
import csv, json, os, subprocess, sys, math, collections, numpy as np
sys.path.insert(0, '.')
from where129 import speeds, W
import steer162 as s
from gait155 import band
variant = sys.argv[1]
F = ('circle180', 'circle360', 'zigzag90', 'shuttle')
table = json.load(open(os.path.join(W, 'res', variant, 'steer162.json')))
runs = [x for x in table if x['gait'] == 'WALK' and x['course'].startswith(F) and x['under'] > 30]
bands = {}
out = []
for x in runs:
    name, label, moment = x['set'], x['course'], x['moment']
    course = dict(s.COURSES)[label]
    sp = speeds(name)
    v = float(sp['WALK'])
    a = 4.0 + moment * float(sp['CW']) / 4
    keys, last, flips = ['0:0:0', '1:0:%.4f' % v], (0.0, v), []
    for k in range(240):
        now = s.steered(course, v, float(sp['RUN']), k / 60.0)
        if now != last:
            keys.append('%.4f:%.5f:%.5f' % (a + k / 60.0, now[0], now[1]))
            if math.hypot(now[0] - last[0], now[1] - last[1]) > 0.5 * v:
                flips.append(k / 60.0)
            last = now
    flips.append(4.0)
    path = os.path.join(W, 'tmp', 'c165_%s.csv' % variant)
    lab = os.path.join(W, 'res', variant, 'lab101')
    subprocess.run([lab, ','.join(keys), path, '%.3f' % (a + 5.5)], check=True, stdout=subprocess.DEVNULL)
    rows = list(csv.DictReader(open(path)))
    d = {k: np.array([float(z[k]) for z in rows]) for k in rows[0]}
    t = d['t']
    if (name, v) not in bands:
        bands[(name, v)] = band(variant, name, v)[0]
    under = 1000 * (bands[(name, v)] - d['b0y'])
    win = (t >= a - 0.1) & (t < a + 5.0)
    k0 = int(np.argmax(np.where(win, under, -1e9)))
    u = t[k0] - a
    since = min([u - f for f in flips if f <= u] or [9.0])
    vx, vz = np.gradient(d['fx'], t), np.gradient(d['fz'], t)
    sp0, sp1 = math.hypot(vx[k0], vz[k0]), math.hypot(vx[k0 - 6], vz[k0 - 6])
    swings = int(d['swingL'][k0]) + int(d['swingR'][k0])
    turn = abs(math.degrees((d['heading'][k0] - d['heading'][k0 - 6] + math.pi) % (2 * math.pi) - math.pi)) / 0.1
    out.append((label, name, moment, under[k0], u, since, sp0 / v, (sp0 - sp1) / 0.1, swings, turn, d['crouch'][k0] * 1000,
                d['rdrop'][k0] * 1000))
print('%d sunk runs. course set moment | under mm | t | since flip s | speed/v | accel m/s2 | feet in air | heading deg/s | '
      'crouch mm | rdrop mm' % len(out))
for o in sorted(out, key=lambda o: -o[3]):
    print('%-13s %-5s %d | %3.0f | %.2f | %.2f | %.2f | %5.1f | %d | %4.0f | %3.0f | %3.0f' % o)
print('after the course (t > 4): %d; within 0.5 s after a flip: %d; speed under 0.4 v: %d; turning faster than 100 deg/s: '
      '%d; both: %d; crouch over 15 mm: %d' % (
          sum(o[4] > 4.0 for o in out), sum(o[5] < 0.5 for o in out), sum(o[6] < 0.4 for o in out),
          sum(o[9] > 100 for o in out), sum(o[6] < 0.45 and o[9] > 100 for o in out), sum(o[10] > 15 for o in out)))
