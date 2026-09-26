// SPDX-License-Identifier: MIT

// Load check: a static structure has to carry its own weight.
//
// Every chunk is a rigid body and every bond an elastic layer between two chunks. The layer resists
// opening, closing and sliding in proportion to its area, and bending and twisting in proportion to the
// second moments of its area. Under the weight of the chunks the structure settles by a small
// displacement and rotation of every chunk that is not anchored. That is one sparse linear system with
// six unknowns per chunk, solved exactly by a Cholesky factorization in minimum degree order. Where the
// structure is shattered, nearby fragments are grouped into rigid clusters to keep the system small.
//
// Load spreads over the bonds by stiffness like in a real structure: wide contacts carry more than
// small ones, a beam rests on both of its supports and a wall carries its load around an opening. The
// force and moment in every bond give its stress: the normal stress plus the bending stress at the edge
// of the contact, and the shear plus the torsion. Bonds stressed beyond the tensile, compressive or
// shear strength of the weaker of their two materials break, the most overloaded ones first. Detached
// parts fall and the check runs again on the next update, because the load moves on to the remaining
// bonds, until the rest of the structure holds.
//
// Chunks are rigid and never bend themselves. Large pieces need a pre-fracture (cellSize) so that
// beams and slabs can break along their span.

#include "world.h"

#include <float.h>
#include <math.h>
#include <string.h>

// Stiffness of the bond layer per area against opening and closing, in Pa/m. The forces only depend on
// the ratios, the value keeps the displacements in a sensible range.
#define NB_LOAD_STIFFNESS 1.0e10

// Stiffness against sliding relative to opening, about the ratio of shear modulus to Young's modulus
#define NB_LOAD_SHEAR_RATIO 0.4

// Friction adds to the shear strength of a bond under compression
#define NB_LOAD_FRICTION 0.6f

// Damaged bonds keep at least this fraction of their stiffness, so the system stays well posed
#define NB_LOAD_MIN_HEALTH 0.05f

// Limit on the 6x6 blocks of the factor. Structures that would need more are not checked.
#define NB_LOAD_MAX_BLOCKS 262144

// Size of the grid cells that group fragments into clusters, in fragment sizes
#define NB_LOAD_CLUSTER_SIZE 4.0f

// Clusters with unknowns a check solves at most. Larger structures are checked on a coarser grid, which
// bounds the cost of the factorization to a few milliseconds.
#define NB_LOAD_MAX_NODES 320

// Overloaded bonds break in the order of their load. Bonds loaded at least this fraction of the worst one
// break together, the others wait for the next check with the load that is left once these are gone.
#define NB_LOAD_BREAK_FRACTION 0.8f

// Bonds loaded this many times their strength break in any case. Only bonds close to their strength can
// be saved by the load moving elsewhere, and a few crushed fragments should not hold up a collapse.
#define NB_LOAD_BREAK_ALWAYS 4.0f

typedef struct nbLoadBond
{
	int bondIndex;

	// Unknowns of the clusters of the two chunks, NB_NULL_INDEX for an anchored cluster
	int node[2];

	// Interface centroid relative to the centers of mass of the two clusters
	b3Vec3 arm[2];

	// Remaining fraction of the bond strength
	float health;

	// Strength of the weaker of the two materials in tension and compression
	float tensile;
	float compressive;

	// Stiffness of the layer against the relative displacement and rotation at the interface centroid,
	// row major 3x3
	double translation[9];
	double rotation[9];
} nbLoadBond;

// Chunks grouped into rigid clusters, the bodies of the linear system
typedef struct nbLoadClusters
{
	// Cluster of every chunk in the order of the chunk list
	int* cluster;
	int count;

	// Mass and first moment of every cluster, four values each, and the center of mass
	double* mass;
	b3Vec3* center;

	// Unknown of every cluster, NB_NULL_INDEX for an anchored cluster, and the cluster of every unknown
	int* node;
	int* nodeCluster;
	int nodeCount;
} nbLoadClusters;

typedef struct nbLoadList
{
	int* items;
	int count;
	int capacity;
} nbLoadList;

typedef struct nbLoadHeap
{
	uint64_t* keys;
	int count;
	int capacity;
} nbLoadHeap;

// Lower triangular factor of the stiffness matrix in 6x6 blocks. Column v holds the blocks of the nodes
// that are eliminated after v and are coupled to it, sorted by elimination position.
typedef struct nbLoadFactor
{
	int nodeCount;
	int* order;
	int* position;
	int* columnStart;
	int* columnCount;
	int* rows;
	int blockCount;
	double* diagonal;
	double* blocks;
} nbLoadFactor;

static void nbLoadList_Push( nbArena* arena, nbLoadList* list, int item )
{
	if ( list->count == list->capacity )
	{
		int capacity = list->capacity < 4 ? 8 : 2 * list->capacity;
		int* items = nbArena_AllocArray( arena, int, capacity );
		if ( list->count > 0 )
		{
			memcpy( items, list->items, sizeof( int ) * (size_t)list->count );
		}
		list->items = items;
		list->capacity = capacity;
	}
	list->items[list->count++] = item;
}

