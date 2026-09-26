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
// of the contact, and the shear plus the torsion.
//
// A glued bond stressed beyond its tensile or shear strength while it is pressed together cracks open
// instead of breaking. An open joint, cracked or dry from the start, carries compression and friction
// over the part of the interface that stays in contact: the strip at the edge where the normal force
// acts, three times as deep as the distance from the force to that edge, like the triangle of pressure
// under a tilted block. The contact moves the stiffness of the joint to that edge, so the structure is
// solved again with the new contacts until they settle, a few times per check and on the next updates
// if needed. This is how an arch of loose stones stands, how a slab rests on its walls and lifts off at
// the edges, and how a wall cracks above an opening and still carries the load around it.
//
// A bond breaks when it is pulled apart beyond its strength, crushed or sheared off, or when the normal
// force of an open joint would have to act beyond the edge of the contact. There the joint turns into a
// hinge, and a structure with too many hinges is a mechanism that falls. The most overloaded bonds
// break first. Detached parts fall and the check runs again on the next update, because the load moves
// on to the remaining bonds, until the rest of the structure holds.
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

// Solves per check while the contacts of open joints move. A check whose contacts did not settle continues on
// the next update and only breaks bonds loaded NB_LOAD_BREAK_ALWAYS times their strength, until it has run
// NB_LOAD_MAX_PASSES times in a row.
#define NB_LOAD_MAX_ITERATIONS 3
#define NB_LOAD_MAX_PASSES 8

// Smallest contact of an open joint as a fraction of its depth, where the joint acts as a hinge
#define NB_LOAD_MIN_CONTACT 0.1f

// A contact that moves less than this, as a fraction of its depth and of the size of the interface, has
// settled
#define NB_LOAD_CONTACT_TOLERANCE 0.1f

// Stiffness left to a joint that is pulled apart, so the system stays solvable where it holds a part that
// nothing else supports
#define NB_LOAD_SEPARATED_STIFFNESS 1.0e-6

// A dry joint whose normal is closer to horizontal than this cosine from the vertical stands upright
#define NB_LOAD_UPRIGHT 0.5f

// Joints with a normal force below this fraction of the weight of the chunks around them carry too little to move
// the loads when they open or close
#define NB_LOAD_SIGNIFICANT 0.05f

// A sliding joint slides until it holds this fraction of its friction, the rest is its reserve
#define NB_LOAD_SLIDING_FRICTION 0.9f

// Solves per factorization while the joints slide. Sliding only changes the loads, not the stiffness.
#define NB_LOAD_SLIP_ITERATIONS 8

// Tension in Pascal an open joint is taken to hold. Joints that barely touch carry a tiny load either way,
// and it should not count as an overload, while any real part hanging from an open joint loads it far more.
#define NB_LOAD_COHESION 100.0f

