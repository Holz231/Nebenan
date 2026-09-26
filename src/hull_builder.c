// SPDX-License-Identifier: MIT

// Build Box3D hull data straight from the exact topology of a chunk. The clipper already knows
// every face loop, so running quickhull again would only rediscover what is known. The layout
// mirrors b3CreateHull: sections aligned to 8 bytes, twin half-edges stored as pairs (2k, 2k + 1),
// explicit zero padding because Box3D compares hulls byte by byte.

#include "hull_builder.h"

#include <float.h>

static size_t nbAlignUp8( size_t x )
{
	return ( x + 7u ) & ~(size_t)7u;
}

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

static nbHullLayout nbComputeHullLayout( int vertexCount, int edgeCount, int faceCount )
{
	int soaVertexCount = ( vertexCount + 3 ) & ~3;
	int soaNormalCount = ( faceCount + 3 ) & ~3;

	nbHullLayout layout;
	size_t byteCount = nbAlignUp8( sizeof( b3HullData ) );
	layout.vertexOffset = byteCount;
	byteCount += nbAlignUp8( (size_t)vertexCount * sizeof( b3HullVertex ) );
	layout.pointOffset = byteCount;
	byteCount += nbAlignUp8( (size_t)vertexCount * sizeof( b3Vec3 ) );
	layout.edgeOffset = byteCount;
	byteCount += nbAlignUp8( (size_t)edgeCount * sizeof( b3HullHalfEdge ) );
	layout.planeOffset = byteCount;
	byteCount += nbAlignUp8( (size_t)faceCount * sizeof( b3Plane ) );
	layout.faceOffset = byteCount;
	byteCount += nbAlignUp8( (size_t)faceCount * sizeof( b3HullFace ) );
	layout.soaVertexOffset = byteCount;
	byteCount += nbAlignUp8( 3 * (size_t)soaVertexCount * sizeof( float ) );
	layout.soaNormalOffset = byteCount;
	byteCount += nbAlignUp8( 3 * (size_t)soaNormalCount * sizeof( float ) );
	layout.byteCount = byteCount;
	return layout;
}

int nbGetHullByteCount( const nbShape* shape )
{
	int halfEdgeCount = shape->indexCount;
	if ( shape->vertexCount > B3_MAX_HULL_VERTICES || shape->faceCount > B3_MAX_HULL_FACES ||
		 halfEdgeCount > 2 * B3_MAX_HULL_EDGES || halfEdgeCount > 256 )
	{
		return 0;
	}

	nbHullLayout layout = nbComputeHullLayout( shape->vertexCount, halfEdgeCount, shape->faceCount );
	return (int)layout.byteCount;
}

// 64-bit hash over whole words. The byte count of a hull is a multiple of eight.
static uint64_t nbHashHullBytes( const uint8_t* bytes, size_t byteCount )
{
	uint64_t hash = 0x9E3779B97F4A7C15ull ^ (uint64_t)byteCount;
	for ( size_t i = 0; i < byteCount; i += 8 )
	{
		uint64_t word;
		memcpy( &word, bytes + i, sizeof( word ) );
		hash ^= word;
		hash *= 0xBF58476D1CE4E5B9ull;
		hash ^= hash >> 31;
	}
	hash ^= hash >> 29;
	hash *= 0x94D049BB133111EBull;
	hash ^= hash >> 32;
	return hash != 0 ? hash : 1;
}

