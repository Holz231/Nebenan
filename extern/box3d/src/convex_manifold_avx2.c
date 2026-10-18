// SPDX-License-Identifier: MIT

// Added for Nebenan: the collision of convex shapes from convex_manifold.c compiled a second time for AVX2. A world with
// eight lanes collides with these, see b3ComputeConvexManifold. Without FMA the compiler only writes the same instructions
// in their AVX form, so the results are the same.

#include "core.h"

#if defined( B3_SIMD_HAS_WIDTH_8 )

#if defined( _MSC_VER ) && !defined( __clang__ ) && !defined( __AVX2__ )
#error "MSVC must compile this file with /arch:AVX2, or define BOX3D_DISABLE_AVX2"
#endif

#include "algorithm.h"
#include "manifold.h"
#include "shape.h"
#include "simd.h"

#include "box3d/base.h"
#include "box3d/collision.h"
#include "box3d/constants.h"

#define B3_CONVEX_MANIFOLD_AVX2

B3_AVX2_BEGIN

#include "convex_manifold.c"

B3_AVX2_END

#endif