typedef struct nbLoadBond
{
	int bondIndex;

	// Clusters of the two chunks and their unknowns, NB_NULL_INDEX for an anchored cluster
	int cluster[2];
	int node[2];

	// Interface centroid relative to the centers of mass of the two clusters
	b3Vec3 arm[2];

	// Unit normal of the interface from the first chunk to the second, its area and its planar second moment about
	// the centroid, copied from the bond
	b3Vec3 normal;
	float area;
	float inertia[6];

	// Weight of the lighter of the two clusters that can move. Changes of a joint that carries a small part of it
	// do not move the loads around.
	float weight;

	// Remaining fraction of the bond strength
	float health;

	// Strength of the joint in tension, zero once it is open, and of the weaker material in compression, and
	// the friction coefficient of the joint
	float tensile;
	float compressive;
	float friction;

	// nbJointState, the point of the normal force of an open joint relative to the interface centroid and how far
	// the joint slid
	uint8_t state;
	b3Vec3 eccentricity;
	b3Vec3 slip;

	// Centroid of the contact relative to the interface centroid, the whole interface unless the joint is open
	b3Vec3 offset;

	// Stiffness of the layer against the relative displacement at the contact centroid, shear I + normal n n^T,
	// and against the relative rotation, row major 3x3
	double shearStiffness;
	double normalStiffness;
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
static void nbSkew( const double* r, double* s )
{
	s[0] = 0.0;
	s[1] = -r[2];
	s[2] = r[1];
	s[3] = r[2];
	s[4] = 0.0;
	s[5] = -r[0];
	s[6] = -r[1];
	s[7] = r[0];
	s[8] = 0.0;
}

static void nbCross3( const double* a, const double* b, double* c )
{
	c[0] = a[1] * b[2] - a[2] * b[1];
	c[1] = a[2] * b[0] - a[0] * b[2];
	c[2] = a[0] * b[1] - a[1] * b[0];
}

// Add sign times the stiffness block B_x^T D B_y that couples chunk x and chunk y through a bond. B maps the
// displacement and rotation of a chunk to those at the contact centroid. With the arms rx and ry of the two
// chunks, their cross product matrices Rx and Ry and the translation stiffness Dt = a I + b n n^T the block is
// [ Dt, -Dt Ry ; Rx Dt, Dr - Rx Dt Ry ], where Dt Ry = a Ry + b n ( n x ry )^T, Rx Dt = a Rx + b ( rx x n ) n^T
// and Rx Ry = ry rx^T - ( rx . ry ) I. The factorization only reads the lower triangle of a diagonal block.
static void nbBondBlock( const nbLoadBond* loadBond, int x, int y, double sign, double* block )
{
	b3Vec3 normal = loadBond->normal;
	bool diagonal = x == y;
	b3Vec3 armX = b3Add( loadBond->arm[x], loadBond->offset );
	b3Vec3 armY = b3Add( loadBond->arm[y], loadBond->offset );
	double p[3] = { armX.x, armX.y, armX.z };
	double q[3] = { armY.x, armY.y, armY.z };
	double n[3] = { normal.x, normal.y, normal.z };
	double u[3], v[3], sx[9], sy[9];
	nbCross3( n, q, u );
	nbCross3( p, n, v );
	nbSkew( p, sx );
	nbSkew( q, sy );

	double a = sign * loadBond->shearStiffness;
	double b = sign * loadBond->normalStiffness;
	double dot = p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
	const double* dr = loadBond->rotation;

	for ( int i = 0; i < 3; ++i )
	{
		double* top = block + 6 * i;
		double* bottom = block + 6 * ( i + 3 );
		int last = diagonal ? i : 2;
		for ( int j = 0; j <= last; ++j )
		{
			double identity = i == j ? 1.0 : 0.0;
			top[j] += a * identity + b * n[i] * n[j];
			bottom[j + 3] += sign * dr[3 * i + j] - a * ( q[i] * p[j] - dot * identity ) - b * v[i] * u[j];
		}

		for ( int j = 0; j < 3; ++j )
		{
			bottom[j] += a * sx[3 * i + j] + b * v[i] * n[j];
		}

		if ( diagonal == false )
		{
			for ( int j = 0; j < 3; ++j )
			{
				top[j + 3] -= a * sy[3 * i + j] + b * n[i] * u[j];
			}
		}
	}
}

// Depth of an interface along a direction in its plane, taking it as a rectangle with the same area and second
// moment
static float nbContactDepth( const nbLoadBond* loadBond, b3Vec3 direction )
{
	float secondMoment = b3MaxFloat( nbSecondMomentAlong( loadBond->inertia, direction ), 0.0f );
	return loadBond->area > 0.0f ? sqrtf( 12.0f * secondMoment / loadBond->area ) : 0.0f;
}

// Contact of an open joint whose normal force acts at the eccentricity: the strip at the edge beyond it, as a
// fraction of the depth of the interface. The pressure under a force at distance e from the centroid of an
// interface of depth h falls to zero 3 ( h / 2 - e ) from the edge, and all of the interface is in contact up to
// e = h / 6.
static float nbContactFraction( const nbLoadBond* loadBond, b3Vec3 eccentricity, b3Vec3* direction, float* depth )
{
	*direction = b3Vec3_zero;
	*depth = 0.0f;
	float distance = b3Length( eccentricity );
	if ( !( distance > 0.0f ) )
	{
		return 1.0f;
	}

	*direction = b3MulSV( 1.0f / distance, eccentricity );
	*depth = nbContactDepth( loadBond, *direction );
	if ( !( *depth > 0.0f ) )
	{
		return 1.0f;
	}

	return b3ClampFloat( 3.0f * ( 0.5f - distance / *depth ), NB_LOAD_MIN_CONTACT, 1.0f );
}

// Whether the contact of an open joint moved noticeably from one point of its normal force to another
static bool nbContactMoved( const nbLoadBond* loadBond, b3Vec3 before, b3Vec3 after )
{
	b3Vec3 direction0, direction1;
	float depth0, depth1;
	float fraction0 = nbContactFraction( loadBond, before, &direction0, &depth0 );
	float fraction1 = nbContactFraction( loadBond, after, &direction1, &depth1 );
	b3Vec3 offset0 = b3MulSV( 0.5f * depth0 * ( 1.0f - fraction0 ), direction0 );
	b3Vec3 offset1 = b3MulSV( 0.5f * depth1 * ( 1.0f - fraction1 ), direction1 );
	return b3AbsFloat( fraction1 - fraction0 ) > NB_LOAD_CONTACT_TOLERANCE ||
		   b3Distance( offset0, offset1 ) > NB_LOAD_CONTACT_TOLERANCE * b3MaxFloat( depth0, depth1 );
}

// Stiffness of the elastic layer of a bond over its contact. A damaged bond acts like a smaller contact, an open
// joint only touches over a strip at one edge and a separated joint barely resists anything.
static void nbComputeBondStiffness( nbLoadBond* loadBond )
{
	double scale = b3MaxFloat( loadBond->health, NB_LOAD_MIN_HEALTH );
	double fraction = 1.0;
	const float* in = loadBond->inertia;
	double inertia[9] = { in[0], in[3], in[4], in[3], in[1], in[5], in[4], in[5], in[2] };
	loadBond->offset = b3Vec3_zero;

	if ( loadBond->state == nb_jointSeparated )
	{
		scale *= NB_LOAD_SEPARATED_STIFFNESS;
	}
	else if ( loadBond->state == nb_jointOpen )
	{
		b3Vec3 direction;
		float depth;
		float contact = nbContactFraction( loadBond, loadBond->eccentricity, &direction, &depth );
		if ( contact < 1.0f )
		{
			// The strip is a rectangle along the direction that keeps the width of the interface: the fraction of its
			// area and of its second moment across the direction, and the cube of the fraction of its second moment
			// along the direction
			loadBond->offset = b3MulSV( 0.5f * depth * ( 1.0f - contact ), direction );
			fraction = contact;
			b3Vec3 side = b3Cross( loadBond->normal, direction );
			double d[3] = { direction.x, direction.y, direction.z };
			double e[3] = { side.x, side.y, side.z };
			double along = fraction * fraction * fraction * nbSecondMomentAlong( loadBond->inertia, direction );
			double across = fraction * nbSecondMomentAlong( loadBond->inertia, side );
			for ( int i = 0; i < 3; ++i )
			{
				for ( int j = 0; j < 3; ++j )
				{
					inertia[3 * i + j] = along * d[i] * d[j] + across * e[i] * e[j];
				}
			}
		}
	}

	double area = scale * fraction * loadBond->area;
	double normal[3] = { loadBond->normal.x, loadBond->normal.y, loadBond->normal.z };
	double kn = NB_LOAD_STIFFNESS;
	double kt = NB_LOAD_SHEAR_RATIO * NB_LOAD_STIFFNESS;

	// Bending resists with the planar second moment I of the contact turned by a right angle about the normal,
	// which is polar ( 1 - n n^T ) - I for a planar I, and twisting with the polar moment. A contact that
	// degenerates to a line or a point would act as a hinge. A tiny stiffness keeps the system solvable, the
	// stress in such a bond still comes from its real geometry.
	double polar = scale * ( inertia[0] + inertia[4] + inertia[8] );
	double regular = 1.0e-4 * area * area;
	double inPlane = kn * ( polar + regular );
	double twisting = kt * ( polar + 2.0 * regular );

	loadBond->shearStiffness = kt * area;
	loadBond->normalStiffness = ( kn - kt ) * area;
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			double nn = normal[i] * normal[j];
			double identity = i == j ? 1.0 : 0.0;
			loadBond->rotation[3 * i + j] = inPlane * ( identity - nn ) - kn * scale * inertia[3 * i + j] + twisting * nn;
		}
	}
}

