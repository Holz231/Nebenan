# eval154.py old.csv new.csv [hits_old.csv hits_new.csv meet_old.csv meet_new.csv]: the rule of round 154,
# docs/Regeln/RULE154.txt, on the tables of meet154.c for the state before and the build, and with the four tables of
# hit153.c the protections D1 and D5. Prints every condition with its numbers and ends with 1 if one is missed.
import csv, sys
RADII = 0.612        # the two capsules of the humanoid of 1.8 m, m
CLOSE = 0.001        # A2: how far below it the frames may come, m
WALL = 1.5           # A3: a meeting may be this many times as hard as the harder of the two walls
FLOOR = 2.0          # A3: and this much more, mm
FOLLOW = 0.9         # A4: behind a slower figure, no slower than this share of its speed
FASTER = 0.3         # A4: where the first is this much faster than the one ahead, m/s
PUSHED = 0.01        # A5: how far a standing figure may be moved, m
LEFT_MIN = 4         # A1 counts as shown where this many figures left the animated state before
MEET = ('stand', 'toward', 'ahead', 'cross')
def read(p): return list(csv.DictReader(open(p)))
old, new = read(sys.argv[1]), read(sys.argv[2])
ok = True
def say(name, good, text):
    global ok
    ok = ok and good
    print(('met     ' if good else 'MISSED  ') + name + ': ' + text)
def left(rows): return sum((float(r['leftA']) >= 0) + (float(r['leftC']) >= 0) for r in rows if r['kind'] in MEET)
mo, mn = [r for r in old if r['kind'] in MEET], [r for r in new if r['kind'] in MEET]
lo, ln = left(old), left(new)
say('A1', ln == 0 and lo >= LEFT_MIN, f'figures that left the animated state in {len(mn)} meetings: {lo} before, {ln} now (shown from {LEFT_MIN} before)')
co, cn = min(float(r['closest']) for r in mo), min(float(r['closest']) for r in mn)
say('A2', cn >= RADII - CLOSE, f'closest frames {cn:.3f} m now, {co:.3f} m before, at least {RADII - CLOSE:.3f} m')
speeds = sorted({float(r['speed']) for r in new})
walls = {s: max(float(r['jumpA']) for r in new if float(r['speed']) == s and r['kind'] in ('wall', 'wall30')) for s in speeds}
def limit(v): return WALL * walls[min([s for s in speeds if s >= abs(v) - 1e-6] or [speeds[-1]])] + FLOOR
worst, good = [], True
for r in mn:
    for who, v in (('A', float(r['speed'])), ('C', float(r['second']))):
        j, most = float(r['jump' + who]), limit(v)
        good = good and j <= most
        worst.append((j / most, j, most, who, r['speed'], r['kind'], r['side']))
w = max(worst)
jo = max(max(float(r['jumpA']), float(r['jumpC'])) for r in mo)
say('A3', good, f'hardest jump against {WALL} times the walls + {FLOOR} mm: {w[1]:.1f} of {w[2]:.1f} mm (figure {w[3]}, {w[4]} m/s, {w[5]}, {w[6]} m); hardest before {jo:.1f} mm')
rows = [r for r in mn if r['kind'] == 'ahead' and float(r['side']) == 0.0 and float(r['speed']) >= float(r['second']) + FASTER]
good = all(float(r['blocked']) == 0.0 and float(r['slowest']) >= FOLLOW * float(r['second']) for r in rows)
say('A4', good and len(rows) > 0, 'behind a slower figure, slowest over its speed: ' + ', '.join(f"{float(r['slowest']) / float(r['second']):.2f}" for r in rows))
far = max(float(r['wayC']) for r in mn if r['kind'] == 'stand')
say('A5', far <= PUSHED, f'a standing figure is moved {far:.2f} m at most, before {max(float(r["wayC"]) for r in mo if r["kind"] == "stand"):.2f} m')
same = [r for r in old if r['kind'] not in MEET] == [r for r in new if r['kind'] not in MEET]
say('D2', same, 'alone and at the walls the tables are the same')
if len(sys.argv) > 6:
    say('D1', open(sys.argv[3], 'rb').read() == open(sys.argv[4], 'rb').read(), 'the hits of hit153.c are the same to the byte')
    def falls(p): return sum(int(r['fellA']) + int(r['fellC']) for r in read(p) if float(r['engage']) > 0.0)
    fo, fn = falls(sys.argv[5]), falls(sys.argv[6])
    say('D5', fn <= fo, f'falls in the meetings of hit153.c with engageRadius: {fo} before, {fn} now')
sys.exit(0 if ok else 1)