static void nbLoadHeap_Push( nbArena* arena, nbLoadHeap* heap, uint64_t key )
{
	if ( heap->count == heap->capacity )
	{
		int capacity = heap->capacity < 32 ? 64 : 2 * heap->capacity;
		uint64_t* keys = nbArena_AllocArray( arena, uint64_t, capacity );
		if ( heap->count > 0 )
		{
			memcpy( keys, heap->keys, sizeof( uint64_t ) * (size_t)heap->count );
		}
		heap->keys = keys;
		heap->capacity = capacity;
	}

	int index = heap->count++;
	while ( index > 0 )
	{
		int parent = ( index - 1 ) / 2;
		if ( heap->keys[parent] <= key )
		{
			break;
		}
		heap->keys[index] = heap->keys[parent];
		index = parent;
	}
	heap->keys[index] = key;
}

static uint64_t nbLoadHeap_Pop( nbLoadHeap* heap )
{
	uint64_t top = heap->keys[0];
	int count = --heap->count;
	if ( count == 0 )
	{
		return top;
	}

	uint64_t key = heap->keys[count];
	int index = 0;
	for ( ;; )
	{
		int child = 2 * index + 1;
		if ( child >= count )
		{
			break;
		}
		if ( child + 1 < count && heap->keys[child + 1] < heap->keys[child] )
		{
			child += 1;
		}
		if ( key <= heap->keys[child] )
		{
			break;
		}
		heap->keys[index] = heap->keys[child];
		index = child;
	}
	heap->keys[index] = key;
	return top;
}

static int nbFindRoot( int* parent, int i )
{
	while ( parent[i] != i )
	{
		parent[i] = parent[parent[i]];
		i = parent[i];
	}
	return i;
}

// Order the unknowns by minimum degree and find the structure of the factor. Eliminating a node couples
// all of its remaining neighbors, and these neighbors are the rows of its column in the factor. Ties go
// to the lower node index, so the order is deterministic.
static bool nbLoadOrder( nbArena* arena, nbLoadFactor* factor, nbLoadList* adjacency )
{
	int m = factor->nodeCount;
	int* mark = nbArena_AllocArray( arena, int, m );
	bool* eliminated = nbArena_AllocArray( arena, bool, m );
	int** columns = nbArena_AllocArray( arena, int*, m );
	nbLoadHeap heap = { 0 };
	for ( int v = 0; v < m; ++v )
	{
		mark[v] = -1;
		eliminated[v] = false;
		nbLoadHeap_Push( arena, &heap, ( (uint64_t)adjacency[v].count << 32 ) | (uint32_t)v );
	}

	int blockCount = 0;
	int stamp = 0;
	for ( int k = 0; k < m; ++k )
	{
		int v;
		for ( ;; )
		{
			uint64_t key = nbLoadHeap_Pop( &heap );
			v = (int)( key & 0xFFFFFFFFu );
			if ( eliminated[v] == false && (int)( key >> 32 ) == adjacency[v].count )
			{
				break;
			}
		}

		factor->order[k] = v;
		factor->position[v] = k;
		eliminated[v] = true;

		nbLoadList* neighbors = adjacency + v;
		int degree = neighbors->count;
		blockCount += degree;
		if ( blockCount > NB_LOAD_MAX_BLOCKS )
		{
			return false;
		}

		columns[v] = nbArena_AllocArray( arena, int, degree > 0 ? degree : 1 );
		if ( degree > 0 )
		{
			memcpy( columns[v], neighbors->items, sizeof( int ) * (size_t)degree );
		}
		factor->columnCount[v] = degree;

		for ( int i = 0; i < degree; ++i )
		{
			int a = neighbors->items[i];
			nbLoadList* list = adjacency + a;
			stamp += 1;
			for ( int j = 0; j < list->count; ++j )
			{
				if ( list->items[j] == v )
				{
					list->items[j] = list->items[--list->count];
					break;
				}
			}

			for ( int j = 0; j < list->count; ++j )
			{
				mark[list->items[j]] = stamp;
			}
			mark[a] = stamp;

			for ( int j = 0; j < degree; ++j )
			{
				int b = neighbors->items[j];
				if ( mark[b] != stamp )
				{
					mark[b] = stamp;
					nbLoadList_Push( arena, list, b );
				}
			}

			nbLoadHeap_Push( arena, &heap, ( (uint64_t)list->count << 32 ) | (uint32_t)a );
		}
	}

	// Rows of every column sorted by elimination position
	factor->blockCount = blockCount;
	factor->rows = nbArena_AllocArray( arena, int, blockCount > 0 ? blockCount : 1 );
	int offset = 0;
	for ( int v = 0; v < m; ++v )
	{
		int count = factor->columnCount[v];
		int* rows = factor->rows + offset;
		factor->columnStart[v] = offset;
		offset += count;
		for ( int i = 0; i < count; ++i )
		{
			int row = columns[v][i];
			int j = i - 1;
			while ( j >= 0 && factor->position[rows[j]] > factor->position[row] )
			{
				rows[j + 1] = rows[j];
				j -= 1;
			}
			rows[j + 1] = row;
		}
	}

	return true;
}

