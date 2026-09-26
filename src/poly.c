// SPDX-License-Identifier: MIT

#include "poly.h"

#include <float.h>

// Box faces as vertex loops, counter clockwise seen from outside. Vertex i has the coordinates
// (i & 1 ? +x : -x, i & 2 ? +y : -y, i & 4 ? +z : -z).
static const uint8_t nb_boxLoops[6][4] = {
	{ 1, 3, 7, 5 }, // +x
	{ 0, 4, 6, 2 }, // -x
	{ 2, 6, 7, 3 }, // +y
	{ 0, 1, 5, 4 }, // -y
	{ 4, 5, 7, 6 }, // +z
	{ 0, 2, 3, 1 }, // -z
};

static const b3Vec3 nb_boxNormals[6] = {
	{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
};

void nbPoly_MakeBox( nbPoly* poly, b3Vec3 h, b3Transform transform, uint8_t material )
{
	for ( int i = 0; i < 8; ++i )
	{
		b3Vec3 p = {
			( i & 1 ) ? h.x : -h.x,
			( i & 2 ) ? h.y : -h.y,
			( i & 4 ) ? h.z : -h.z,
		};
		poly->vertices[i] = b3TransformPoint( transform, p );
	}
	poly->vertexCount = 8;

	for ( int f = 0; f < 6; ++f )
	{
		nbPolyFace* face = poly->faces + f;
		b3Vec3 n = b3RotateVector( transform.q, nb_boxNormals[f] );
		face->plane.normal = n;
		face->plane.offset = b3Dot( n, poly->vertices[nb_boxLoops[f][0]] );
		face->first = (uint16_t)( 4 * f );
		face->count = 4;
		face->material = material;
		face->tag = -1;
		for ( int k = 0; k < 4; ++k )
		{
			poly->indices[4 * f + k] = nb_boxLoops[f][k];
		}
	}
	poly->faceCount = 6;
	poly->indexCount = 24;
}

bool nbPoly_MakeFromHull( nbPoly* poly, const b3HullData* hull, b3Transform transform, uint8_t material )
{
	if ( hull == NULL || hull->vertexCount > NB_POLY_MAX_VERTICES || hull->faceCount > NB_POLY_MAX_FACES )
	{
		return false;
	}

	const b3Vec3* points = b3GetHullPoints( hull );
	const b3Plane* planes = b3GetHullPlanes( hull );
	const b3HullFace* faces = b3GetHullFaces( hull );
	const b3HullHalfEdge* edges = b3GetHullEdges( hull );

	for ( int i = 0; i < hull->vertexCount; ++i )
	{
		poly->vertices[i] = b3TransformPoint( transform, points[i] );
	}
	poly->vertexCount = hull->vertexCount;

	int indexCount = 0;
	for ( int f = 0; f < hull->faceCount; ++f )
	{
		nbPolyFace* face = poly->faces + f;
		b3Vec3 n = b3RotateVector( transform.q, planes[f].normal );
		face->first = (uint16_t)indexCount;
		face->material = material;
		face->tag = -1;

		int first = faces[f].edge;
		int edge = first;
		int count = 0;
		do
		{
			if ( indexCount >= NB_POLY_MAX_INDICES || count > 255 )
			{
				return false;
			}
			poly->indices[indexCount++] = edges[edge].origin;
			count += 1;
			edge = edges[edge].next;
		}
		while ( edge != first );

		face->count = (uint8_t)count;
		face->plane.normal = n;
		face->plane.offset = b3Dot( n, poly->vertices[poly->indices[face->first]] );
	}

	poly->faceCount = hull->faceCount;
	poly->indexCount = indexCount;
	return true;
}

void nbPoly_Translate( nbPoly* poly, b3Vec3 translation )
{
	for ( int i = 0; i < poly->vertexCount; ++i )
	{
		poly->vertices[i] = b3Add( poly->vertices[i], translation );
	}

	for ( int i = 0; i < poly->faceCount; ++i )
	{
		b3Plane* plane = &poly->faces[i].plane;
		plane->offset += b3Dot( plane->normal, translation );
	}
}

nbClipResult nbPoly_Clip( const nbPoly* in, b3Plane plane, uint8_t material, int32_t tag, float tolerance, nbPoly* out )
{
	NB_ASSERT( in != out );

	int vertexCount = in->vertexCount;
	float s[NB_POLY_MAX_VERTICES];
	float minS = FLT_MAX;
	float maxS = -FLT_MAX;
	for ( int i = 0; i < vertexCount; ++i )
	{
		float si = b3Dot( plane.normal, in->vertices[i] ) - plane.offset;
		s[i] = si;
		minS = si < minS ? si : minS;
		maxS = si > maxS ? si : maxS;
	}

	if ( maxS <= tolerance )
	{
		return nb_clipUnchanged;
	}

	if ( minS >= -tolerance )
	{
		return nb_clipEmpty;
	}

	// Push the plane outward until no vertex is within the tolerance. This removes every degenerate
	// case (vertices, edges or faces in the plane) for the price of a sub-tolerance change of the cut.
	float shift = 0.0f;
	for ( int iteration = 0; iteration < 8; ++iteration )
	{
		bool isClose = false;
		for ( int i = 0; i < vertexCount; ++i )
		{
			if ( b3AbsFloat( s[i] - shift ) < tolerance )
			{
				isClose = true;
				break;
			}
		}

		if ( isClose == false )
		{
			break;
		}

		shift += 2.0f * tolerance;
	}

	int insideCount = 0;
	for ( int i = 0; i < vertexCount; ++i )
	{
		float si = s[i] - shift;
		if ( b3AbsFloat( si ) < tolerance )
		{
			si = -tolerance;
		}
		s[i] = si;
		insideCount += si < 0.0f ? 1 : 0;
	}

	if ( insideCount == vertexCount )
	{
		return nb_clipUnchanged;
	}

	if ( insideCount == 0 )
	{
		return nb_clipEmpty;
	}

	int map[NB_POLY_MAX_VERTICES];
	int outVertexCount = 0;
	for ( int i = 0; i < vertexCount; ++i )
	{
		if ( s[i] < 0.0f )
		{
			map[i] = outVertexCount;
			out->vertices[outVertexCount] = in->vertices[i];
			outVertexCount += 1;
		}
		else
		{
			map[i] = -1;
		}
	}

	// Edges crossing the plane, keyed by their sorted end points so both faces sharing
	// the edge get the same new vertex.
	uint8_t crossLower[NB_POLY_MAX_FACES];
	uint8_t crossUpper[NB_POLY_MAX_FACES];
	uint8_t crossVertex[NB_POLY_MAX_FACES];
	int crossCount = 0;

	// Next vertex along the cap loop, indexed by new vertex index.
	int capNext[NB_POLY_MAX_VERTICES];

	int faceCount = 0;
	int indexCount = 0;

	for ( int f = 0; f < in->faceCount; ++f )
	{
		const nbPolyFace* face = in->faces + f;
		const uint8_t* loop = in->indices + face->first;
		int count = face->count;

		int inCount = 0;
		for ( int k = 0; k < count; ++k )
		{
			inCount += s[loop[k]] < 0.0f ? 1 : 0;
		}

		if ( inCount == 0 )
		{
			continue;
		}

		if ( faceCount >= NB_POLY_MAX_FACES - 1 )
		{
			return nb_clipOverflow;
		}

		nbPolyFace* outFace = out->faces + faceCount;
		*outFace = *face;
		outFace->first = (uint16_t)indexCount;

		if ( inCount == count )
		{
			if ( indexCount + count > NB_POLY_MAX_INDICES )
			{
				return nb_clipOverflow;
			}

			for ( int k = 0; k < count; ++k )
			{
				out->indices[indexCount++] = (uint8_t)map[loop[k]];
			}

			faceCount += 1;
			continue;
		}

		int entry = -1;
		int exit = -1;
		int crossings = 0;
		int start = indexCount;

		for ( int k = 0; k < count; ++k )
		{
			int a = loop[k];
			int b = loop[k + 1 == count ? 0 : k + 1];
			bool aIn = s[a] < 0.0f;
			bool bIn = s[b] < 0.0f;

			if ( aIn )
			{
				if ( indexCount >= NB_POLY_MAX_INDICES )
				{
					return nb_clipOverflow;
				}
				out->indices[indexCount++] = (uint8_t)map[a];
			}

			if ( aIn == bIn )
			{
				continue;
			}

			int lower = a < b ? a : b;
			int upper = a < b ? b : a;
			int vertex = -1;
			for ( int c = 0; c < crossCount; ++c )
			{
				if ( crossLower[c] == lower && crossUpper[c] == upper )
				{
					vertex = crossVertex[c];
					break;
				}
			}

			if ( vertex < 0 )
			{
				if ( outVertexCount >= NB_POLY_MAX_VERTICES || crossCount >= NB_POLY_MAX_FACES )
				{
					return nb_clipOverflow;
				}

				// Interpolate from the lower index so the result does not depend on the edge direction.
				float t = s[lower] / ( s[lower] - s[upper] );
				b3Vec3 p1 = in->vertices[lower];
				b3Vec3 p2 = in->vertices[upper];
				vertex = outVertexCount;
				out->vertices[vertex] = b3MulAdd( p1, t, b3Sub( p2, p1 ) );
				capNext[vertex] = -1;
				outVertexCount += 1;

				crossLower[crossCount] = (uint8_t)lower;
				crossUpper[crossCount] = (uint8_t)upper;
				crossVertex[crossCount] = (uint8_t)vertex;
				crossCount += 1;
			}

			if ( indexCount >= NB_POLY_MAX_INDICES )
			{
				return nb_clipOverflow;
			}
			out->indices[indexCount++] = (uint8_t)vertex;
			crossings += 1;

			if ( aIn )
			{
				exit = vertex;
			}
			else
			{
				entry = vertex;
			}
		}

		// A convex face is crossed exactly twice. Anything else means the face lost planarity.
		if ( crossings != 2 )
		{
			return nb_clipOverflow;
		}

		outFace->count = (uint8_t)( indexCount - start );

		// The face got the edge exit -> entry in the cut plane, so the cap gets the twin entry -> exit.
		capNext[entry] = exit;
		faceCount += 1;
	}

	// Walk the cap loop
	int capFirst = indexCount;
	int startVertex = crossVertex[0];
	int vertex = startVertex;
	int capCount = 0;
	do
	{
		if ( vertex < 0 || capCount >= crossCount || indexCount >= NB_POLY_MAX_INDICES )
		{
			return nb_clipOverflow;
		}
		out->indices[indexCount++] = (uint8_t)vertex;
		capCount += 1;
		vertex = capNext[vertex];
	}
	while ( vertex != startVertex );

	if ( capCount != crossCount || capCount < 3 )
	{
		return nb_clipOverflow;
	}

	nbPolyFace* cap = out->faces + faceCount;
	cap->plane.normal = plane.normal;
	cap->plane.offset = plane.offset + shift;
	cap->first = (uint16_t)capFirst;
	cap->count = (uint8_t)capCount;
	cap->material = material;
	cap->tag = tag;
	faceCount += 1;

	out->vertexCount = outVertexCount;
	out->faceCount = faceCount;
	out->indexCount = indexCount;
	return nb_clipCut;
}

static b3Vec3 nbPoly_AveragePoint( const nbPoly* poly )
{
	b3Vec3 sum = b3Vec3_zero;
	for ( int i = 0; i < poly->vertexCount; ++i )
	{
		sum = b3Add( sum, poly->vertices[i] );
	}
	return b3MulSV( 1.0f / (float)poly->vertexCount, sum );
}

void nbPoly_ComputeMass( const nbPoly* poly, float* volume, b3Vec3* centroid )
{
	// Sum signed tetrahedra from an interior reference point. The reference is the vertex average,
	// which keeps the products small and the result accurate far from the origin.
	b3Vec3 origin = nbPoly_AveragePoint( poly );
	float sixVolume = 0.0f;
	b3Vec3 weighted = b3Vec3_zero;

	for ( int f = 0; f < poly->faceCount; ++f )
	{
		const nbPolyFace* face = poly->faces + f;
		const uint8_t* loop = poly->indices + face->first;
		b3Vec3 a = b3Sub( poly->vertices[loop[0]], origin );
		for ( int k = 1; k + 1 < face->count; ++k )
		{
			b3Vec3 b = b3Sub( poly->vertices[loop[k]], origin );
			b3Vec3 c = b3Sub( poly->vertices[loop[k + 1]], origin );
			float v = b3Dot( a, b3Cross( b, c ) );
			sixVolume += v;
			weighted = b3MulAdd( weighted, v, b3Add( a, b3Add( b, c ) ) );
		}
	}

	*volume = sixVolume / 6.0f;
	if ( sixVolume > 0.0f )
	{
		*centroid = b3MulAdd( origin, 0.25f / sixVolume, weighted );
	}
	else
	{
		*centroid = origin;
	}
}

float nbSecondMomentAlong( const float inertia[6], b3Vec3 d )
{
	return inertia[0] * d.x * d.x + inertia[1] * d.y * d.y + inertia[2] * d.z * d.z +
		   2.0f * ( inertia[3] * d.x * d.y + inertia[4] * d.x * d.z + inertia[5] * d.y * d.z );
}

// Second moment of a triangle relative to the origin of its vertex coordinates: area / 12 times the sum of
// the vertex outer products plus the outer product of the vertex sum
static void nbAddTriangleInertia( float inertia[6], float area, b3Vec3 p0, b3Vec3 p1, b3Vec3 p2 )
{
	b3Vec3 s = b3Add( b3Add( p0, p1 ), p2 );
	float k = area / 12.0f;
	inertia[0] += k * ( p0.x * p0.x + p1.x * p1.x + p2.x * p2.x + s.x * s.x );
	inertia[1] += k * ( p0.y * p0.y + p1.y * p1.y + p2.y * p2.y + s.y * s.y );
	inertia[2] += k * ( p0.z * p0.z + p1.z * p1.z + p2.z * p2.z + s.z * s.z );
	inertia[3] += k * ( p0.x * p0.y + p1.x * p1.y + p2.x * p2.y + s.x * s.y );
	inertia[4] += k * ( p0.x * p0.z + p1.x * p1.z + p2.x * p2.z + s.x * s.z );
	inertia[5] += k * ( p0.y * p0.z + p1.y * p1.z + p2.y * p2.z + s.y * s.z );
}

// Parallel axis theorem: add scale * d d^T
static void nbAddOuterProduct( float inertia[6], float scale, b3Vec3 d )
{
	inertia[0] += scale * d.x * d.x;
	inertia[1] += scale * d.y * d.y;
	inertia[2] += scale * d.z * d.z;
	inertia[3] += scale * d.x * d.y;
	inertia[4] += scale * d.x * d.z;
	inertia[5] += scale * d.y * d.z;
}

float nbPoly_FaceGeometry( const nbPoly* poly, int faceIndex, nbBondGeometry* geometry )
{
	const nbPolyFace* face = poly->faces + faceIndex;
	const uint8_t* loop = poly->indices + face->first;
	b3Vec3 a = poly->vertices[loop[0]];
	b3Vec3 n = face->plane.normal;
	float twiceArea = 0.0f;
	b3Vec3 weighted = b3Vec3_zero;
	float inertia[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	for ( int k = 1; k + 1 < face->count; ++k )
	{
		b3Vec3 b = b3Sub( poly->vertices[loop[k]], a );
		b3Vec3 c = b3Sub( poly->vertices[loop[k + 1]], a );
		float area = b3Dot( n, b3Cross( b, c ) );
		twiceArea += area;
		weighted = b3MulAdd( weighted, area, b3Add( b, c ) );
		nbAddTriangleInertia( inertia, 0.5f * area, b3Vec3_zero, b, c );
	}

	geometry->normal = n;
	geometry->area = 0.5f * twiceArea;
	if ( twiceArea > 0.0f )
	{
		b3Vec3 offset = b3MulSV( 1.0f / ( 3.0f * twiceArea ), weighted );
		geometry->centroid = b3Add( a, offset );
		nbAddOuterProduct( inertia, -geometry->area, offset );
	}
	else
	{
		geometry->centroid = a;
	}
	memcpy( geometry->inertia, inertia, sizeof( inertia ) );

	return geometry->area;
}

b3AABB nbPoly_ComputeBounds( const nbPoly* poly )
{
	b3AABB box = { poly->vertices[0], poly->vertices[0] };
	for ( int i = 1; i < poly->vertexCount; ++i )
	{
		box.lowerBound = b3Min( box.lowerBound, poly->vertices[i] );
		box.upperBound = b3Max( box.upperBound, poly->vertices[i] );
	}
	return box;
}

bool nbPoly_ContainsPoint( const nbPoly* poly, b3Vec3 point, float margin )
{
	for ( int f = 0; f < poly->faceCount; ++f )
	{
		const b3Plane* plane = &poly->faces[f].plane;
		if ( b3Dot( plane->normal, point ) - plane->offset > -margin )
		{
			return false;
		}
	}
	return true;
}

float nbPoly_MaxDistanceSquared( const nbPoly* poly, b3Vec3 point )
{
	float maxDistanceSquared = 0.0f;
	for ( int i = 0; i < poly->vertexCount; ++i )
	{
		float distanceSquared = b3DistanceSquared( poly->vertices[i], point );
		maxDistanceSquared = distanceSquared > maxDistanceSquared ? distanceSquared : maxDistanceSquared;
	}
	return maxDistanceSquared;
}

bool nbPoly_IsValid( const nbPoly* poly, float tolerance )
{
	if ( poly->vertexCount < 4 || poly->faceCount < 4 )
	{
		return false;
	}

	int used[NB_POLY_MAX_VERTICES] = { 0 };
	int edgeCount = 0;

	for ( int f = 0; f < poly->faceCount; ++f )
	{
		const nbPolyFace* face = poly->faces + f;
		if ( face->count < 3 || face->first + face->count > poly->indexCount )
		{
			return false;
		}

		for ( int k = 0; k < face->count; ++k )
		{
			int a = poly->indices[face->first + k];
			int b = poly->indices[face->first + ( k + 1 ) % face->count];
			if ( a >= poly->vertexCount || a == b )
			{
				return false;
			}

			used[a] = 1;
			edgeCount += 1;

			// The vertex must lie in the face plane.
			float distance = b3Dot( face->plane.normal, poly->vertices[a] ) - face->plane.offset;
			if ( b3AbsFloat( distance ) > tolerance )
			{
				return false;
			}

			// The twin edge b -> a must exist exactly once in another face.
			int twinCount = 0;
			for ( int g = 0; g < poly->faceCount; ++g )
			{
				const nbPolyFace* other = poly->faces + g;
				for ( int j = 0; j < other->count; ++j )
				{
					int c = poly->indices[other->first + j];
					int d = poly->indices[other->first + ( j + 1 ) % other->count];
					if ( c == b && d == a )
					{
						twinCount += 1;
					}
				}
			}

			if ( twinCount != 1 )
			{
				return false;
			}
		}
	}

	for ( int i = 0; i < poly->vertexCount; ++i )
	{
		if ( used[i] == 0 )
		{
			return false;
		}

		// Convexity: every vertex is behind every plane.
		for ( int f = 0; f < poly->faceCount; ++f )
		{
			const b3Plane* plane = &poly->faces[f].plane;
			if ( b3Dot( plane->normal, poly->vertices[i] ) - plane->offset > tolerance )
			{
				return false;
			}
		}
	}

	// Euler characteristic of a closed genus zero surface
	int fullEdgeCount = edgeCount / 2;
	return poly->vertexCount - fullEdgeCount + poly->faceCount == 2;
}

nbShape* nbShape_Create( const nbPoly* poly )
{
	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( poly, &volume, &centroid );
	return nbShape_CreateWithMass( poly, volume, centroid );
}

nbShape* nbShape_CreateWithMass( const nbPoly* poly, float volume, b3Vec3 centroid )
{
	if ( poly->vertexCount < 4 || poly->faceCount < 4 || volume <= 0.0f )
	{
		return NULL;
	}

	size_t headerSize = ( sizeof( nbShape ) + 15 ) & ~(size_t)15;
	size_t vertexSize = ( sizeof( b3Vec3 ) * (size_t)poly->vertexCount + 15 ) & ~(size_t)15;
	size_t faceSize = ( sizeof( nbFace ) * (size_t)poly->faceCount + 15 ) & ~(size_t)15;
	size_t indexSize = (size_t)poly->indexCount;
	size_t byteCount = headerSize + vertexSize + faceSize + indexSize;

	uint8_t* memory = nbAlloc( byteCount );
	nbShape* shape = (nbShape*)memory;
	shape->vertices = (b3Vec3*)( memory + headerSize );
	shape->faces = (nbFace*)( memory + headerSize + vertexSize );
	shape->indices = memory + headerSize + vertexSize + faceSize;
	shape->vertexCount = poly->vertexCount;
	shape->faceCount = poly->faceCount;
	shape->indexCount = poly->indexCount;
	shape->byteCount = (int)byteCount;

	memcpy( shape->vertices, poly->vertices, sizeof( b3Vec3 ) * (size_t)poly->vertexCount );
	memcpy( shape->indices, poly->indices, (size_t)poly->indexCount );
	for ( int f = 0; f < poly->faceCount; ++f )
	{
		const nbPolyFace* src = poly->faces + f;
		nbFace* dst = shape->faces + f;
		dst->plane = src->plane;
		dst->firstIndex = src->first;
		dst->indexCount = src->count;
		dst->material = src->material;
	}

	shape->bounds = nbPoly_ComputeBounds( poly );
	shape->centroid = centroid;
	shape->volume = volume;
	shape->radius = sqrtf( nbPoly_MaxDistanceSquared( poly, centroid ) );
	return shape;
}

void nbShape_Destroy( nbShape* shape )
{
	if ( shape != NULL )
	{
		nbFree( shape, (size_t)shape->byteCount );
	}
}

void nbShape_ToPoly( const nbShape* shape, nbPoly* poly )
{
	poly->vertexCount = shape->vertexCount;
	poly->faceCount = shape->faceCount;
	poly->indexCount = shape->indexCount;
	memcpy( poly->vertices, shape->vertices, sizeof( b3Vec3 ) * (size_t)shape->vertexCount );
	memcpy( poly->indices, shape->indices, (size_t)shape->indexCount );
	for ( int f = 0; f < shape->faceCount; ++f )
	{
		const nbFace* src = shape->faces + f;
		nbPolyFace* dst = poly->faces + f;
		dst->plane = src->plane;
		dst->first = src->firstIndex;
		dst->count = src->indexCount;
		dst->material = src->material;
		dst->tag = -1 - f;
	}
}

void nbShape_Translate( nbShape* shape, b3Vec3 translation )
{
	for ( int i = 0; i < shape->vertexCount; ++i )
	{
		shape->vertices[i] = b3Add( shape->vertices[i], translation );
	}

	for ( int i = 0; i < shape->faceCount; ++i )
	{
		b3Plane* plane = &shape->faces[i].plane;
		plane->offset += b3Dot( plane->normal, translation );
	}

	shape->bounds.lowerBound = b3Add( shape->bounds.lowerBound, translation );
	shape->bounds.upperBound = b3Add( shape->bounds.upperBound, translation );
	shape->centroid = b3Add( shape->centroid, translation );
}

nbGeometry nbShape_GetGeometry( const nbShape* shape )
{
	nbGeometry geometry = {
		.vertices = shape->vertices,
		.faces = shape->faces,
		.indices = shape->indices,
		.vertexCount = shape->vertexCount,
		.faceCount = shape->faceCount,
		.indexCount = shape->indexCount,
	};
	return geometry;
}

float nbShape_Distance( const nbShape* shape, b3Vec3 point )
{
	float distance = -FLT_MAX;
	for ( int f = 0; f < shape->faceCount; ++f )
	{
		const b3Plane* plane = &shape->faces[f].plane;
		float d = b3Dot( plane->normal, point ) - plane->offset;
		distance = d > distance ? d : distance;
	}
	return distance;
}

// Signed area and centroid of a 2D polygon
static float nbPolygonArea2D( const b3Vec2* points, int count, b3Vec2* centroid )
{
	float twiceArea = 0.0f;
	float cx = 0.0f;
	float cy = 0.0f;
	b3Vec2 origin = points[0];
	for ( int i = 1; i + 1 < count; ++i )
	{
		float ax = points[i].x - origin.x;
		float ay = points[i].y - origin.y;
		float bx = points[i + 1].x - origin.x;
		float by = points[i + 1].y - origin.y;
		float cross = ax * by - ay * bx;
		twiceArea += cross;
		cx += cross * ( ax + bx );
		cy += cross * ( ay + by );
	}

	if ( twiceArea != 0.0f )
	{
		centroid->x = origin.x + cx / ( 3.0f * twiceArea );
		centroid->y = origin.y + cy / ( 3.0f * twiceArea );
	}
	else
	{
		*centroid = origin;
	}

	return 0.5f * twiceArea;
}

#define NB_MAX_CLIP_POINTS 512

// Clip a convex polygon by the left side of the directed line a -> b.
static int nbClipPolygon2D( const b3Vec2* in, int count, b3Vec2 a, b3Vec2 b, b3Vec2* out )
{
	b3Vec2 e = { b.x - a.x, b.y - a.y };
	int outCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		b3Vec2 p = in[i];
		b3Vec2 q = in[i + 1 == count ? 0 : i + 1];
		float sp = e.x * ( p.y - a.y ) - e.y * ( p.x - a.x );
		float sq = e.x * ( q.y - a.y ) - e.y * ( q.x - a.x );

		if ( sp >= 0.0f )
		{
			out[outCount++] = p;
		}

		if ( ( sp >= 0.0f ) != ( sq >= 0.0f ) && outCount < NB_MAX_CLIP_POINTS )
		{
			float t = sp / ( sp - sq );
			out[outCount++] = (b3Vec2){ p.x + t * ( q.x - p.x ), p.y + t * ( q.y - p.y ) };
		}

		if ( outCount >= NB_MAX_CLIP_POINTS - 1 )
		{
			break;
		}
	}
	return outCount;
}

float nbShape_FaceOverlap( const nbShape* a, int faceIndexA, const nbShape* b, int faceIndexB, nbBondGeometry* geometry )
{
	const nbFace* faceA = a->faces + faceIndexA;
	const nbFace* faceB = b->faces + faceIndexB;
	b3Vec3 n = faceA->plane.normal;

	// Project both faces onto the plane of face A. Face B winds the other way, so reverse it.
	b3Vec2 buffer1[NB_MAX_CLIP_POINTS];
	b3Vec2 buffer2[NB_MAX_CLIP_POINTS];
	b3Vec2 clipper[256];

	b3Vec3 origin = a->vertices[a->indices[faceA->firstIndex]];
	b3Vec3 u = b3Perp( n );
	b3Vec3 v = b3Cross( n, u );

	int countA = faceA->indexCount;
	for ( int k = 0; k < countA; ++k )
	{
		b3Vec3 d = b3Sub( a->vertices[a->indices[faceA->firstIndex + k]], origin );
		buffer1[k] = (b3Vec2){ b3Dot( d, u ), b3Dot( d, v ) };
	}

	int countB = faceB->indexCount;
	for ( int k = 0; k < countB; ++k )
	{
		b3Vec3 d = b3Sub( b->vertices[b->indices[faceB->firstIndex + countB - 1 - k]], origin );
		clipper[k] = (b3Vec2){ b3Dot( d, u ), b3Dot( d, v ) };
	}

	b3Vec2* input = buffer1;
	b3Vec2* output = buffer2;
	int count = countA;
	for ( int k = 0; k < countB && count >= 3; ++k )
	{
		count = nbClipPolygon2D( input, count, clipper[k], clipper[k + 1 == countB ? 0 : k + 1], output );
		b3Vec2* swap = input;
		input = output;
		output = swap;
	}

	if ( count < 3 )
	{
		return 0.0f;
	}

	b3Vec2 c2;
	float area = nbPolygonArea2D( input, count, &c2 );
	if ( area <= 0.0f )
	{
		return 0.0f;
	}

	// Second moment in the plane about the centroid, then lifted into 3D
	float ixx = 0.0f, iyy = 0.0f, ixy = 0.0f;
	for ( int k = 1; k + 1 < count; ++k )
	{
		b3Vec2 p0 = { input[0].x - c2.x, input[0].y - c2.y };
		b3Vec2 p1 = { input[k].x - c2.x, input[k].y - c2.y };
		b3Vec2 p2 = { input[k + 1].x - c2.x, input[k + 1].y - c2.y };
		float triangleArea = 0.5f * ( ( p1.x - p0.x ) * ( p2.y - p0.y ) - ( p1.y - p0.y ) * ( p2.x - p0.x ) );
		float sx = p0.x + p1.x + p2.x;
		float sy = p0.y + p1.y + p2.y;
		float scale = triangleArea / 12.0f;
		ixx += scale * ( p0.x * p0.x + p1.x * p1.x + p2.x * p2.x + sx * sx );
		iyy += scale * ( p0.y * p0.y + p1.y * p1.y + p2.y * p2.y + sy * sy );
		ixy += scale * ( p0.x * p0.y + p1.x * p1.y + p2.x * p2.y + sx * sy );
	}

	geometry->centroid = b3MulAdd( b3MulAdd( origin, c2.x, u ), c2.y, v );
	geometry->normal = n;
	geometry->area = area;
	for ( int k = 0; k < 6; ++k )
	{
		geometry->inertia[k] = 0.0f;
	}
	nbAddOuterProduct( geometry->inertia, ixx, u );
	nbAddOuterProduct( geometry->inertia, iyy, v );
	nbAddOuterProduct( geometry->inertia, ixy, b3Add( u, v ) );
	nbAddOuterProduct( geometry->inertia, -ixy, u );
	nbAddOuterProduct( geometry->inertia, -ixy, v );
	return area;
}

float nbShape_ContactArea( const nbShape* a, const nbShape* b, float tolerance, nbBondGeometry* geometry )
{
	b3AABB boxA = b3AABB_Inflate( a->bounds, tolerance );
	if ( b3AABB_Overlaps( boxA, b->bounds ) == false )
	{
		return 0.0f;
	}

	float totalArea = 0.0f;
	b3Vec3 weightedCentroid = b3Vec3_zero;
	b3Vec3 weightedNormal = b3Vec3_zero;

	// Patches of up to 16 face pairs, combined with the parallel axis theorem at the end
	nbBondGeometry patches[16];
	int patchCount = 0;

	for ( int fa = 0; fa < a->faceCount; ++fa )
	{
		const nbFace* faceA = a->faces + fa;
		b3Vec3 n = faceA->plane.normal;

		for ( int fb = 0; fb < b->faceCount; ++fb )
		{
			const nbFace* faceB = b->faces + fb;

			// Opposing normals within about one degree
			if ( b3Dot( n, faceB->plane.normal ) > -0.99985f )
			{
				continue;
			}

			// Coplanar: the offsets of opposing planes cancel
			if ( b3AbsFloat( faceA->plane.offset + faceB->plane.offset ) > tolerance )
			{
				continue;
			}

			nbBondGeometry patch;
			float area = nbShape_FaceOverlap( a, fa, b, fb, &patch );
			if ( area <= 0.0f )
			{
				continue;
			}

			totalArea += area;
			weightedCentroid = b3MulAdd( weightedCentroid, area, patch.centroid );
			weightedNormal = b3MulAdd( weightedNormal, area, n );
			if ( patchCount < 16 )
			{
				patches[patchCount++] = patch;
			}
		}
	}

	if ( totalArea > 0.0f )
	{
		geometry->centroid = b3MulSV( 1.0f / totalArea, weightedCentroid );
		geometry->normal = b3Normalize( weightedNormal );
		geometry->area = totalArea;
		for ( int k = 0; k < 6; ++k )
		{
			geometry->inertia[k] = 0.0f;
		}
		for ( int i = 0; i < patchCount; ++i )
		{
			for ( int k = 0; k < 6; ++k )
			{
				geometry->inertia[k] += patches[i].inertia[k];
			}
			nbAddOuterProduct( geometry->inertia, patches[i].area, b3Sub( patches[i].centroid, geometry->centroid ) );
		}
	}

	return totalArea;
}

int nbShape_GetMeshVertexCount( const nbShape* shape )
{
	int count = 0;
	for ( int f = 0; f < shape->faceCount; ++f )
	{
		count += 3 * ( shape->faces[f].indexCount - 2 );
	}
	return count;
}

static void nbBoxProject( b3Vec3 p, b3Vec3 n, float scale, float* u, float* v )
{
	b3Vec3 a = b3Abs( n );
	if ( a.x >= a.y && a.x >= a.z )
	{
		*u = ( n.x > 0.0f ? -p.z : p.z ) * scale;
		*v = p.y * scale;
	}
	else if ( a.y >= a.z )
	{
		*u = p.x * scale;
		*v = ( n.y > 0.0f ? -p.z : p.z ) * scale;
	}
	else
	{
		*u = ( n.z > 0.0f ? p.x : -p.x ) * scale;
		*v = p.y * scale;
	}
}

int nbShape_BuildMesh( const nbShape* shape, nbMeshVertex* vertices, int capacity, float uvScale )
{
	int count = 0;
	for ( int f = 0; f < shape->faceCount; ++f )
	{
		const nbFace* face = shape->faces + f;
		const uint8_t* loop = shape->indices + face->firstIndex;
		b3Vec3 n = face->plane.normal;

		for ( int k = 1; k + 1 < face->indexCount; ++k )
		{
			if ( count + 3 > capacity )
			{
				return count;
			}

			int triangle[3] = { loop[0], loop[k], loop[k + 1] };
			for ( int j = 0; j < 3; ++j )
			{
				nbMeshVertex* vertex = vertices + count++;
				vertex->position = shape->vertices[triangle[j]];
				vertex->normal = n;
				nbBoxProject( vertex->position, n, uvScale, &vertex->u, &vertex->v );
				vertex->material = face->material;
			}
		}
	}
	return count;
}
