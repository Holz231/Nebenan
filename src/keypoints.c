// SPDX-License-Identifier: MIT

// Poses of the humanoid from the keypoints of a pose estimator, see the roadmap of the game, chapter 32.5: the pelvis
// from the hips and the spine, every limb from the points at its ends, the twist of an arm or a leg from the bend of its
// elbow or knee. What the points do not show keeps its rest pose with confidence 0. The result is a proposal: the
// author checks it in the editor and corrects it before approving it.

#include "articulation.h"
#include "json.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NB_KEYPOINT_FORMAT "nebenan-keypoints"
#define NB_KEYPOINT_VERSION 1

// A bone whose length, measured on the scale of the whole figure, lies outside this range does not fit
#define NB_LENGTH_LOW 0.8f
#define NB_LENGTH_HIGH 1.25f

// The confidence a bone of a misfit length keeps at most
#define NB_MISFIT_CONFIDENCE 0.25f

// The points of the humanoid that the layouts name
typedef enum nbPoint
{
	nb_pointNose,
	nb_pointEarLeft,
	nb_pointEarRight,
	nb_pointShoulderLeft,
	nb_pointShoulderRight,
	nb_pointElbowLeft,
	nb_pointElbowRight,
	nb_pointWristLeft,
	nb_pointWristRight,
	nb_pointHipLeft,
	nb_pointHipRight,
	nb_pointKneeLeft,
	nb_pointKneeRight,
	nb_pointAnkleLeft,
	nb_pointAnkleRight,
	nb_pointHandLeft,
	nb_pointHandRight,
	nb_pointHeelLeft,
	nb_pointHeelRight,
	nb_pointToeLeft,
	nb_pointToeRight,
	nb_pointCount
} nbPoint;

static const int s_layoutCounts[nb_keypointLayoutCount] = { 17, 33 };
static const char* s_layoutNames[nb_keypointLayoutCount] = { "coco17", "mediapipe33" };

// For every point of the humanoid its index in a layout, and a second one for a point between two, like the middle of
// the hand between index finger and little finger. -1 for none.
static const int s_coco[nb_pointCount][2] = {
	{ 0, -1 },	{ 3, -1 },	{ 4, -1 },	{ 5, -1 },	{ 6, -1 },	{ 7, -1 },	{ 8, -1 },
	{ 9, -1 },	{ 10, -1 }, { 11, -1 }, { 12, -1 }, { 13, -1 }, { 14, -1 }, { 15, -1 },
	{ 16, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 },
};
static const int s_mediaPipe[nb_pointCount][2] = {
	{ 0, -1 },	{ 7, -1 },	{ 8, -1 },	{ 11, -1 }, { 12, -1 }, { 13, -1 }, { 14, -1 },
	{ 15, -1 }, { 16, -1 }, { 23, -1 }, { 24, -1 }, { 25, -1 }, { 26, -1 }, { 27, -1 },
	{ 28, -1 }, { 19, 17 }, { 20, 18 }, { 29, -1 }, { 30, -1 }, { 31, -1 }, { 32, -1 },
};

// A rotation whose y axis points along a direction and whose x or z axis lies as near to a hint as it can, a right
// handed frame
static b3Quat nbFrameYX( b3Vec3 y, b3Vec3 hint )
{
	y = b3Normalize( y );
	b3Vec3 x = b3MulSub( hint, b3Dot( hint, y ), y );
	if ( b3Length( x ) < 1.0e-6f )
	{
		x = fabsf( y.x ) < 0.9f ? b3Cross( y, b3Vec3_axisX ) : b3Cross( y, b3Vec3_axisZ );
	}
	x = b3Normalize( x );
	b3Matrix3 m = { x, y, b3Cross( x, y ) };
	return b3NormalizeQuat( b3MakeQuatFromMatrix( &m ) );
}

static b3Quat nbFrameYZ( b3Vec3 y, b3Vec3 hint )
{
	y = b3Normalize( y );
	b3Vec3 z = b3MulSub( hint, b3Dot( hint, y ), y );
	if ( b3Length( z ) < 1.0e-6f )
	{
		z = fabsf( y.z ) < 0.9f ? b3Cross( b3Vec3_axisZ, y ) : b3Cross( b3Vec3_axisX, y );
	}
	z = b3Normalize( z );
	b3Matrix3 m = { b3Cross( y, z ), y, z };
	return b3NormalizeQuat( b3MakeQuatFromMatrix( &m ) );
}

