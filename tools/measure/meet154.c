// meet154.c out.csv toward ahead side speed...: what two steered humanoids of the library do when they meet, measured for
// round 154, see docs/Roadmap.md. Each case in a world of its own, with engageRadius 0, the default, for 10 s. The first
// figure walks or runs along +z at each speed given. Alone (free), against a wall across its way and one turned by 30
// degrees, and into a second figure: one that stands, one that comes toward it at the speed toward, one that walks ahead
// of it at the speed ahead, and one that crosses its way from the left at the speed toward, each head on and 1, 2 and 3
// times side meters off. Per case: whether and when either leaves the animated state and whether it fell, how close
// their frames come, how long the first stands almost still within 1 m of the second or from 2 s on at a wall, the
// slowest it gets from 1 s on, the hardest jump of a bone of each (second difference of its origin, mm) within 1.5 m of
// the other or from 2 s on alone and at a wall, and how far each got.
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
static void AddWall( Scene* s, float z, float turned )
{
	b3BodyDef bd = b3DefaultBodyDef();
	bd.position = b3ToPos( (b3Vec3){ 0.0f, 1.5f, z } );
	bd.rotation = b3MakeQuatFromAxisAngle( b3Vec3_axisY, turned );
	b3BodyId wall = b3CreateBody( s->physics, &bd );
	b3BoxHull box = b3MakeBoxHull( 6.0f, 1.5f, 0.1f );
	b3ShapeDef sd = b3DefaultShapeDef();
	b3CreateHullShape( wall, &sd, &box.base );
}

