// lab101.c: lab100.c with the turn of the legs, the wish and the heading of each foot written too. lab100.c script out.csv [seconds] [hz]: lab99.c where a key may also give the heading, "t:vx:vz:degrees", and the figure
// then faces that heading instead of where it moves (nbSteer::faceVelocity false), as a game strafes. lab98.c: lab96.c with
// clips of the gait. A steered humanoid on level ground follows a script
// of velocities, "t:vx:vz,..." meaning that velocity from t seconds on, and every update is written: the time, the frame
// and its heading, the gait phase, which feet swing and how far they roll, the origin and rotation of every bone, and heel
// and toe of both feet. CLIPS=rec gives it the gait of the library recorded at 1.6 and 4.8 m/s as clips of its gait,
// CLIPS=walk only the one at 1.6 m/s, CLIPS=arms both with the upper arms raised 60 degrees forward, CLIPS=bob both with
// the root 3 cm higher; TARGETS=1 writes the targets of the bones instead of the bodies.
#include "nebenan/nebenan.h"

#include "figure.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Key
{
	float t, vx, vz, h;
	int face;
} Key;

int main( int argc, char** argv )
{
	if ( argc < 3 )
	{
		fprintf( stderr, "lab script out.csv [seconds] [hz]\n" );
		return 1;
	}
	// up to a key per update of a course a player steers, see steer162.py
	static Key keys[4096];
	int keyCount = 0;
	char* buffer = malloc( strlen( argv[1] ) + 1 );
	strcpy( buffer, argv[1] );
	for ( char* tok = strtok( buffer, "," ); tok != NULL && keyCount < 4096; tok = strtok( NULL, "," ) )
	{
		Key k = { 0 };
		int n = sscanf( tok, "%f:%f:%f:%f", &k.t, &k.vx, &k.vz, &k.h );
		if ( n >= 3 )
		{
			k.face = n == 3;
			keys[keyCount++] = k;
		}
	}
	float seconds = argc > 3 ? (float)atof( argv[3] ) : 12.0f;
	float hz = argc > 4 ? (float)atof( argv[4] ) : 60.0f;
	// NUDGE=p: with SYNC, the figure goes p percent faster than its keys say until half a second before the change
	// waits for its step. A step of a steady run takes nearly a whole number of updates, 0.733 s against 44 updates at
	// 3.2 m/s, so where a step falls between two updates stays for many steps and depends on how the figure got going:
	// a shin jumped 35.8 or 38.3 mm at most in a steady run after two starts. Standing longer changes nothing, the figure
	// forgets as it stands; going a little faster for a while moves the step against the updates. The same state from
	// other such moments, to tell what a change of the library does from what any other start does.
	float nudge = getenv( "NUDGE" ) ? 1.0f + 0.01f * (float)atof( getenv( "NUDGE" ) ) : 1.0f;

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
	def.workerCount = 1;
	def.asyncStep = false;
	nbWorldId world = nbCreateWorld( &def );
	nbBoneDef bones[nb_humanoidBoneCount];
	float stand = nbMakeHumanoidBones( 1.8f, 74.0f, bones, nb_humanoidBoneCount );
	nbRigDef rig = nbDefaultRigDef();
	rig.bones = bones;
	rig.boneCount = nb_humanoidBoneCount;
	nbFigureDef fd = nbDefaultFigureDef();
	fd.rig = &rig;
	fd.standHeight = stand;
	fd.position = b3ToPos( b3Vec3_zero );
	fd.heading = 0.0f;
	fd.steered = true;
	if ( getenv( "TURNRATE" ) ) fd.steering.turnRate = (float)atof( getenv( "TURNRATE" ) );
	if ( getenv( "ACC" ) ) fd.steering.acceleration = (float)atof( getenv( "ACC" ) );
#ifndef LAB_NO_START
	// START: nbFigureSteering::startAcceleration, in the library from round 169 on
	if ( getenv( "START" ) ) fd.steering.startAcceleration = (float)atof( getenv( "START" ) );
#endif
#ifdef LAB_BEND
	// BEND: nbFigureSteering::walkBend, in the library from round 161 on, so only where the build asks for it
	if ( getenv( "BEND" ) ) fd.steering.walkBend = (float)atof( getenv( "BEND" ) );
#endif
	if ( getenv( "TURNACC" ) ) fd.steering.turnAcceleration = (float)atof( getenv( "TURNACC" ) );
	nbFigureId figure = nbCreateFigure( world, &fd );
	const char* clips = getenv( "CLIPS" );
	static nbKeyframe frames[2][512];
	static b3Quat rots[2][512 * nb_humanoidBoneCount];
	nbGaitClipDef gait[2];
	if ( clips != NULL && strcmp( clips, "0" ) != 0 )
	{
		const float speeds[2] = { 1.6f, 4.8f };
		int count = strcmp( clips, "walk" ) == 0 ? 1 : 2;
		for ( int c = 0; c < count; ++c )
		{
			int n = nbMakeHumanoidGaitClip( speeds[c], 1.8f, frames[c], rots[c], 512, gait + c );
			if ( n == 0 )
			{
				fprintf( stderr, "no clip at %g\n", speeds[c] );
				return 1;
			}
			for ( int k = 0; k < n; ++k )
			{
				if ( strcmp( clips, "arms" ) == 0 )
				{
					b3Quat raise = b3MakeQuatFromAxisAngle( b3Vec3_axisX, -60.0f * B3_PI / 180.0f );
					b3Quat* r = rots[c] + k * nb_humanoidBoneCount;
					r[nb_humanoidUpperArmLeft] = b3MulQuat( r[nb_humanoidUpperArmLeft], raise );
					r[nb_humanoidUpperArmRight] = b3MulQuat( r[nb_humanoidUpperArmRight], raise );
				}
				if ( strcmp( clips, "bob" ) == 0 )
				{
					frames[c][k].root.p.y += 0.03f;
				}
			}
			if ( getenv( "CLIPINFO" ) )
			{
				fprintf( stderr, "clip %g: %d keys, cycle %.4f, down %.4f %.4f up %.4f %.4f\n", speeds[c], n, frames[c][n - 1].time,
						 gait[c].footDown[0], gait[c].footDown[1], gait[c].footUp[0], gait[c].footUp[1] );
			}
		}
		nbFigure_SetGaitClips( figure, gait, count );
	}
	bool targets = getenv( "TARGETS" ) != NULL;
	if ( getenv( "FREEZE" ) )
	{
		nbGetWorldFromId( world )->figures.data[figure.index1 - 1].clipSpeed = 0.0f;
	}
	const nbBoneDef* footDef = bones + nb_humanoidFootLeft;
	b3Vec3 toeLocal = { footDef->center.x, footDef->center.y + footDef->halfExtents.y, footDef->center.z - footDef->halfExtents.z };
	b3Vec3 heelLocal = { footDef->center.x, footDef->center.y - footDef->halfExtents.y, footDef->center.z - footDef->halfExtents.z };

	FILE* out = fopen( argv[2], "w" );
	fprintf( out, "t,gait,phase,swingL,swingR,rollL,rollR,fx,fy,fz,heading,stair,shift" );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		fprintf( out, ",b%dx,b%dy,b%dz,b%dqx,b%dqy,b%dqz,b%dqw", i, i, i, i, i, i, i );
	}
	fprintf( out, ",toeLx,toeLy,toeLz,heelLx,heelLy,heelLz,toeRx,toeRy,toeRz,heelRx,heelRy,heelRz,rootY,crouch,rdrop,rrate,shortL,shortR,shortRateL,shortRateR,alongL,alongR,behindL,behindR,hurryL,hurryR,gaitSpeed,runTurn,bend,tLx,tLy,tLz,tRx,tRy,tRz,svL,svR,legTurn,legAim,legBack,legRate,wishX,wishZ,fhL,fhR,legHeading,gaitRun\n" );
	float dt = 1.0f / hz;
	int steps = (int)( seconds * hz );
	nbWorld* w = nbGetWorldFromId( world );
	// SYNC=t0: the keys after t0 wait until the left foot lifts next, 2 s at most, so a change out of the steady gait
	// comes at the same moment of the step whatever the figure did before, and the run lasts as much longer. shift is
	// how long they waited, written in every row from then on.
	float sync = getenv( "SYNC" ) ? (float)atof( getenv( "SYNC" ) ) : -1.0f;
	bool synced = sync < 0.0f;
	bool swung = false;
	float shift = 0.0f;
	for ( int s = 0; s < steps; ++s )
	{
		float t = (float)s * dt;
		b3Vec3 v = b3Vec3_zero;
		float wantHeading = 0.0f;
		bool face = true;
		for ( int k = 0; k < keyCount; ++k )
		{
			float at = keys[k].t;
			if ( sync >= 0.0f && at > sync )
			{
				if ( synced == false )
				{
					continue;
				}
				at += shift;
			}
			if ( t >= at )
			{
				v = (b3Vec3){ keys[k].vx, 0.0f, keys[k].vz };
				face = keys[k].face != 0;
				wantHeading = face ? 0.0f : keys[k].h * B3_PI / 180.0f;
			}
		}
		if ( sync >= 0.0f && t < sync - 0.5f )
		{
			v = b3MulSV( nudge, v );
		}
		nbSteer steer = { v, wantHeading, face };
		nbFigure_Steer( figure, &steer );
		nbWorld_Step( world, dt, 4 );
		const nbFigure* f = w->figures.data + ( figure.index1 - 1 );
		if ( synced == false && t + dt >= sync && ( ( f->footSwings[0] && swung == false ) || t + dt >= sync + 2.0f ) )
		{
			synced = true;
			shift = t + dt - sync;
			steps += (int)lroundf( shift * hz );
		}
		swung = f->footSwings[0];
		b3WorldTransform frame = nbFigure_GetFrame( figure );
		b3Vec3 p = b3ToVec3( frame.p );
		b3Vec3 fwd = b3RotateVector( frame.q, b3Vec3_axisZ );
		float heading = atan2f( fwd.x, fwd.z );
		fprintf( out, "%.5f,%d,%.5f,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f,%.5f", t + dt, f->gaitOn ? 1 : 0, f->gaitPhase,
				 f->footSwings[0] ? 1 : 0, f->footSwings[1] ? 1 : 0, f->footRoll[0], f->footRoll[1], p.x, p.y, p.z, heading,
				 f->stairWeight, shift );
		for ( int i = 0; i < nb_humanoidBoneCount; ++i )
		{
			b3BodyId body = nbFigure_GetBody( figure, i );
			b3Vec3 bp = b3ToVec3( b3Body_GetPosition( body ) );
			b3Quat q = b3Body_GetRotation( body );
			if ( targets )
			{
				bp = b3ToVec3( f->targets[i].p );
				q = f->targets[i].q;
			}
			fprintf( out, ",%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.6f", bp.x, bp.y, bp.z, q.v.x, q.v.y, q.v.z, q.s );
		}
		for ( int k = 0; k < 2; ++k )
		{
			b3BodyId foot = nbFigure_GetBody( figure, k == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight );
			b3Vec3 tl = toeLocal, hl = heelLocal;
			if ( k == 1 )
			{
				tl.x = -tl.x;
				hl.x = -hl.x;
			}
			b3Vec3 toe = b3ToVec3( b3Body_GetWorldPoint( foot, tl ) );
			b3Vec3 heel = b3ToVec3( b3Body_GetWorldPoint( foot, hl ) );
			if ( targets )
			{
				b3WorldTransform ft = f->targets[k == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight];
				toe = b3ToVec3( b3TransformWorldPoint( ft, tl ) );
				heel = b3ToVec3( b3TransformWorldPoint( ft, hl ) );
			}
			fprintf( out, ",%.5f,%.5f,%.5f,%.5f,%.5f,%.5f", toe.x, toe.y, toe.z, heel.x, heel.y, heel.z );
		}
		fprintf( out, ",%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f,%.4f\n", f->root.p.y, f->gaitCrouch, f->rootDrop, f->rootDropRate,
				 f->footOut[0] < 1.0e6f ? f->footOut[0] : 9.0f, f->footOut[1] < 1.0e6f ? f->footOut[1] : 9.0f,
				 0.0f, 0.0f, f->swingAlong[0], f->swingAlong[1] );
		fseek( out, -1, SEEK_CUR );
		{ b3Vec3 tL = b3ToVec3( f->feet[0].target ), tR = b3ToVec3( f->feet[1].target );
		fprintf( out, ",%d,%d,%.3f,%.3f,%.5f,%.5f,%.4f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f,%.4f\n", f->outBehind[0] ? 1 : 0, f->outBehind[1] ? 1 : 0, f->swingHurry[0], f->swingHurry[1], f->gaitSpeed, f->runTurn, f->gaitBend,
			tL.x, tL.y, tL.z, tR.x, tR.y, tR.z, b3Length( f->swingVelocity[0] ), b3Length( f->swingVelocity[1] ) ); }
		fseek( out, -1, SEEK_CUR );
		fprintf( out, ",%.5f,%.5f,%d,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.4f\n", f->legTurn, f->legAim, f->legBack ? 1 : 0, f->legHeadingRate,
				 f->wishVelocity.x, f->wishVelocity.z, f->footHeading[0], f->footHeading[1], f->legHeading, f->gaitRun );
	}
	fclose( out );
	nbFigureInfo info = nbFigure_GetInfo( figure );
	printf( "done, state %d\n", (int)info.state );
	return 0;
}
