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

// The face two chunks share
typedef struct nbBondGeometry
{
	b3Vec3 centroid;

	// Unit normal, pointing from the first chunk to the second
	b3Vec3 normal;

	float area;

	// Second moments of the face about its centroid per square meter of area: xx, yy, zz and xy, xz, yz. They tell
	// how far the face reaches in each direction, which sets how much bending it carries.
	b3Vec3 moments;
	b3Vec3 crossMoments;
} nbBondGeometry;


typedef enum nbClipResult
{
	nb_clipUnchanged,
	nb_clipCut,
	nb_clipEmpty,
	nb_clipOverflow,
} nbClipResult;

// Compact immutable polyhedron owned by a chunk. One allocation holds the faces, the loops and, built right into it, the
// Box3D hull of the chunk: the vertices and face planes are the points and planes of that hull, and Box3D shapes use it
// in place (b3ShapeDef::externalHull), so the geometry is stored once. A polyhedron beyond the direct hull builder keeps
// its vertices and planes in the allocation and gets a hull of its own from quickhull, see nbShape_BuildHull.
typedef struct nbShape
{
	// Null until nbShape_BuildHull, and for a sliver without a valid hull. Lies in the allocation, unless quickhull made
	// it.
	const b3HullData* hull;
	b3Vec3* vertices;
	b3Plane* planes;
	nbFace* faces;
	uint8_t* indices;
	int vertexCount;
	int faceCount;
	int indexCount;
	b3AABB bounds;
	b3Vec3 centroid;
	float volume;
	// Largest distance from the centroid to a vertex
	float radius;
} nbShape;

// Where the parts of a shape lie in its allocation. The counts give it, so the shape does not keep it.
typedef struct nbShapeLayout
{
	size_t faceOffset;
	size_t indexOffset;
	// Start of the hull built into the allocation, zero for a polyhedron beyond the direct hull builder
	size_t hullOffset;
	size_t vertexOffset;
	size_t planeOffset;
	size_t byteCount;
} nbShapeLayout;

void nbPoly_MakeBox( nbPoly* poly, b3Vec3 halfExtents, b3Transform transform, uint8_t material );

// Copy the used part of a polyhedron, a small part of its fixed capacity of about 7 KB
static inline void nbPoly_Copy( nbPoly* dst, const nbPoly* src )
{
	dst->vertexCount = src->vertexCount;
	dst->faceCount = src->faceCount;
	dst->indexCount = src->indexCount;
	memcpy( dst->vertices, src->vertices, sizeof( b3Vec3 ) * (size_t)src->vertexCount );
	memcpy( dst->faces, src->faces, sizeof( nbPolyFace ) * (size_t)src->faceCount );
	memcpy( dst->indices, src->indices, (size_t)src->indexCount );
}
bool nbPoly_MakeFromHull( nbPoly* poly, const b3HullData* hull, b3Transform transform, uint8_t material );
void nbPoly_Translate( nbPoly* poly, b3Vec3 translation );

// Keep the part of the polyhedron behind the plane: dot(normal, x) <= offset.
// The new cap face gets the material and tag. The output must not alias the input.
nbClipResult nbPoly_Clip( const nbPoly* in, b3Plane plane, uint8_t material, int32_t tag, float tolerance, nbPoly* out );

void nbPoly_ComputeMass( const nbPoly* poly, float* volume, b3Vec3* centroid );
// Area, centroid and normal of a face
float nbPoly_FaceGeometry( const nbPoly* poly, int faceIndex, nbBondGeometry* geometry );
b3AABB nbPoly_ComputeBounds( const nbPoly* poly );
bool nbPoly_ContainsPoint( const nbPoly* poly, b3Vec3 point, float margin );
float nbPoly_MaxDistanceSquared( const nbPoly* poly, b3Vec3 point );

// Checks topology and planes. For tests and debugging.
bool nbPoly_IsValid( const nbPoly* poly, float tolerance );

// Convert to the compact shape. Returns null if the polyhedron is degenerate.
nbShape* nbShape_Create( const nbPoly* poly );

// Same, with the volume and centroid from nbPoly_ComputeMass already at hand
nbShape* nbShape_CreateWithMass( const nbPoly* poly, float volume, b3Vec3 centroid );
nbShapeLayout nbGetShapeLayout( int vertexCount, int faceCount, int indexCount );
void nbShape_Destroy( nbShape* shape );
void nbShape_ToPoly( const nbShape* shape, nbPoly* poly );

// Move the shape. Only before its hull is built.
void nbShape_Translate( nbShape* shape, b3Vec3 translation );
nbGeometry nbShape_GetGeometry( const nbShape* shape );

// Signed distance from a point to the convex shape. Negative inside. Exact outside faces,
// a lower bound near edges and corners, which is what the impact queries need.
float nbShape_Distance( const nbShape* shape, b3Vec3 point );

// The overlap of two coplanar faces with opposing normals. Returns the area, the normal is the one of face A.
float nbShape_FaceOverlap( const nbShape* a, int faceA, const nbShape* b, int faceB, nbBondGeometry* geometry );

// Find the overlap of two shapes that touch with opposing coplanar faces.
// Returns the contact area and writes the combined geometry with the normal from a to b.
float nbShape_ContactArea( const nbShape* a, const nbShape* b, float tolerance, nbBondGeometry* geometry );

