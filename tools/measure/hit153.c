// hit153.c hits.csv meet.csv: what a steered humanoid of the library does today when it is hit or meets another figure
// while it walks or runs, measured for round 153, see docs/Roadmap.md. Nothing in the library is changed for it.
// Hits: the figure walks at 1.4 m/s or runs at 4.3 m/s facing where it moves, and 3 s on, at four moments five updates
// apart, a hit of 20 to 140 N s in steps of 20 lands on its chest or its left thigh from eight directions (0 from
// behind, along its way, 90 from its right, 180 from ahead), each in a world of its own. For 6 s after it: the state
// right after the hit and the states it went through, whether it fell, how long it was not animated, when it moves
// again at nine tenths of the speed it is steered at, the slowest it got, how far a bone jumps in an update (second
// difference of its origin, mm, the hardest of all bones) in the update of the hit and the two after, at the hardest
// change of state after it and from a quarter second after it had come back on, and how far the frame is from where an
// unhit figure would be.
// Meetings: two steered figures, with engageRadius 0 (the default) or ENGAGE meters (default 1.5) for both. One walks
// or runs into the other, which stands, walks toward it or walks ahead of it at 1.0 m/s, head on or 0.3 m to the side,
// for 10 s. While both are animated: how close their frames and their pelvises come, how long the first stands almost
// still against the second, whether it gets beyond it, the slowest it gets, and the hardest jump of a bone of each
// within 1.5 m. And whether they followed their gait with dynamic bones, when each left the animated state, the states
// at the end, whether they fell, and how far the second is ahead of the first at the end.
#include "nebenan/nebenan.h"

#include "figure.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Scene
{
	b3WorldId physics;
	nbWorldId world;
	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig;
	float stand;
} Scene;

