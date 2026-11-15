# steady130.py old new...: whether a build leaves the steady gait as it is, searched over a grid of speeds: steady runs
# of 9 s with each built variant (lab101), forward facing where it moves and sideways and backward with a fixed heading,
# at 1.00 to 4.80 m/s in steps of 0.05, 231 runs, or with FINE=1 sideways and backward at 4.30 to 4.80 m/s in steps of
# 0.01 without the speeds of FRESH (default those kept for the second check of round 130), 96 runs. For every new
# build the runs whose rows differ from old to the byte from 5 s on: starting to walk or run is a change, see round
# 130 in docs/Roadmap.md. The strength of a limit that is not to touch the steady gait is searched with this.
import os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

M = os.path.dirname(os.path.abspath(__file__))
W = os.environ.get('NB_MEASURE_OUT', os.path.join(M, 'out'))
if os.environ.get('FINE'):
    fresh = os.environ.get('FRESH', '4.44 4.63 4.73').split()
    speeds = [v for v in ('%.2f' % (4.30 + 0.01 * i) for i in range(51)) if v not in fresh]
    RUNS = [('s' + v, '0:0:0:0,1:' + v + ':0:0') for v in speeds] + [('b' + v, '0:0:0:0,1:0:-' + v + ':0') for v in speeds]
else:
    speeds = ['%.2f' % (1.0 + 0.05 * i) for i in range(77)]
    RUNS = [('f' + v, '0:0:0,1:0:' + v) for v in speeds] + [('s' + v, '0:0:0:0,1:' + v + ':0:0') for v in speeds] + \
           [('b' + v, '0:0:0:0,1:0:-' + v + ':0') for v in speeds]


def tail(v, name, script):
    path = os.path.join(W, 'tmp', 'steady130_%s_%s.csv' % (v, name))
    subprocess.run([os.path.join(W, 'res', v, 'lab101'), script, path, '9.0'], check=True, stdout=subprocess.DEVNULL)
    rows = [line for line in open(path).read().split('\n')[1:] if line and float(line.split(',')[0]) >= 5.0]
    os.remove(path)
    return rows


def all_runs(v):
    with ThreadPoolExecutor(4) as pool:
        return list(pool.map(lambda r: tail(v, r[0], r[1]), RUNS))


if __name__ == '__main__':
    os.makedirs(os.path.join(W, 'tmp'), exist_ok=True)
    old = all_runs(sys.argv[1])
    for new in sys.argv[2:]:
        got = all_runs(new)
        bad = [RUNS[i][0] for i in range(len(RUNS)) if got[i] != old[i]]
        print('%s against %s: %d of %d steady runs differ from 5 s on: %s' % (
            new, sys.argv[1], len(bad), len(RUNS), ' '.join(bad)))