// Block of row node r in the column of node c, the row has to be in the column
static int nbLoadFindBlock( const nbLoadFactor* factor, int c, int r )
{
	int low = factor->columnStart[c];
	int high = low + factor->columnCount[c] - 1;
	int target = factor->position[r];
	while ( low <= high )
	{
		int middle = ( low + high ) / 2;
		int position = factor->position[factor->rows[middle]];
		if ( position == target )
		{
			return middle;
		}
		if ( position < target )
		{
			low = middle + 1;
		}
		else
		{
			high = middle - 1;
		}
	}
	return NB_NULL_INDEX;
}

// In place Cholesky factor of a symmetric positive definite 6x6 matrix, row major. The lower triangle
// holds the factor afterwards.
static bool nbCholesky6( double* a )
{
	for ( int j = 0; j < 6; ++j )
	{
		double original = a[6 * j + j];
		double d = original;
		for ( int k = 0; k < j; ++k )
		{
			d -= a[6 * j + k] * a[6 * j + k];
		}

		// A pivot that cancels out means the structure can move without deforming a bond
		if ( !( d > 1.0e-12 * original ) )
		{
			return false;
		}

		d = sqrt( d );
		a[6 * j + j] = d;
		double inverse = 1.0 / d;
		for ( int i = j + 1; i < 6; ++i )
		{
			double s = a[6 * i + j];
			for ( int k = 0; k < j; ++k )
			{
				s -= a[6 * i + k] * a[6 * j + k];
			}
			a[6 * i + j] = s * inverse;
		}
	}
	return true;
}

// Solve L x = b in place
static void nbSolveLower6( const double* l, double* b )
{
	for ( int i = 0; i < 6; ++i )
	{
		double s = b[i];
		for ( int k = 0; k < i; ++k )
		{
			s -= l[6 * i + k] * b[k];
		}
		b[i] = s / l[6 * i + i];
	}
}

// Solve L^T x = b in place
static void nbSolveUpper6( const double* l, double* b )
{
	for ( int i = 5; i >= 0; --i )
	{
		double s = b[i];
		for ( int k = i + 1; k < 6; ++k )
		{
			s -= l[6 * k + i] * b[k];
		}
		b[i] = s / l[6 * i + i];
	}
}

// target -= p q^T with q given transposed. The sums run over k in order like a dot product, but the loop
// over j inside keeps six independent sums that vectorize.
static void nbSubtractProduct6( double* target, const double* p, const double* qTransposed )
{
	for ( int i = 0; i < 6; ++i )
	{
		double sum[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		for ( int k = 0; k < 6; ++k )
		{
			double factor = p[6 * i + k];
			const double* q = qTransposed + 6 * k;
			for ( int j = 0; j < 6; ++j )
			{
				sum[j] += factor * q[j];
			}
		}

		double* row = target + 6 * i;
		for ( int j = 0; j < 6; ++j )
		{
			row[j] -= sum[j];
		}
	}
}

// Right looking block Cholesky factorization in elimination order
static bool nbLoadFactorize( nbLoadFactor* factor )
{
	const int* rows = factor->rows;
	for ( int k = 0; k < factor->nodeCount; ++k )
	{
		int v = factor->order[k];
		double* pivot = factor->diagonal + 36 * v;
		if ( nbCholesky6( pivot ) == false )
		{
			return false;
		}

		int first = factor->columnStart[v];
		int last = first + factor->columnCount[v];
		for ( int e = first; e < last; ++e )
		{
			double* block = factor->blocks + 36 * e;
			for ( int i = 0; i < 6; ++i )
			{
				nbSolveLower6( pivot, block + 6 * i );
			}
		}

		// The rows of this column form a clique in the rest of the matrix
		for ( int e = first; e < last; ++e )
		{
			int a = rows[e];
			const double* block = factor->blocks + 36 * e;
			double left[36];
			for ( int i = 0; i < 6; ++i )
			{
				for ( int j = 0; j < 6; ++j )
				{
					left[6 * j + i] = block[6 * i + j];
				}
			}
			nbSubtractProduct6( factor->diagonal + 36 * a, block, left );

			int search = factor->columnStart[a];
			int end = search + factor->columnCount[a];
			for ( int f = e + 1; f < last; ++f )
			{
				int b = rows[f];
				while ( search < end && rows[search] != b )
				{
					search += 1;
				}
				if ( search == end )
				{
					return false;
				}
				nbSubtractProduct6( factor->blocks + 36 * search, factor->blocks + 36 * f, left );
			}
		}
	}
	return true;
}

// Solve L L^T x = b in place
static void nbLoadSolve( const nbLoadFactor* factor, double* x )
{
	int m = factor->nodeCount;
	for ( int k = 0; k < m; ++k )
	{
		int v = factor->order[k];
		double* xv = x + 6 * v;
		nbSolveLower6( factor->diagonal + 36 * v, xv );

		int first = factor->columnStart[v];
		int last = first + factor->columnCount[v];
		for ( int e = first; e < last; ++e )
		{
			const double* l = factor->blocks + 36 * e;
			double* xa = x + 6 * factor->rows[e];
			for ( int i = 0; i < 6; ++i )
			{
				double s = 0.0;
				for ( int j = 0; j < 6; ++j )
				{
					s += l[6 * i + j] * xv[j];
				}
				xa[i] -= s;
			}
		}
	}

	for ( int k = m - 1; k >= 0; --k )
	{
		int v = factor->order[k];
		double* xv = x + 6 * v;

		int first = factor->columnStart[v];
		int last = first + factor->columnCount[v];
		for ( int e = first; e < last; ++e )
		{
			const double* l = factor->blocks + 36 * e;
			const double* xa = x + 6 * factor->rows[e];
			for ( int j = 0; j < 6; ++j )
			{
				double s = 0.0;
				for ( int i = 0; i < 6; ++i )
				{
					s += l[6 * i + j] * xa[i];
				}
				xv[j] -= s;
			}
		}
		nbSolveUpper6( factor->diagonal + 36 * v, xv );
	}
}

// Cross product matrix: skew( r ) v = r x v
static void nbSkew( b3Vec3 r, double* s )
{
	s[0] = 0.0;
	s[1] = -(double)r.z;
	s[2] = (double)r.y;
	s[3] = (double)r.z;
	s[4] = 0.0;
	s[5] = -(double)r.x;
	s[6] = -(double)r.y;
	s[7] = (double)r.x;
	s[8] = 0.0;
}

static void nbMul3( const double* a, const double* b, double* c )
{
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			c[3 * i + j] = a[3 * i + 0] * b[0 + j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
		}
	}
}