// The least turn of a rotation that makes its y axis point along a direction
static b3Quat nbSwingY( b3Quat rotation, b3Vec3 direction )
{
	b3Vec3 from = b3RotateVector( rotation, b3Vec3_axisY );
	b3Quat swing = b3ComputeQuatBetweenUnitVectors( b3Normalize( from ), b3Normalize( direction ) );
	return b3NormalizeQuat( b3MulQuat( b3NormalizeQuat( swing ), rotation ) );
}

// A bone bent about the hinge of its parent: along a direction, its x axis that of the parent. Built from the axes, not
// from an angle, so it is as exact as the points.
static b3Quat nbHinged( b3Quat parent, b3Vec3 direction )
{
	return nbFrameYX( direction, b3RotateVector( parent, b3Vec3_axisX ) );
}

// The part of a direction across another one, unit length, and whether there is enough of it to tell a direction
static bool nbAcross( b3Vec3 v, b3Vec3 axis, b3Vec3* across )
{
	b3Vec3 unit = b3Normalize( axis );
	b3Vec3 rest = b3MulSub( v, b3Dot( v, unit ), unit );
	float length = b3Length( rest );
	if ( length < 0.05f * b3Length( v ) )
	{
		return false;
	}
	*across = b3MulSV( 1.0f / length, rest );
	return true;
}

static float nbMin3( float a, float b, float c )
{
	return b3MinFloat( a, b3MinFloat( b, c ) );
}

typedef struct nbLimbLength
{
	int bone;
	float measured;
	float expected;
	float confidence;
} nbLimbLength;