b3HullData* nbBuildHull( const nbShape* shape, void* memory )
{
	int vertexCount = shape->vertexCount;
	int faceCount = shape->faceCount;
	int halfEdgeCount = shape->indexCount;
	NB_ASSERT( nbGetHullByteCount( shape ) > 0 );

	nbHullLayout layout = nbComputeHullLayout( vertexCount, halfEdgeCount, faceCount );
	memset( memory, 0, layout.byteCount );

	b3HullData* hull = memory;
	uint8_t* base = memory;
	hull->version = B3_HULL_VERSION;
	hull->vertexCount = vertexCount;
	hull->edgeCount = halfEdgeCount;
	hull->faceCount = faceCount;
	hull->vertexOffset = (int32_t)layout.vertexOffset;
	hull->pointOffset = (int32_t)layout.pointOffset;
	hull->edgeOffset = (int32_t)layout.edgeOffset;
	hull->planeOffset = (int32_t)layout.planeOffset;
	hull->faceOffset = (int32_t)layout.faceOffset;
	hull->soaVertexOffset = (int32_t)layout.soaVertexOffset;
	hull->soaNormalOffset = (int32_t)layout.soaNormalOffset;
	hull->byteCount = (int32_t)layout.byteCount;

	b3HullVertex* vertices = (b3HullVertex*)( base + layout.vertexOffset );
	b3Vec3* points = (b3Vec3*)( base + layout.pointOffset );
	b3HullHalfEdge* edges = (b3HullHalfEdge*)( base + layout.edgeOffset );
	b3Plane* planes = (b3Plane*)( base + layout.planeOffset );
	b3HullFace* faces = (b3HullFace*)( base + layout.faceOffset );

	// Face of each loop position and the position of the next edge in its face
	uint8_t positionFace[256];
	uint8_t positionNext[256];
	for ( int f = 0; f < faceCount; ++f )
	{
		const nbFace* face = shape->faces + f;
		int first = face->firstIndex;
		int count = face->indexCount;
		for ( int k = 0; k < count; ++k )
		{
			positionFace[first + k] = (uint8_t)f;
			positionNext[first + k] = (uint8_t)( first + ( k + 1 == count ? 0 : k + 1 ) );
		}
	}

	// Outgoing loop positions per vertex, bucketed by origin
	uint8_t outStart[B3_MAX_HULL_VERTICES + 1];
	uint8_t outPositions[256];
	uint8_t outCount[B3_MAX_HULL_VERTICES];
	memset( outCount, 0, sizeof( outCount ) );
	for ( int p = 0; p < halfEdgeCount; ++p )
	{
		outCount[shape->indices[p]] += 1;
	}

	int sum = 0;
	for ( int v = 0; v < vertexCount; ++v )
	{
		outStart[v] = (uint8_t)sum;
		sum += outCount[v];
		outCount[v] = 0;
	}
	outStart[vertexCount] = (uint8_t)sum;

	for ( int p = 0; p < halfEdgeCount; ++p )
	{
		int v = shape->indices[p];
		outPositions[outStart[v] + outCount[v]] = (uint8_t)p;
		outCount[v] += 1;
	}

	// Assign half-edge indices so that twins are adjacent, in face order like b3CreateHull
	int16_t edgeIndex[256];
	for ( int p = 0; p < halfEdgeCount; ++p )
	{
		edgeIndex[p] = -1;
	}

	int assigned = 0;
	for ( int p = 0; p < halfEdgeCount; ++p )
	{
		if ( edgeIndex[p] >= 0 )
		{
			continue;
		}

		int origin = shape->indices[p];
		int destination = shape->indices[positionNext[p]];

		// The twin starts at the destination and ends at the origin
		int twin = -1;
		for ( int k = outStart[destination]; k < outStart[destination + 1]; ++k )
		{
			int q = outPositions[k];
			if ( shape->indices[positionNext[q]] == origin )
			{
				twin = q;
				break;
			}
		}

		if ( twin < 0 || edgeIndex[twin] >= 0 )
		{
			// Not a closed two-manifold
			return NULL;
		}

		edgeIndex[p] = (int16_t)assigned;
		edgeIndex[twin] = (int16_t)( assigned + 1 );
		assigned += 2;
	}

	for ( int p = 0; p < halfEdgeCount; ++p )
	{
		int index = edgeIndex[p];
		b3HullHalfEdge* edge = edges + index;
		edge->next = (uint8_t)edgeIndex[positionNext[p]];
		edge->twin = (uint8_t)( index ^ 1 );
		edge->origin = shape->indices[p];
		edge->face = positionFace[p];
		vertices[edge->origin].edge = (uint8_t)index;
	}

	for ( int f = 0; f < faceCount; ++f )
	{
		faces[f].edge = (uint8_t)edgeIndex[shape->faces[f].firstIndex];
		planes[f] = shape->faces[f].plane;
	}

	int soaVertexCount = ( vertexCount + 3 ) & ~3;
	int soaNormalCount = ( faceCount + 3 ) & ~3;
	float* vx = (float*)( base + layout.soaVertexOffset );
	float* vy = vx + soaVertexCount;
	float* vz = vy + soaVertexCount;
	for ( int v = 0; v < soaVertexCount; ++v )
	{
		b3Vec3 p = shape->vertices[v < vertexCount ? v : 0];
		if ( v < vertexCount )
		{
			points[v] = p;
		}
		vx[v] = p.x;
		vy[v] = p.y;
		vz[v] = p.z;
	}

	float* nx = (float*)( base + layout.soaNormalOffset );
	float* ny = nx + soaNormalCount;
	float* nz = ny + soaNormalCount;
	for ( int f = 0; f < faceCount; ++f )
	{
		nx[f] = planes[f].normal.x;
		ny[f] = planes[f].normal.y;
		nz[f] = planes[f].normal.z;
	}

	// Bounds and mass properties, the same integration Box3D uses (Kallay)
	b3AABB bounds = { points[0], points[0] };
	for ( int v = 1; v < vertexCount; ++v )
	{
		bounds.lowerBound = b3Min( bounds.lowerBound, points[v] );
		bounds.upperBound = b3Max( bounds.upperBound, points[v] );
	}
	hull->aabb = bounds;

	b3Vec3 origin = points[0];
	float area = 0.0f;
	float volume = 0.0f;
	b3Vec3 center = b3Vec3_zero;
	float xx = 0.0f, xy = 0.0f, yy = 0.0f, xz = 0.0f, zz = 0.0f, yz = 0.0f;

	for ( int f = 0; f < faceCount; ++f )
	{
		const b3HullHalfEdge* edge1 = edges + faces[f].edge;
		const b3HullHalfEdge* edge2 = edges + edge1->next;
		const b3HullHalfEdge* edge3 = edges + edge2->next;
		b3Vec3 v1 = b3Sub( points[edge1->origin], origin );

		do
		{
			b3Vec3 v2 = b3Sub( points[edge2->origin], origin );
			b3Vec3 v3 = b3Sub( points[edge3->origin], origin );

			area += b3Length( b3Cross( b3Sub( v2, v1 ), b3Sub( v3, v1 ) ) );

			float det = b3Dot( v1, b3Cross( v2, v3 ) );
			volume += det;

			b3Vec3 v4 = b3Add( v1, b3Add( v2, v3 ) );
			center = b3Add( center, b3MulSV( det, v4 ) );

			xx += det * ( v1.x * v1.x + v2.x * v2.x + v3.x * v3.x + v4.x * v4.x );
			yy += det * ( v1.y * v1.y + v2.y * v2.y + v3.y * v3.y + v4.y * v4.y );
			zz += det * ( v1.z * v1.z + v2.z * v2.z + v3.z * v3.z + v4.z * v4.z );
			xy += det * ( v1.x * v1.y + v2.x * v2.y + v3.x * v3.y + v4.x * v4.y );
			xz += det * ( v1.x * v1.z + v2.x * v2.z + v3.x * v3.z + v4.x * v4.z );
			yz += det * ( v1.y * v1.z + v2.y * v2.z + v3.y * v3.z + v4.y * v4.z );

			edge2 = edge3;
			edge3 = edges + edge3->next;
		}
		while ( edge1 != edge3 );
	}

	if ( volume <= 0.0f || area <= 0.0f )
	{
		return NULL;
	}

	b3Vec3 localCenter = b3MulSV( 0.25f / volume, center );
	center = b3Add( localCenter, origin );

	float innerRadius = FLT_MAX;
	for ( int f = 0; f < faceCount; ++f )
	{
		float separation = b3Dot( planes[f].normal, center ) - planes[f].offset;
		if ( separation >= 0.0f )
		{
			return NULL;
		}
		innerRadius = b3MinFloat( innerRadius, -separation );
	}

	b3Matrix3 inertia;
	inertia.cx.x = yy + zz;
	inertia.cy.x = -xy;
	inertia.cz.x = -xz;
	inertia.cx.y = -xy;
	inertia.cy.y = xx + zz;
	inertia.cz.y = -yz;
	inertia.cx.z = -xz;
	inertia.cy.z = -yz;
	inertia.cz.z = xx + yy;

	float mass = volume / 6.0f;
	b3Matrix3 centralInertia = b3MulSM( 1.0f / 120.0f, inertia );
	centralInertia = b3SubMM( centralInertia, b3Steiner( mass, localCenter ) );

	hull->center = center;
	hull->centralInertia = centralInertia;
	hull->volume = mass;
	hull->surfaceArea = 0.5f * area;
	hull->innerRadius = innerRadius;

	hull->hash = 0;
	hull->hash = nbHashHullBytes( base, layout.byteCount );
	return hull;
}

b3HullData* nbCreateHullInArena( const nbShape* shape, nbArena* arena, int* fallbackCount )
{
	int byteCount = nbGetHullByteCount( shape );
	if ( byteCount > 0 )
	{
		void* memory = nbArena_Alloc( arena, (size_t)byteCount );
		b3HullData* hull = nbBuildHull( shape, memory );
		if ( hull != NULL )
		{
			return hull;
		}
	}

	b3HullData* heapHull = b3CreateHull( shape->vertices, shape->vertexCount, B3_MAX_HULL_VERTICES );
	if ( heapHull == NULL )
	{
		return NULL;
	}

	b3HullData* hull = nbArena_Alloc( arena, (size_t)heapHull->byteCount );
	memcpy( hull, heapHull, (size_t)heapHull->byteCount );
	b3DestroyHull( heapHull );
	*fallbackCount += 1;
	return hull;
}