// Relative displacement and rotation of the second chunk of a bond against the first at the contact centroid
static void nbBondMotion( const nbLoadBond* loadBond, const double* solution, double* delta, double* phi )
{
	for ( int i = 0; i < 3; ++i )
	{
		delta[i] = 0.0;
		phi[i] = 0.0;
	}

	for ( int side = 0; side < 2; ++side )
	{
		int node = loadBond->node[side];
		if ( node == NB_NULL_INDEX )
		{
			continue;
		}

		// The contact centroid moves with the cluster by u + theta x arm
		const double* q = solution + 6 * node;
		b3Vec3 r = b3Add( loadBond->arm[side], loadBond->offset );
		double sign = side == 0 ? -1.0 : 1.0;
		delta[0] += sign * ( q[0] + q[4] * r.z - q[5] * r.y );
		delta[1] += sign * ( q[1] + q[5] * r.x - q[3] * r.z );
		delta[2] += sign * ( q[2] + q[3] * r.y - q[4] * r.x );
		phi[0] += sign * q[3];
		phi[1] += sign * q[4];
		phi[2] += sign * q[5];
	}
}

// Force of the layer on the second chunk: the spring over the relative displacement less the slip
static void nbBondForce( const nbLoadBond* loadBond, const double* delta, double* force )
{
	double n[3] = { loadBond->normal.x, loadBond->normal.y, loadBond->normal.z };
	double s[3] = { loadBond->slip.x, loadBond->slip.y, loadBond->slip.z };
	double closing = n[0] * delta[0] + n[1] * delta[1] + n[2] * delta[2];
	for ( int i = 0; i < 3; ++i )
	{
		force[i] = -loadBond->shearStiffness * ( delta[i] - s[i] ) - loadBond->normalStiffness * n[i] * closing;
	}
}

