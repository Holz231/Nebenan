// SPDX-License-Identifier: MIT

#pragma once

#include "poly.h"

// Byte layout of the Box3D hull the direct builder writes for a polyhedron
typedef struct nbHullLayout
{
	size_t vertexOffset;
	size_t pointOffset;
	size_t edgeOffset;
	size_t planeOffset;
	size_t faceOffset;
	size_t soaVertexOffset;
	size_t soaNormalOffset;
	size_t byteCount;
} nbHullLayout;

// Layout of the hull for a polyhedron with these counts. False if the polyhedron exceeds Box3D's hull limits.
bool nbGetHullLayout( int vertexCount, int faceCount, int indexCount, nbHullLayout* layout );

// Byte count of the Box3D hull for this shape, or zero if the shape exceeds Box3D's hull limits.
int nbGetHullByteCount( const nbShape* shape );

// Write a Box3D hull for the shape into memory of nbGetHullByteCount bytes. The memory may be the hull built into the
// shape, which already holds its vertices and planes, see nbShape_CreateWithMass. Returns null if the topology is not a
// closed convex two-manifold.
b3HullData* nbBuildHull( const nbShape* shape, void* memory );

// Build the hull of the shape, right into the shape where the direct builder takes it. Falls back to Box3D's quickhull
// for shapes beyond the direct path, which also merges degenerate features, and counts that in fallbackCount. Returns
// false for slivers that have no valid hull. Thread safe.
bool nbShape_BuildHull( nbShape* shape, int* fallbackCount );
