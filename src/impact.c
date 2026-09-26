// SPDX-License-Identifier: MIT

// The impact pipeline:
//  1. query the chunks overlapping the damage sphere
//  2. refine large chunks with a Voronoi fracture concentrated at the impact
//  3. damage the bonds near the impact, broken bonds disconnect chunks
//  4. split actors into rigid islands, islands without an anchor become dynamic bodies
//  5. commit shape changes to Box3D and set the velocities of the new bodies

#include "fracture.h"
#include "world.h"

#include <float.h>

typedef struct nbImpactFrame
{
	int actorIndex;
	b3Vec3 localPoint;
	b3Vec3 localDirection;
} nbImpactFrame;

// A face pair where a refined parent touched a bonded neighbor
typedef struct nbInterface
{
	int neighborIndex;
	int neighborFace;
	b3Plane parentPlane;
	float healthFraction;
} nbInterface;

typedef struct nbQueryContext
{
	nbWorld* world;
	int destructibleIndex;
	int actorIndex;
} nbQueryContext;

static bool nbQueryCallback( b3ShapeId shapeId, void* context )
{
	nbQueryContext* queryContext = context;
	nbWorld* world = queryContext->world;
	int chunkIndex = nbFindChunkFromShape( world, shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return true;
	}

	nbChunk* chunk = world->chunks.data + chunkIndex;
	if ( queryContext->destructibleIndex != NB_NULL_INDEX && chunk->destructibleIndex != queryContext->destructibleIndex )
	{
		return true;
	}

	if ( queryContext->actorIndex != NB_NULL_INDEX && chunk->actorIndex != queryContext->actorIndex )
	{
		return true;
	}

	nbArray_Push( world->scratchList, chunkIndex );
	return true;
}

static void nbSortInts( int* values, int count )
{
	for ( int i = 1; i < count; ++i )
	{
		int key = values[i];
		int j = i - 1;
		while ( j >= 0 && values[j] > key )
		{
			values[j + 1] = values[j];
			j -= 1;
		}
		values[j + 1] = key;
	}
}

static const nbImpactFrame* nbFindFrame( const nbImpactFrame* frames, int count, int actorIndex )
{
	for ( int i = 0; i < count; ++i )
	{
		if ( frames[i].actorIndex == actorIndex )
		{
			return frames + i;
		}
	}
	return NULL;
}

// Set up the fracture of one chunk into Voronoi cells concentrated at the impact. The sites are drawn here,
// in a fixed order, so the fracture pattern does not depend on the workers that compute the cells.
static bool nbPrepareRefine( nbWorld* world, int chunkIndex, b3Vec3 localPoint, float radius, int innerCount, nbFractureJob* job )
{
	nbChunk* chunk = world->chunks.data + chunkIndex;
	nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
	const nbMaterial* material = destructible->materials + chunk->materialIndex;
	const nbShape* parentShape = chunk->shape;

	// Center the working polyhedron for precision
	b3Vec3 origin = parentShape->centroid;
	nbPoly* parent = nbArena_AllocArray( &world->arena, nbPoly, 1 );
	nbShape_ToPoly( parentShape, parent );
	nbPoly_Translate( parent, b3Neg( origin ) );

	uint64_t stream = nbHashSeed( destructible->fractureCounter, ( (uint64_t)chunkIndex << 16 ) | chunk->generation );
	destructible->fractureCounter += 1;
	nbRandom rng = nbMakeRandom( destructible->seed, stream );

	float fragmentSize = nbGetFragmentSize( world, material );
	int ringCount = innerCount / 3;
	ringCount = ringCount < 4 ? 4 : ( ringCount > 24 ? 24 : ringCount );

	// Far pieces are cut to about three damage radii
	b3Vec3 extent = b3Sub( parentShape->bounds.upperBound, parentShape->bounds.lowerBound );
	float outerSize = b3MaxFloat( 3.0f * radius, 4.0f * fragmentSize );
	int nx = (int)( extent.x / outerSize ) + 1;
	int ny = (int)( extent.y / outerSize ) + 1;
	int nz = (int)( extent.z / outerSize ) + 1;
	int outerCount = nx * ny * nz - 1;
	outerCount = outerCount > 32 ? 32 : outerCount;

	nbSiteParams params = {
		.center = b3Sub( localPoint, origin ),
		.radius = radius,
		.innerCount = innerCount,
		.ringCount = ringCount,
		.outerCount = outerCount,
		.minSpacing = 0.45f * fragmentSize,
	};

	int capacity = innerCount + ringCount + outerCount;
	b3Vec3* sites = nbArena_AllocArray( &world->arena, b3Vec3, capacity );
	int siteCount = nbGenerateSites( parent, &params, &rng, sites, capacity );
	if ( siteCount < 2 )
	{
		return false;
	}

	*job = (nbFractureJob){
		.parent = parent,
		.sites = sites,
		.siteCount = siteCount,
		.origin = origin,
		.interiorMaterial = chunk->interiorMaterial,
		.tolerance = 1.0e-6f + 2.0e-6f * parentShape->radius,
		.minVolume = material->minFragmentVolume,
		.buildHulls = true,
	};
	return true;
}

