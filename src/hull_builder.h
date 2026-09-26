// SPDX-License-Identifier: MIT

#pragma once

#include "poly.h"

// Byte count of the Box3D hull for this shape, or zero if the shape exceeds Box3D's hull limits.
int nbGetHullByteCount( const nbShape* shape );

// Write a Box3D hull for the shape into memory of nbGetHullByteCount bytes.
// Returns null if the topology is not a closed convex two-manifold.
b3HullData* nbBuildHull( const nbShape* shape, void* memory );

// Build the hull in arena memory. Falls back to Box3D's quickhull for shapes beyond the direct path,
// which also merges degenerate features, and counts that in fallbackCount. Returns null for slivers
// that have no valid hull. Thread safe as long as every thread uses its own arena.
b3HullData* nbCreateHullInArena( const nbShape* shape, nbArena* arena, int* fallbackCount );
