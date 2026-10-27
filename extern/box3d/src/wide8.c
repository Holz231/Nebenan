// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

// Added for Nebenan: the convex contact constraints in eight lanes with AVX2, from upstream Box3D (#174). A world picks them
// at run time on a CPU with AVX2, see b3GetSIMDWidth, and they compute the same results as the four lanes in
// contact_solver.c.

#define B3_SIMD_WIDTH 8

#include "core.h"

#if defined( B3_SIMD_HAS_WIDTH_8 )

#if defined( _MSC_VER ) && !defined( __clang__ ) && !defined( __AVX2__ )
#error "MSVC must compile this file with /arch:AVX2, or define BOX3D_DISABLE_AVX2"
#endif

#include "contact_solver.h"

#include "body.h"
#include "constraint_graph.h"
#include "contact.h"
#include "ctz.h"
#include "math_internal.h"
#include "physics_world.h"
#include "platform.h"
#include "solver_set.h"

#if B3_ENABLE_VALIDATION
#include "shape.h"
#endif

#include <float.h>
#include <immintrin.h>

B3_AVX2_BEGIN

#include "simd_avx2.h"

#define B3_WIDE( name ) name##W8
#include "contact_solver_wide.inl"
#undef B3_WIDE

B3_AVX2_END

#else

// Nothing to compile without eight lanes, but a translation unit must not be empty
typedef int b3WideEightUnused;

#endif