// An open joint slides where it is sheared beyond its friction. The slip grows until the joint holds a share of its
// grip, the rest of the shear moves to the other bonds. Returns whether the slip changed noticeably.
static bool nbUpdateSlip( nbLoadBond* loadBond, const double* solution )
{
	if ( loadBond->state != nb_jointOpen )
	{
		return false;
	}

	double delta[3], phi[3], force[3];
	nbBondMotion( loadBond, solution, delta, phi );
	nbBondForce( loadBond, delta, force );

	b3Vec3 f = { (float)force[0], (float)force[1], (float)force[2] };
	b3Vec3 n = loadBond->normal;
	float normalForce = b3Dot( f, n );
	if ( normalForce <= 0.0f )
	{
		return false;
	}

	b3Vec3 shear = b3MulSub( f, normalForce, n );
	float shearForce = b3Length( shear );
	float grip = loadBond->friction * normalForce + loadBond->health * NB_LOAD_COHESION * loadBond->area;
	if ( shearForce <= grip )
	{
		return false;
	}

	// Slide so that the shear force drops to the held share of the grip
	float held = NB_LOAD_SLIDING_FRICTION * grip;
	float stiffness = (float)loadBond->shearStiffness;
	b3Vec3 excess = b3MulSV( ( shearForce - held ) / ( shearForce * stiffness ), shear );
	loadBond->slip = b3Sub( loadBond->slip, excess );
	return stiffness * b3Length( excess ) > NB_LOAD_CONTACT_TOLERANCE * grip;
}

