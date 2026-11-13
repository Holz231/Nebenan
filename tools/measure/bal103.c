// bal103.c out.csv: a standing humanoid of the library, hit on the pelvis, the belly, the ribs, the lower chest, the
// chest, the head, the left upper arm, the left thigh or the left shin from 32 directions with 20 to 140 N s in steps of
// 4, each hit in a world of its own, and how it ends: fell (ragdoll),
// caught (balancing, then animated again) or stood (never left the animated state), after how many seconds and steps.
// HOLD=own|world|none sets the upright hold, HZ the rate of the updates, each with the substeps of the library for that
// rate (nbGetSubStepCount, 240 per second), SHIFT moves the point of the hit up by that many meters, PART=k and PARTS=n
// run every n-th hit from the k-th on.
#include "nebenan/nebenan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main( int argc, char** argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "bal103 out.csv\n" );
		return 1;
	}
	const char* hzText = getenv( "HZ" );
	float hz = hzText != NULL ? (float)atof( hzText ) : 60.0f;
	const char* hold = getenv( "HOLD" );
	const char* shiftText = getenv( "SHIFT" );
	float shift = shiftText != NULL ? (float)atof( shiftText ) : 0.0f;
	int subSteps = nbGetSubStepCount( hz );
	int part = getenv( "PART" ) != NULL ? atoi( getenv( "PART" ) ) : 0;
	int parts = getenv( "PARTS" ) != NULL ? atoi( getenv( "PARTS" ) ) : 1;

	nbBoneDef bones[nb_humanoidBoneCount];
	float standHeight = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;

	FILE* file = fopen( argv[1], "w" );
	if ( file == NULL )
	{
		return 1;
	}
	fprintf( file, "bone,dir,impulse,outcome,seconds,steps\n" );

	const int hitBones[9] = { nb_humanoidPelvis,	  nb_humanoidBelly,			nb_humanoidRibs,
							  nb_humanoidLowerChest, nb_humanoidChest,			nb_humanoidHead,
							  nb_humanoidUpperArmLeft, nb_humanoidThighLeft, nb_humanoidShinLeft };
	int index = 0;
	for ( int b = 0; b < 9; ++b )
	{
		for ( int d = 0; d < 32; ++d )
		{
			for ( int m = 20; m <= 140; m += 4, ++index )
			{
				if ( index % parts != part )
				{
					continue;
				}

				b3WorldDef worldDef = b3DefaultWorldDef();
				b3WorldId physicsWorld = b3CreateWorld( &worldDef );
				b3BodyDef bodyDef = b3DefaultBodyDef();
				bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
				b3BodyId groundId = b3CreateBody( physicsWorld, &bodyDef );
				b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
				b3ShapeDef shapeDef = b3DefaultShapeDef();
				b3CreateHullShape( groundId, &shapeDef, &box.base );

				nbWorldDef def = nbDefaultWorldDef();
				def.physicsWorld = physicsWorld;
				def.workerCount = 1;
				nbWorldId world = nbCreateWorld( &def );

				nbFigureDef figureDef = nbDefaultFigureDef();
				figureDef.rig = &rig;
				figureDef.standHeight = standHeight;
				figureDef.position = b3ToPos( b3Vec3_zero );
				figureDef.heading = 0.0f;
				if ( hold != NULL && hold[0] == 'o' )
				{
					figureDef.motors.uprightAgainstWorld = false;
				}
				else if ( hold != NULL && hold[0] == 'n' )
				{
					figureDef.motors.uprightTorque = 0.0f;
				}
				nbFigureId figure = nbCreateFigure( world, &figureDef );

				int settle = (int)( 0.5f * hz + 0.5f );
				for ( int i = 0; i < settle; ++i )
				{
					nbWorld_Step( world, 1.0f / hz, subSteps );
				}

				float angle = 2.0f * 3.14159265f * (float)d / 32.0f;
				b3Vec3 push = { sinf( angle ), 0.0f, cosf( angle ) };
				b3BodyId body = nbFigure_GetBody( figure, hitBones[b] );
				const nbBoneDef* bone = bones + hitBones[b];
				b3Vec3 local = bone->shape == nb_capsuleBone ? b3MulSV( 0.5f, b3Add( bone->start, bone->end ) ) : bone->center;
				b3Pos point = b3Body_GetWorldPoint( body, local );
				point = b3OffsetPos( point, (b3Vec3){ -0.1f * push.x, shift, -0.1f * push.z } );
				nbHitDef hit = { 1, hitBones[b], b3MulSV( (float)m, push ), point };
				nbFigure_ApplyHit( figure, &hit );

				const char* outcome = "stood";
				int steps = 0;
				int limit = (int)( 10.0f * hz + 0.5f );
				bool balanced = false;
				while ( steps < limit )
				{
					nbWorld_Step( world, 1.0f / hz, subSteps );
					steps += 1;
					nbFigureState state = nbFigure_GetState( figure );
					if ( state == nb_figureRagdoll || state == nb_figureGettingUp )
					{
						outcome = "fell";
						break;
					}
					if ( state == nb_figureBalancing )
					{
						balanced = true;
					}
					else if ( state == nb_figureAnimated && ( balanced || steps > settle ) )
					{
						outcome = balanced ? "caught" : "stood";
						break;
					}
				}
				if ( steps >= limit )
				{
					outcome = "restless";
				}
				nbFigureInfo info = nbFigure_GetInfo( figure );
				fprintf( file, "%d,%d,%d,%s,%.3f,%d\n", hitBones[b], d, m, outcome, (float)steps / hz, info.stepCount );

				nbDestroyWorld( world );
				b3DestroyWorld( physicsWorld );
			}
		}
	}
	fclose( file );
	return 0;
}