// Add sign times the stiffness block B_x^T D B_y that couples chunk x and chunk y through a bond. B maps the
// displacement and rotation of a chunk to those at the interface centroid, and with R the cross product
// matrix of the arm the block is [ Dt, -Dt Ry ; Rx Dt, Dr - Rx Dt Ry ].
static void nbBondBlock( const nbLoadBond* bond, int x, int y, double sign, double* block )
{
	double rx[9], ry[9], t0[9], t1[9], t2[9];
	nbSkew( bond->arm[x], rx );
	nbSkew( bond->arm[y], ry );
	const double* dt = bond->translation;
	const double* dr = bond->rotation;

	nbMul3( dt, ry, t0 );
	nbMul3( rx, dt, t1 );
	nbMul3( t1, ry, t2 );

	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			block[6 * i + j] += sign * dt[3 * i + j];
			block[6 * i + j + 3] -= sign * t0[3 * i + j];
			block[6 * ( i + 3 ) + j] += sign * t1[3 * i + j];
			block[6 * ( i + 3 ) + j + 3] += sign * ( dr[3 * i + j] - t2[3 * i + j] );
		}
	}
}

// Section modulus of an interface for bending about an axis in its plane, taking the contact as a
// rectangle with the same second moment
static float nbBendingModulus( const nbBond* bond, b3Vec3 axis )
{
	b3Vec3 across = b3Cross( bond->normal, axis );
	float secondMoment = b3MaxFloat( nbSecondMomentAlong( bond->inertia, across ), 0.0f );
	return sqrtf( secondMoment * bond->area / 3.0f );
}

// Stiffness of the elastic layer of a bond. A damaged bond acts like a smaller contact.
static void nbComputeBondStiffness( const nbBond* bond, nbLoadBond* loadBond )
{
	double scale = b3MaxFloat( loadBond->health, NB_LOAD_MIN_HEALTH );
	double area = scale * bond->area;
	double normal[3] = { bond->normal.x, bond->normal.y, bond->normal.z };
	double kn = NB_LOAD_STIFFNESS;
	double kt = NB_LOAD_SHEAR_RATIO * NB_LOAD_STIFFNESS;

	// Planar second moment I of the interface. Bending uses N I N^T with N the cross product matrix of the
	// normal, twisting the polar moment.
	const float* in = bond->inertia;
	double inertia[9] = {
		scale * in[0], scale * in[3], scale * in[4], scale * in[3], scale * in[1],
		scale * in[5], scale * in[4], scale * in[5], scale * in[2],
	};
	double nm[9], nmT[9], t0[9], bending[9];
	nbSkew( bond->normal, nm );
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			nmT[3 * i + j] = nm[3 * j + i];
		}
	}
	nbMul3( nm, inertia, t0 );
	nbMul3( t0, nmT, bending );
	double polar = inertia[0] + inertia[4] + inertia[8];

	// A contact that degenerates to a line or a point would act as a hinge. A tiny stiffness keeps the system
	// solvable, the stress in such a bond still comes from its real geometry.
	double regular = 1.0e-4 * area * area;

	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			double nn = normal[i] * normal[j];
			double identity = i == j ? 1.0 : 0.0;
			loadBond->translation[3 * i + j] = area * ( kt * identity + ( kn - kt ) * nn );
			loadBond->rotation[3 * i + j] = kn * ( bending[3 * i + j] + regular * ( identity - nn ) ) + kt * ( polar + 2.0 * regular ) * nn;
		}
	}
}

