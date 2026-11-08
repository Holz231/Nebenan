// SPDX-License-Identifier: MIT

// The gait of the library recorded as a clip of the gait, see nbMakeHumanoidGaitClip: a steered humanoid walks or runs
// straight ahead on level ground in a world of its own until its gait is steady, and the pose it goes for in every
// update of one cycle becomes a keyframe, at the time its gait has, with the touchdowns and lift-offs where its gait
// times them. Played back as the clip of the gait of a figure, see nbFigureDef::gaitClips, it moves the body as the
// gait of the library does at that speed, so a program can try the clips of its gait against it, or start from it.

#include "figure.h"
#include "world.h"

#include "nebenan/nebenan.h"

#include <math.h>
#include <string.h>

// The recording steps 60 times a second, with 4 substeps as the tests and the demo
#define NB_RECORD_RATE 60.0f
#define NB_RECORD_SUBSTEPS 4

// The figure walks this long before the recording starts, its gait steady by then, and records for at most this long
#define NB_RECORD_SETTLE 4.0f
#define NB_RECORD_MOST 4.0f

// It starts this far back on ground this long, so it never reaches the end at the speeds a figure can go
#define NB_RECORD_START 240.0f
#define NB_RECORD_GROUND 250.0f

int nbMakeHumanoidGaitClip( float speed, float height, nbKeyframe* keyframes, b3Quat* rotations, int capacity,
							nbGaitClipDef* clip )
{
	if ( keyframes == NULL || rotations == NULL || clip == NULL || capacity < 2 || ( speed > 0.0f ) == false ||
		 ( height > 0.0f ) == false )
	{
		return 0;
	}
	memset( clip, 0, sizeof( nbGaitClipDef ) );

	b3WorldDef physicsDef = b3DefaultWorldDef();
	b3WorldId physicsWorld = b3CreateWorld( &physicsDef );
	if ( b3World_IsValid( physicsWorld ) == false )
	{
		return 0;
	}
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = (b3Vec3){ 0.0f, -1.0f, 0.0f };
	b3BodyId groundId = b3CreateBody( physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 20.0f, 1.0f, NB_RECORD_GROUND );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3CreateHullShape( groundId, &shapeDef, &box.base );

	nbWorldDef worldDef = nbDefaultWorldDef();
	worldDef.physicsWorld = physicsWorld;
	worldDef.asyncStep = false;
	nbWorldId worldId = nbCreateWorld( &worldDef );
	if ( nbWorld_IsValid( worldId ) == false )
	{
		b3DestroyWorld( physicsWorld );
		return 0;
	}

	nbBoneDef bones[nb_humanoidBoneCount];
	float stand = nbMakeHumanoidBones( height, 74.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureDef figureDef = nbDefaultFigureDef();
	figureDef.rig = &rig;
	figureDef.standHeight = stand;
	figureDef.position = b3ToPos( (b3Vec3){ 0.0f, 0.0f, -NB_RECORD_START } );
	figureDef.steered = true;
	nbFigureId figureId = nbCreateFigure( worldId, &figureDef );
	nbSteer steer = { { 0.0f, 0.0f, speed }, 0.0f, true };
	nbFigure_Steer( figureId, &steer );

	// The idle clip holds its first pose, so the breath it takes does not run through the cycle and the clip loops
	// without a jump
	nbWorld* world = nbGetWorldFromId( worldId );
	world->figures.data[figureId.index1 - 1].clipSpeed = 0.0f;

	float timeStep = 1.0f / NB_RECORD_RATE;
	int settle = (int)( NB_RECORD_SETTLE * NB_RECORD_RATE );
	for ( int i = 0; i < settle; ++i )
	{
		nbWorld_Step( worldId, timeStep, NB_RECORD_SUBSTEPS );
	}

	// From the update after the cycle comes round, the phase of the gait as it goes on over the cycles, and the time a
	// keyframe has at it
	world = nbGetWorldFromId( worldId );
	const nbFigure* f = world->figures.data + ( figureId.index1 - 1 );
	int count = 0;
	bool recording = false;
	bool complete = false;
	float before = f->gaitPhase;
	float start = 0.0f;
	float phase = f->gaitPhase;
	float cycle = 0.0f;
	float stance = 0.0f;
	int most = (int)( NB_RECORD_MOST * NB_RECORD_RATE );
	for ( int i = 0; i < most && nbFigure_GetState( figureId ) == nb_figureAnimated && f->gaitOn; ++i )
	{
		nbWorld_Step( worldId, timeStep, NB_RECORD_SUBSTEPS );
		world = nbGetWorldFromId( worldId );
		f = world->figures.data + ( figureId.index1 - 1 );
		float step = f->gaitPhase - before;
		phase += step - floorf( step );
		bool round = f->gaitPhase < before;
		before = f->gaitPhase;
		if ( recording == false )
		{
			if ( round == false || f->gaitPhaseRate <= 0.0f )
			{
				continue;
			}
			recording = true;
			start = phase;
			cycle = 1.0f / f->gaitPhaseRate;
			stance = f->gaitStance;
		}
		if ( phase - start >= 1.0f )
		{
			complete = true;
			break;
		}
		if ( count + 1 >= capacity )
		{
			break;
		}
		b3Quat* pose = rotations + count * nb_humanoidBoneCount;
		memcpy( pose, f->pose, sizeof( b3Quat ) * nb_humanoidBoneCount );
		keyframes[count] = (nbKeyframe){ ( phase - start ) * cycle, pose, f->root };
		count += 1;
	}

	if ( complete && count > 1 )
	{
		// The cycle ends on the pose it started with, and the feet touch down and lift off where the gait times them
		b3Quat* pose = rotations + count * nb_humanoidBoneCount;
		memcpy( pose, rotations, sizeof( b3Quat ) * nb_humanoidBoneCount );
		keyframes[count] = (nbKeyframe){ cycle, pose, keyframes[0].root };
		count += 1;

		float offset = start - floorf( start );
		for ( int k = 0; k < 2; ++k )
		{
			float down = 0.5f * (float)k - offset;
			float up = down + stance;
			clip->footDown[k] = ( down - floorf( down ) ) * cycle;
			clip->footUp[k] = ( up - floorf( up ) ) * cycle;
		}
		clip->clip.keyframes = keyframes;
		clip->clip.keyframeCount = count;
		clip->clip.boneCount = nb_humanoidBoneCount;
		clip->clip.loop = true;
		clip->clip.moveRoot = true;
		clip->speed = speed;
	}
	else
	{
		count = 0;
	}

	nbDestroyWorld( worldId );
	b3DestroyWorld( physicsWorld );
	return count;
}