int nbHumanoidPoseFromKeypoints( float height, nbKeypointLayout layout, const b3Vec3* points, const float* confidence,
								 b3Quat* rotations, float* boneConfidence, b3Transform* root, nbPoseIssue* issues, int capacity )
{
	if ( points == NULL || rotations == NULL || boneConfidence == NULL || root == NULL || (int)layout < 0 ||
		 layout >= nb_keypointLayoutCount || height <= 0.0f )
	{
		return 0;
	}

	nbBoneDef bones[nb_humanoidBoneCount];
	nbMakeHumanoidBones( height, 75.0f, bones, nb_humanoidBoneCount );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		rotations[i] = b3Quat_identity;
		boneConfidence[i] = 0.0f;
	}
	*root = b3Transform_identity;

	// The points of the humanoid from the layout
	const int( *map )[2] = layout == nb_keypointsCoco17 ? s_coco : s_mediaPipe;
	b3Vec3 p[nb_pointCount];
	float c[nb_pointCount];
	for ( int k = 0; k < nb_pointCount; ++k )
	{
		int a = map[k][0];
		int b = map[k][1];
		p[k] = b3Vec3_zero;
		c[k] = 0.0f;
		if ( a < 0 )
		{
			continue;
		}
		float ca = confidence != NULL ? b3ClampFloat( confidence[a], 0.0f, 1.0f ) : 1.0f;
		p[k] = points[a];
		c[k] = ca;
		if ( b >= 0 )
		{
			float cb = confidence != NULL ? b3ClampFloat( confidence[b], 0.0f, 1.0f ) : 1.0f;
			p[k] = b3MulSV( 0.5f, b3Add( points[a], points[b] ) );
			c[k] = b3MinFloat( ca, cb );
		}
	}

	// The pelvis from the hips and the spine, its origin above the middle of the hip joints as the rig has it
	b3Quat world[nb_humanoidBoneCount];
	b3Vec3 hipMiddle = b3MulSV( 0.5f, b3Add( p[nb_pointHipLeft], p[nb_pointHipRight] ) );
	b3Vec3 shoulderMiddle = b3MulSV( 0.5f, b3Add( p[nb_pointShoulderLeft], p[nb_pointShoulderRight] ) );
	world[nb_humanoidPelvis] = nbFrameYX( b3Sub( shoulderMiddle, hipMiddle ), b3Sub( p[nb_pointHipLeft], p[nb_pointHipRight] ) );
	b3Vec3 restHips =
		b3MulSV( 0.5f, b3Add( bones[nb_humanoidThighLeft].restTransform.p, bones[nb_humanoidThighRight].restTransform.p ) );
	b3Vec3 pelvis = b3Sub( hipMiddle, b3RotateVector( world[nb_humanoidPelvis], restHips ) );
	root->p = pelvis;
	root->q = world[nb_humanoidPelvis];
	float trunk = b3MinFloat( b3MinFloat( c[nb_pointHipLeft], c[nb_pointHipRight] ),
							  b3MinFloat( c[nb_pointShoulderLeft], c[nb_pointShoulderRight] ) );
	boneConfidence[nb_humanoidPelvis] = trunk;

	// The chest from the first joint of the trunk to the shoulders, turned by the line of the shoulders. No point lies on
	// the spine between them, so belly, ribs and chest share the turn from the pelvis to the chest evenly.
	b3Vec3 joint = b3Add( pelvis, b3RotateVector( world[nb_humanoidPelvis], bones[nb_humanoidBelly].restTransform.p ) );
	b3Quat chest = nbFrameYX( b3Sub( shoulderMiddle, joint ), b3Sub( p[nb_pointShoulderLeft], p[nb_pointShoulderRight] ) );
	b3Quat share = nbRootOf( b3InvMulQuat( world[nb_humanoidPelvis], chest ), 3 );
	const int spine[3] = { nb_humanoidBelly, nb_humanoidRibs, nb_humanoidChest };
	for ( int k = 0; k < 3; ++k )
	{
		b3Quat parent = world[bones[spine[k]].parent];
		if ( k > 0 )
		{
			joint = b3Add( joint, b3RotateVector( parent, bones[spine[k]].restTransform.p ) );
		}
		world[spine[k]] = b3NormalizeQuat( b3MulQuat( parent, share ) );
		boneConfidence[spine[k]] = trunk;
	}

	// The neck as the chest has it, no point tells how it bends, and the head from the top of the neck to the middle of the
	// ears, turned by the line of the ears, or toward the nose without them
	world[nb_humanoidNeck] = world[nb_humanoidChest];
	b3Vec3 neckBase = b3Add( joint, b3RotateVector( world[nb_humanoidChest], bones[nb_humanoidNeck].restTransform.p ) );
	b3Vec3 neck = b3Add( neckBase, b3RotateVector( world[nb_humanoidNeck], bones[nb_humanoidHead].restTransform.p ) );
	float ears = b3MinFloat( c[nb_pointEarLeft], c[nb_pointEarRight] );
	if ( ears > 0.0f )
	{
		b3Vec3 earMiddle = b3MulSV( 0.5f, b3Add( p[nb_pointEarLeft], p[nb_pointEarRight] ) );
		world[nb_humanoidHead] = nbFrameYX( b3Sub( earMiddle, neck ), b3Sub( p[nb_pointEarLeft], p[nb_pointEarRight] ) );
		boneConfidence[nb_humanoidHead] = b3MinFloat( ears, trunk );
		boneConfidence[nb_humanoidNeck] = boneConfidence[nb_humanoidHead];
	}
	else if ( c[nb_pointNose] > 0.0f )
	{
		world[nb_humanoidHead] = nbSwingY( world[nb_humanoidChest], b3Sub( p[nb_pointNose], neck ) );
		boneConfidence[nb_humanoidHead] = 0.5f * b3MinFloat( c[nb_pointNose], trunk );
		boneConfidence[nb_humanoidNeck] = boneConfidence[nb_humanoidHead];
	}
	else
	{
		world[nb_humanoidHead] = world[nb_humanoidChest];
	}

	// The arms: the upper arm along shoulder to elbow, turned so the elbow bends the forearm toward its front, the forearm
	// bent about its hinge, the hand toward the middle of the fingers
	for ( int side = 0; side < 2; ++side )
	{
		int upper = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
		int fore = side == 0 ? nb_humanoidForearmLeft : nb_humanoidForearmRight;
		int hand = side == 0 ? nb_humanoidHandLeft : nb_humanoidHandRight;
		b3Vec3 shoulder = p[nb_pointShoulderLeft + side];
		b3Vec3 elbow = p[nb_pointElbowLeft + side];
		b3Vec3 wrist = p[nb_pointWristLeft + side];
		float cs = c[nb_pointShoulderLeft + side];
		float ce = c[nb_pointElbowLeft + side];
		float cw = c[nb_pointWristLeft + side];

		b3Vec3 u = b3Sub( elbow, shoulder );
		b3Vec3 f = b3Sub( wrist, elbow );
		b3Vec3 front;
		b3Quat rest = b3MulQuat( world[nb_humanoidChest], bones[upper].restTransform.q );
		if ( nbAcross( f, u, &front ) )
		{
			world[upper] = nbFrameYZ( u, front );
			boneConfidence[upper] = nbMin3( cs, ce, cw );
		}
		else
		{
			world[upper] = nbSwingY( rest, u );
			boneConfidence[upper] = b3MinFloat( cs, ce );
		}

		world[fore] = nbHinged( world[upper], f );
		boneConfidence[fore] = b3MinFloat( ce, cw );

		float ch = c[nb_pointHandLeft + side];
		if ( ch > 0.0f )
		{
			world[hand] = nbSwingY( world[fore], b3Sub( p[nb_pointHandLeft + side], wrist ) );
			boneConfidence[hand] = b3MinFloat( cw, ch );
		}
		else
		{
			world[hand] = world[fore];
		}
	}

	// The legs: the thigh along hip to knee, turned so the knee bends the shin backward, the shin bent about its hinge,
	// the foot along heel to toe with the ankle above its sole
	for ( int side = 0; side < 2; ++side )
	{
		int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
		int shin = side == 0 ? nb_humanoidShinLeft : nb_humanoidShinRight;
		int foot = side == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight;
		b3Vec3 hip = p[nb_pointHipLeft + side];
		b3Vec3 knee = p[nb_pointKneeLeft + side];
		b3Vec3 ankle = p[nb_pointAnkleLeft + side];
		float chip = c[nb_pointHipLeft + side];
		float ck = c[nb_pointKneeLeft + side];
		float ca = c[nb_pointAnkleLeft + side];

		b3Vec3 t = b3Sub( knee, hip );
		b3Vec3 s = b3Sub( ankle, knee );
		b3Vec3 back;
		b3Quat rest = b3MulQuat( world[nb_humanoidPelvis], bones[thigh].restTransform.q );
		if ( nbAcross( s, t, &back ) )
		{
			world[thigh] = nbFrameYZ( t, b3Neg( back ) );
			boneConfidence[thigh] = nbMin3( chip, ck, ca );
		}
		else
		{
			world[thigh] = nbSwingY( rest, t );
			boneConfidence[thigh] = b3MinFloat( chip, ck );
		}

		world[shin] = nbHinged( world[thigh], s );
		boneConfidence[shin] = b3MinFloat( ck, ca );

		b3Quat footRest = b3MulQuat( world[shin], bones[foot].restTransform.q );
		float cheel = c[nb_pointHeelLeft + side];
		float ctoe = c[nb_pointToeLeft + side];
		if ( b3MinFloat( cheel, ctoe ) > 0.0f )
		{
			b3Vec3 heel = p[nb_pointHeelLeft + side];
			b3Vec3 along = b3Sub( p[nb_pointToeLeft + side], heel );
			b3Vec3 up;
			if ( nbAcross( b3Sub( ankle, heel ), along, &up ) == false )
			{
				up = b3Vec3_axisY;
			}
			world[foot] = nbFrameYZ( along, up );
			boneConfidence[foot] = nbMin3( ca, cheel, ctoe );
		}
		else
		{
			world[foot] = footRest;
		}
	}

	// Every bone against its parent turned by its rest pose
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		b3Quat parent = b3MulQuat( world[bones[i].parent], bones[i].restTransform.q );
		rotations[i] = b3NormalizeQuat( b3InvMulQuat( parent, world[i] ) );
	}

	// The lengths of the limbs against the figure, on the scale of the figure the points show, which may be another
	// person and another unit
	nbLimbLength limbs[10];
	int limbCount = 0;
	for ( int side = 0; side < 2; ++side )
	{
		int upper = side == 0 ? nb_humanoidUpperArmLeft : nb_humanoidUpperArmRight;
		int fore = side == 0 ? nb_humanoidForearmLeft : nb_humanoidForearmRight;
		int thigh = side == 0 ? nb_humanoidThighLeft : nb_humanoidThighRight;
		int shin = side == 0 ? nb_humanoidShinLeft : nb_humanoidShinRight;
		limbs[limbCount++] = (nbLimbLength){ upper, b3Distance( p[nb_pointShoulderLeft + side], p[nb_pointElbowLeft + side] ),
											 b3Length( bones[fore].restTransform.p ),
											 b3MinFloat( c[nb_pointShoulderLeft + side], c[nb_pointElbowLeft + side] ) };
		limbs[limbCount++] =
			(nbLimbLength){ fore, b3Distance( p[nb_pointElbowLeft + side], p[nb_pointWristLeft + side] ),
							b3Length( bones[side == 0 ? nb_humanoidHandLeft : nb_humanoidHandRight].restTransform.p ),
							b3MinFloat( c[nb_pointElbowLeft + side], c[nb_pointWristLeft + side] ) };
		limbs[limbCount++] = (nbLimbLength){ thigh, b3Distance( p[nb_pointHipLeft + side], p[nb_pointKneeLeft + side] ),
											 b3Length( bones[shin].restTransform.p ),
											 b3MinFloat( c[nb_pointHipLeft + side], c[nb_pointKneeLeft + side] ) };
		limbs[limbCount++] =
			(nbLimbLength){ shin, b3Distance( p[nb_pointKneeLeft + side], p[nb_pointAnkleLeft + side] ),
							b3Length( bones[side == 0 ? nb_humanoidFootLeft : nb_humanoidFootRight].restTransform.p ),
							b3MinFloat( c[nb_pointKneeLeft + side], c[nb_pointAnkleLeft + side] ) };
	}
	limbs[limbCount++] = (nbLimbLength){ nb_humanoidChest, b3Distance( p[nb_pointShoulderLeft], p[nb_pointShoulderRight] ),
										 2.0f * fabsf( bones[nb_humanoidUpperArmLeft].restTransform.p.x ), trunk };
	limbs[limbCount++] = (nbLimbLength){ nb_humanoidPelvis, b3Distance( p[nb_pointHipLeft], p[nb_pointHipRight] ),
										 2.0f * fabsf( bones[nb_humanoidThighLeft].restTransform.p.x ), trunk };

	// The scale is the middle of the ratios of the limbs that were seen, so one wrong limb does not move it
	float ratios[10];
	int ratioCount = 0;
	for ( int i = 0; i < limbCount; ++i )
	{
		if ( limbs[i].confidence > 0.0f && limbs[i].expected > 0.0f )
		{
			float ratio = limbs[i].measured / limbs[i].expected;
			int j = ratioCount++;
			while ( j > 0 && ratios[j - 1] > ratio )
			{
				ratios[j] = ratios[j - 1];
				j -= 1;
			}
			ratios[j] = ratio;
		}
	}

	int count = 0;
	if ( ratioCount > 0 )
	{
		float scale =
			ratioCount % 2 == 1 ? ratios[ratioCount / 2] : 0.5f * ( ratios[ratioCount / 2 - 1] + ratios[ratioCount / 2] );
		for ( int i = 0; i < limbCount && scale > 0.0f; ++i )
		{
			if ( limbs[i].confidence <= 0.0f || limbs[i].expected <= 0.0f )
			{
				continue;
			}
			float fit = limbs[i].measured / limbs[i].expected / scale;
			if ( fit < NB_LENGTH_LOW || fit > NB_LENGTH_HIGH )
			{
				if ( issues != NULL && count < capacity )
				{
					issues[count] = (nbPoseIssue){ nb_issueBoneLength, -1, limbs[i].bone, fit };
				}
				count += 1;
			}
		}
	}
	return count;
}