// Utilization of a bond from the relative displacement and rotation of its clusters: the largest ratio of load to
// capacity, above one the bond fails. A glued bond that cracks while it is pressed together becomes an open joint
// and one that is pulled apart separates. The contact of an open joint follows its normal force. Returns in changed
// whether the joint changed in a way that moves the loads, then the utilization is provisional.
static float nbEvaluateBond( nbLoadBond* loadBond, const double* solution, bool* changed )
{
	double delta[3], phi[3], force[3], moment[3];
	nbBondMotion( loadBond, solution, delta, phi );
	nbBondForce( loadBond, delta, force );
	for ( int i = 0; i < 3; ++i )
	{
		const double* dr = loadBond->rotation + 3 * i;
		moment[i] = -( dr[0] * phi[0] + dr[1] * phi[1] + dr[2] * phi[2] );
	}

	// The moment is taken about the interface centroid
	b3Vec3 f = { (float)force[0], (float)force[1], (float)force[2] };
	b3Vec3 t = { (float)moment[0], (float)moment[1], (float)moment[2] };
	t = b3Add( t, b3Cross( loadBond->offset, f ) );
	b3Vec3 n = loadBond->normal;
	float area = loadBond->area;

	// Compression is positive: the layer pushes the second chunk away from the first
	float normalForce = b3Dot( f, n );
	float shearForce = b3Length( b3MulSub( f, normalForce, n ) );
	float torsion = b3Dot( t, n );
	b3Vec3 bending = b3MulSub( t, torsion, n );
	float bendingMoment = b3Length( bending );

	*changed = false;
	float health = loadBond->health;
	if ( health <= 0.0f )
	{
		return FLT_MAX;
	}

	// Torsion as the shear force it adds at the edge of the interface
	float polar = b3MaxFloat( loadBond->inertia[0] + loadBond->inertia[1] + loadBond->inertia[2], 0.0f );
	float torsionModulus = sqrtf( polar * area / 3.0f );
	float torsionForce = torsionModulus > 0.0f ? b3AbsFloat( torsion ) * area / torsionModulus : 0.0f;

	// Bending presses the interface together on the side of n x M and pulls it apart on the other
	float bendingStress = 0.0f;
	if ( bendingMoment > 0.0f )
	{
		b3Vec3 across = b3Cross( n, b3MulSV( 1.0f / bendingMoment, bending ) );
		float modulus = area * nbContactDepth( loadBond, across ) / 6.0f;
		bendingStress = modulus > 0.0f ? bendingMoment / modulus : FLT_MAX;
	}

	float normalStress = normalForce / area;
	float compressive = health * loadBond->compressive;
	float cohesion = health * NB_LOAD_COHESION;
	uint8_t previous = loadBond->state;

	if ( previous == nb_jointGlued )
	{
		float tensile = health * loadBond->tensile;
		float tension = ( bendingStress - normalStress ) / tensile;
		float compression = ( normalStress + bendingStress ) / compressive;
		float sliding = ( shearForce + torsionForce ) / ( area * ( tensile + loadBond->friction * b3MaxFloat( normalStress, 0.0f ) ) );
		if ( ( tension <= 1.0f && sliding <= 1.0f ) || normalForce <= 0.0f )
		{
			return b3MaxFloat( tension, b3MaxFloat( compression, sliding ) );
		}

		// Pressed together, the cracked joint goes on as an open joint
		*changed = true;
	}

	// A separated joint closes again once it would be pressed together by more than its cohesion holds, a closed
	// one separates when it is pulled apart. The closing force of a separated joint is the one of a full contact.
	float closingForce = normalForce;
	if ( previous == nb_jointSeparated )
	{
		closingForce = normalForce / (float)NB_LOAD_SEPARATED_STIFFNESS;
	}

	float significant = NB_LOAD_SIGNIFICANT * loadBond->weight;
	if ( closingForce <= ( previous == nb_jointSeparated ? b3MaxFloat( cohesion * area, significant ) : 0.0f ) )
	{
		*changed = *changed || ( previous != nb_jointSeparated && -normalForce > significant );
		loadBond->state = nb_jointSeparated;
		loadBond->slip = b3Vec3_zero;
		float pulled = ( bendingStress - normalStress ) / cohesion;
		float torn = ( shearForce + torsionForce ) / ( area * cohesion );
		return b3MaxFloat( pulled, torn );
	}

	if ( previous == nb_jointSeparated )
	{
		// Closed again, the next pass finds its contact
		*changed = true;
		loadBond->state = nb_jointOpen;
		loadBond->eccentricity = b3Vec3_zero;
		loadBond->slip = b3Vec3_zero;
		return 0.0f;
	}

	// The normal force acts where it balances the bending moment. The pressure crushes a strip at the edge that
	// grows with the force, and the force can act up to the middle of that strip. A little cohesion lets joints
	// that barely touch hold their tiny moments.
	b3Vec3 eccentricity = b3MulSV( 1.0f / normalForce, b3Cross( n, bending ) );
	b3Vec3 direction;
	float depth;
	nbContactFraction( loadBond, eccentricity, &direction, &depth );

	float rotation = 0.0f;
	if ( bendingMoment > 0.0f )
	{
		float crushed = normalForce * depth / ( area * compressive );
		float reach = b3MaxFloat( 0.5f * ( depth - crushed ), 0.0f );
		float capacity = normalForce * reach + cohesion * area * depth / 6.0f;
		rotation = capacity > 0.0f ? bendingMoment / capacity : FLT_MAX;
	}

	// The slip keeps the shear within the friction, sliding still counts twisting and a slip that did not settle
	float crushing = normalForce / ( area * compressive );
	float sliding = ( shearForce + torsionForce ) / ( loadBond->friction * normalForce + cohesion * area );

	*changed = *changed || previous == nb_jointGlued ||
			   ( normalForce > significant && nbContactMoved( loadBond, loadBond->eccentricity, eccentricity ) );
	loadBond->state = nb_jointOpen;
	loadBond->eccentricity = eccentricity;
	return b3MaxFloat( rotation, b3MaxFloat( crushing, sliding ) );
}

