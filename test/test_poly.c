// SPDX-License-Identifier: MIT

#include "fracture.h"
#include "hull_builder.h"
#include "poly.h"
#include "test_macros.h"

#include "box3d/collision.h"

#include <float.h>
#include <stdlib.h>

static int BoxTest( void )
{
	nbPoly poly;
	b3Transform transform = { { 1.0f, 2.0f, 3.0f }, b3MakeQuatFromAxisAngle( b3Normalize( (b3Vec3){ 1, 2, 3 } ), 0.7f ) };
	nbPoly_MakeBox( &poly, (b3Vec3){ 0.5f, 1.0f, 2.0f }, transform, 3 );

	ENSURE( nbPoly_IsValid( &poly, 1.0e-5f ) );

	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( &poly, &volume, &centroid );
	ENSURE_SMALL( volume - 8.0f, 1.0e-4f );
	ENSURE_SMALL( b3Distance( centroid, transform.p ), 1.0e-5f );

	for ( int f = 0; f < poly.faceCount; ++f )
	{
		ENSURE( poly.faces[f].material == 3 );
	}

	return 0;
}

static int ClipTest( void )
{
	nbPoly box;
	nbPoly out;
	nbPoly_MakeBox( &box, (b3Vec3){ 1.0f, 1.0f, 1.0f }, b3Transform_identity, 0 );

	// Plane through the center halves the volume
	b3Plane plane = { b3Normalize( (b3Vec3){ 1.0f, 0.3f, -0.2f } ), 0.0f };
	nbClipResult result = nbPoly_Clip( &box, plane, 7, 42, 1.0e-6f, &out );
	ENSURE( result == nb_clipCut );
	ENSURE( nbPoly_IsValid( &out, 1.0e-5f ) );

	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( &out, &volume, &centroid );
	ENSURE_SMALL( volume - 4.0f, 1.0e-4f );
	ENSURE( b3Dot( centroid, plane.normal ) < 0.0f );

	// The cap face carries the material and tag
	int capCount = 0;
	for ( int f = 0; f < out.faceCount; ++f )
	{
		if ( out.faces[f].tag == 42 )
		{
			capCount += 1;
			ENSURE( out.faces[f].material == 7 );
		}
	}
	ENSURE( capCount == 1 );

	// Missing and containing planes
	b3Plane far = { { 1.0f, 0.0f, 0.0f }, 2.0f };
	ENSURE( nbPoly_Clip( &box, far, 0, 0, 1.0e-6f, &out ) == nb_clipUnchanged );

	b3Plane away = { { 1.0f, 0.0f, 0.0f }, -2.0f };
	ENSURE( nbPoly_Clip( &box, away, 0, 0, 1.0e-6f, &out ) == nb_clipEmpty );

	// A plane in a face plane leaves the box alone
	b3Plane facePlane = { { 1.0f, 0.0f, 0.0f }, 1.0f };
	ENSURE( nbPoly_Clip( &box, facePlane, 0, 0, 1.0e-6f, &out ) == nb_clipUnchanged );

	return 0;
}

static int DegenerateClipTest( void )
{
	nbPoly box;
	nbPoly out;
	nbPoly_MakeBox( &box, (b3Vec3){ 1.0f, 1.0f, 1.0f }, b3Transform_identity, 0 );

	// The plane x = y passes exactly through four box vertices
	b3Plane diagonal = { b3Normalize( (b3Vec3){ 1.0f, -1.0f, 0.0f } ), 0.0f };
	ENSURE( nbPoly_Clip( &box, diagonal, 1, 1, 1.0e-5f, &out ) == nb_clipCut );
	ENSURE( nbPoly_IsValid( &out, 1.0e-4f ) );

	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( &out, &volume, &centroid );
	ENSURE_SMALL( volume - 4.0f, 1.0e-3f );

	// The plane through a corner edge
	b3Plane corner = { b3Normalize( (b3Vec3){ 1.0f, 1.0f, 0.0f } ), 0.0f };
	ENSURE( nbPoly_Clip( &box, corner, 1, 1, 1.0e-5f, &out ) == nb_clipCut );
	ENSURE( nbPoly_IsValid( &out, 1.0e-4f ) );

	return 0;
}

