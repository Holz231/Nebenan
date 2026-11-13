# common.sh: sourced by the scripts here. M is this directory, W the work directory (NB_MEASURE_OUT, default out/ here,
# which git ignores), T the source tree to build (NB_SRC, default this repository), and the speed sets: walking and
# running speed in m/s, the speed per axis walking diagonally, and the spread of the 24 points of a scene over one cycle
# of the gait walking and running, in seconds. F sets are for figures that face where they move: three speeds.
M=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
W=${NB_MEASURE_OUT:-$M/out}
T=${NB_SRC:-$(cd "$M/../.." && pwd)}
mkdir -p "$W/res" "$W/vb" "$W/tmp" "$W/tr"
SET_h1="WALK=1.5 RUN=3.2 DIAG=1.06 CW=1.035 CR=0.734"
SET_h2="WALK=1.05 RUN=4.3 DIAG=0.74 CW=1.24 CR=0.672"
SET_h5="WALK=1.18 RUN=4.45 DIAG=0.83 CW=1.173 CR=0.665"
SET_h6="WALK=1.58 RUN=3.05 DIAG=1.12 CW=1.008 CR=0.743"
SET_h9="WALK=1.12 RUN=4.25 DIAG=0.79 CW=1.199 CR=0.675"
SET_h10="WALK=1.72 RUN=3.35 DIAG=1.22 CW=0.960 CR=0.725"
SET_h11="WALK=1.08 RUN=4.15 DIAG=0.76 CW=1.220 CR=0.680"
SET_h12="WALK=1.68 RUN=3.45 DIAG=1.19 CW=0.974 CR=0.719"
SET_hf7="WALK=1.22 RUN=3.55 FAST=4.35 CW=1.156 CR=0.712 CF=0.670"
SET_hf9="WALK=1.12 RUN=3.35 FAST=4.25 CW=1.199 CR=0.725 CF=0.675"
SET_hf11="WALK=1.08 RUN=3.45 FAST=4.15 CW=1.220 CR=0.719 CF=0.680"