// Utilization of a bond from the relative displacement and rotation of its clusters: the largest ratio of
// stress to strength. The bond fails above one.
static float nbBondUtilization( const nbBond* bond, const nbLoadBond* loadBond, const double* solution )
{
	double delta[3] = { 0.0, 0.0, 0.0 };
	double phi[3] = { 0.0, 0.0, 0.0 };
	for ( int side = 0; side < 2; ++side )
	{
		int node = loadBond->node[side];
		if ( node == NB_NULL_INDEX )
		{
			continue;
		}

		// The interface centroid moves with the cluster by u + theta x arm
		const double* q = solution + 6 * node;
		b3Vec3 r = loadBond->arm[side];
		double sign = side == 0 ? -1.0 : 1.0;
		delta[0] += sign * ( q[0] + q[4] * r.z - q[5] * r.y );
		delta[1] += sign * ( q[1] + q[5] * r.x - q[3] * r.z );
		delta[2] += sign * ( q[2] + q[3] * r.y - q[4] * r.x );
		phi[0] += sign * q[3];
		phi[1] += sign * q[4];
		phi[2] += sign * q[5];
	}

	// Force and moment of the layer on the second chunk
	double force[3], moment[3];
	for ( int i = 0; i < 3; ++i )
	{
		const double* dt = loadBond->translation + 3 * i;
		const double* dr = loadBond->rotation + 3 * i;
		force[i] = -( dt[0] * delta[0] + dt[1] * delta[1] + dt[2] * delta[2] );
		moment[i] = -( dr[0] * phi[0] + dr[1] * phi[1] + dr[2] * phi[2] );
	}

	b3Vec3 n = bond->normal;
	b3Vec3 f = { (float)force[0], (float)force[1], (float)force[2] };
	b3Vec3 t = { (float)moment[0], (float)moment[1], (float)moment[2] };
	float area = bond->area;

	// Compression is positive: the layer pushes the second chunk away from the first
	float normalForce = b3Dot( f, n );
	float shearForce = b3Length( b3MulSub( f, normalForce, n ) );
	float torsion = b3Dot( t, n );
	b3Vec3 bending = b3MulSub( t, torsion, n );
	float bendingMoment = b3Length( bending );

	float normalStress = normalForce / area;
	float shearStress = shearForce / area;
	float bendingStress = 0.0f;
	if ( bendingMoment > 0.0f )
	{
		float modulus = nbBendingModulus( bond, b3MulSV( 1.0f / bendingMoment, bending ) );
		bendingStress = modulus > 0.0f ? bendingMoment / modulus : FLT_MAX;
	}

	float polar = b3MaxFloat( bond->inertia[0] + bond->inertia[1] + bond->inertia[2], 0.0f );
	float torsionModulus = sqrtf( polar * area / 3.0f );
	float torsionStress = torsionModulus > 0.0f ? b3AbsFloat( torsion ) / torsionModulus : 0.0f;

	// Bending adds tension on one edge of the contact and compression on the other
	float health = loadBond->health;
	if ( health <= 0.0f )
	{
		return FLT_MAX;
	}

	float tensile = loadBond->tensile;
	float tension = ( bendingStress - normalStress ) / ( health * tensile );
	float compression = ( normalStress + bendingStress ) / ( health * loadBond->compressive );
	float shear = ( shearStress + torsionStress ) / ( health * ( tensile + NB_LOAD_FRICTION * b3MaxFloat( normalStress, 0.0f ) ) );
	return b3MaxFloat( tension, b3MaxFloat( compression, shear ) );
}

// A bond of the static actor with the indices of its chunks in the chunk list
typedef struct nbLoadLink
{
	int bondIndex;
	int chunk[2];
} nbLoadLink;