// Replace a chunk by the cells of its fracture job. The children take the place of the chunk in its
// actor, inherit its outer bonds and are glued to each other.
static int nbFinishRefine( nbWorld* world, int chunkIndex, const nbFractureJob* job, nbImpactResult* result )
{
	int validCount = 0;
	for ( int i = 0; i < job->siteCount; ++i )
	{
		validCount += job->cells[i].shape != NULL ? 1 : 0;
	}

	if ( validCount < 2 )
	{
		for ( int i = 0; i < job->siteCount; ++i )
		{
			nbShape_Destroy( job->cells[i].shape );
		}
		return 0;
	}

	nbChunk* chunk = world->chunks.data + chunkIndex;
	int destructibleIndex = chunk->destructibleIndex;
	int actorIndex = chunk->actorIndex;
	int depth = chunk->depth + 1;
	uint8_t interiorMaterial = chunk->interiorMaterial;
	int materialIndex = chunk->materialIndex;
	nbMaterial material = world->destructibles.data[destructibleIndex].materials[materialIndex];
	const nbShape* parentShape = chunk->shape;
	float parentRadius = parentShape->radius;
	float fragmentSize = nbGetFragmentSize( world, &material );
	b3Vec3 origin = job->origin;

	// Snapshot the interfaces of the parent with its bonded neighbors: the coplanar face pairs.
	// Children can only touch a neighbor through a face they inherited from the parent face of an
	// interface, so the bonds to the neighbors are found without testing every face pair.
	float contactTolerance = 1.0e-4f + 1.0e-5f * parentRadius;
	int interfaceCapacity = 4 * chunk->bondCount + 1;
	nbInterface* interfaces = nbArena_AllocArray( &world->arena, nbInterface, interfaceCapacity );
	int interfaceCount = 0;
	for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
	{
		const nbBond* bond = world->bonds.data + ( key >> 1 );
		int side = key & 1;
		int neighborIndex = bond->chunk[side ^ 1];
		key = bond->nextKey[side];

		float fullHealth = nbGetBondStrength( world, bond ) * bond->area;
		float healthFraction = fullHealth > 0.0f ? b3ClampFloat( bond->health / fullHealth, 0.0f, 1.0f ) : 0.0f;
		const nbShape* neighborShape = world->chunks.data[neighborIndex].shape;

		for ( int pf = 0; pf < parentShape->faceCount && interfaceCount < interfaceCapacity; ++pf )
		{
			b3Plane parentPlane = parentShape->faces[pf].plane;
			for ( int nf = 0; nf < neighborShape->faceCount; ++nf )
			{
				b3Plane neighborPlane = neighborShape->faces[nf].plane;
				if ( b3Dot( parentPlane.normal, neighborPlane.normal ) > -0.99985f ||
					 b3AbsFloat( parentPlane.offset + neighborPlane.offset ) > contactTolerance )
				{
					continue;
				}

				if ( interfaceCount < interfaceCapacity )
				{
					interfaces[interfaceCount++] = (nbInterface){ neighborIndex, nf, parentPlane, healthFraction };
				}
			}
		}
	}

	// Remove the parent first so the children can take over its place in the actor
	nbDestroyChunk( world, chunkIndex );
	result->fracturedChunkCount += 1;

	int* childIndices = nbArena_AllocArray( &world->arena, int, job->siteCount );
	int childCount = 0;
	for ( int i = 0; i < job->siteCount; ++i )
	{
		childIndices[i] = NB_NULL_INDEX;
		const nbCell* cell = job->cells + i;
		if ( cell->shape == NULL )
		{
			continue;
		}

		childIndices[i] =
			nbCreateChunkWithHull( world, destructibleIndex, actorIndex, cell->shape, cell->hull, depth, interiorMaterial, materialIndex );
		childCount += childIndices[i] != NB_NULL_INDEX ? 1 : 0;
	}
	result->createdChunkCount += childCount;

	// Glue the children along their shared Voronoi faces
	float minBondArea = 0.01f * fragmentSize * fragmentSize;
	for ( int i = 0; i < job->siteCount; ++i )
	{
		if ( childIndices[i] == NB_NULL_INDEX )
		{
			continue;
		}

		const nbCell* cell = job->cells + i;
		for ( int k = 0; k < cell->neighborCount; ++k )
		{
			const nbCellNeighbor* neighbor = cell->neighbors + k;
			int j = neighbor->site;
			if ( j <= i || childIndices[j] == NB_NULL_INDEX || neighbor->geometry.area < minBondArea )
			{
				continue;
			}

			nbBondGeometry geometry = neighbor->geometry;
			geometry.centroid = b3Add( geometry.centroid, origin );
			nbCreateBond( world, childIndices[i], childIndices[j], &geometry, material.strength * geometry.area );
		}
	}

	// Glue the children to the former neighbors of the parent where their inherited faces touch
	for ( int i = 0; i < job->siteCount; ++i )
	{
		int childIndex = childIndices[i];
		if ( childIndex == NB_NULL_INDEX )
		{
			continue;
		}

		for ( int k = 0; k < interfaceCount; ++k )
		{
			const nbInterface* face = interfaces + k;
			const nbShape* childShape = world->chunks.data[childIndex].shape;
			for ( int cf = 0; cf < childShape->faceCount; ++cf )
			{
				b3Plane childPlane = childShape->faces[cf].plane;
				if ( b3Dot( childPlane.normal, face->parentPlane.normal ) < 0.99999f ||
					 b3AbsFloat( childPlane.offset - face->parentPlane.offset ) > contactTolerance )
				{
					continue;
				}

				// The bond keeps the damage of the parent bond and is as strong as the weaker material
				const nbChunk* neighbor = world->chunks.data + face->neighborIndex;
				nbBondGeometry geometry;
				float area = nbShape_FaceOverlap( childShape, cf, neighbor->shape, face->neighborFace, &geometry );
				float strength = b3MinFloat( material.strength, nbGetChunkMaterial( world, neighbor )->strength );
				float health = strength * area * face->healthFraction;
				if ( area > minBondArea && health > 0.0f )
				{
					nbCreateBond( world, childIndex, face->neighborIndex, &geometry, health );
				}
				break;
			}
		}
	}

	return childCount;
}

