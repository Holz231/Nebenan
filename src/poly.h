// SPDX-License-Identifier: MIT

#pragma once

#include "core.h"

#include "nebenan/types.h"

#include "box3d/collision.h"
#include "box3d/math_functions.h"

// Vertex indices are stored as bytes, so a polyhedron has at most 255 vertices.
#define NB_POLY_MAX_VERTICES 255
#define NB_POLY_MAX_FACES 128
#define NB_POLY_MAX_INDICES 768

// A face of a working polyhedron. The tag records where the face came from: the index of the
// Voronoi site on the other side of the face, or a negative value for faces of the parent.
typedef struct nbPolyFace
{
	b3Plane plane;
	uint16_t first;
	uint8_t count;
	uint8_t material;
	int32_t tag;
} nbPolyFace;

// A convex polyhedron with convex polygon faces. The face loops are counter clockwise seen from
// outside. The capacity is fixed so the clipper never allocates.
typedef struct nbPoly
{
	int vertexCount;
	int faceCount;
	int indexCount;
	b3Vec3 vertices[NB_POLY_MAX_VERTICES];
	nbPolyFace faces[NB_POLY_MAX_FACES];
	uint8_t indices[NB_POLY_MAX_INDICES];
} nbPoly;

typedef enum nbClipResult
{
	nb_clipUnchanged,
	nb_clipCut,
	nb_clipEmpty,
	nb_clipOverflow,
} nbClipResult;

// Compact immutable polyhedron owned by a chunk. One allocation holds all arrays.
typedef struct nbShape
{
	b3Vec3* vertices;
	nbFace* faces;
	uint8_t* indices;
	int vertexCount;
	int faceCount;
	int indexCount;
	int byteCount;
	b3AABB bounds;
	b3Vec3 centroid;
	float volume;
	// Largest distance from the centroid to a vertex
	float radius;
} nbShape;

void nbPoly_MakeBox( nbPoly* poly, b3Vec3 halfExtents, b3Transform transform, uint8_t material );
bool nbPoly_MakeFromHull( nbPoly* poly, const b3HullData* hull, b3Transform transform, uint8_t material );
void nbPoly_Translate( nbPoly* poly, b3Vec3 translation );

// Keep the part of the polyhedron behind the plane: dot(normal, x) <= offset.
// The new cap face gets the material and tag. The output must not alias the input.
nbClipResult nbPoly_Clip( const nbPoly* in, b3Plane plane, uint8_t material, int32_t tag, float tolerance, nbPoly* out );

void nbPoly_ComputeMass( const nbPoly* poly, float* volume, b3Vec3* centroid );
float nbPoly_FaceArea( const nbPoly* poly, int faceIndex, b3Vec3* centroid );
b3AABB nbPoly_ComputeBounds( const nbPoly* poly );
bool nbPoly_ContainsPoint( const nbPoly* poly, b3Vec3 point, float margin );
float nbPoly_MaxDistanceSquared( const nbPoly* poly, b3Vec3 point );

// Checks topology and planes. For tests and debugging.
bool nbPoly_IsValid( const nbPoly* poly, float tolerance );

// Convert to the compact shape. Returns null if the polyhedron is degenerate.
nbShape* nbShape_Create( const nbPoly* poly );

// Same, with the volume and centroid from nbPoly_ComputeMass already at hand
nbShape* nbShape_CreateWithMass( const nbPoly* poly, float volume, b3Vec3 centroid );
void nbShape_Destroy( nbShape* shape );
void nbShape_ToPoly( const nbShape* shape, nbPoly* poly );
void nbShape_Translate( nbShape* shape, b3Vec3 translation );
nbGeometry nbShape_GetGeometry( const nbShape* shape );

// Signed distance from a point to the convex shape. Negative inside. Exact outside faces,
// a lower bound near edges and corners, which is what the impact queries need.
float nbShape_Distance( const nbShape* shape, b3Vec3 point );

// Area and centroid of the overlap of two coplanar faces with opposing normals.
float nbShape_FaceOverlap( const nbShape* a, int faceA, const nbShape* b, int faceB, b3Vec3* centroid );

// Find the overlap of two shapes that touch with opposing coplanar faces.
// Returns the contact area and writes the area weighted centroid and the normal from a to b.
float nbShape_ContactArea( const nbShape* a, const nbShape* b, float tolerance, b3Vec3* centroid, b3Vec3* normal );

// Number of triangle list vertices for a flat shaded mesh.
int nbShape_GetMeshVertexCount( const nbShape* shape );
int nbShape_BuildMesh( const nbShape* shape, nbMeshVertex* vertices, int capacity, float uvScale );
