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
SET_h13="WALK=1.14 RUN=4.5 DIAG=0.81 CW=1.190 CR=0.663"
SET_h14="WALK=1.52 RUN=3.25 DIAG=1.07 CW=1.028 CR=0.731"
SET_h15="WALK=1.55 RUN=3.15 DIAG=1.10 CW=1.018 CR=0.737"
SET_h17="WALK=1.16 RUN=4.38 DIAG=0.82 CW=1.182 CR=0.669"
SET_h18="WALK=1.64 RUN=3.38 DIAG=1.16 CW=0.987 CR=0.723"
SET_h19="WALK=1.32 RUN=4.22 DIAG=0.93 CW=1.105 CR=0.681"
SET_h20="WALK=1.48 RUN=3.65 DIAG=1.05 CW=1.044 CR=0.710"
SET_h21="WALK=1.28 RUN=4.28 DIAG=0.91 CW=1.122 CR=0.679"
SET_h22="WALK=1.38 RUN=3.72 DIAG=0.98 CW=1.081 CR=0.706"
SET_hf7="WALK=1.22 RUN=3.55 FAST=4.35 CW=1.156 CR=0.712 CF=0.670"
SET_hf9="WALK=1.12 RUN=3.35 FAST=4.25 CW=1.199 CR=0.725 CF=0.675"
SET_hf11="WALK=1.08 RUN=3.45 FAST=4.15 CW=1.220 CR=0.719 CF=0.680"
SET_hf13="WALK=1.14 RUN=3.25 FAST=4.5 CW=1.190 CR=0.731 CF=0.663"
SET_hf15="WALK=1.10 RUN=3.30 FAST=4.30 CW=1.207 CR=0.728 CF=0.672"
SET_hf17="WALK=1.16 RUN=3.38 FAST=4.38 CW=1.182 CR=0.723 CF=0.669"
SET_hf19="WALK=1.24 RUN=3.62 FAST=4.52 CW=1.147 CR=0.708 CF=0.662"
SET_hf21="WALK=1.32 RUN=3.65 FAST=4.22 CW=1.105 CR=0.710 CF=0.681"
SET_hf23="WALK=1.42 RUN=3.75 FAST=4.55 CW=1.065 CR=0.704 CF=0.668"
SET_hf25="WALK=1.28 RUN=3.72 FAST=4.28 CW=1.122 CR=0.706 CF=0.679"
SET_hf27="WALK=1.78 RUN=3.85 FAST=4.62 CW=0.952 CR=0.699 CF=0.665"