static void nbApplyVelocities( nbWorld* world, const nbImpactDef* def, nbRandom* rng, const int* hitActors, int hitActorCount )
{
	b3Vec3 direction = b3Normalize( def->direction );
	bool isDirected = b3LengthSquared( direction ) > 0.5f;
	b3Vec3 normal = b3Normalize( def->normal );
	bool haveNormal = b3LengthSquared( normal ) > 0.5f;
	float reach = 1.5f * def->radius;

	for ( int i = 0; i < world->touchedActors.count; ++i )
	{
		int actorIndex = world->touchedActors.data[i];
		nbActor* actor = world->actors.data + actorIndex;
		if ( actor->isFree || actor->isStatic || actor->isNew == false )
		{
			continue;
		}
		actor->isNew = false;

		// Inherit the velocity field of the source body
		b3Pos center = b3Body_GetWorldCenter( actor->bodyId );
		b3Vec3 offset = b3SubPos( center, actor->sourceCenter );
		b3Vec3 linearVelocity = b3Add( actor->sourceLinearVelocity, b3Cross( actor->sourceAngularVelocity, offset ) );
		b3Vec3 angularVelocity = actor->sourceAngularVelocity;

		if ( def->ejectSpeed > 0.0f )
		{
			b3Vec3 delta = b3SubPos( center, def->point );
			float distance;
			b3Vec3 radial = b3GetLengthAndNormalize( &distance, delta );
			if ( distance < reach )
			{
				float falloff = 1.0f - distance / reach;
				if ( distance < 1.0e-4f )
				{
					radial = isDirected ? direction : b3Vec3_axisY;
				}

				b3Vec3 jitter = nbRandomUnitVector( rng );
				b3Vec3 eject;
				if ( isDirected && haveNormal && -b3Dot( delta, normal ) < 0.4f * def->radius )
				{
					// Crater: fragments near the surface spall back toward the shooter in a cone
					b3Vec3 tangent = b3MulSub( radial, b3Dot( radial, normal ), normal );
					eject = b3Add( b3Add( normal, b3MulSV( 0.8f, tangent ) ), b3MulSV( 0.35f, jitter ) );
				}
				else if ( isDirected )
				{
					// Exit side: the projectile drives the fragments on through the material
					eject = b3Add( b3Add( b3MulSV( 0.8f, direction ), b3MulSV( 0.6f, radial ) ), b3MulSV( 0.35f, jitter ) );
				}
				else
				{
					eject = b3Add( radial, b3MulSV( 0.3f, jitter ) );
				}

				float speed = def->ejectSpeed * falloff * nbRandomRange( rng, 0.6f, 1.0f );
				linearVelocity = b3MulAdd( linearVelocity, speed, b3Normalize( eject ) );
				float spin = falloff * nbRandomRange( rng, 3.0f, 18.0f );
				b3Vec3 spinAxis = nbRandomUnitVector( rng );
				angularVelocity = b3MulAdd( angularVelocity, spin, spinAxis );
			}
		}

		b3Body_SetLinearVelocity( actor->bodyId, linearVelocity );
		b3Body_SetAngularVelocity( actor->bodyId, angularVelocity );
	}

	// Loose debris inside the blast gets pushed as well. Heavy parts barely move.
	if ( def->ejectSpeed <= 0.0f )
	{
		return;
	}

	for ( int i = 0; i < hitActorCount; ++i )
	{
		nbActor* actor = world->actors.data + hitActors[i];
		if ( actor->isFree || actor->isStatic || actor->chunkCount == 0 )
		{
			continue;
		}

		float fragmentSize = nbGetFragmentSize( world, nbGetChunkMaterial( world, world->chunks.data + actor->headChunk ) );
		float fragmentVolume = fragmentSize * fragmentSize * fragmentSize;

		b3Pos center = b3Body_GetWorldCenter( actor->bodyId );
		b3Vec3 delta = b3SubPos( center, def->point );
		float distance;
		b3Vec3 radial = b3GetLengthAndNormalize( &distance, delta );
		if ( distance >= reach )
		{
			continue;
		}

		float falloff = 1.0f - distance / reach;
		float scale = b3MinFloat( 1.0f, 4.0f * fragmentVolume / b3MaxFloat( actor->volume, 1.0e-9f ) );
		b3Vec3 push = isDirected ? b3Normalize( b3Add( direction, b3MulSV( 0.5f, radial ) ) ) : radial;
		b3Vec3 velocity = b3Body_GetLinearVelocity( actor->bodyId );
		velocity = b3MulAdd( velocity, 0.5f * def->ejectSpeed * falloff * scale, push );
		b3Body_SetLinearVelocity( actor->bodyId, velocity );
		b3Body_SetAwake( actor->bodyId, true );
	}
}