// Group the chunks into clusters: chunks in the same cell of a grid that are bonded to each other
static void nbBuildClusters( nbArena* arena, const b3Vec3* centroids, const double* masses, const bool* anchoredChunks, int n,
							 const nbLoadLink* links, int linkCount, float cellSize, nbLoadClusters* clusters )
{
	float cellInverse = 1.0f / cellSize;
	int* parent = nbArena_AllocArray( arena, int, n );
	int* cell = nbArena_AllocArray( arena, int, 3 * n );
	for ( int i = 0; i < n; ++i )
	{
		b3Vec3 p = b3MulSV( cellInverse, centroids[i] );
		parent[i] = i;
		cell[3 * i + 0] = (int)floorf( p.x );
		cell[3 * i + 1] = (int)floorf( p.y );
		cell[3 * i + 2] = (int)floorf( p.z );
	}

	for ( int k = 0; k < linkCount; ++k )
	{
		int i = links[k].chunk[0];
		int j = links[k].chunk[1];
		if ( cell[3 * i] != cell[3 * j] || cell[3 * i + 1] != cell[3 * j + 1] || cell[3 * i + 2] != cell[3 * j + 2] )
		{
			continue;
		}

		// The lower index is the root, so the clusters do not depend on the order of the bonds
		int a = nbFindRoot( parent, i );
		int b = nbFindRoot( parent, j );
		if ( a < b )
		{
			parent[b] = a;
		}
		else if ( b < a )
		{
			parent[a] = b;
		}
	}

	// Clusters in chunk order with their mass and center of mass. A cluster with an anchored chunk is anchored.
	int* cluster = nbArena_AllocArray( arena, int, n );
	int clusterCount = 0;
	for ( int i = 0; i < n; ++i )
	{
		int root = nbFindRoot( parent, i );
		cluster[i] = root == i ? clusterCount++ : cluster[root];
	}

	double* mass = nbArena_AllocArray( arena, double, 4 * clusterCount );
	b3Vec3* center = nbArena_AllocArray( arena, b3Vec3, clusterCount );
	bool* anchored = nbArena_AllocArray( arena, bool, clusterCount );
	bool* linked = nbArena_AllocArray( arena, bool, clusterCount );
	memset( mass, 0, sizeof( double ) * 4 * (size_t)clusterCount );
	memset( anchored, 0, sizeof( bool ) * (size_t)clusterCount );
	memset( linked, 0, sizeof( bool ) * (size_t)clusterCount );
	for ( int i = 0; i < n; ++i )
	{
		int k = cluster[i];
		double* sums = mass + 4 * k;
		sums[0] += masses[i];
		sums[1] += masses[i] * centroids[i].x;
		sums[2] += masses[i] * centroids[i].y;
		sums[3] += masses[i] * centroids[i].z;
		anchored[k] = anchored[k] || anchoredChunks[i];
	}

	for ( int k = 0; k < linkCount; ++k )
	{
		int a = cluster[links[k].chunk[0]];
		int b = cluster[links[k].chunk[1]];
		if ( a != b )
		{
			linked[a] = true;
			linked[b] = true;
		}
	}

	// Every cluster that is not anchored gets six unknowns: displacement and rotation about its center of mass
	int* node = nbArena_AllocArray( arena, int, clusterCount );
	int* nodeCluster = nbArena_AllocArray( arena, int, clusterCount );
	int nodeCount = 0;
	for ( int k = 0; k < clusterCount; ++k )
	{
		const double* sums = mass + 4 * k;
		double inverse = sums[0] > 0.0 ? 1.0 / sums[0] : 0.0;
		center[k] = (b3Vec3){ (float)( inverse * sums[1] ), (float)( inverse * sums[2] ), (float)( inverse * sums[3] ) };
		node[k] = NB_NULL_INDEX;
		if ( anchored[k] == false && linked[k] )
		{
			node[k] = nodeCount;
			nodeCluster[nodeCount++] = k;
		}
	}

	*clusters = (nbLoadClusters){
		.cluster = cluster,
		.count = clusterCount,
		.mass = mass,
		.center = center,
		.node = node,
		.nodeCluster = nodeCluster,
		.nodeCount = nodeCount,
	};
}