//----------------------------------------------------------------------------------------------------------------------
// Files of keypoints

static void nbKeypointFail( nbPoseClipError* error, const nbJsonDocument* document, int value, const char* message )
{
	if ( error == NULL || error->message[0] != '\0' )
	{
		return;
	}
	const nbJsonValue* at = value >= 0 ? nbJson_Get( document, value ) : NULL;
	error->line = at != NULL ? at->line : 0;
	error->column = at != NULL ? at->column : 0;
	snprintf( error->message, sizeof( error->message ), "%s", message );
}

nbPoseClip* nbParseKeypointClip( const char* text, int length, float height, nbPoseClipError* error )
{
	nbPoseClipError ignored;
	nbPoseClipError* e = error != NULL ? error : &ignored;
	memset( e, 0, sizeof( nbPoseClipError ) );

	nbJsonDocument document;
	if ( nbJson_Parse( &document, text, length, &e->line, &e->column, e->message, (int)sizeof( e->message ) ) == false )
	{
		return NULL;
	}

	const nbJsonDocument* d = &document;
	nbPoseClip* clip = NULL;
	int layout = -1;
	int frameRate = 30;
	if ( nbJson_Get( d, 0 )->type != nb_jsonObject )
	{
		nbKeypointFail( e, d, 0, "a keypoint file is an object" );
	}
	else
	{
		int value = nbJson_Find( d, 0, "format" );
		int version = nbJson_Find( d, 0, "version" );
		int layoutValue = nbJson_Find( d, 0, "layout" );
		int rate = nbJson_Find( d, 0, "frameRate" );
		if ( value < 0 || nbJson_Equals( d, value, NB_KEYPOINT_FORMAT ) == false )
		{
			nbKeypointFail( e, d, value >= 0 ? value : 0, "the format is not " NB_KEYPOINT_FORMAT );
		}
		else if ( version < 0 || nbJson_Get( d, version )->type != nb_jsonNumber ||
				  nbJson_Get( d, version )->number != NB_KEYPOINT_VERSION )
		{
			nbKeypointFail( e, d, version >= 0 ? version : 0, "this library reads version 1 of the keypoint format" );
		}
		for ( int i = 0; i < nb_keypointLayoutCount && layoutValue >= 0; ++i )
		{
			layout = nbJson_Equals( d, layoutValue, s_layoutNames[i] ) ? i : layout;
		}
		if ( layout < 0 )
		{
			nbKeypointFail( e, d, layoutValue >= 0 ? layoutValue : 0, "the layout is coco17 or mediapipe33" );
		}
		if ( rate >= 0 )
		{
			double number = nbJson_Get( d, rate )->type == nb_jsonNumber ? nbJson_Get( d, rate )->number : 0.0;
			if ( number != floor( number ) || number < 1.0 || number > 1000.0 )
			{
				nbKeypointFail( e, d, rate, "the frame rate is a whole number from 1 to 1000" );
			}
			frameRate = (int)number;
		}
	}

	int frames = e->message[0] == '\0' ? nbJson_Find( d, 0, "frames" ) : -1;
	if ( e->message[0] == '\0' && ( frames < 0 || nbJson_Get( d, frames )->type != nb_jsonArray ) )
	{
		nbKeypointFail( e, d, frames >= 0 ? frames : 0, "a keypoint file needs a list of frames" );
	}

	if ( e->message[0] == '\0' )
	{
		nbBoneDef bones[nb_humanoidBoneCount];
		nbMakeHumanoidBones( height, 75.0f, bones, nb_humanoidBoneCount );
		nbRigDef rig = nbDefaultRigDef();
		rig.bones = bones;
		rig.boneCount = nb_humanoidBoneCount;
		int name = nbJson_Find( d, 0, "name" );
		clip = nbCreatePoseClip(
			&rig, NB_HUMANOID_SKELETON,
			name >= 0 && nbJson_Get( d, name )->type == nb_jsonString ? nbJson_String( d, name ) : "keypoints", frameRate );
	}

	int pointCount = layout >= 0 ? s_layoutCounts[layout] : 0;
	b3Vec3 points[33];
	float sure[33];
	int item = frames >= 0 && e->message[0] == '\0' ? frames + 1 : -1;
	int last = -1;
	for ( int f = 0; clip != NULL && item >= 0 && f < nbJson_Get( d, frames )->count && e->message[0] == '\0'; ++f )
	{
		int frameValue = nbJson_Find( d, item, "frame" );
		int list = nbJson_Find( d, item, "points" );
		int confidence = nbJson_Find( d, item, "confidence" );
		int source = nbJson_Find( d, item, "sourceFrame" );
		const nbJsonValue* frameNumber = frameValue >= 0 ? nbJson_Get( d, frameValue ) : NULL;
		if ( frameNumber == NULL || frameNumber->type != nb_jsonNumber || frameNumber->number != floor( frameNumber->number ) ||
			 frameNumber->number <= (double)last || frameNumber->number > 2147483647.0 )
		{
			nbKeypointFail( e, d, frameValue >= 0 ? frameValue : item, "frames need increasing whole numbers from zero on" );
			break;
		}
		if ( list < 0 || nbJson_Get( d, list )->type != nb_jsonArray || nbJson_Get( d, list )->count != pointCount )
		{
			nbKeypointFail( e, d, list >= 0 ? list : item, "a frame needs as many points as the layout has" );
			break;
		}

		int point = list + 1;
		for ( int i = 0; i < pointCount && e->message[0] == '\0'; ++i )
		{
			const nbJsonValue* coordinates = nbJson_Get( d, point );
			if ( coordinates->type != nb_jsonArray || coordinates->count != 3 ||
				 nbJson_Get( d, point + 1 )->type != nb_jsonNumber || nbJson_Get( d, point + 2 )->type != nb_jsonNumber ||
				 nbJson_Get( d, point + 3 )->type != nb_jsonNumber )
			{
				nbKeypointFail( e, d, point, "a point has three numbers, x, y and z in meters with y up" );
				break;
			}
			points[i] = (b3Vec3){ (float)nbJson_Get( d, point + 1 )->number, (float)nbJson_Get( d, point + 2 )->number,
								  (float)nbJson_Get( d, point + 3 )->number };
			sure[i] = 1.0f;
			point = nbJson_Next( d, point );
		}
		if ( confidence >= 0 )
		{
			if ( nbJson_Get( d, confidence )->type != nb_jsonArray || nbJson_Get( d, confidence )->count != pointCount )
			{
				nbKeypointFail( e, d, confidence, "the confidence has a number from 0 to 1 for every point" );
				break;
			}
			int number = confidence + 1;
			for ( int i = 0; i < pointCount; ++i )
			{
				const nbJsonValue* v = nbJson_Get( d, number );
				if ( v->type != nb_jsonNumber || v->number < 0.0 || v->number > 1.0 )
				{
					nbKeypointFail( e, d, number, "the confidence has a number from 0 to 1 for every point" );
					break;
				}
				sure[i] = (float)v->number;
				number = nbJson_Next( d, number );
			}
		}
		if ( e->message[0] != '\0' )
		{
			break;
		}

		b3Quat rotations[nb_humanoidBoneCount];
		float boneConfidence[nb_humanoidBoneCount];
		b3Transform rootTransform;
		nbPoseIssue issues[10];
		int issueCount = nbHumanoidPoseFromKeypoints( height, (nbKeypointLayout)layout, points, sure, rotations, boneConfidence,
													  &rootTransform, issues, 10 );
		for ( int i = 0; i < issueCount && i < 10; ++i )
		{
			boneConfidence[issues[i].bone] = b3MinFloat( boneConfidence[issues[i].bone], NB_MISFIT_CONFIDENCE );
		}

		int frame = (int)frameNumber->number;
		int keyframe = nbPoseClip_AddKeyframe( clip, frame, rotations, rootTransform, nb_originEstimator );
		nbPoseClip_SetConfidence( clip, keyframe, boneConfidence );
		int sourceFrame = frame;
		if ( source >= 0 && nbJson_Get( d, source )->type == nb_jsonNumber )
		{
			sourceFrame = (int)nbJson_Get( d, source )->number;
		}
		nbPoseClip_SetSourceFrame( clip, keyframe, sourceFrame );
		last = frame;
		item = nbJson_Next( d, item );
	}

	nbJson_Free( &document );
	if ( e->message[0] != '\0' )
	{
		nbDestroyPoseClip( clip );
		return NULL;
	}
	return clip;
}