nbImpactResult nbApplyImpact( nbWorld* world, const nbImpactDef* def, int actorFilter )
{
	nbImpactResult result = { 0 };
	if ( def->radius <= 0.0f )
	{
		return result;
	}

	uint64_t ticks = b3GetTicks();
	nbBeginOperation( world );
	world->scratchList.count = 0;

	int destructibleFilter = NB_NULL_INDEX;
	if ( NB_IS_NON_NULL( def->destructibleId ) )
	{
		if ( nbGetDestructibleFromId( def->destructibleId, NULL ) == NULL )
		{
			return result;
		}
		destructibleFilter = def->destructibleId.index1 - 1;
	}

	// 1. Query, after the rubble in reach came back to life
	float radius = def->radius;
	b3Pos point = def->point;
	b3AABB box = {
		{ (float)point.x - radius, (float)point.y - radius, (float)point.z - radius },
		{ (float)point.x + radius, (float)point.y + radius, (float)point.z + radius },
	};
	nbThawRubble( world, box );

	nbQueryContext queryContext = { world, destructibleFilter, actorFilter };
	b3World_OverlapAABB( world->physicsWorld, box, b3DefaultQueryFilter(), nbQueryCallback, &queryContext );

	int queryCount = world->scratchList.count;
	if ( queryCount == 0 )
	{
		return result;
	}

	// Box3D reports in tree order. Sort for a deterministic result.
	nbSortInts( world->scratchList.data, queryCount );

	int seedDestructible = world->chunks.data[world->scratchList.data[0]].destructibleIndex;
	int* candidates = nbArena_AllocArray( &world->arena, int, queryCount );
	float* overlaps = nbArena_AllocArray( &world->arena, float, queryCount );
	nbImpactFrame* frames = nbArena_AllocArray( &world->arena, nbImpactFrame, queryCount );
	int* hitActors = nbArena_AllocArray( &world->arena, int, queryCount );
	int candidateCount = 0;
	int frameCount = 0;

	for ( int i = 0; i < queryCount; ++i )
	{
		int chunkIndex = world->scratchList.data[i];
		if ( i > 0 && chunkIndex == world->scratchList.data[i - 1] )
		{
			continue;
		}

		nbChunk* chunk = world->chunks.data + chunkIndex;
		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		if ( frame == NULL )
		{
			nbActor* actor = world->actors.data + chunk->actorIndex;
			b3WorldTransform transform = nbActor_GetTransform( world, actor );
			nbImpactFrame* newFrame = frames + frameCount;
			newFrame->actorIndex = chunk->actorIndex;
			newFrame->localPoint = b3InvTransformWorldPoint( transform, point );
			newFrame->localDirection = b3InvRotateVector( transform.q, def->direction );
			hitActors[frameCount] = chunk->actorIndex;
			frameCount += 1;
			frame = newFrame;
		}

		float distance = nbShape_Distance( chunk->shape, frame->localPoint );
		if ( distance > radius )
		{
			continue;
		}

		candidates[candidateCount++] = chunkIndex;
	}

	if ( candidateCount == 0 )
	{
		return result;
	}

	world->stats.impactCount += 1;

	// 2. Refine chunks that are much larger than the fragments this impact creates.
	// The fragment count grows with the fracture area, (volume / fragment volume)^(2/3), not with the
	// volume, and the total per impact is capped. This keeps large blasts affordable.
	int refineCount = 0;
	float totalOverlap = 0.0f;
	float totalDesired = 0.0f;
	float* desired = nbArena_AllocArray( &world->arena, float, candidateCount );
	for ( int i = 0; i < candidateCount; ++i )
	{
		nbChunk* chunk = world->chunks.data + candidates[i];
		const nbDestructible* destructible = world->destructibles.data + chunk->destructibleIndex;
		const nbMaterial* material = destructible->materials + chunk->materialIndex;
		float fragmentSize = nbGetFragmentSize( world, material );
		float fragmentVolume = fragmentSize * fragmentSize * fragmentSize;

		overlaps[i] = 0.0f;
		desired[i] = 0.0f;
		bool canRefine = chunk->depth < material->maxDepth && chunk->shape->volume > 3.0f * fragmentVolume &&
						 chunk->shape->radius > 1.2f * fragmentSize;
		if ( canRefine == false )
		{
			continue;
		}

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		nbPoly* poly = nbArena_AllocArray( &world->arena, nbPoly, 1 );
		nbShape_ToPoly( chunk->shape, poly );
		nbRandom sampler = nbMakeRandom( destructible->seed, nbHashSeed( candidates[i], chunk->generation ) );
		overlaps[i] = nbEstimateSphereOverlap( poly, frame->localPoint, radius, &sampler, 64 );
		totalOverlap += overlaps[i];
		refineCount += overlaps[i] > 0.0f ? 1 : 0;

		float ratio = overlaps[i] / fragmentVolume;
		desired[i] = ratio > 0.0f ? 1.5f * nbCbrt( ratio * ratio ) : 0.0f;
		totalDesired += desired[i];
	}

	float fragmentBudget = def->fragmentCount > 0 ? (float)def->fragmentCount : totalDesired;
	fragmentBudget = b3MinFloat( fragmentBudget, (float)world->def.maxFragmentsPerImpact );

	// Draw the sites of every chunk first, then compute all cells on the workers, then build the chunks.
	// Each phase runs in candidate order, so the result is the same for any number of workers.
	int firstChild = world->touchedChunks.count;
	nbFractureJob* jobs = nbArena_AllocArray( &world->arena, nbFractureJob, candidateCount );
	int* jobCandidates = nbArena_AllocArray( &world->arena, int, candidateCount );
	int jobCount = 0;
	for ( int i = 0; i < candidateCount && refineCount > 0; ++i )
	{
		if ( overlaps[i] <= 0.0f )
		{
			continue;
		}

		int chunkIndex = candidates[i];
		nbChunk* chunk = world->chunks.data + chunkIndex;

		// Share the budget by damaged volume
		float share = def->fragmentCount > 0 ? overlaps[i] / totalOverlap : desired[i] / b3MaxFloat( totalDesired, 1.0e-6f );
		int innerCount = (int)( fragmentBudget * share + 0.5f );
		innerCount = innerCount < 3 ? 3 : ( innerCount > 192 ? 192 : innerCount );

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		if ( nbPrepareRefine( world, chunkIndex, frame->localPoint, radius, innerCount, jobs + jobCount ) )
		{
			jobCandidates[jobCount] = i;
			jobCount += 1;
		}
	}

	if ( jobCount > 0 )
	{
		uint64_t fractureTicks = b3GetTicks();
		nbRunFractureJobs( world, jobs, jobCount );
		result.fractureTime = b3GetMilliseconds( fractureTicks );

		for ( int k = 0; k < jobCount; ++k )
		{
			int i = jobCandidates[k];
			if ( nbFinishRefine( world, candidates[i], jobs + k, &result ) > 0 )
			{
				candidates[i] = NB_NULL_INDEX;
			}
		}
	}

	// 3. Damage bonds around the impact. Every bond is visited once.
	world->bondStamp += 1;
	uint32_t stamp = world->bondStamp;
	int touchedEnd = world->touchedChunks.count;
	int damageCount = candidateCount + ( touchedEnd - firstChild );
	int* damageChunks = nbArena_AllocArray( &world->arena, int, damageCount );
	int damageChunkCount = 0;
	for ( int i = 0; i < candidateCount; ++i )
	{
		if ( candidates[i] != NB_NULL_INDEX )
		{
			damageChunks[damageChunkCount++] = candidates[i];
		}
	}
	for ( int i = firstChild; i < touchedEnd; ++i )
	{
		int chunkIndex = world->touchedChunks.data[i];
		if ( world->chunks.data[chunkIndex].shape != NULL )
		{
			damageChunks[damageChunkCount++] = chunkIndex;
		}
	}

	for ( int i = 0; i < damageChunkCount; ++i )
	{
		int chunkIndex = damageChunks[i];
		nbChunk* chunk = world->chunks.data + chunkIndex;
		if ( chunk->shape == NULL )
		{
			continue;
		}

		const nbImpactFrame* frame = nbFindFrame( frames, frameCount, chunk->actorIndex );
		if ( frame == NULL )
		{
			continue;
		}

		for ( int key = chunk->headBondKey; key != NB_NULL_INDEX; )
		{
			int bondIndex = key >> 1;
			nbBond* bond = world->bonds.data + bondIndex;
			int side = key & 1;
			key = bond->nextKey[side];

			if ( bond->stamp == stamp )
			{
				continue;
			}
			bond->stamp = stamp;

			float distance = b3Distance( bond->centroid, frame->localPoint );
			if ( distance >= radius )
			{
				continue;
			}

			bond->health -= def->damage * ( 1.0f - distance / radius );
			if ( bond->health <= 0.0f )
			{
				nbDestroyBond( world, bondIndex );
				result.brokenBondCount += 1;
			}
		}
	}

	// 4. Islands
	nbSplitActors( world, &result );

	// 5. Physics
	nbCommitPhysics( world );

	nbRandom rng = nbMakeRandom( world->destructibles.data[seedDestructible].seed, nbHashSeed( world->stats.impactCount, 0xe7ec7 ) );
	nbApplyVelocities( world, def, &rng, hitActors, frameCount );

	world->touchedActors.count = 0;

	result.totalTime = b3GetMilliseconds( ticks );
	world->stats.lastImpactTime = result.totalTime;
	world->stats.lastFractureTime = result.fractureTime;
	world->stats.fractureCount += result.fracturedChunkCount;
	return result;
}

