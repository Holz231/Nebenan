// SPDX-License-Identifier: MIT

#pragma once

#include "poly.h"

// Byte count of the Box3D hull for this shape, or zero if the shape exceeds Box3D's hull limits.
int nbGetHullByteCount( const nbShape* shape );

// Write a Box3D hull for the shape into memory of nbGetHullByteCount bytes.
// Returns null if the topology is not a closed convex two-manifold.
b3HullData* nbBuildHull( const nbShape* shape, void* memory );