static int RandomClipTest( void )
{
	nbRandom rng = nbMakeRandom( 1234, 0 );
	nbPoly* a = malloc( sizeof( nbPoly ) );
	nbPoly* b = malloc( sizeof( nbPoly ) );

	for ( int trial = 0; trial < 200; ++trial )
	{
		nbPoly_MakeBox( a, (b3Vec3){ 1.0f, 0.5f, 0.25f }, b3Transform_identity, 0 );
		float previousVolume = 1.0f;

		for ( int i = 0; i < 40; ++i )
		{
			b3Vec3 n = nbRandomUnitVector( &rng );
			b3Vec3 p = { nbRandomRange( &rng, -0.5f, 0.5f ), nbRandomRange( &rng, -0.2f, 0.2f ),
						 nbRandomRange( &rng, -0.1f, 0.1f ) };

			// Keep the side containing the origin so the polyhedron does not vanish
			float offset = b3Dot( n, p );
			if ( offset < 0.0f )
			{
				n = b3Neg( n );
				offset = -offset;
			}

			nbClipResult result = nbPoly_Clip( a, (b3Plane){ n, offset }, 1, i, 1.0e-6f, b );
			ENSURE( result != nb_clipOverflow );
			ENSURE( result != nb_clipEmpty );
			if ( result == nb_clipCut )
			{
				nbPoly* swap = a;
				a = b;
				b = swap;
				ENSURE( nbPoly_IsValid( a, 1.0e-4f ) );

				float volume;
				b3Vec3 centroid;
				nbPoly_ComputeMass( a, &volume, &centroid );
				ENSURE( volume <= previousVolume + 1.0e-5f );
				ENSURE( volume > 0.0f );
				previousVolume = volume;
			}
		}
	}

	free( a );
	free( b );
	return 0;
}

static int HullTest( void )
{
	nbRandom rng = nbMakeRandom( 99, 0 );
	b3Vec3 points[64];
	for ( int i = 0; i < 64; ++i )
	{
		points[i] = b3MulSV( nbRandomRange( &rng, 0.5f, 1.0f ), nbRandomUnitVector( &rng ) );
	}

	b3HullData* hull = b3CreateHull( points, 64, B3_MAX_HULL_VERTICES );
	ENSURE( hull != NULL );

	nbPoly poly;
	ENSURE( nbPoly_MakeFromHull( &poly, hull, b3Transform_identity, 0 ) );
	ENSURE( nbPoly_IsValid( &poly, 1.0e-4f ) );

	float volume;
	b3Vec3 centroid;
	nbPoly_ComputeMass( &poly, &volume, &centroid );
	ENSURE_SMALL( volume - hull->volume, 1.0e-3f * hull->volume );

	b3DestroyHull( hull );
	return 0;
}

static int VoronoiTest( void )
{
	nbArena arena;
	nbArena_Create( &arena, 1 << 16 );

	nbPoly* parent = malloc( sizeof( nbPoly ) );
	nbPoly_MakeBox( parent, (b3Vec3){ 2.0f, 1.0f, 0.15f }, b3Transform_identity, 0 );

	nbRandom rng = nbMakeRandom( 7, 0 );
	nbSiteParams params = {
		.center = { 0.3f, -0.2f, 0.1f },
		.radius = 0.6f,
		.innerCount = 60,
		.ringCount = 12,
		.outerCount = 10,
		.minSpacing = 0.04f,
	};

	b3Vec3 sites[256];
	int siteCount = nbGenerateSites( parent, &params, &rng, sites, 256 );
	ENSURE( siteCount > 40 );

	for ( int i = 0; i < siteCount; ++i )
	{
		ENSURE( nbPoly_ContainsPoint( parent, sites[i], 0.0f ) );
	}

	nbFractureOutput output;
	nbComputeVoronoiCells( &arena, parent, sites, siteCount, 5, 1.0e-6f, 0.0f, &output );
	ENSURE( output.failureCount == 0 );

	// The cells tile the parent
	float totalVolume = 0.0f;
	int cellCount = 0;
	nbPoly* poly = malloc( sizeof( nbPoly ) );
	for ( int i = 0; i < output.cellCount; ++i )
	{
		nbShape* shape = output.cells[i].shape;
		if ( shape == NULL )
		{
			continue;
		}

		nbShape_ToPoly( shape, poly );
		ENSURE( nbPoly_IsValid( poly, 1.0e-4f ) );
		ENSURE( nbPoly_ContainsPoint( poly, sites[i], -1.0e-4f ) );
		totalVolume += shape->volume;
		cellCount += 1;
	}

	ENSURE( cellCount == siteCount );
	ENSURE_SMALL( totalVolume - 2.4f, 1.0e-4f );

	// Neighbors are mutual and share the same face area
	int neighborChecks = 0;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		const nbCell* cell = output.cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* a = output.neighbors + cell->firstNeighbor + k;
			const nbCell* other = output.cells + a->site;
			bool found = false;
			for ( int m = 0; m < other->neighborCount; ++m )
			{
				const nbCellNeighbor* b = output.neighbors + other->firstNeighbor + m;
				if ( b->site == i )
				{
					found = true;
					ENSURE_SMALL( a->area - b->area, 1.0e-4f );
					ENSURE_SMALL( b3Distance( a->centroid, b->centroid ), 1.0e-3f );
				}
			}
			ENSURE( found );
			neighborChecks += 1;
		}
	}
	ENSURE( neighborChecks > siteCount );

	// Shape contact areas agree with the Voronoi faces
	const nbCell* cell = output.cells + 0;
	for ( int k = 0; k < cell->neighborCount; ++k )
	{
		const nbCellNeighbor* neighbor = output.neighbors + cell->firstNeighbor + k;
		b3Vec3 centroid, normal;
		float area = nbShape_ContactArea( cell->shape, output.cells[neighbor->site].shape, 1.0e-4f, &centroid, &normal );
		ENSURE_SMALL( area - neighbor->area, 1.0e-3f );
	}

	for ( int i = 0; i < output.cellCount; ++i )
	{
		nbShape_Destroy( output.cells[i].shape );
	}

	free( poly );
	free( parent );
	nbArena_Destroy( &arena );
	return 0;
}