nbImpactResult nbWorld_ApplyImpact( nbWorldId worldId, const nbImpactDef* def )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return (nbImpactResult){ 0 };
	}
	return nbApplyImpact( world, def, NB_NULL_INDEX );
}

bool nbWorld_CastImpact( nbWorldId worldId, b3Pos origin, b3Vec3 translation, const nbImpactDef* def, nbImpactResult* result )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return false;
	}

	b3RayResult ray = b3World_CastRayClosest( world->physicsWorld, origin, translation, b3DefaultQueryFilter() );
	if ( ray.hit == false )
	{
		return false;
	}

	int chunkIndex = nbFindChunkFromShape( world, ray.shapeId );
	if ( chunkIndex == NB_NULL_INDEX )
	{
		return false;
	}

	if ( NB_IS_NON_NULL( def->destructibleId ) && world->chunks.data[chunkIndex].destructibleIndex != def->destructibleId.index1 - 1 )
	{
		return false;
	}

	nbImpactDef impact = *def;
	impact.point = ray.point;
	impact.direction = b3Normalize( translation );
	impact.normal = ray.normal;

	nbImpactResult impactResult = nbWorld_ApplyImpact( worldId, &impact );
	if ( result != NULL )
	{
		*result = impactResult;
	}
	return true;
}
