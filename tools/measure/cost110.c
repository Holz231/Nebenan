// cost110.c mode count steps: time of the figure update for figures that stand (mode 0), walk steered facing where they
// move (mode 1, as cost84), walk steered facing ahead while they go to the side (mode 2, faceVelocity false), walk
// forward and run backward facing ahead (mode 3), or walk steadily to the side at 1.6 m/s facing ahead (mode 4)
#include "nebenan/nebenan.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char** argv )
{
	int mode = argc > 1 ? atoi( argv[1] ) : 1;
	int count = argc > 2 ? atoi( argv[2] ) : 20;
	b3WorldDef wd = b3DefaultWorldDef();
	b3WorldId physics = b3CreateWorld( &wd );
	b3BodyDef bd = b3DefaultBodyDef();
	bd.position = b3ToPos( (b3Vec3){ 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( physics, &bd );
	b3BoxHull box = b3MakeBoxHull( 100.0f, 1.0f, 100.0f );
	b3ShapeDef sd = b3DefaultShapeDef();
	b3CreateHullShape( ground, &sd, &box.base );
	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = physics;
	nbWorldId world = nbCreateWorld( &def );
	nbBoneDef bones[nb_humanoidBoneCount];
	float stand = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureId figures[64];
	for ( int k = 0; k < count; ++k )
	{
		nbFigureDef fd = nbDefaultFigureDef();
		fd.rig = &rig;
		fd.standHeight = stand;
		fd.position = b3ToPos( (b3Vec3){ 3.0f * (float)( k % 8 ) - 10.0f, 0.0f, 6.0f * (float)( k / 8 ) - 10.0f } );
		fd.steered = mode != 0;
		figures[k] = nbCreateFigure( world, &fd );
	}
	double update = 0.0;
	int steps = argc > 3 ? atoi( argv[3] ) : 1200;
	for ( int s = 0; s < steps; ++s )
	{
		// Walk forth and back every 2 s, run in between; facing ahead, the same to the side
		float t = (float)s / 60.0f;
		float phase = t - 4.0f * floorf( t / 4.0f );
		float speed = phase < 2.0f ? 1.5f : -4.5f;
		for ( int k = 0; k < count && mode != 0; ++k )
		{
			nbSteer steer = { { 0.0f, 0.0f, speed }, 0.0f, true };
			if ( mode == 3 )
			{
				steer.faceVelocity = false;
			}
			if ( mode == 2 )
			{
				steer.velocity = (b3Vec3){ speed, 0.0f, 0.0f };
				steer.faceVelocity = false;
			}
			if ( mode == 4 )
			{
				steer.velocity = (b3Vec3){ 1.6f, 0.0f, 0.0f };
				steer.faceVelocity = false;
			}
			nbFigure_Steer( figures[k], &steer );
		}
		nbWorld_Step( world, 1.0f / 60.0f, 4 );
		nbProfile profile = nbWorld_GetProfile( world );
		update += profile.stages[nb_profileFigures];
	}
	printf( "mode %d, %d figures: figure update avg %.4f ms\n", mode, count, update / steps );
	return 0;
}
