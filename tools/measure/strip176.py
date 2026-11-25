# strip176.py variant script from to out.png [every]: draws a steered humanoid as a strip of stick figures, to look at
# a gait and not only at its numbers, for round 176, see docs/Roadmap.md. Runs lab101 of the variant with the script
# (keys "t:x:z" of the stick, see lab101.c; with SYNC in the environment as for the other tools) and draws every
# "every"-th update (default 4) between the times from and to: in the first row seen from its right side, in the second
# from the front, both turning with where it faces and held at the capsule, with the ground as a line; below, from above
# and fixed in the world, the way of the capsule with a tick where it faces at each drawn update and the feet where
# they stand. The left arm and leg are red, the right ones blue; a foot in the air is drawn thin. Needs Pillow.
import csv, math, os, subprocess, sys
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from where129 import W

TRUNK = [0, 1, 2, 3, 4, 5, 6]
LIMBS = {'L': ([4, 7, 8, 9], [0, 13, 14, 15], (200, 40, 40)), 'R': ([4, 10, 11, 12], [0, 16, 17, 18], (40, 80, 200))}
CELL, HIGH, SCALE = 120, 230, 105.0


def main():
    variant, script, t0, t1, out = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
    every = int(sys.argv[6]) if len(sys.argv) > 6 else 4
    path = os.path.join(W, 'tmp', 'strip176_%d.csv' % os.getpid())
    subprocess.run([os.path.join(W, 'res', variant, 'lab101'), script, path, '%.4f' % (t1 + 0.1)], check=True,
                   stdout=subprocess.DEVNULL)
    rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(open(path))]
    os.remove(path)
    for a, b in zip(rows[:-1], rows[1:]):
        b['speed'] = math.hypot(b['fx'] - a['fx'], b['fz'] - a['fz']) / (b['t'] - a['t'])
    rows = [r for r in rows[1:] if t0 <= r['t'] <= t1][::every]
    n = len(rows)
    # from above, fixed in the world
    xs = [r['fx'] for r in rows] + [r[k] for r in rows for k in ('toeLx', 'toeRx', 'heelLx', 'heelRx')]
    zs = [r['fz'] for r in rows] + [r[k] for r in rows for k in ('toeLz', 'toeRz', 'heelLz', 'heelRz')]
    scale = min((CELL * n - 40) / max(max(zs) - min(zs), 0.5), 220.0)

    top = int(60 + scale * (max(xs) - min(xs)))
    img = Image.new('RGB', (CELL * n, 2 * HIGH + top), (255, 255, 255))
    g = ImageDraw.Draw(img)
    ground = min(min(r['toeLy'], r['heelLy'], r['toeRy'], r['heelRy']) for r in rows)

    def joint(r, b):
        return r['b%dx' % b], r['b%dy' % b], r['b%dz' % b]

    def cell(r, i, row, p):
        """a point of the world in the cell of update i: row 0 from the right side, row 1 from the front"""
        s, c = math.sin(r['heading']), math.cos(r['heading'])
        x, z = p[0] - r['fx'], p[2] - r['fz']
        along, aside = x * s + z * c, x * c - z * s
        u = along if row == 0 else aside
        return CELL * i + CELL / 2 + SCALE * u, HIGH * row + HIGH - 14 - SCALE * (p[1] - ground)

    for i, r in enumerate(rows):
        for row in (0, 1):
            y = HIGH * row + HIGH - 14
            g.line([CELL * i, y, CELL * (i + 1), y], fill=(170, 170, 170))
            order = ['L', 'R'] if row == 0 else ['R', 'L']
            for side in order:
                arm, leg, colour = LIMBS[side]
                g.line([cell(r, i, row, joint(r, b)) for b in arm], fill=colour, width=2)
                air = r['swing' + side] > 0.5
                foot = [tuple(r['%s%s%s' % (e, side, a)] for a in 'xyz') for e in ('heel', 'toe')]
                pts = [cell(r, i, row, joint(r, b)) for b in leg] + [cell(r, i, row, foot[0]), cell(r, i, row, foot[1])]
                g.line(pts, fill=colour, width=2 if air else 4)
            g.line([cell(r, i, row, joint(r, b)) for b in TRUNK], fill=(30, 30, 30), width=4)
            hx, hy = cell(r, i, row, joint(r, 6))
            g.ellipse([hx - 7, hy - 16, hx + 7, hy - 2], outline=(30, 30, 30), width=2)
        g.text((CELL * i + 3, 2), '%.2f' % r['t'], fill=(0, 0, 0))
        g.text((CELL * i + 3, 14), '%.2f m/s' % r['speed'], fill=(120, 120, 120))
    def above(x, z):
        return 20 + scale * (z - min(zs)), 2 * HIGH + 20 + scale * (x - min(xs))

    g.line([above(r['fx'], r['fz']) for r in rows], fill=(120, 120, 120), width=1)
    for i, r in enumerate(rows):
        a = above(r['fx'], r['fz'])
        b = above(r['fx'] + 0.12 * math.sin(r['heading']), r['fz'] + 0.12 * math.cos(r['heading']))
        g.line([a, b], fill=(0, 0, 0), width=2)
        if i % 5 == 0:
            g.text((a[0] - 8, a[1] + 14), '%.2f' % r['t'], fill=(90, 90, 90))
        for side in 'LR':
            if r['swing' + side] < 0.5:
                g.line([above(r['heel%sx' % side], r['heel%sz' % side]), above(r['toe%sx' % side], r['toe%sz' % side])],
                       fill=LIMBS[side][2], width=3)
    g.line([20, 2 * HIGH + top - 8, 20 + scale, 2 * HIGH + top - 8], fill=(0, 0, 0), width=2)
    g.text((24 + scale, 2 * HIGH + top - 14), '1 m', fill=(0, 0, 0))
    img.save(out)


main()