void nbCheckLoads( nbWorld* world, int destructibleIndex )
{
	nbDestructible* destructible = world->destructibles.data + destructibleIndex;

	// Strengths of zero leave a material out of the check. Clusters follow the finest fragments.
	float tensiles[NB_MAX_MATERIALS];
	float compressives[NB_MAX_MATERIALS];
	float fragmentSize = FLT_MAX;
	bool anyStrength = false;
	for ( int k = 0; k < destructible->materialCount; ++k )
	{
		const nbMaterial* material = destructible->materials + k;
		tensiles[k] = material->tensileStrength > 0.0f ? material->tensileStrength : FLT_MAX;
		compressives[k] = material->compressiveStrength > 0.0f ? material->compressiveStrength : FLT_MAX;
		anyStrength = anyStrength || material->tensileStrength > 0.0f || material->compressiveStrength > 0.0f;
		fragmentSize = b3MinFloat( fragmentSize, material->fragmentSize );
	}

	if ( destructible->isStatic == false || anyStrength == false )
	{
		return;
	}

	int actorIndex = NB_NULL_INDEX;
	for ( int a = destructible->headActor; a != NB_NULL_INDEX; a = world->actors.data[a].nextActor )
	{
		if ( world->actors.data[a].isStatic )
		{
			actorIndex = a;
			break;
		}
	}

	if ( actorIndex == NB_NULL_INDEX )
	{
		return;
	}

	b3Vec3 gravityVector = b3World_GetGravity( world->physicsWorld );
	if ( b3LengthSquared( gravityVector ) <= 0.0f )
	{
		return;
	}
	b3Vec3 gravity = b3InvRotateVector( destructible->transform.q, gravityVector );

	nbBeginOperation( world );
	nbArena* arena = &world->arena;

	// The chunks and bonds of the structure in compact arrays
	const nbActor* actor = world->actors.data + actorIndex;
	int n = actor->chunkCount;
	b3Vec3* centroids = nbArena_AllocArray( arena, b3Vec3, n );
	double* masses = nbArena_AllocArray( arena, double, n );
	bool* anchoredChunks = nbArena_AllocArray( arena, bool, n );
	int bondCapacity = 0;
	int count = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->scratch = count;
		centroids[count] = chunk->shape->centroid;
		masses[count] = (double)destructible->materials[chunk->materialIndex].density * (double)chunk->shape->volume;
		anchoredChunks[count] = ( chunk->flags & nb_chunkAnchored ) != 0;
		bondCapacity += chunk->bondCount;
		count += 1;
	}

	nbLoadLink* links = nbArena_AllocArray( arena, nbLoadLink, bondCapacity / 2 + 1 );
	int linkCount = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		const nbChunk* chunk = world->chunks.data + c;
		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			const nbBond* bond = world->bonds.data + ( key >> 1 );
			int side = key & 1;
			if ( side == 0 )
			{
				links[linkCount++] = (nbLoadLink){ key >> 1, { chunk->scratch, world->chunks.data[bond->chunk[1]].scratch } };
			}
			key = bond->nextKey[side];
		}
	}

	// Chunks in the same cell of a grid a few fragments wide that are bonded to each other form a cluster, one
	// rigid body in the system. This keeps the system small where the structure is shattered, and fragments
	// that small never fail under their own weight. Bonds inside a cluster are taken as rigid. A structure
	// with too many clusters is checked on a coarser grid.
	float cellSize = b3MaxFloat( NB_LOAD_CLUSTER_SIZE * fragmentSize, 1.0e-3f );
	nbLoadClusters clusters;
	for ( ;; )
	{
		nbBuildClusters( arena, centroids, masses, anchoredChunks, n, links, linkCount, cellSize, &clusters );
		if ( clusters.nodeCount <= NB_LOAD_MAX_NODES )
		{
			break;
		}
		cellSize *= 1.5f;
	}

	int m = clusters.nodeCount;
	if ( m == 0 )
	{
		return;
	}

	const int* cluster = clusters.cluster;
	const int* clusterNode = clusters.node;
	const b3Vec3* clusterCenter = clusters.center;

	// Stiffness of every bond between two clusters of which at least one can move
	nbLoadBond* loadBonds = nbArena_AllocArray( arena, nbLoadBond, linkCount + 1 );
	int loadBondCount = 0;
	for ( int l = 0; l < linkCount; ++l )
	{
		int clusterA = cluster[links[l].chunk[0]];
		int clusterB = cluster[links[l].chunk[1]];
		if ( clusterA == clusterB || ( clusterNode[clusterA] == NB_NULL_INDEX && clusterNode[clusterB] == NB_NULL_INDEX ) )
		{
			continue;
		}

		const nbBond* bond = world->bonds.data + links[l].bondIndex;
		int materialA = world->chunks.data[bond->chunk[0]].materialIndex;
		int materialB = world->chunks.data[bond->chunk[1]].materialIndex;
		float fullHealth = nbGetBondStrength( world, bond ) * bond->area;
		nbLoadBond* loadBond = loadBonds + loadBondCount++;
		loadBond->bondIndex = links[l].bondIndex;
		loadBond->node[0] = clusterNode[clusterA];
		loadBond->node[1] = clusterNode[clusterB];
		loadBond->arm[0] = b3Sub( bond->centroid, clusterCenter[clusterA] );
		loadBond->arm[1] = b3Sub( bond->centroid, clusterCenter[clusterB] );
		loadBond->health = fullHealth > 0.0f ? b3ClampFloat( bond->health / fullHealth, 0.0f, 1.0f ) : 1.0f;
		loadBond->tensile = b3MinFloat( tensiles[materialA], tensiles[materialB] );
		loadBond->compressive = b3MinFloat( compressives[materialA], compressives[materialB] );
		nbComputeBondStiffness( bond, loadBond );
	}

	// Coupling graph of the unknowns, one edge per pair of clusters that share a bond
	nbLoadList* adjacency = nbArena_AllocArray( arena, nbLoadList, m );
	memset( adjacency, 0, sizeof( nbLoadList ) * (size_t)m );
	for ( int k = 0; k < loadBondCount; ++k )
	{
		int a = loadBonds[k].node[0];
		int b = loadBonds[k].node[1];
		if ( a == NB_NULL_INDEX || b == NB_NULL_INDEX )
		{
			continue;
		}

		bool known = false;
		for ( int j = 0; j < adjacency[a].count && known == false; ++j )
		{
			known = adjacency[a].items[j] == b;
		}
		if ( known == false )
		{
			nbLoadList_Push( arena, adjacency + a, b );
			nbLoadList_Push( arena, adjacency + b, a );
		}
	}

	nbLoadFactor factor = { 0 };
	factor.nodeCount = m;
	factor.order = nbArena_AllocArray( arena, int, m );
	factor.position = nbArena_AllocArray( arena, int, m );
	factor.columnStart = nbArena_AllocArray( arena, int, m );
	factor.columnCount = nbArena_AllocArray( arena, int, m );
	if ( nbLoadOrder( arena, &factor, adjacency ) == false )
	{
		return;
	}

	// Assemble the lower triangle in elimination order
	factor.diagonal = nbArena_AllocArray( arena, double, 36 * m );
	factor.blocks = nbArena_AllocArray( arena, double, 36 * ( factor.blockCount > 0 ? factor.blockCount : 1 ) );
	memset( factor.diagonal, 0, sizeof( double ) * 36 * (size_t)m );
	memset( factor.blocks, 0, sizeof( double ) * 36 * (size_t)factor.blockCount );
	for ( int k = 0; k < loadBondCount; ++k )
	{
		const nbLoadBond* loadBond = loadBonds + k;
		for ( int side = 0; side < 2; ++side )
		{
			int node = loadBond->node[side];
			if ( node != NB_NULL_INDEX )
			{
				nbBondBlock( loadBond, side, side, 1.0, factor.diagonal + 36 * node );
			}
		}

		int a = loadBond->node[0];
		int b = loadBond->node[1];
		if ( a != NB_NULL_INDEX && b != NB_NULL_INDEX )
		{
			// The later cluster in the elimination order is the row: -B_row^T D B_column
			int row = factor.position[a] > factor.position[b] ? 0 : 1;
			int block = nbLoadFindBlock( &factor, loadBond->node[row ^ 1], loadBond->node[row] );
			NB_ASSERT( block != NB_NULL_INDEX );
			nbBondBlock( loadBond, row, row ^ 1, -1.0, factor.blocks + 36 * block );
		}
	}

	if ( nbLoadFactorize( &factor ) == false )
	{
		return;
	}

	// Weight of every cluster at its center of mass
	double* solution = nbArena_AllocArray( arena, double, 6 * m );
	for ( int v = 0; v < m; ++v )
	{
		double mass = clusters.mass[4 * clusters.nodeCluster[v]];
		double* f = solution + 6 * v;
		f[0] = mass * gravity.x;
		f[1] = mass * gravity.y;
		f[2] = mass * gravity.z;
		f[3] = 0.0;
		f[4] = 0.0;
		f[5] = 0.0;
	}
	nbLoadSolve( &factor, solution );

	// The most overloaded bonds break first. Breaking them moves their load, so bonds that are much less
	// overloaded may hold once these are gone, like a beam that cracks at one section and not at every one.
	float* utilization = nbArena_AllocArray( arena, float, loadBondCount + 1 );
	float worst = 0.0f;
	for ( int k = 0; k < loadBondCount; ++k )
	{
		const nbLoadBond* loadBond = loadBonds + k;
		utilization[k] = nbBondUtilization( world->bonds.data + loadBond->bondIndex, loadBond, solution );
		worst = b3MaxFloat( worst, utilization[k] );
	}

	nbIntArray* broken = &world->scratchList;
	broken->count = 0;
	float threshold = b3MaxFloat( 1.0f, b3MinFloat( NB_LOAD_BREAK_FRACTION * worst, NB_LOAD_BREAK_ALWAYS ) );
	for ( int k = 0; k < loadBondCount; ++k )
	{
		if ( utilization[k] > 1.0f && utilization[k] >= threshold )
		{
			nbArray_Push( *broken, loadBonds[k].bondIndex );
		}
	}

	if ( broken->count == 0 )
	{
		return;
	}

	// Copy the broken bonds, splitting reuses the scratch list
	int brokenCount = broken->count;
	int* brokenBonds = nbArena_AllocArray( arena, int, brokenCount );
	memcpy( brokenBonds, broken->data, sizeof( int ) * (size_t)brokenCount );

	for ( int k = 0; k < brokenCount; ++k )
	{
		nbPushCrackDust( world, brokenBonds[k] );
		nbDestroyBond( world, brokenBonds[k] );
		world->stats.overloadedBondCount += 1;
	}

	nbImpactResult result = { 0 };
	nbSplitActors( world, &result );
	nbCommitPhysics( world );

	for ( int k = 0; k < world->touchedActors.count; ++k )
	{
		world->actors.data[world->touchedActors.data[k]].isNew = false;
	}
	world->touchedActors.count = 0;

	// The load moves on to the remaining bonds, check again on the next update
	world->destructibles.data[destructibleIndex].structureDirty = true;
}