static void CreateScene( Scene* s )
{
	b3WorldDef wd = b3DefaultWorldDef();
	s->physics = b3CreateWorld( &wd );
	b3BodyDef bd = b3DefaultBodyDef();
	bd.position = b3ToPos( (b3Vec3){ 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( s->physics, &bd );
	b3BoxHull box = b3MakeBoxHull( 200.0f, 1.0f, 200.0f );
	b3ShapeDef sd = b3DefaultShapeDef();
	b3CreateHullShape( ground, &sd, &box.base );
	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = s->physics;
	def.workerCount = 1;
	def.asyncStep = false;
	s->world = nbCreateWorld( &def );
	s->stand = nbMakeHumanoidBones( 1.8f, 74.0f, s->bones, nb_humanoidBoneCount );
	s->rig = nbDefaultRigDef();
	s->rig.bones = s->bones;
	s->rig.boneCount = nb_humanoidBoneCount;
}

static void DestroyScene( Scene* s )
{
	nbDestroyWorld( s->world );
	b3DestroyWorld( s->physics );
}

static nbFigureId AddFigure( Scene* s, b3Vec3 position, float heading, float engage )
{
	nbFigureDef fd = nbDefaultFigureDef();
	fd.rig = &s->rig;
	fd.standHeight = s->stand;
	fd.position = b3ToPos( position );
	fd.heading = heading;
	fd.steered = true;
	fd.engageRadius = engage;
	return nbCreateFigure( s->world, &fd );
}

// The hardest second difference of the origin of a bone, in mm, with the origins of the two updates before
static float Jump( nbFigureId figure, b3Vec3* last, b3Vec3* before, bool count )
{
	float hardest = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		b3Vec3 now = b3ToVec3( b3Body_GetPosition( nbFigure_GetBody( figure, i ) ) );
		if ( count )
		{
			hardest = b3MaxFloat( hardest, 1000.0f * b3Length( b3Add( b3Sub( now, last[i] ), b3Sub( before[i], last[i] ) ) ) );
		}
		before[i] = last[i];
		last[i] = now;
	}
	return hardest;
}

static const char* StateName( nbFigureState state )
{
	return state == nb_figureAnimated	 ? "animated"
		   : state == nb_figureBalancing ? "balancing"
		   : state == nb_figureRagdoll	 ? "ragdoll"
										 : "gettingUp";
}

int main( int argc, char** argv )
{
	if ( argc < 3 )
	{
		fprintf( stderr, "hit153 hits.csv meet.csv\n" );
		return 1;
	}
	const float hz = 60.0f;
	int subSteps = nbGetSubStepCount( hz );
	FILE* out = fopen( argv[1], "w" );
	fprintf( out, "speed,bone,direction,impulse,moment,landed,first,stepped,balanced,fell,away,again,slowest,jumpHit,jumpChange,"
				  "jumpLate,off,end\n" );
	const float speeds[2] = { 1.4f, 4.3f };
	const int hitBones[2] = { nb_humanoidChest, nb_humanoidThighLeft };
	for ( int g = 0; g < 2; ++g )
	{
		// where an unhit figure is, update by update
		static b3Vec3 clean[600];
		for ( int b = -1; b < 2; ++b )
		{
			for ( int d = 0; d < ( b < 0 ? 1 : 8 ); ++d )
			{
				for ( int m = 20; m <= ( b < 0 ? 20 : 140 ); m += 20 )
				{
					for ( int moment = 0; moment < ( b < 0 ? 1 : 4 ); ++moment )
					{
						Scene scene;
						CreateScene( &scene );
						nbFigureId figure = AddFigure( &scene, b3Vec3_zero, 0.0f, 0.0f );
						nbSteer steer = { { 0.0f, 0.0f, speeds[g] }, 0.0f, true };
						static b3Vec3 last[nb_humanoidBoneCount], before[nb_humanoidBoneCount];
						int hitAt = 180 + 5 * moment;
						bool landed = false, stepped = false, balanced = false, fell = false;
						const char* first = "-";
						int away = 0, again = -1, fast = 0, backAt = -1;
						float slowest = speeds[g], jumpHit = 0.0f, jumpChange = 0.0f, jumpLate = 0.0f, off = 0.0f;
						nbFigureState state = nb_figureAnimated;
						for ( int s = 0; s < 560; ++s )
						{
							nbFigure_Steer( figure, &steer );
							if ( b >= 0 && s == hitAt )
							{
								float angle = 2.0f * 3.14159265f * (float)d / 8.0f;
								b3Vec3 push = { sinf( angle ), 0.0f, cosf( angle ) };
								b3BodyId body = nbFigure_GetBody( figure, hitBones[b] );
								const nbBoneDef* bone = scene.bones + hitBones[b];
								b3Vec3 local = bone->center;
								if ( bone->shape == nb_capsuleBone )
								{
									local = b3MulSV( 0.5f, b3Add( bone->start, bone->end ) );
								}
								b3Pos point = b3Body_GetWorldPoint( body, local );
								point = b3OffsetPos( point, (b3Vec3){ -0.1f * push.x, 0.0f, -0.1f * push.z } );
								nbHitDef hit = { 1, hitBones[b], b3MulSV( (float)m, push ), point };
								landed = nbFigure_ApplyHit( figure, &hit );
							}
							nbWorld_Step( scene.world, 1.0f / hz, subSteps );
							float jump = Jump( figure, last, before, s >= 2 );
							nbFigureInfo info = nbFigure_GetInfo( figure );
							b3Vec3 at = b3ToVec3( nbFigure_GetFrame( figure ).p );
							if ( b < 0 )
							{
								clean[s] = at;
								continue;
							}
							if ( s < hitAt )
							{
								continue;
							}
							if ( s == hitAt )
							{
								first = StateName( info.state );
							}
							stepped = stepped || info.stepping;
							balanced = balanced || info.state == nb_figureBalancing;
							fell = fell || info.state == nb_figureRagdoll || info.state == nb_figureGettingUp;
							away += info.state != nb_figureAnimated ? 1 : 0;
							if ( s < hitAt + 3 )
							{
								jumpHit = b3MaxFloat( jumpHit, jump );
							}
							if ( info.state != state || ( s > hitAt && s < backAt + 3 ) )
							{
								// a change of state, and the two updates after the figure is animated again
								if ( info.state != state && info.state == nb_figureAnimated )
								{
									backAt = s;
								}
								if ( s > hitAt + 2 )
								{
									jumpChange = b3MaxFloat( jumpChange, jump );
								}
							}
							state = info.state;
							float speed = b3Length( (b3Vec3){ info.moveVelocity.x, 0.0f, info.moveVelocity.z } );
							slowest = b3MinFloat( slowest, speed );
							fast = info.state == nb_figureAnimated && speed >= 0.9f * speeds[g] ? fast + 1 : 0;
							if ( again < 0 && fast >= 15 && s > hitAt + 15 )
							{
								again = s - 14 - hitAt;
							}
							if ( again >= 0 && s > hitAt + again + 30 && info.state == nb_figureAnimated )
							{
								jumpLate = b3MaxFloat( jumpLate, jump );
							}
							if ( s == 559 )
							{
								off = b3Length( b3Sub( at, clean[s] ) );
							}
						}
						if ( b >= 0 )
						{
							fprintf( out, "%.1f,%s,%d,%d,%d,%d,%s,%d,%d,%d,%.3f,%.3f,%.2f,%.1f,%.1f,%.1f,%.2f,%s\n", speeds[g],
									 b == 0 ? "chest" : "thigh", 45 * d, m, moment, landed, first, stepped, balanced, fell,
									 (float)away / hz, again < 0 ? -1.0f : (float)again / hz, slowest, jumpHit, jumpChange,
									 jumpLate, off, StateName( state ) );
						}
						DestroyScene( &scene );
					}
				}
			}
		}
	}
	fclose( out );

	out = fopen( argv[2], "w" );
	fprintf( out, "engage,first,second,side,closestFrame,closestPelvis,blocked,through,slowest,followedA,followedC,leftA,leftC,"
				  "stateA,stateC,fellA,fellC,jumpA,jumpC,gapEnd,pushes,hits,letGo\n" );
	float engageOn = getenv( "ENGAGE" ) != NULL ? (float)atof( getenv( "ENGAGE" ) ) : 1.5f;
	// the second figure: stands, walks toward the first, or walks ahead of it at 1.0 m/s
	const float seconds[3] = { 0.0f, -1.4f, 1.0f };
	for ( int e = 0; e < 2; ++e )
	{
		for ( int g = 0; g < 2; ++g )
		{
			for ( int k = 0; k < 3; ++k )
			{
				for ( int side = 0; side < 2; ++side )
				{
					float engage = e == 0 ? 0.0f : engageOn;
					Scene scene;
					CreateScene( &scene );
					nbFigureId a = AddFigure( &scene, b3Vec3_zero, 0.0f, engage );
					// far enough that the first has its speed when they meet, near enough that it catches up in time
					float apart = seconds[k] > 0.0f ? ( g == 0 ? 2.5f : 9.0f ) : ( g == 0 ? 4.0f : 9.0f );
					float heading = seconds[k] < 0.0f ? 3.14159265f : 0.0f;
					nbFigureId c = AddFigure( &scene, (b3Vec3){ 0.3f * (float)side, 0.0f, apart }, heading, engage );
					nbSteer steerA = { { 0.0f, 0.0f, speeds[g] }, 0.0f, true };
					nbSteer steerC = { { 0.0f, 0.0f, seconds[k] }, heading, seconds[k] != 0.0f };
					static b3Vec3 lastA[nb_humanoidBoneCount], beforeA[nb_humanoidBoneCount], lastC[nb_humanoidBoneCount],
						beforeC[nb_humanoidBoneCount];
					// while both are animated: how close they come, the slowest the first gets, the hardest jumps within 1.5 m
					float closestPelvis = 1.0e9f, closestFrame = 1.0e9f, slowest = speeds[g];
					float jumpA = 0.0f, jumpC = 0.0f, gap = 0.0f;
					bool followedA = false, followedC = false, fellA = false, fellC = false, through = false;
					int leftA = -1, leftC = -1, blocked = 0;
					nbFigureState stateA = nb_figureAnimated, stateC = nb_figureAnimated;
					for ( int s = 0; s < 600; ++s )
					{
						nbFigure_Steer( a, &steerA );
						nbFigure_Steer( c, &steerC );
						nbWorld_Step( scene.world, 1.0f / hz, subSteps );
						float ja = Jump( a, lastA, beforeA, s >= 2 );
						float jc = Jump( c, lastC, beforeC, s >= 2 );
						nbFigureInfo ia = nbFigure_GetInfo( a ), ic = nbFigure_GetInfo( c );
						b3Vec3 pa = b3ToVec3( b3Body_GetPosition( nbFigure_GetBody( a, nb_humanoidPelvis ) ) );
						b3Vec3 pc = b3ToVec3( b3Body_GetPosition( nbFigure_GetBody( c, nb_humanoidPelvis ) ) );
						b3Vec3 fa = b3ToVec3( nbFigure_GetFrame( a ).p ), fc = b3ToVec3( nbFigure_GetFrame( c ).p );
						float frame = b3Length( (b3Vec3){ fa.x - fc.x, 0.0f, fa.z - fc.z } );
						followedA = followedA || ia.following;
						followedC = followedC || ic.following;
						fellA = fellA || ia.state == nb_figureRagdoll;
						fellC = fellC || ic.state == nb_figureRagdoll;
						// the first update either is not animated, in seconds below
						leftA = leftA < 0 && ia.state != nb_figureAnimated ? s : leftA;
						leftC = leftC < 0 && ic.state != nb_figureAnimated ? s : leftC;
						stateA = ia.state;
						stateC = ic.state;
						if ( leftA < 0 && leftC < 0 )
						{
							float speed = b3Length( (b3Vec3){ ia.moveVelocity.x, 0.0f, ia.moveVelocity.z } );
							closestPelvis = b3MinFloat( closestPelvis, b3Length( (b3Vec3){ pa.x - pc.x, 0.0f, pa.z - pc.z } ) );
							closestFrame = b3MinFloat( closestFrame, frame );
							slowest = s > 60 ? b3MinFloat( slowest, speed ) : slowest;
							// updates the first stands almost still against the second
							blocked += frame < 1.0f && speed < 0.1f * speeds[g] ? 1 : 0;
							// beyond the second along its way while both walk on: it went past or through
							through = through || fc.z - fa.z < -0.2f;
							if ( frame < 1.5f )
							{
								jumpA = b3MaxFloat( jumpA, ja );
								jumpC = b3MaxFloat( jumpC, jc );
							}
						}
						gap = fc.z - fa.z;
					}
					// how often a push made either catch itself, how many hits landed on them from their contacts while they
					// followed, and how often either was pushed off its gait while it followed
					nbFigureInfo ia = nbFigure_GetInfo( a ), ic = nbFigure_GetInfo( c );
					fprintf( out, "%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.2f,%d,%.2f,%d,%d,%.2f,%.2f,", engage, speeds[g], seconds[k],
							 0.3f * (float)side, closestFrame, closestPelvis, (float)blocked / hz, through, slowest, followedA,
							 followedC, leftA < 0 ? -1.0f : (float)leftA / hz, leftC < 0 ? -1.0f : (float)leftC / hz );
					fprintf( out, "%s,%s,%d,%d,%.1f,%.1f,%.2f,%d,%d,%d\n", StateName( stateA ), StateName( stateC ), fellA, fellC,
							 jumpA, jumpC, gap, ia.pushCount + ic.pushCount, ia.hitCount + ic.hitCount,
							 ia.letGoCount + ic.letGoCount );
					DestroyScene( &scene );
				}
			}
		}
	}
	fclose( out );
	return 0;
}