int main( int argc, char** argv )
{
	if ( argc < 6 )
	{
		fprintf( stderr, "meet154 out.csv toward ahead side speed...\n" );
		return 1;
	}
	const float hz = 60.0f;
	int subSteps = nbGetSubStepCount( hz );
	float toward = (float)atof( argv[2] ), ahead = (float)atof( argv[3] ), sideStep = (float)atof( argv[4] );
	FILE* out = fopen( argv[1], "w" );
	fprintf( out, "speed,kind,second,side,closest,blocked,slowest,leftA,leftC,fellA,fellC,jumpA,jumpC,wayA,wayC,endA,endC\n" );
	static const char* kinds[7] = { "free", "wall", "wall30", "stand", "toward", "ahead", "cross" };
	for ( int g = 5; g < argc; ++g )
	{
		float speed = (float)atof( argv[g] );
		// far enough that the first has its speed when they meet
		float apart = speed < 2.0f ? 4.0f : 9.0f;
		// the update the first alone gets there, for the one that crosses its way
		int there = 0;
		for ( int kind = 0; kind < 7; ++kind )
		{
			for ( int side = 0; side < ( kind < 3 ? 1 : 4 ); ++side )
			{
				Scene scene;
				CreateScene( &scene );
				nbFigureId a = AddFigure( &scene, b3Vec3_zero, 0.0f, 0.0f );
				nbFigureId c = a;
				float off = sideStep * (float)side, second = 0.0f;
				b3Vec3 startC = b3Vec3_zero;
				nbSteer steerA = { { 0.0f, 0.0f, speed }, 0.0f, true };
				nbSteer steerC = { { 0.0f, 0.0f, 0.0f }, 0.0f, false };
				if ( kind == 1 || kind == 2 )
				{
					AddWall( &scene, apart, kind == 2 ? 0.5235988f : 0.0f );
				}
				else if ( kind == 3 )
				{
					startC = (b3Vec3){ off, 0.0f, apart };
				}
				else if ( kind == 4 )
				{
					second = -toward;
					startC = (b3Vec3){ off, 0.0f, apart };
					steerC = (nbSteer){ { 0.0f, 0.0f, second }, 3.14159265f, true };
				}
				else if ( kind == 5 )
				{
					second = ahead;
					startC = (b3Vec3){ off, 0.0f, speed < 2.0f ? 2.5f : apart };
					steerC = (nbSteer){ { 0.0f, 0.0f, second }, 0.0f, true };
				}
				else if ( kind == 6 )
				{
					// from the left, as far from the crossing as it walks until the first gets there, less the offset
					second = toward;
					startC = (b3Vec3){ toward * (float)there / hz - off, 0.0f, apart };
					steerC = (nbSteer){ { -second, 0.0f, 0.0f }, -1.5707963f, true };
				}
				if ( kind >= 3 )
				{
					c = AddFigure( &scene, startC, steerC.heading, 0.0f );
				}
				static b3Vec3 lastA[nb_humanoidBoneCount], beforeA[nb_humanoidBoneCount], lastC[nb_humanoidBoneCount],
					beforeC[nb_humanoidBoneCount];
				float closest = 1.0e9f, slowest = speed, jumpA = 0.0f, jumpC = 0.0f;
				bool fellA = false, fellC = false;
				int leftA = -1, leftC = -1, blocked = 0;
				b3Vec3 fa = b3Vec3_zero, fc = startC;
				nbFigureState stateA = nb_figureAnimated, stateC = nb_figureAnimated;
				for ( int s = 0; s < 600; ++s )
				{
					nbFigure_Steer( a, &steerA );
					if ( kind >= 3 )
					{
						nbFigure_Steer( c, &steerC );
					}
					nbWorld_Step( scene.world, 1.0f / hz, subSteps );
					float ja = Jump( a, lastA, beforeA, s >= 2 );
					float jc = kind >= 3 ? Jump( c, lastC, beforeC, s >= 2 ) : 0.0f;
					nbFigureInfo ia = nbFigure_GetInfo( a ), ic = nbFigure_GetInfo( c );
					fa = b3ToVec3( nbFigure_GetFrame( a ).p );
					fc = kind >= 3 ? b3ToVec3( nbFigure_GetFrame( c ).p ) : fc;
					float frame = kind >= 3 ? b3Length( (b3Vec3){ fa.x - fc.x, 0.0f, fa.z - fc.z } ) : 1.0e9f;
					there = kind == 0 && there == 0 && fa.z >= apart ? s : there;
					fellA = fellA || ia.state == nb_figureRagdoll;
					fellC = fellC || ic.state == nb_figureRagdoll;
					leftA = leftA < 0 && ia.state != nb_figureAnimated ? s : leftA;
					leftC = leftC < 0 && ic.state != nb_figureAnimated ? s : leftC;
					stateA = ia.state;
					stateC = ic.state;
					if ( leftA < 0 && leftC < 0 )
					{
						float moves = b3Length( (b3Vec3){ ia.moveVelocity.x, 0.0f, ia.moveVelocity.z } );
						closest = b3MinFloat( closest, frame );
						slowest = s >= 60 ? b3MinFloat( slowest, moves ) : slowest;
						bool near = kind >= 3 ? frame < 1.0f : s >= 120;
						blocked += near && moves < 0.1f * speed ? 1 : 0;
						if ( kind >= 3 ? frame < 1.5f : s >= 120 )
						{
							jumpA = b3MaxFloat( jumpA, ja );
							jumpC = b3MaxFloat( jumpC, jc );
						}
					}
				}
				fprintf( out, "%.3f,%s,%.2f,%.2f,%.3f,%.2f,%.2f,%.2f,%.2f,%d,%d,%.1f,%.1f,%.2f,%.2f,%s,%s\n", speed, kinds[kind],
						 second, off, kind >= 3 ? closest : 0.0f, (float)blocked / hz, slowest,
						 leftA < 0 ? -1.0f : (float)leftA / hz, leftC < 0 ? -1.0f : (float)leftC / hz, fellA, fellC, jumpA, jumpC,
						 b3Length( fa ), b3Length( b3Sub( fc, startC ) ), StateName( stateA ), StateName( stateC ) );
				DestroyScene( &scene );
			}
		}
	}
	fclose( out );
	return 0;
}