// A bond of the static actor with the indices of its chunks in the chunk list
typedef struct nbLoadLink
{
	int bondIndex;
	int chunk[2];
} nbLoadLink;

// Group the chunks into clusters: chunks in the same cell of a grid that are bonded to each other. Chunks as large
// as a cell stay on their own, so the joints between blocks and between the cells of a pre-fracture are checked.
static void nbBuildClusters( nbArena* arena, const b3Vec3* centroids, const float* radii, const double* masses, const bool* anchoredChunks,
							 int n, const nbLoadLink* links, int linkCount, float cellSize, nbLoadClusters* clusters )
{
	float maxRadius = 0.5f * cellSize;
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
		if ( cell[3 * i] != cell[3 * j] || cell[3 * i + 1] != cell[3 * j + 1] || cell[3 * i + 2] != cell[3 * j + 2] ||
			 radii[i] > maxRadius || radii[j] > maxRadius )
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

	// Strengths of zero leave a material out of the check, unless pieces are joined by joints of their own.
	// Clusters follow the finest fragments.
	float compressives[NB_MAX_MATERIALS];
	float frictions[NB_MAX_MATERIALS];
	float fragmentSize = FLT_MAX;
	bool anyStrength = destructible->hasJoints;
	for ( int k = 0; k < destructible->materialCount; ++k )
	{
		const nbMaterial* material = destructible->materials + k;
		compressives[k] = material->compressiveStrength > 0.0f ? material->compressiveStrength : FLT_MAX;
		frictions[k] = b3MaxFloat( material->friction, 0.0f );
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
	b3Vec3 down = b3Normalize( gravity );
	float weight = b3Length( gravity );

	nbBeginOperation( world );
	nbArena* arena = &world->arena;

	// The chunks and bonds of the structure in compact arrays
	const nbActor* actor = world->actors.data + actorIndex;
	int n = actor->chunkCount;
	b3Vec3* centroids = nbArena_AllocArray( arena, b3Vec3, n );
	float* radii = nbArena_AllocArray( arena, float, n );
	double* masses = nbArena_AllocArray( arena, double, n );
	bool* anchoredChunks = nbArena_AllocArray( arena, bool, n );
	int bondCapacity = 0;
	int count = 0;
	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->scratch = count;
		chunk->utilization = 0.0f;
		centroids[count] = chunk->shape->centroid;
		radii[count] = chunk->shape->radius;
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
	// with too many clusters is checked on a coarser grid. The search starts at the grid of the last check.
	float baseCellSize = b3MaxFloat( NB_LOAD_CLUSTER_SIZE * fragmentSize, 1.0e-3f );
	float cellSize = b3MaxFloat( baseCellSize, destructible->loadCellSize );
	nbLoadClusters clusters;
	for ( ;; )
	{
		nbBuildClusters( arena, centroids, radii, masses, anchoredChunks, n, links, linkCount, cellSize, &clusters );
		if ( clusters.nodeCount <= NB_LOAD_MAX_NODES )
		{
			break;
		}
		cellSize *= 1.5f;
	}

	// Far below the limit the next check tries a finer grid
	destructible->loadCellSize = 3 * clusters.nodeCount < NB_LOAD_MAX_NODES ? cellSize / 1.5f : cellSize;

	int m = clusters.nodeCount;
	if ( m == 0 )
	{
		destructible->loadPasses = 0;
		return;
	}

	const int* cluster = clusters.cluster;
	const int* clusterNode = clusters.node;
	const b3Vec3* clusterCenter = clusters.center;

	// Every bond between two clusters of which at least one can move, with the state of its joint
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
		loadBond->cluster[0] = clusterA;
		loadBond->cluster[1] = clusterB;
		loadBond->node[0] = clusterNode[clusterA];
		loadBond->node[1] = clusterNode[clusterB];
		loadBond->arm[0] = b3Sub( bond->centroid, clusterCenter[clusterA] );
		loadBond->arm[1] = b3Sub( bond->centroid, clusterCenter[clusterB] );
		double massA = clusterNode[clusterA] != NB_NULL_INDEX ? clusters.mass[4 * clusterA] : DBL_MAX;
		double massB = clusterNode[clusterB] != NB_NULL_INDEX ? clusters.mass[4 * clusterB] : DBL_MAX;
		loadBond->weight = weight * (float)( massA < massB ? massA : massB );
		loadBond->normal = bond->normal;
		loadBond->area = bond->area;
		memcpy( loadBond->inertia, bond->inertia, sizeof( loadBond->inertia ) );
		loadBond->health = fullHealth > 0.0f ? b3ClampFloat( bond->health / fullHealth, 0.0f, 1.0f ) : 1.0f;
		loadBond->tensile = bond->tensileStrength;
		loadBond->compressive = b3MinFloat( compressives[materialA], compressives[materialB] );
		loadBond->friction = b3MinFloat( frictions[materialA], frictions[materialB] );
		loadBond->state = bond->jointState;
		loadBond->eccentricity = bond->eccentricity;
		loadBond->slip = bond->slip;
		if ( bond->jointState == nb_jointDry )
		{
			float upright = b3AbsFloat( b3Dot( bond->normal, down ) ) < NB_LOAD_UPRIGHT ? 1.0f : 0.0f;
			loadBond->state = upright > 0.0f ? nb_jointSeparated : nb_jointOpen;
		}
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


	// Position of the coupling block of every bond between two unknowns in the factor
	int* bondBlocks = nbArena_AllocArray( arena, int, loadBondCount + 1 );
	int* bondRows = nbArena_AllocArray( arena, int, loadBondCount + 1 );
	for ( int k = 0; k < loadBondCount; ++k )
	{
		const nbLoadBond* loadBond = loadBonds + k;
		int a = loadBond->node[0];
		int b = loadBond->node[1];
		bondBlocks[k] = NB_NULL_INDEX;
		bondRows[k] = 0;
		if ( a != NB_NULL_INDEX && b != NB_NULL_INDEX )
		{
			// The later cluster in the elimination order is the row: -B_row^T D B_column
			int row = factor.position[a] > factor.position[b] ? 0 : 1;
			bondRows[k] = row;
			bondBlocks[k] = nbLoadFindBlock( &factor, loadBond->node[row ^ 1], loadBond->node[row] );
			NB_ASSERT( bondBlocks[k] != NB_NULL_INDEX );
		}
	}

	factor.diagonal = nbArena_AllocArray( arena, double, 36 * m );
	factor.blocks = nbArena_AllocArray( arena, double, 36 * ( factor.blockCount > 0 ? factor.blockCount : 1 ) );
	double* solution = nbArena_AllocArray( arena, double, 6 * m );
	float* utilization = nbArena_AllocArray( arena, float, loadBondCount + 1 );
	bool* provisional = nbArena_AllocArray( arena, bool, loadBondCount + 1 );
	float* previousUtilization = nbArena_AllocArray( arena, float, loadBondCount + 1 );

	// Solve until the contacts of the open joints settle. Every pass solves the same structure with the stiffness
	// of the new contacts, so the order of the factor is found once.
	bool settled = false;
	for ( int iteration = 0; iteration < NB_LOAD_MAX_ITERATIONS && settled == false; ++iteration )
	{
		memset( factor.diagonal, 0, sizeof( double ) * 36 * (size_t)m );
		memset( factor.blocks, 0, sizeof( double ) * 36 * (size_t)factor.blockCount );
		for ( int k = 0; k < loadBondCount; ++k )
		{
			nbLoadBond* loadBond = loadBonds + k;
			nbComputeBondStiffness( loadBond );

			// Assemble the lower triangle in elimination order
			for ( int side = 0; side < 2; ++side )
			{
				int node = loadBond->node[side];
				if ( node != NB_NULL_INDEX )
				{
					nbBondBlock( loadBond, side, side, 1.0, factor.diagonal + 36 * node );
				}
			}

			if ( bondBlocks[k] != NB_NULL_INDEX )
			{
				int row = bondRows[k];
				nbBondBlock( loadBond, row, row ^ 1, -1.0, factor.blocks + 36 * bondBlocks[k] );
			}
		}

		if ( nbLoadFactorize( &factor ) == false )
		{
			return;
		}

		// Solve under the weight of every cluster at its center of mass. Joints that slid pull their chunks by the
		// slip, which is a load at their contacts. Sliding moves the load within the same stiffness, so the slip
		// settles over solves with the same factor.
		for ( int pass = 0; pass < NB_LOAD_SLIP_ITERATIONS; ++pass )
		{
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

			for ( int k = 0; k < loadBondCount; ++k )
			{
				const nbLoadBond* loadBond = loadBonds + k;
				if ( loadBond->state != nb_jointOpen || b3LengthSquared( loadBond->slip ) == 0.0f )
				{
					continue;
				}

				b3Vec3 pull = b3MulSV( (float)loadBond->shearStiffness, loadBond->slip );
				for ( int side = 0; side < 2; ++side )
				{
					int node = loadBond->node[side];
					if ( node == NB_NULL_INDEX )
					{
						continue;
					}

					b3Vec3 force = side == 0 ? b3Neg( pull ) : pull;
					b3Vec3 torque = b3Cross( b3Add( loadBond->arm[side], loadBond->offset ), force );
					double* f = solution + 6 * node;
					f[0] += force.x;
					f[1] += force.y;
					f[2] += force.z;
					f[3] += torque.x;
					f[4] += torque.y;
					f[5] += torque.z;
				}
			}
			nbLoadSolve( &factor, solution );
			if ( pass + 1 == NB_LOAD_SLIP_ITERATIONS )
			{
				break;
			}

			bool slipped = false;
			for ( int k = 0; k < loadBondCount; ++k )
			{
				slipped = nbUpdateSlip( loadBonds + k, solution ) || slipped;
			}

			if ( slipped == false )
			{
				break;
			}
		}

		settled = true;
		for ( int k = 0; k < loadBondCount; ++k )
		{
			previousUtilization[k] = iteration > 0 ? utilization[k] : FLT_MAX;
			utilization[k] = nbEvaluateBond( loadBonds + k, solution, provisional + k );
			settled = settled && provisional[k] == false;
		}
	}

	// The joints keep their cracks and contacts for the next check
	for ( int k = 0; k < loadBondCount; ++k )
	{
		const nbLoadBond* loadBond = loadBonds + k;
		nbBond* bond = world->bonds.data + loadBond->bondIndex;
		if ( bond->jointState == nb_jointGlued && loadBond->state != nb_jointGlued )
		{
			world->stats.crackedBondCount += 1;
		}
		bond->jointState = loadBond->state;
		bond->eccentricity = loadBond->eccentricity;
		bond->slip = loadBond->slip;
	}

	// The most overloaded bonds break first. Breaking them moves their load, so bonds that are much less
	// overloaded may hold once these are gone, like a beam that cracks at one section and not at every one.
	float worst = 0.0f;
	for ( int k = 0; k < loadBondCount; ++k )
	{
		worst = b3MaxFloat( worst, utilization[k] );
	}

	// Every chunk reports the most loaded bond around its cluster, the bonds inside are taken as rigid
	float* clusterUtilization = nbArena_AllocArray( arena, float, clusters.count );
	memset( clusterUtilization, 0, sizeof( float ) * (size_t)clusters.count );
	for ( int k = 0; k < loadBondCount; ++k )
	{
		for ( int side = 0; side < 2; ++side )
		{
			int clusterIndex = loadBonds[k].cluster[side];
			clusterUtilization[clusterIndex] = b3MaxFloat( clusterUtilization[clusterIndex], utilization[k] );
		}
	}

	for ( int c = actor->headChunk; c != NB_NULL_INDEX; c = world->chunks.data[c].nextChunk )
	{
		nbChunk* chunk = world->chunks.data + c;
		chunk->utilization = clusterUtilization[cluster[chunk->scratch]];
	}

	// While the joints still change the loads are not final. Only bonds far beyond their strength break, and not
	// those that just changed, whose load is about to move. The check goes on with the next update, and after a few
	// passes the loads count as they are.
	float threshold = b3MaxFloat( 1.0f, b3MinFloat( NB_LOAD_BREAK_FRACTION * worst, NB_LOAD_BREAK_ALWAYS ) );
	bool final = settled || destructible->loadPasses + 1 >= NB_LOAD_MAX_PASSES;
	if ( final )
	{
		destructible->loadPasses = 0;
	}
	else
	{
		destructible->loadPasses += 1;
		destructible->structureDirty = true;
		threshold = NB_LOAD_BREAK_ALWAYS;
	}

	// A joint that still changes breaks only when it was overloaded in the pass before as well
	nbIntArray* broken = &world->scratchList;
	broken->count = 0;
	for ( int k = 0; k < loadBondCount; ++k )
	{
		float load = provisional[k] ? b3MinFloat( utilization[k], previousUtilization[k] ) : utilization[k];
		if ( load > 1.0f && load >= threshold && ( final || provisional[k] == false ) )
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
