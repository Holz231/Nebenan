# leg150.py run.csv from to L|R: one leg of a run of lab101, update by update between two times in seconds: whether the
# gait runs (0 to 1) and its speed beside that of the capsule, the height of the root, how far the foot is rolled,
# whether it trails and swings, how far the knee bends in degrees, how long the leg is from hip to ankle in mm and as a
# share of its full length, how much longer than the update before, and how far ankle, knee and hip jump (second
# difference of the origin of foot, shin and thigh, mm). A leg that gets longer by the same amount update after update
# and then stops in one is a jump at the knee, about twice as large on a leg stretched to 0.98: there the leg ran into
# its reach, see round 150 in docs/Roadmap.md. The bodies follow the pose of the update before.
import csv, sys
import numpy as np

if __name__ == '__main__':
    rows = list(csv.DictReader(open(sys.argv[1])))
    start, end, side = float(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
    thigh, shin, foot = (13, 14, 15) if side == 'L' else (16, 17, 18)

    def at(r, b):
        return np.array([float(r['b%d%s' % (b, a)]) for a in 'xyz'])

    before = None
    print('t      run gait  speed rootY  roll  trails swings bend  leg mm share  longer  ankle knee  hip')
    for i in range(1, len(rows) - 1):
        r = rows[i]
        t = float(r['t'])
        hip, knee, ankle = at(r, thigh), at(r, shin), at(r, foot)
        length = float(np.linalg.norm(ankle - hip))
        if start <= t <= end:
            full = float(np.linalg.norm(hip - knee) + np.linalg.norm(knee - ankle))
            u, w = hip - knee, ankle - knee
            bend = 180.0 - np.degrees(np.arccos(np.clip(np.dot(u, w) / np.linalg.norm(u) / np.linalg.norm(w), -1.0, 1.0)))
            dt = float(rows[i + 1]['t']) - float(rows[i - 1]['t'])
            v = np.array([float(rows[i + 1][k]) - float(rows[i - 1][k]) for k in ('fx', 'fz')]) / dt

            def jump(b):
                return 1000.0 * float(np.linalg.norm(at(rows[i + 1], b) - 2.0 * at(r, b) + at(rows[i - 1], b)))

            print('%.3f %.2f %5.2f %5.2f %.3f %5.3f %6s %6s %5.1f %6.1f %.3f %7s %5.1f %5.1f %4.1f' % (
                t, float(r['gaitRun']), float(r['gaitSpeed']), float(np.linalg.norm(v)), float(r['rootY']),
                float(r['roll' + side]), r['behind' + side], r['swing' + side], bend, 1000.0 * length, length / full,
                '-' if before is None else '%+.1f' % (1000.0 * (length - before)), jump(foot), jump(shin), jump(thigh)))
        before = length