static int ContactAreaTest( void )
{
	nbPoly a;
	nbPoly b;
	nbPoly_MakeBox( &a, (b3Vec3){ 1.0f, 1.0f, 1.0f }, b3Transform_identity, 0 );
	nbPoly_MakeBox( &b, (b3Vec3){ 0.5f, 0.5f, 0.5f }, (b3Transform){ { 1.5f, 0.75f, 0.0f }, b3Quat_identity }, 0 );

	nbShape* shapeA = nbShape_Create( &a );
	nbShape* shapeB = nbShape_Create( &b );

	b3Vec3 centroid, normal;
	float area = nbShape_ContactArea( shapeA, shapeB, 1.0e-4f, &centroid, &normal );

	// b touches the +x face of a over y in [0.25, 1], z in [-0.5, 0.5]
	ENSURE_SMALL( area - 0.75f, 1.0e-5f );
	ENSURE_SMALL( centroid.x - 1.0f, 1.0e-5f );
	ENSURE_SMALL( centroid.y - 0.625f, 1.0e-5f );
	ENSURE_SMALL( normal.x - 1.0f, 1.0e-5f );

	// Separated boxes do not touch
	nbPoly_MakeBox( &b, (b3Vec3){ 0.5f, 0.5f, 0.5f }, (b3Transform){ { 1.6f, 0.0f, 0.0f }, b3Quat_identity }, 0 );
	nbShape* shapeC = nbShape_Create( &b );
	ENSURE( nbShape_ContactArea( shapeA, shapeC, 1.0e-4f, &centroid, &normal ) == 0.0f );

	nbShape_Destroy( shapeA );
	nbShape_Destroy( shapeB );
	nbShape_Destroy( shapeC );
	return 0;
}

static int MeshTest( void )
{
	nbPoly box;
	nbPoly_MakeBox( &box, (b3Vec3){ 1.0f, 1.0f, 1.0f }, b3Transform_identity, 2 );
	nbShape* shape = nbShape_Create( &box );

	int count = nbShape_GetMeshVertexCount( shape );
	ENSURE( count == 36 );

	nbMeshVertex vertices[36];
	ENSURE( nbShape_BuildMesh( shape, vertices, 36, 1.0f ) == 36 );

	// Triangles wind counter clockwise around the face normal
	for ( int i = 0; i < 36; i += 3 )
	{
		b3Vec3 n = b3Cross( b3Sub( vertices[i + 1].position, vertices[i].position ),
							b3Sub( vertices[i + 2].position, vertices[i].position ) );
		ENSURE( b3Dot( n, vertices[i].normal ) > 0.0f );
		ENSURE( vertices[i].material == 2 );
	}

	nbShape_Destroy( shape );
	return 0;
}

static int CbrtTest( void )
{
	float values[] = { 1.0e-6f, 0.001f, 0.5f, 1.0f, 8.0f, 27.0f, 1000.0f, 12345.0f };
	for ( int i = 0; i < NB_ARRAY_COUNT( values ); ++i )
	{
		float x = values[i];
		float y = nbCbrt( x );
		ENSURE_SMALL( y * y * y / x - 1.0f, 1.0e-5f );
	}
	return 0;
}

static bool IsValidHullData( const b3HullData* hull )
{
	if ( hull->vertexCount - hull->edgeCount / 2 + hull->faceCount != 2 )
	{
		return false;
	}

	const b3HullVertex* vertices = b3GetHullVertices( hull );
	const b3HullHalfEdge* edges = b3GetHullEdges( hull );
	const b3HullFace* faces = b3GetHullFaces( hull );
	const b3Plane* planes = b3GetHullPlanes( hull );
	const b3Vec3* points = b3GetHullPoints( hull );

	for ( int i = 0; i < hull->vertexCount; ++i )
	{
		if ( edges[vertices[i].edge].origin != i )
		{
			return false;
		}
	}

	for ( int i = 0; i < hull->edgeCount; i += 2 )
	{
		if ( edges[i].twin != i + 1 || edges[i + 1].twin != i )
		{
			return false;
		}
	}

	for ( int f = 0; f < hull->faceCount; ++f )
	{
		int first = faces[f].edge;
		int index = first;
		int guard = 0;
		do
		{
			const b3HullHalfEdge* edge = edges + index;
			const b3HullHalfEdge* next = edges + edge->next;
			const b3HullHalfEdge* twin = edges + edge->twin;
			if ( edge->face != f || twin->twin != index || next->origin != twin->origin )
			{
				return false;
			}

			float separation = b3Dot( planes[f].normal, points[edge->origin] ) - planes[f].offset;
			if ( b3AbsFloat( separation ) > 1.0e-4f )
			{
				return false;
			}

			index = edge->next;
			guard += 1;
		}
		while ( index != first && guard < 256 );
	}

	return hull->volume > 0.0f && hull->surfaceArea > 0.0f && hull->innerRadius > 0.0f && hull->hash != 0;
}

static int HullBuilderTest( void )
{
	nbArena arena;
	nbArena_Create( &arena, 1 << 16 );

	nbPoly* parent = malloc( sizeof( nbPoly ) );
	nbPoly_MakeBox( parent, (b3Vec3){ 1.0f, 0.6f, 0.2f }, b3Transform_identity, 0 );

	nbRandom rng = nbMakeRandom( 3, 0 );
	nbSiteParams params = {
		.center = { 0.1f, 0.0f, 0.1f },
		.radius = 0.5f,
		.innerCount = 50,
		.ringCount = 10,
		.outerCount = 8,
		.minSpacing = 0.03f,
	};

	b3Vec3 sites[128];
	int siteCount = nbGenerateSites( parent, &params, &rng, sites, 128 );
	nbFractureOutput output;
	nbComputeVoronoiCells( &arena, parent, sites, siteCount, 1, 1.0e-6f, 0.0f, &output );

	int built = 0;
	for ( int i = 0; i < output.cellCount; ++i )
	{
		nbShape* shape = output.cells[i].shape;
		if ( shape == NULL )
		{
			continue;
		}

		int byteCount = nbGetHullByteCount( shape );
		ENSURE( byteCount > 0 );
		void* memory = malloc( (size_t)byteCount );
		b3HullData* direct = nbBuildHull( shape, memory );
		ENSURE( direct != NULL );
		ENSURE( IsValidHullData( direct ) );

		// Same mass properties as Box3D's own builder
		b3HullData* reference = b3CreateHull( shape->vertices, shape->vertexCount, B3_MAX_HULL_VERTICES );
		ENSURE( reference != NULL );
		float scale = reference->volume;
		ENSURE_SMALL( direct->volume / scale - 1.0f, 1.0e-3f );
		ENSURE_SMALL( b3Distance( direct->center, reference->center ), 1.0e-4f );
		ENSURE_SMALL( direct->surfaceArea / reference->surfaceArea - 1.0f, 1.0e-3f );
		float inertiaScale = reference->centralInertia.cx.x + reference->centralInertia.cy.y + reference->centralInertia.cz.z;
		ENSURE_SMALL( ( direct->centralInertia.cx.x - reference->centralInertia.cx.x ) / inertiaScale, 1.0e-3f );
		ENSURE_SMALL( ( direct->centralInertia.cy.z - reference->centralInertia.cy.z ) / inertiaScale, 1.0e-3f );

		// The same shape gives the same bytes
		void* memory2 = malloc( (size_t)byteCount );
		b3HullData* again = nbBuildHull( shape, memory2 );
		ENSURE( memcmp( again, direct, (size_t)byteCount ) == 0 );

		b3DestroyHull( reference );
		free( memory );
		free( memory2 );
		built += 1;
	}
	ENSURE( built == siteCount );

	for ( int i = 0; i < output.cellCount; ++i )
	{
		nbShape_Destroy( output.cells[i].shape );
	}
	free( parent );
	nbArena_Destroy( &arena );
	return 0;
}

int PolyTest( void );

int PolyTest( void )
{
	RUN_TEST( BoxTest );
	RUN_TEST( ClipTest );
	RUN_TEST( DegenerateClipTest );
	RUN_TEST( RandomClipTest );
	RUN_TEST( HullTest );
	RUN_TEST( VoronoiTest );
	RUN_TEST( ContactAreaTest );
	RUN_TEST( MeshTest );
	RUN_TEST( CbrtTest );
	RUN_TEST( HullBuilderTest );
	return 0;
}
