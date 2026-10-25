// SPDX-License-Identifier: MIT

// Authored clips: keyframes with their status, origin, root, contacts and confidence, events, a text format, the rules
// that lock approved keyframes, and checks, see the roadmap of the game, chapters 31.2, 32.5, 32.6 and 33.2. Approved
// keyframes are hard constraints of authoring: proposals, corrections that spread and recorded simulations change only
// proposed keyframes.

#include "articulation.h"
#include "json.h"

#include "nebenan/nebenan.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NB_POSE_FORMAT "nebenan-pose-clip"
#define NB_POSE_VERSION 1
#define NB_NAME_LENGTH 64
#define NB_BONE_NAME_LENGTH 48

// A rotation of other than unit length by more than this is not normalized
#define NB_UNIT_TOLERANCE 1.0e-3f

// A contact point of a recording touches this close, in meters, or once it pushed. Points a contact only expects do not.
#define NB_RECORD_TOUCH 0.005f

typedef struct nbPoseKeyframeData
{
	int frame;
	nbKeyframeStatus status;
	nbKeyframeOrigin origin;
	int sourceFrame;
	b3Transform root;
	uint32_t contacts;
} nbPoseKeyframeData;

struct nbPoseClip
{
	char skeleton[NB_NAME_LENGTH];
	char name[NB_NAME_LENGTH];
	nbPoseClipKind kind;
	int frameRate;
	bool loop;
	int boneCount;
	char ( *boneNames )[NB_BONE_NAME_LENGTH];

	// Sorted by frame. The rotations and confidences of a keyframe start at its index times the number of bones.
	nbPoseKeyframeData* keyframes;
	b3Quat* rotations;
	float* confidence;
	int keyframeCount;
	int keyframeCapacity;

	// Sorted by frame
	nbPoseEvent* events;
	int eventCount;
	int eventCapacity;
};

static const char* s_statusNames[] = { "proposed", "approved" };
static const char* s_originNames[nb_originCount] = { "hand", "estimator", "assistant", "simulation" };
static const char* s_kindNames[nb_clipKindCount] = { "authored", "computed", "simulated" };
static const char* s_contactNames[4] = { "leftFoot", "rightFoot", "leftHand", "rightHand" };
static const char* s_balanceEvents[] = { "off", "stand", "step", "fall", "down", "getUp" };

static void nbCopyName( char* target, int size, const char* source )
{
	snprintf( target, (size_t)size, "%s", source != NULL ? source : "" );
}

//----------------------------------------------------------------------------------------------------------------------
// Storage

static nbPoseClip* nbAllocPoseClip( int boneCount )
{
	nbPoseClip* clip = nbAlloc( sizeof( nbPoseClip ) );
	memset( clip, 0, sizeof( nbPoseClip ) );
	clip->boneCount = boneCount;
	clip->boneNames = nbAlloc( sizeof( *clip->boneNames ) * (size_t)boneCount );
	memset( clip->boneNames, 0, sizeof( *clip->boneNames ) * (size_t)boneCount );
	clip->frameRate = 30;
	return clip;
}

void nbDestroyPoseClip( nbPoseClip* clip )
{
	if ( clip == NULL )
	{
		return;
	}

	size_t bones = (size_t)clip->boneCount;
	size_t capacity = (size_t)clip->keyframeCapacity;
	nbFree( clip->keyframes, sizeof( nbPoseKeyframeData ) * capacity );
	nbFree( clip->rotations, sizeof( b3Quat ) * capacity * bones );
	nbFree( clip->confidence, sizeof( float ) * capacity * bones );
	nbFree( clip->events, sizeof( nbPoseEvent ) * (size_t)clip->eventCapacity );
	nbFree( clip->boneNames, sizeof( *clip->boneNames ) * bones );
	nbFree( clip, sizeof( nbPoseClip ) );
}

static void nbReserveKeyframes( nbPoseClip* clip, int count )
{
	if ( count <= clip->keyframeCapacity )
	{
		return;
	}

	int capacity = clip->keyframeCapacity < 8 ? 8 : 2 * clip->keyframeCapacity;
	while ( capacity < count )
	{
		capacity *= 2;
	}

	size_t bones = (size_t)clip->boneCount;
	size_t old = (size_t)clip->keyframeCapacity;
	clip->keyframes =
		nbGrowAlloc( clip->keyframes, sizeof( nbPoseKeyframeData ) * old, sizeof( nbPoseKeyframeData ) * (size_t)capacity );
	clip->rotations = nbGrowAlloc( clip->rotations, sizeof( b3Quat ) * old * bones, sizeof( b3Quat ) * (size_t)capacity * bones );
	clip->confidence = nbGrowAlloc( clip->confidence, sizeof( float ) * old * bones, sizeof( float ) * (size_t)capacity * bones );
	clip->keyframeCapacity = capacity;
}

static b3Quat* nbRotationsOf( const nbPoseClip* clip, int keyframe )
{
	return clip->rotations + (size_t)keyframe * (size_t)clip->boneCount;
}

static float* nbConfidenceOf( const nbPoseClip* clip, int keyframe )
{
	return clip->confidence + (size_t)keyframe * (size_t)clip->boneCount;
}

// The index a keyframe at a frame has or would get
static int nbLowerBound( const nbPoseClip* clip, int frame )
{
	int lower = 0;
	int upper = clip->keyframeCount;
	while ( lower < upper )
	{
		int middle = ( lower + upper ) / 2;
		if ( clip->keyframes[middle].frame < frame )
		{
			lower = middle + 1;
		}
		else
		{
			upper = middle;
		}
	}
	return lower;
}

// A new keyframe at an index, its arrays uninitialized, the others moved up
static void nbInsertKeyframe( nbPoseClip* clip, int index )
{
	nbReserveKeyframes( clip, clip->keyframeCount + 1 );
	size_t bones = (size_t)clip->boneCount;
	size_t after = (size_t)( clip->keyframeCount - index );
	memmove( clip->keyframes + index + 1, clip->keyframes + index, sizeof( nbPoseKeyframeData ) * after );
	memmove( nbRotationsOf( clip, index + 1 ), nbRotationsOf( clip, index ), sizeof( b3Quat ) * after * bones );
	memmove( nbConfidenceOf( clip, index + 1 ), nbConfidenceOf( clip, index ), sizeof( float ) * after * bones );
	clip->keyframeCount += 1;
}

static void nbEraseKeyframe( nbPoseClip* clip, int index )
{
	size_t bones = (size_t)clip->boneCount;
	size_t after = (size_t)( clip->keyframeCount - index - 1 );
	memmove( clip->keyframes + index, clip->keyframes + index + 1, sizeof( nbPoseKeyframeData ) * after );
	memmove( nbRotationsOf( clip, index ), nbRotationsOf( clip, index + 1 ), sizeof( b3Quat ) * after * bones );
	memmove( nbConfidenceOf( clip, index ), nbConfidenceOf( clip, index + 1 ), sizeof( float ) * after * bones );
	clip->keyframeCount -= 1;
}

static bool nbValidKeyframe( const nbPoseClip* clip, int keyframe )
{
	return clip != NULL && 0 <= keyframe && keyframe < clip->keyframeCount;
}

static bool nbEditable( const nbPoseClip* clip, int keyframe )
{
	return nbValidKeyframe( clip, keyframe ) && clip->keyframes[keyframe].status != nb_keyframeApproved;
}

//----------------------------------------------------------------------------------------------------------------------
// Clips

nbPoseClip* nbCreatePoseClip( const nbRigDef* rig, const char* skeleton, const char* name, int frameRate )
{
	if ( rig == NULL || rig->bones == NULL || rig->boneCount < 1 )
	{
		return NULL;
	}

	NB_ASSERT( rig->internalValue == NB_SECRET_COOKIE );
	nbPoseClip* clip = nbAllocPoseClip( rig->boneCount );
	nbCopyName( clip->skeleton, NB_NAME_LENGTH, skeleton );
	nbCopyName( clip->name, NB_NAME_LENGTH, name );
	clip->frameRate = b3ClampInt( frameRate, 1, 1000 );
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		nbCopyName( clip->boneNames[i], NB_BONE_NAME_LENGTH, rig->bones[i].name );
	}
	return clip;
}

const char* nbPoseClip_GetSkeleton( const nbPoseClip* clip )
{
	return clip != NULL ? clip->skeleton : "";
}

const char* nbPoseClip_GetName( const nbPoseClip* clip )
{
	return clip != NULL ? clip->name : "";
}

void nbPoseClip_SetName( nbPoseClip* clip, const char* name )
{
	if ( clip != NULL )
	{
		nbCopyName( clip->name, NB_NAME_LENGTH, name );
	}
}

int nbPoseClip_GetFrameRate( const nbPoseClip* clip )
{
	return clip != NULL ? clip->frameRate : 0;
}

bool nbPoseClip_IsLooping( const nbPoseClip* clip )
{
	return clip != NULL && clip->loop;
}

void nbPoseClip_SetLooping( nbPoseClip* clip, bool loop )
{
	if ( clip != NULL )
	{
		clip->loop = loop;
	}
}

nbPoseClipKind nbPoseClip_GetKind( const nbPoseClip* clip )
{
	return clip != NULL ? clip->kind : nb_clipAuthored;
}

void nbPoseClip_SetKind( nbPoseClip* clip, nbPoseClipKind kind )
{
	if ( clip != NULL && 0 <= (int)kind && kind < nb_clipKindCount )
	{
		clip->kind = kind;
	}
}

int nbPoseClip_GetBoneCount( const nbPoseClip* clip )
{
	return clip != NULL ? clip->boneCount : 0;
}

const char* nbPoseClip_GetBoneName( const nbPoseClip* clip, int bone )
{
	return clip != NULL && 0 <= bone && bone < clip->boneCount ? clip->boneNames[bone] : "";
}

int nbPoseClip_GetKeyframeCount( const nbPoseClip* clip )
{
	return clip != NULL ? clip->keyframeCount : 0;
}

nbPoseKeyframe nbPoseClip_GetKeyframe( const nbPoseClip* clip, int keyframe )
{
	nbPoseKeyframe result = { 0 };
	result.sourceFrame = -1;
	result.root = b3Transform_identity;
	if ( nbValidKeyframe( clip, keyframe ) == false )
	{
		return result;
	}

	const nbPoseKeyframeData* data = clip->keyframes + keyframe;
	result.frame = data->frame;
	result.status = data->status;
	result.origin = data->origin;
	result.sourceFrame = data->sourceFrame;
	result.root = data->root;
	result.contacts = data->contacts;
	result.rotations = nbRotationsOf( clip, keyframe );
	result.confidence = nbConfidenceOf( clip, keyframe );
	return result;
}

int nbPoseClip_FindKeyframe( const nbPoseClip* clip, int frame )
{
	if ( clip == NULL )
	{
		return -1;
	}

	int index = nbLowerBound( clip, frame );
	return index < clip->keyframeCount && clip->keyframes[index].frame == frame ? index : -1;
}

int nbPoseClip_GetLength( const nbPoseClip* clip )
{
	return clip != NULL && clip->keyframeCount > 0 ? clip->keyframes[clip->keyframeCount - 1].frame : 0;
}

//----------------------------------------------------------------------------------------------------------------------
// Editing

int nbPoseClip_AddKeyframe( nbPoseClip* clip, int frame, const b3Quat* rotations, b3Transform root, nbKeyframeOrigin origin )
{
	if ( clip == NULL || rotations == NULL || frame < 0 )
	{
		return -1;
	}

	int index = nbLowerBound( clip, frame );
	bool exists = index < clip->keyframeCount && clip->keyframes[index].frame == frame;
	if ( exists && clip->keyframes[index].status == nb_keyframeApproved )
	{
		return -1;
	}

	if ( exists == false )
	{
		nbInsertKeyframe( clip, index );
		clip->keyframes[index].sourceFrame = -1;
		clip->keyframes[index].contacts = 0;
	}

	nbPoseKeyframeData* data = clip->keyframes + index;
	data->frame = frame;
	data->status = nb_keyframeProposed;
	data->origin = 0 <= (int)origin && origin < nb_originCount ? origin : nb_originHand;
	data->root = root;
	memcpy( nbRotationsOf( clip, index ), rotations, sizeof( b3Quat ) * (size_t)clip->boneCount );
	float* confidence = nbConfidenceOf( clip, index );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		confidence[i] = 1.0f;
	}
	return index;
}

// The part of a rotation, from none to all of it: the same axis, that part of the angle, the short way round
static b3Quat nbPartOf( b3Quat rotation, float weight )
{
	float angle;
	b3Vec3 axis = b3GetAxisAngle( &angle, b3NormalizeQuat( rotation ) );
	if ( b3LengthSquared( axis ) == 0.0f )
	{
		return b3Quat_identity;
	}
	angle = angle > B3_PI ? angle - 2.0f * B3_PI : angle;
	return b3MakeQuatFromAxisAngle( axis, b3ClampFloat( weight, 0.0f, 1.0f ) * angle );
}

bool nbPoseClip_SetPose( nbPoseClip* clip, int keyframe, const b3Quat* rotations, b3Transform root, int spread )
{
	if ( nbEditable( clip, keyframe ) == false || rotations == NULL )
	{
		return false;
	}

	// The correction spreads to the proposed keyframes around, less the farther they are
	nbPoseKeyframeData* data = clip->keyframes + keyframe;
	const b3Quat* old = nbRotationsOf( clip, keyframe );
	b3Vec3 move = b3Sub( root.p, data->root.p );
	b3Quat turn = b3NormalizeQuat( b3InvMulQuat( data->root.q, root.q ) );
	for ( int k = 0; k < clip->keyframeCount && spread > 0; ++k )
	{
		int distance = abs( clip->keyframes[k].frame - data->frame );
		if ( k == keyframe || distance > spread || clip->keyframes[k].status == nb_keyframeApproved )
		{
			continue;
		}

		float weight = 1.0f - (float)distance / (float)( spread + 1 );
		b3Quat* neighbour = nbRotationsOf( clip, k );
		for ( int i = 0; i < clip->boneCount; ++i )
		{
			b3Quat delta = b3NormalizeQuat( b3InvMulQuat( old[i], rotations[i] ) );
			neighbour[i] = b3NormalizeQuat( b3MulQuat( neighbour[i], nbPartOf( delta, weight ) ) );
		}
		nbPoseKeyframeData* other = clip->keyframes + k;
		other->root.p = b3MulAdd( other->root.p, weight, move );
		other->root.q = b3NormalizeQuat( b3MulQuat( other->root.q, nbPartOf( turn, weight ) ) );
	}

	// Bones set by hand are sure, the others keep their confidence
	float* confidence = nbConfidenceOf( clip, keyframe );
	b3Quat* target = nbRotationsOf( clip, keyframe );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		if ( memcmp( target + i, rotations + i, sizeof( b3Quat ) ) != 0 )
		{
			confidence[i] = 1.0f;
		}
		target[i] = rotations[i];
	}
	data->root = root;
	data->origin = nb_originHand;
	return true;
}

bool nbPoseClip_MoveKeyframe( nbPoseClip* clip, int keyframe, int frame )
{
	if ( nbEditable( clip, keyframe ) == false || frame < 0 )
	{
		return false;
	}
	if ( clip->keyframes[keyframe].frame == frame )
	{
		return true;
	}
	if ( nbPoseClip_FindKeyframe( clip, frame ) >= 0 )
	{
		return false;
	}

	// Out and in again at its place, with all it holds
	nbPoseKeyframeData data = clip->keyframes[keyframe];
	size_t bones = (size_t)clip->boneCount;
	b3Quat* rotations = nbAlloc( sizeof( b3Quat ) * bones );
	float* confidence = nbAlloc( sizeof( float ) * bones );
	memcpy( rotations, nbRotationsOf( clip, keyframe ), sizeof( b3Quat ) * bones );
	memcpy( confidence, nbConfidenceOf( clip, keyframe ), sizeof( float ) * bones );
	nbEraseKeyframe( clip, keyframe );

	int index = nbLowerBound( clip, frame );
	nbInsertKeyframe( clip, index );
	data.frame = frame;
	clip->keyframes[index] = data;
	memcpy( nbRotationsOf( clip, index ), rotations, sizeof( b3Quat ) * bones );
	memcpy( nbConfidenceOf( clip, index ), confidence, sizeof( float ) * bones );
	nbFree( rotations, sizeof( b3Quat ) * bones );
	nbFree( confidence, sizeof( float ) * bones );
	return true;
}

bool nbPoseClip_RemoveKeyframe( nbPoseClip* clip, int keyframe )
{
	if ( nbEditable( clip, keyframe ) == false )
	{
		return false;
	}
	nbEraseKeyframe( clip, keyframe );
	return true;
}

void nbPoseClip_Approve( nbPoseClip* clip, int keyframe, bool approved )
{
	if ( nbValidKeyframe( clip, keyframe ) )
	{
		clip->keyframes[keyframe].status = approved ? nb_keyframeApproved : nb_keyframeProposed;
	}
}

bool nbPoseClip_SetContacts( nbPoseClip* clip, int keyframe, uint32_t contacts )
{
	if ( nbEditable( clip, keyframe ) == false )
	{
		return false;
	}
	clip->keyframes[keyframe].contacts = contacts & 15u;
	return true;
}

bool nbPoseClip_SetSourceFrame( nbPoseClip* clip, int keyframe, int sourceFrame )
{
	if ( nbEditable( clip, keyframe ) == false )
	{
		return false;
	}
	clip->keyframes[keyframe].sourceFrame = sourceFrame < 0 ? -1 : sourceFrame;
	return true;
}

bool nbPoseClip_SetConfidence( nbPoseClip* clip, int keyframe, const float* confidence )
{
	if ( nbEditable( clip, keyframe ) == false || confidence == NULL )
	{
		return false;
	}
	float* target = nbConfidenceOf( clip, keyframe );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		target[i] = b3ClampFloat( confidence[i], 0.0f, 1.0f );
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Events

int nbPoseClip_GetEventCount( const nbPoseClip* clip )
{
	return clip != NULL ? clip->eventCount : 0;
}

nbPoseEvent nbPoseClip_GetEvent( const nbPoseClip* clip, int event )
{
	nbPoseEvent result = { 0 };
	if ( clip != NULL && 0 <= event && event < clip->eventCount )
	{
		result = clip->events[event];
	}
	return result;
}

int nbPoseClip_AddEvent( nbPoseClip* clip, int frame, const char* name )
{
	if ( clip == NULL )
	{
		return -1;
	}

	if ( clip->eventCount == clip->eventCapacity )
	{
		int capacity = clip->eventCapacity < 4 ? 4 : 2 * clip->eventCapacity;
		clip->events = nbGrowAlloc( clip->events, sizeof( nbPoseEvent ) * (size_t)clip->eventCapacity,
									sizeof( nbPoseEvent ) * (size_t)capacity );
		clip->eventCapacity = capacity;
	}

	// After the events at the same frame, so they keep the order they came in
	int index = clip->eventCount;
	while ( index > 0 && clip->events[index - 1].frame > frame )
	{
		index -= 1;
	}
	memmove( clip->events + index + 1, clip->events + index, sizeof( nbPoseEvent ) * (size_t)( clip->eventCount - index ) );
	clip->events[index].frame = frame;
	nbCopyName( clip->events[index].name, (int)sizeof( clip->events[index].name ), name );
	clip->eventCount += 1;
	return index;
}

void nbPoseClip_RemoveEvent( nbPoseClip* clip, int event )
{
	if ( clip == NULL || event < 0 || event >= clip->eventCount )
	{
		return;
	}
	memmove( clip->events + event, clip->events + event + 1, sizeof( nbPoseEvent ) * (size_t)( clip->eventCount - event - 1 ) );
	clip->eventCount -= 1;
}

//----------------------------------------------------------------------------------------------------------------------
// Proposals and sampling

static bool nbSameBones( const nbPoseClip* a, const nbPoseClip* b )
{
	if ( a->boneCount != b->boneCount )
	{
		return false;
	}
	for ( int i = 0; i < a->boneCount; ++i )
	{
		if ( strcmp( a->boneNames[i], b->boneNames[i] ) != 0 )
		{
			return false;
		}
	}
	return true;
}

int nbPoseClip_MergeProposals( nbPoseClip* clip, const nbPoseClip* proposals, int* refused )
{
	if ( refused != NULL )
	{
		*refused = 0;
	}
	if ( clip == NULL || proposals == NULL || nbSameBones( clip, proposals ) == false || clip->frameRate != proposals->frameRate )
	{
		return -1;
	}

	size_t bones = (size_t)clip->boneCount;
	int merged = 0;
	for ( int k = 0; k < proposals->keyframeCount; ++k )
	{
		const nbPoseKeyframeData* proposal = proposals->keyframes + k;
		int index = nbLowerBound( clip, proposal->frame );
		bool exists = index < clip->keyframeCount && clip->keyframes[index].frame == proposal->frame;
		if ( exists && clip->keyframes[index].status == nb_keyframeApproved )
		{
			// The hard constraint of authoring
			if ( refused != NULL )
			{
				*refused += 1;
			}
			continue;
		}

		if ( exists == false )
		{
			nbInsertKeyframe( clip, index );
		}
		clip->keyframes[index] = *proposal;
		clip->keyframes[index].status = nb_keyframeProposed;
		memcpy( nbRotationsOf( clip, index ), nbRotationsOf( proposals, k ), sizeof( b3Quat ) * bones );
		memcpy( nbConfidenceOf( clip, index ), nbConfidenceOf( proposals, k ), sizeof( float ) * bones );
		merged += 1;
	}
	return merged;
}

void nbPoseClip_Sample( const nbPoseClip* clip, float frame, b3Quat* rotations, b3Transform* root )
{
	if ( clip == NULL || rotations == NULL )
	{
		return;
	}

	int bones = clip->boneCount;
	if ( clip->keyframeCount == 0 )
	{
		for ( int i = 0; i < bones; ++i )
		{
			rotations[i] = b3Quat_identity;
		}
		if ( root != NULL )
		{
			*root = b3Transform_identity;
		}
		return;
	}

	// The keyframes around the frame, the first or last before or after the clip
	int next = nbLowerBound( clip, (int)ceilf( frame ) );
	if ( next >= clip->keyframeCount || ( next == 0 && frame <= (float)clip->keyframes[0].frame ) )
	{
		int only = next >= clip->keyframeCount ? clip->keyframeCount - 1 : 0;
		memcpy( rotations, nbRotationsOf( clip, only ), sizeof( b3Quat ) * (size_t)bones );
		if ( root != NULL )
		{
			*root = clip->keyframes[only].root;
		}
		return;
	}

	int previous = next > 0 && (float)clip->keyframes[next].frame > frame ? next - 1 : next;
	const nbPoseKeyframeData* a = clip->keyframes + previous;
	const nbPoseKeyframeData* b = clip->keyframes + next;
	float span = (float)( b->frame - a->frame );
	float t = span > 0.0f ? b3ClampFloat( ( frame - (float)a->frame ) / span, 0.0f, 1.0f ) : 0.0f;
	const b3Quat* ra = nbRotationsOf( clip, previous );
	const b3Quat* rb = nbRotationsOf( clip, next );
	for ( int i = 0; i < bones; ++i )
	{
		rotations[i] = b3NLerp( ra[i], rb[i], t );
	}
	if ( root != NULL )
	{
		root->p = b3Lerp( a->root.p, b->root.p, t );
		root->q = b3NLerp( a->root.q, b->root.q, t );
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Poses of rigs

void nbComputePoseTransforms( const nbRigDef* rig, b3Transform root, const b3Quat* rotations, b3Transform* transforms )
{
	if ( rig == NULL || rotations == NULL || transforms == NULL || rig->boneCount < 1 )
	{
		return;
	}

	transforms[0] = (b3Transform){ root.p, b3NormalizeQuat( root.q ) };
	for ( int i = 1; i < rig->boneCount; ++i )
	{
		const nbBoneDef* bone = rig->bones + i;
		b3Transform local = { bone->restTransform.p, b3MulQuat( bone->restTransform.q, b3NormalizeQuat( rotations[i] ) ) };
		transforms[i] = b3MulTransforms( transforms[bone->parent], local );
	}
}

// The lowest point of the shape of a bone along y, and the middle of the shape
static float nbShapeBottom( const nbBoneDef* bone, b3Transform transform, b3Vec3* middle )
{
	if ( bone->shape == nb_boxBone )
	{
		float lowest = FLT_MAX;
		for ( int k = 0; k < 8; ++k )
		{
			b3Vec3 corner = { k & 1 ? bone->halfExtents.x : -bone->halfExtents.x,
							  k & 2 ? bone->halfExtents.y : -bone->halfExtents.y,
							  k & 4 ? bone->halfExtents.z : -bone->halfExtents.z };
			lowest = b3MinFloat( lowest, b3TransformPoint( transform, b3Add( bone->center, corner ) ).y );
		}
		*middle = b3TransformPoint( transform, bone->center );
		return lowest;
	}

	b3Vec3 start = b3TransformPoint( transform, bone->start );
	b3Vec3 end = b3TransformPoint( transform, bone->end );
	*middle = b3MulSV( 0.5f, b3Add( start, end ) );
	return b3MinFloat( start.y, end.y ) - bone->radius;
}

// The angle of a rotation, the short way round
static float nbTurnAngle( b3Quat q )
{
	float angle = b3GetQuatAngle( b3NormalizeQuat( q ) );
	return angle > B3_PI ? 2.0f * B3_PI - angle : angle;
}

// How far a joint is turned beyond its limits, zero within
static float nbBeyondLimits( const nbBoneDef* bone, b3Quat rotation )
{
	b3Quat q = b3NormalizeQuat( rotation );
	b3Quat frame = b3NormalizeQuat( bone->jointFrame );
	b3Quat relative = b3NormalizeQuat( b3MulQuat( b3MulQuat( b3Conjugate( frame ), q ), frame ) );
	switch ( bone->joint )
	{
		case nb_hingeJoint:
		{
			float angle = b3GetTwistAngle( relative );
			float beyond = b3MaxFloat( bone->lowerAngle - angle, angle - bone->upperAngle );
			return b3MaxFloat( b3MaxFloat( beyond, 0.0f ), b3GetSwingAngle( relative ) );
		}
		case nb_ballJoint:
		{
			float swing = b3GetSwingAngle( relative ) - b3ClampFloat( bone->coneAngle, 0.0f, 0.5f * B3_PI );
			float twist = b3GetTwistAngle( relative );
			float beyond = b3MaxFloat( bone->lowerAngle - twist, twist - bone->upperAngle );
			return b3MaxFloat( b3MaxFloat( swing, beyond ), 0.0f );
		}
		default:
			return nbTurnAngle( q );
	}
}

nbPoseCheckDef nbDefaultPoseCheckDef( void )
{
	nbPoseCheckDef def = { 0 };
	def.groundHeight = 0.0f;
	def.groundTolerance = 0.01f;
	def.minConfidence = 0.5f;
	def.maxTurnSpeed = 25.0f;
	def.maxRootSpeed = 10.0f;
	def.maxRootTurnSpeed = 15.0f;
	def.slideDistance = 0.03f;
	def.limitTolerance = 0.02f;
	def.feet[0] = nb_humanoidFootLeft;
	def.feet[1] = nb_humanoidFootRight;
	def.hands[0] = nb_humanoidHandLeft;
	def.hands[1] = nb_humanoidHandRight;
	def.internalValue = NB_SECRET_COOKIE;
	return def;
}

typedef struct nbIssueList
{
	nbPoseIssue* issues;
	int capacity;
	int count;
} nbIssueList;

static void nbAddIssue( nbIssueList* list, nbPoseIssueKind kind, int keyframe, int bone, float value )
{
	if ( list->count < list->capacity )
	{
		list->issues[list->count] = (nbPoseIssue){ kind, keyframe, bone, value };
	}
	list->count += 1;
}

static int nbFindBone( const nbRigDef* rig, const char* name )
{
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		if ( rig->bones[i].name != NULL && strcmp( rig->bones[i].name, name ) == 0 )
		{
			return i;
		}
	}
	return -1;
}

int nbPoseClip_Check( const nbPoseClip* clip, const nbRigDef* rig, const nbPoseCheckDef* def, nbPoseIssue* issues, int capacity )
{
	if ( clip == NULL || rig == NULL || def == NULL || rig->bones == NULL )
	{
		return 0;
	}

	NB_ASSERT( def->internalValue == NB_SECRET_COOKIE );
	nbIssueList list = { issues, issues != NULL ? capacity : 0, 0 };

	// The bones by name and in the order of the rig
	for ( int i = 0; i < rig->boneCount; ++i )
	{
		bool found = false;
		for ( int j = 0; j < clip->boneCount && found == false; ++j )
		{
			found = rig->bones[i].name != NULL && strcmp( rig->bones[i].name, clip->boneNames[j] ) == 0;
		}
		if ( found == false )
		{
			nbAddIssue( &list, nb_issueBoneMissing, -1, i, 0.0f );
		}
	}
	for ( int j = 0; j < clip->boneCount; ++j )
	{
		if ( nbFindBone( rig, clip->boneNames[j] ) != j )
		{
			nbAddIssue( &list, nb_issueBoneUnknown, -1, j, 0.0f );
		}
	}
	if ( list.count > 0 )
	{
		return list.count;
	}

	// Where the feet and hands in contact touched, left then right, see nbPoseCheckDef::slideDistance
	int bones = clip->boneCount;
	int limbs[4] = { def->feet[0], def->feet[1], def->hands[0], def->hands[1] };
	b3Vec3 touched[4] = { 0 };
	bool touching[4] = { false, false, false, false };
	b3Transform* transforms = nbAlloc( sizeof( b3Transform ) * (size_t)bones );
	for ( int k = 0; k < clip->keyframeCount; ++k )
	{
		const nbPoseKeyframeData* data = clip->keyframes + k;
		const b3Quat* rotations = nbRotationsOf( clip, k );
		const float* confidence = nbConfidenceOf( clip, k );

		float rootLength = sqrtf( b3DotQuat( data->root.q, data->root.q ) );
		if ( fabsf( rootLength - 1.0f ) > NB_UNIT_TOLERANCE )
		{
			nbAddIssue( &list, nb_issueNotNormalized, k, 0, rootLength );
		}

		// Every bone: unit length, within its limits, and how sure the source was. The rotation of the root is unused.
		for ( int i = 1; i < bones; ++i )
		{
			float length = sqrtf( b3DotQuat( rotations[i], rotations[i] ) );
			if ( fabsf( length - 1.0f ) > NB_UNIT_TOLERANCE )
			{
				nbAddIssue( &list, nb_issueNotNormalized, k, i, length );
			}
			float beyond = length > 0.0f ? nbBeyondLimits( rig->bones + i, rotations[i] ) : 0.0f;
			if ( beyond > def->limitTolerance )
			{
				nbAddIssue( &list, nb_issueJointLimit, k, i, beyond );
			}
		}
		for ( int i = 0; i < bones; ++i )
		{
			if ( confidence[i] < def->minConfidence )
			{
				nbAddIssue( &list, nb_issueUnsure, k, i, confidence[i] );
			}
		}

		// Into the ground
		nbComputePoseTransforms( rig, data->root, rotations, transforms );
		for ( int i = 0; i < bones; ++i )
		{
			b3Vec3 middle;
			float depth = def->groundHeight - nbShapeBottom( rig->bones + i, transforms[i], &middle );
			if ( depth > def->groundTolerance )
			{
				nbAddIssue( &list, nb_issueGround, k, i, depth );
			}
		}

		// From the keyframe before: jumps, and feet and hands that slide
		if ( k > 0 )
		{
			const nbPoseKeyframeData* before = clip->keyframes + k - 1;
			const b3Quat* earlier = nbRotationsOf( clip, k - 1 );
			float seconds = (float)( data->frame - before->frame ) / (float)clip->frameRate;
			for ( int i = 1; i < bones; ++i )
			{
				float speed = nbTurnAngle( b3InvMulQuat( earlier[i], rotations[i] ) ) / seconds;
				if ( speed > def->maxTurnSpeed )
				{
					nbAddIssue( &list, nb_issueJump, k, i, speed );
				}
			}
			float rootSpeed = b3Length( b3Sub( data->root.p, before->root.p ) ) / seconds;
			if ( rootSpeed > def->maxRootSpeed )
			{
				nbAddIssue( &list, nb_issueJump, k, -1, rootSpeed );
			}
			float rootTurn = nbTurnAngle( b3InvMulQuat( before->root.q, data->root.q ) ) / seconds;
			if ( rootTurn > def->maxRootTurnSpeed )
			{
				nbAddIssue( &list, nb_issueJump, k, 0, rootTurn );
			}
		}

		// Feet and hands in contact that move away from where they touched: a slow slide over many keyframes as well as a
		// quick one. A slide found, they touch where they are now.
		for ( int c = 0; c < 4; ++c )
		{
			int limb = limbs[c];
			if ( limb < 0 || limb >= bones || ( data->contacts & ( 1u << c ) ) == 0 )
			{
				touching[c] = false;
				continue;
			}

			b3Vec3 at;
			nbShapeBottom( rig->bones + limb, transforms[limb], &at );
			float slid = touching[c] ? b3Length( b3Sub( at, touched[c] ) ) : 0.0f;
			if ( slid > def->slideDistance )
			{
				nbAddIssue( &list, nb_issueSliding, k, limb, slid );
			}
			if ( touching[c] == false || slid > def->slideDistance )
			{
				touched[c] = at;
				touching[c] = true;
			}
		}
	}

	nbFree( transforms, sizeof( b3Transform ) * (size_t)bones );
	return list.count;
}

float nbPoseClip_Compare( const nbPoseClip* clip, const nbPoseClip* other, int* frame, int* bone )
{
	float largest = 0.0f;
	int largestFrame = -1;
	int largestBone = -1;
	if ( clip != NULL && other != NULL && clip->keyframeCount > 0 && nbSameBones( clip, other ) )
	{
		size_t bones = (size_t)clip->boneCount;
		b3Quat* rotations = nbAlloc( sizeof( b3Quat ) * bones );
		for ( int k = 0; k < other->keyframeCount; ++k )
		{
			// This clip at the time of the keyframe of the other
			int at = other->keyframes[k].frame;
			b3Transform root;
			nbPoseClip_Sample( clip, (float)at * (float)clip->frameRate / (float)other->frameRate, rotations, &root );
			const b3Quat* theirs = nbRotationsOf( other, k );
			for ( int i = 1; i < clip->boneCount; ++i )
			{
				float angle = nbTurnAngle( b3InvMulQuat( rotations[i], theirs[i] ) );
				if ( angle > largest )
				{
					largest = angle;
					largestFrame = at;
					largestBone = i;
				}
			}
		}
		nbFree( rotations, sizeof( b3Quat ) * bones );
	}

	if ( frame != NULL )
	{
		*frame = largestFrame;
	}
	if ( bone != NULL )
	{
		*bone = largestBone;
	}
	return largest;
}

//----------------------------------------------------------------------------------------------------------------------
// Figures

bool nbArticulation_PlayPoseClip( nbArticulationId articulationId, const nbPoseClip* clip, float speed )
{
	if ( clip == NULL || clip->keyframeCount == 0 || nbArticulation_GetBoneCount( articulationId ) != clip->boneCount )
	{
		return false;
	}

	// The clip of the figure copies the keyframes
	nbKeyframe* keyframes = nbAlloc( sizeof( nbKeyframe ) * (size_t)clip->keyframeCount );
	for ( int k = 0; k < clip->keyframeCount; ++k )
	{
		keyframes[k].time = (float)clip->keyframes[k].frame / (float)clip->frameRate;
		keyframes[k].rotations = nbRotationsOf( clip, k );
	}
	nbClipDef def = { keyframes, clip->keyframeCount, clip->boneCount, clip->loop };
	nbArticulation_PlayClip( articulationId, &def, speed );
	nbFree( keyframes, sizeof( nbKeyframe ) * (size_t)clip->keyframeCount );
	return true;
}

nbPoseClip* nbCreatePoseClipFromClip( const nbRigDef* rig, const char* skeleton, const char* name, int frameRate,
									  const nbClipDef* clip )
{
	if ( rig == NULL || clip == NULL || clip->keyframes == NULL || clip->boneCount != rig->boneCount )
	{
		return NULL;
	}

	nbPoseClip* pose = nbCreatePoseClip( rig, skeleton, name, frameRate );
	if ( pose == NULL )
	{
		return NULL;
	}

	pose->loop = clip->loop;
	size_t bones = (size_t)rig->boneCount;
	b3Transform* transforms = nbAlloc( sizeof( b3Transform ) * bones );
	for ( int k = 0; k < clip->keyframeCount; ++k )
	{
		// The root turned as the keyframe says, lifted until its lowest bone touches the ground
		const nbKeyframe* key = clip->keyframes + k;
		b3Transform root = { b3Vec3_zero, b3NormalizeQuat( key->rotations[0] ) };
		nbComputePoseTransforms( rig, root, key->rotations, transforms );
		float lowest = FLT_MAX;
		for ( int i = 0; i < rig->boneCount; ++i )
		{
			b3Vec3 middle;
			lowest = b3MinFloat( lowest, nbShapeBottom( rig->bones + i, transforms[i], &middle ) );
		}
		root.p.y = -lowest;
		int frame = (int)floorf( key->time * (float)pose->frameRate + 0.5f );
		nbPoseClip_AddKeyframe( pose, frame, key->rotations, root, nb_originHand );
	}
	nbFree( transforms, sizeof( b3Transform ) * bones );
	return pose;
}

int nbPoseClip_GetClip( const nbPoseClip* clip, nbKeyframe* keyframes, b3Quat* rotations, int capacity )
{
	if ( clip == NULL || keyframes == NULL || rotations == NULL || clip->keyframeCount > capacity )
	{
		return 0;
	}

	size_t bones = (size_t)clip->boneCount;
	for ( int k = 0; k < clip->keyframeCount; ++k )
	{
		b3Quat* target = rotations + (size_t)k * bones;
		memcpy( target, nbRotationsOf( clip, k ), sizeof( b3Quat ) * bones );
		target[0] = clip->keyframes[k].root.q;
		keyframes[k] = (nbKeyframe){ (float)clip->keyframes[k].frame / (float)clip->frameRate, target };
	}
	return clip->keyframeCount;
}

int nbPoseClip_RecordArticulation( nbPoseClip* clip, nbArticulationId articulationId, int frame )
{
	if ( clip == NULL || nbArticulation_GetBoneCount( articulationId ) != clip->boneCount )
	{
		return -1;
	}

	size_t bones = (size_t)clip->boneCount;
	b3Quat* rotations = nbAlloc( sizeof( b3Quat ) * bones );
	b3WorldTransform root;
	nbArticulation_GetPose( articulationId, rotations, clip->boneCount );
	nbArticulation_GetBoneTransforms( articulationId, &root, 1 );
	b3Transform local = { b3ToVec3( root.p ), root.q };
	int index = nbPoseClip_AddKeyframe( clip, frame, rotations, local, nb_originSimulation );
	nbFree( rotations, sizeof( b3Quat ) * bones );
	return index;
}

bool nbArticulation_RecordPoseClip( nbArticulationId articulationId, nbPoseClip* clip, const nbPoseCheckDef* def )
{
	nbArticulation* articulation = nbGetArticulationFromId( articulationId, NULL );
	if ( articulation == NULL )
	{
		return false;
	}

	articulation->recording = NULL;
	if ( clip == NULL || clip->boneCount != articulation->boneCount || articulation->clipPlaying == false )
	{
		return false;
	}

	nbPoseCheckDef humanoid = nbDefaultPoseCheckDef();
	def = def != NULL ? def : &humanoid;
	int limbs[4] = { def->feet[0], def->feet[1], def->hands[0], def->hands[1] };
	for ( int c = 0; c < 4; ++c )
	{
		articulation->recordBones[c] = 0 <= limbs[c] && limbs[c] < articulation->boneCount ? limbs[c] : -1;
	}

	// One pass of the clip that plays, in frames of the clip it records into
	float duration = articulation->clipTimes[articulation->clipKeyframeCount - 1];
	articulation->recording = clip;
	articulation->recordFrame = -1;
	articulation->recordEnd = (int)floorf( duration * (float)clip->frameRate + 0.5f );
	articulation->recordBalance = nbBalanceStateOf( articulation );
	return true;
}

bool nbArticulation_IsRecording( nbArticulationId articulationId )
{
	const nbArticulation* articulation = nbGetArticulationFromId( articulationId, NULL );
	return articulation != NULL && articulation->recording != NULL;
}

// Which feet and hands of a recording touch something that is not the figure, as contact flags
static uint32_t nbRecordContacts( const nbWorld* world, int index, const nbArticulation* articulation )
{
	uint32_t contacts = 0;
	for ( int c = 0; c < 4; ++c )
	{
		int bone = articulation->recordBones[c];
		if ( bone < 0 )
		{
			continue;
		}

		b3BodyId bodyId = articulation->bones[bone].bodyId;
		b3ContactData data[8];
		int count = b3Body_GetContactData( bodyId, data, 8 );
		for ( int i = 0; i < count; ++i )
		{
			b3BodyId bodyA = b3Shape_GetBody( data[i].shapeIdA );
			b3BodyId other = B3_ID_EQUALS( bodyA, bodyId ) ? b3Shape_GetBody( data[i].shapeIdB ) : bodyA;
			if ( nbIsArticulationBody( world, index, other ) )
			{
				continue;
			}

			for ( int m = 0; m < data[i].manifoldCount; ++m )
			{
				const b3Manifold* manifold = data[i].manifolds + m;
				for ( int p = 0; p < manifold->pointCount; ++p )
				{
					const b3ManifoldPoint* point = manifold->points + p;
					if ( point->separation <= NB_RECORD_TOUCH || point->totalNormalImpulse > 0.0f )
					{
						contacts |= 1u << c;
					}
				}
			}
		}
	}
	return contacts;
}

void nbRecordPose( nbWorld* world, int index, nbArticulation* articulation, float timeStep )
{
	nbPoseClip* clip = articulation->recording;

	// The frame of the time of the clip from half a step before it on, so every frame lands on the step nearest to it
	float rate = (float)clip->frameRate;
	float at = articulation->clipTime * rate + 0.5f * fabsf( articulation->clipSpeed ) * timeStep * rate;
	int frame = b3MinInt( (int)floorf( at ), articulation->recordEnd );

	// A change of balance as an event
	nbBalanceState balance = nbBalanceStateOf( articulation );
	if ( balance != articulation->recordBalance )
	{
		nbPoseClip_AddEvent( clip, b3MaxInt( frame, 0 ), s_balanceEvents[balance] );
		articulation->recordBalance = balance;
	}

	if ( frame <= articulation->recordFrame )
	{
		return;
	}

	// The pose the bodies have, with the root where its body is
	size_t bones = (size_t)articulation->boneCount;
	b3Quat* rotations = nbAlloc( sizeof( b3Quat ) * bones );
	nbGetPose( articulation, rotations, articulation->boneCount );
	b3WorldTransform transform = b3Body_GetTransform( articulation->bones[0].bodyId );
	b3Transform root = { b3ToVec3( transform.p ), transform.q };
	int keyframe = nbPoseClip_AddKeyframe( clip, frame, rotations, root, nb_originSimulation );
	nbFree( rotations, sizeof( b3Quat ) * bones );
	if ( keyframe >= 0 )
	{
		clip->keyframes[keyframe].contacts = nbRecordContacts( world, index, articulation );
	}

	articulation->recordFrame = frame;
	if ( frame >= articulation->recordEnd )
	{
		articulation->recording = NULL;
	}
}

//----------------------------------------------------------------------------------------------------------------------
// Files

static void nbWriteQuat( nbJsonWriter* w, b3Quat q )
{
	nbJsonWriter_Text( w, "[" );
	nbJsonWriter_Float( w, q.v.x );
	nbJsonWriter_Text( w, ", " );
	nbJsonWriter_Float( w, q.v.y );
	nbJsonWriter_Text( w, ", " );
	nbJsonWriter_Float( w, q.v.z );
	nbJsonWriter_Text( w, ", " );
	nbJsonWriter_Float( w, q.s );
	nbJsonWriter_Text( w, "]" );
}

static void nbWriteVec3( nbJsonWriter* w, b3Vec3 v )
{
	nbJsonWriter_Text( w, "[" );
	nbJsonWriter_Float( w, v.x );
	nbJsonWriter_Text( w, ", " );
	nbJsonWriter_Float( w, v.y );
	nbJsonWriter_Text( w, ", " );
	nbJsonWriter_Float( w, v.z );
	nbJsonWriter_Text( w, "]" );
}

static void nbWriteKey( nbJsonWriter* w, const char* key )
{
	nbJsonWriter_String( w, key );
	nbJsonWriter_Text( w, ": " );
}

int nbWritePoseClip( const nbPoseClip* clip, char* buffer, int capacity )
{
	nbJsonWriter writer = { buffer, buffer != NULL ? capacity : 0, 0, 0 };
	nbJsonWriter* w = &writer;
	if ( clip == NULL )
	{
		nbJsonWriter_Finish( w );
		return 0;
	}

	nbJsonWriter_Text( w, "{" );
	w->depth = 1;
	nbJsonWriter_Line( w );
	nbWriteKey( w, "format" );
	nbJsonWriter_String( w, NB_POSE_FORMAT );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "version" );
	nbJsonWriter_Int( w, NB_POSE_VERSION );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "skeleton" );
	nbJsonWriter_String( w, clip->skeleton );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "name" );
	nbJsonWriter_String( w, clip->name );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "kind" );
	nbJsonWriter_String( w, s_kindNames[clip->kind] );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "frameRate" );
	nbJsonWriter_Int( w, clip->frameRate );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );
	nbWriteKey( w, "loop" );
	nbJsonWriter_Text( w, clip->loop ? "true" : "false" );
	nbJsonWriter_Text( w, "," );
	nbJsonWriter_Line( w );

	nbWriteKey( w, "bones" );
	nbJsonWriter_Text( w, "[" );
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		nbJsonWriter_Text( w, i > 0 ? ", " : "" );
		nbJsonWriter_String( w, clip->boneNames[i] );
	}
	nbJsonWriter_Text( w, "]," );
	nbJsonWriter_Line( w );

	nbWriteKey( w, "events" );
	nbJsonWriter_Text( w, "[" );
	w->depth = 2;
	for ( int e = 0; e < clip->eventCount; ++e )
	{
		nbJsonWriter_Line( w );
		nbJsonWriter_Text( w, "{ \"frame\": " );
		nbJsonWriter_Int( w, clip->events[e].frame );
		nbJsonWriter_Text( w, ", \"name\": " );
		nbJsonWriter_String( w, clip->events[e].name );
		nbJsonWriter_Text( w, e + 1 < clip->eventCount ? " }," : " }" );
	}
	w->depth = 1;
	if ( clip->eventCount > 0 )
	{
		nbJsonWriter_Line( w );
	}
	nbJsonWriter_Text( w, "]," );
	nbJsonWriter_Line( w );

	nbWriteKey( w, "keyframes" );
	nbJsonWriter_Text( w, "[" );
	for ( int k = 0; k < clip->keyframeCount; ++k )
	{
		const nbPoseKeyframeData* data = clip->keyframes + k;
		const b3Quat* rotations = nbRotationsOf( clip, k );
		const float* confidence = nbConfidenceOf( clip, k );

		w->depth = 2;
		nbJsonWriter_Line( w );
		nbJsonWriter_Text( w, "{" );
		w->depth = 3;
		nbJsonWriter_Line( w );
		nbWriteKey( w, "frame" );
		nbJsonWriter_Int( w, data->frame );
		nbJsonWriter_Text( w, "," );
		nbJsonWriter_Line( w );
		nbWriteKey( w, "status" );
		nbJsonWriter_String( w, s_statusNames[data->status] );
		nbJsonWriter_Text( w, "," );
		nbJsonWriter_Line( w );
		nbWriteKey( w, "origin" );
		nbJsonWriter_String( w, s_originNames[data->origin] );
		nbJsonWriter_Text( w, "," );
		nbJsonWriter_Line( w );
		nbWriteKey( w, "sourceFrame" );
		nbJsonWriter_Int( w, data->sourceFrame );
		nbJsonWriter_Text( w, "," );
		nbJsonWriter_Line( w );
		nbWriteKey( w, "root" );
		nbJsonWriter_Text( w, "{ \"position\": " );
		nbWriteVec3( w, data->root.p );
		nbJsonWriter_Text( w, ", \"rotation\": " );
		nbWriteQuat( w, data->root.q );
		nbJsonWriter_Text( w, " }," );
		nbJsonWriter_Line( w );
		nbWriteKey( w, "contacts" );
		nbJsonWriter_Text( w, "[" );
		bool first = true;
		for ( int c = 0; c < 4; ++c )
		{
			if ( data->contacts & ( 1u << c ) )
			{
				nbJsonWriter_Text( w, first ? "" : ", " );
				nbJsonWriter_String( w, s_contactNames[c] );
				first = false;
			}
		}
		nbJsonWriter_Text( w, "]," );
		nbJsonWriter_Line( w );

		nbWriteKey( w, "rotations" );
		nbJsonWriter_Text( w, "{" );
		w->depth = 4;
		for ( int i = 0; i < clip->boneCount; ++i )
		{
			nbJsonWriter_Line( w );
			nbWriteKey( w, clip->boneNames[i] );
			nbWriteQuat( w, rotations[i] );
			nbJsonWriter_Text( w, i + 1 < clip->boneCount ? "," : "" );
		}
		w->depth = 3;
		nbJsonWriter_Line( w );
		nbJsonWriter_Text( w, "}" );

		// Full confidence is the rule, a file names it only when a bone is less sure
		bool sure = true;
		for ( int i = 0; i < clip->boneCount; ++i )
		{
			sure = sure && confidence[i] == 1.0f;
		}
		if ( sure == false )
		{
			nbJsonWriter_Text( w, "," );
			nbJsonWriter_Line( w );
			nbWriteKey( w, "confidence" );
			nbJsonWriter_Text( w, "{" );
			w->depth = 4;
			for ( int i = 0; i < clip->boneCount; ++i )
			{
				nbJsonWriter_Line( w );
				nbWriteKey( w, clip->boneNames[i] );
				nbJsonWriter_Float( w, confidence[i] );
				nbJsonWriter_Text( w, i + 1 < clip->boneCount ? "," : "" );
			}
			w->depth = 3;
			nbJsonWriter_Line( w );
			nbJsonWriter_Text( w, "}" );
		}

		w->depth = 2;
		nbJsonWriter_Line( w );
		nbJsonWriter_Text( w, k + 1 < clip->keyframeCount ? "}," : "}" );
	}
	w->depth = 1;
	if ( clip->keyframeCount > 0 )
	{
		nbJsonWriter_Line( w );
	}
	nbJsonWriter_Text( w, "]" );
	w->depth = 0;
	nbJsonWriter_Line( w );
	nbJsonWriter_Text( w, "}\n" );
	nbJsonWriter_Finish( w );
	return w->length;
}

typedef struct nbClipReader
{
	const nbJsonDocument* document;
	nbPoseClipError* error;
	bool failed;
} nbClipReader;

static void nbReadFail( nbClipReader* r, int value, const char* message )
{
	if ( r->failed )
	{
		return;
	}

	r->failed = true;
	if ( r->error != NULL )
	{
		const nbJsonValue* at = value >= 0 ? nbJson_Get( r->document, value ) : NULL;
		r->error->line = at != NULL ? at->line : 0;
		r->error->column = at != NULL ? at->column : 0;
		nbCopyName( r->error->message, (int)sizeof( r->error->message ), message );
	}
}

static bool nbReadType( nbClipReader* r, int value, nbJsonType type, const char* message )
{
	if ( value < 0 || nbJson_Get( r->document, value )->type != type )
	{
		nbReadFail( r, value, message );
		return false;
	}
	return true;
}

static bool nbReadInt( nbClipReader* r, int value, int* result, const char* message )
{
	if ( nbReadType( r, value, nb_jsonNumber, message ) == false )
	{
		return false;
	}
	double number = nbJson_Get( r->document, value )->number;
	if ( number != floor( number ) || number < -2147483647.0 || number > 2147483647.0 )
	{
		nbReadFail( r, value, message );
		return false;
	}
	*result = (int)number;
	return true;
}

static bool nbReadFloats( nbClipReader* r, int value, float* result, int count, const char* message )
{
	if ( nbReadType( r, value, nb_jsonArray, message ) == false || nbJson_Get( r->document, value )->count != count )
	{
		nbReadFail( r, value, message );
		return false;
	}
	int item = value + 1;
	for ( int i = 0; i < count; ++i )
	{
		if ( nbReadType( r, item, nb_jsonNumber, message ) == false )
		{
			return false;
		}
		result[i] = (float)nbJson_Get( r->document, item )->number;
		item = nbJson_Next( r->document, item );
	}
	return true;
}

static bool nbReadQuat( nbClipReader* r, int value, b3Quat* q, const char* message )
{
	float values[4];
	if ( nbReadFloats( r, value, values, 4, message ) == false )
	{
		return false;
	}
	*q = (b3Quat){ { values[0], values[1], values[2] }, values[3] };
	return true;
}

// The index of a name in a list, -1 for none
static int nbNameIndex( const char* const* names, int count, const nbJsonDocument* document, int value )
{
	for ( int i = 0; i < count; ++i )
	{
		if ( nbJson_Equals( document, value, names[i] ) )
		{
			return i;
		}
	}
	return -1;
}

static int nbBoneIndex( const nbPoseClip* clip, const nbJsonDocument* document, int key )
{
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		if ( nbJson_Equals( document, key, clip->boneNames[i] ) )
		{
			return i;
		}
	}
	return -1;
}

static void nbReadKeyframe( nbClipReader* r, nbPoseClip* clip, int object, int index )
{
	const nbJsonDocument* d = r->document;
	nbPoseKeyframeData* data = clip->keyframes + index;
	b3Quat* rotations = nbRotationsOf( clip, index );
	float* confidence = nbConfidenceOf( clip, index );
	data->status = nb_keyframeProposed;
	data->origin = nb_originHand;
	data->sourceFrame = -1;
	data->root = b3Transform_identity;
	data->contacts = 0;
	for ( int i = 0; i < clip->boneCount; ++i )
	{
		rotations[i] = b3Quat_identity;
		confidence[i] = 1.0f;
	}

	int value = nbJson_Find( d, object, "frame" );
	if ( nbReadInt( r, value, &data->frame, "a keyframe needs a frame, a whole number" ) == false )
	{
		return;
	}
	if ( data->frame < 0 || ( index > 0 && data->frame <= clip->keyframes[index - 1].frame ) )
	{
		nbReadFail( r, value, "keyframes need increasing frames from zero on" );
		return;
	}

	value = nbJson_Find( d, object, "status" );
	if ( value >= 0 )
	{
		int status = nbNameIndex( s_statusNames, 2, d, value );
		if ( status < 0 )
		{
			nbReadFail( r, value, "the status is proposed or approved" );
			return;
		}
		data->status = (nbKeyframeStatus)status;
	}

	value = nbJson_Find( d, object, "origin" );
	if ( value >= 0 )
	{
		int origin = nbNameIndex( s_originNames, nb_originCount, d, value );
		if ( origin < 0 )
		{
			nbReadFail( r, value, "the origin is hand, estimator, assistant or simulation" );
			return;
		}
		data->origin = (nbKeyframeOrigin)origin;
	}

	value = nbJson_Find( d, object, "sourceFrame" );
	if ( value >= 0 && nbReadInt( r, value, &data->sourceFrame, "the source frame is a whole number" ) == false )
	{
		return;
	}

	value = nbJson_Find( d, object, "root" );
	if ( value >= 0 )
	{
		if ( nbReadType( r, value, nb_jsonObject, "the root has a position and a rotation" ) == false )
		{
			return;
		}
		int position = nbJson_Find( d, value, "position" );
		int rotation = nbJson_Find( d, value, "rotation" );
		float p[3];
		if ( ( position >= 0 && nbReadFloats( r, position, p, 3, "a position has three numbers" ) == false ) ||
			 ( rotation >= 0 &&
			   nbReadQuat( r, rotation, &data->root.q, "a rotation has four numbers, x, y, z and w" ) == false ) )
		{
			return;
		}
		if ( position >= 0 )
		{
			data->root.p = (b3Vec3){ p[0], p[1], p[2] };
		}
	}

	value = nbJson_Find( d, object, "contacts" );
	if ( value >= 0 )
	{
		if ( nbReadType( r, value, nb_jsonArray, "the contacts are a list of names" ) == false )
		{
			return;
		}
		int item = value + 1;
		for ( int i = 0; i < nbJson_Get( d, value )->count; ++i )
		{
			int contact = nbNameIndex( s_contactNames, 4, d, item );
			if ( contact < 0 )
			{
				nbReadFail( r, item, "a contact is leftFoot, rightFoot, leftHand or rightHand" );
				return;
			}
			data->contacts |= 1u << contact;
			item = nbJson_Next( d, item );
		}
	}

	// Every bone once, by name
	value = nbJson_Find( d, object, "rotations" );
	if ( nbReadType( r, value, nb_jsonObject, "a keyframe needs the rotations of its bones by name" ) == false )
	{
		return;
	}
	bool* seen = nbAlloc( sizeof( bool ) * (size_t)clip->boneCount );
	memset( seen, 0, sizeof( bool ) * (size_t)clip->boneCount );
	int member = value + 1;
	for ( int m = 0; m < nbJson_Get( d, value )->count && r->failed == false; ++m )
	{
		int bone = nbBoneIndex( clip, d, member );
		if ( bone < 0 || seen[bone] )
		{
			nbReadFail( r, member, bone < 0 ? "a rotation of a bone the clip does not have" : "a bone with two rotations" );
			break;
		}
		seen[bone] = true;
		nbReadQuat( r, member + 1, rotations + bone, "a rotation has four numbers, x, y, z and w" );
		member = nbJson_Next( d, member + 1 );
	}
	for ( int i = 0; i < clip->boneCount && r->failed == false; ++i )
	{
		if ( seen[i] == false )
		{
			nbReadFail( r, value, "a keyframe needs a rotation for every bone" );
		}
	}
	nbFree( seen, sizeof( bool ) * (size_t)clip->boneCount );
	if ( r->failed )
	{
		return;
	}

	value = nbJson_Find( d, object, "confidence" );
	if ( value >= 0 )
	{
		if ( nbReadType( r, value, nb_jsonObject, "the confidence is a number from 0 to 1 by bone" ) == false )
		{
			return;
		}
		member = value + 1;
		for ( int m = 0; m < nbJson_Get( d, value )->count; ++m )
		{
			int bone = nbBoneIndex( clip, d, member );
			if ( bone < 0 ||
				 nbReadType( r, member + 1, nb_jsonNumber, "the confidence is a number from 0 to 1 by bone" ) == false )
			{
				nbReadFail( r, member, "a confidence of a bone the clip does not have" );
				return;
			}
			double sure = nbJson_Get( d, member + 1 )->number;
			if ( sure < 0.0 || sure > 1.0 )
			{
				nbReadFail( r, member + 1, "the confidence is a number from 0 to 1 by bone" );
				return;
			}
			confidence[bone] = (float)sure;
			member = nbJson_Next( d, member + 1 );
		}
	}
}

nbPoseClip* nbParsePoseClip( const char* text, int length, nbPoseClipError* error )
{
	if ( error != NULL )
	{
		memset( error, 0, sizeof( nbPoseClipError ) );
	}

	nbJsonDocument document;
	int line = 0;
	int column = 0;
	char message[128] = { 0 };
	if ( nbJson_Parse( &document, text, length, &line, &column, message, (int)sizeof( message ) ) == false )
	{
		if ( error != NULL )
		{
			error->line = line;
			error->column = column;
			nbCopyName( error->message, (int)sizeof( error->message ), message );
		}
		return NULL;
	}

	nbClipReader reader = { &document, error, false };
	nbClipReader* r = &reader;
	const nbJsonDocument* d = &document;
	nbPoseClip* clip = NULL;
	int root = 0;
	if ( nbReadType( r, root, nb_jsonObject, "a clip is an object" ) == false )
	{
		nbJson_Free( &document );
		return NULL;
	}

	int value = nbJson_Find( d, root, "format" );
	if ( value < 0 || nbJson_Equals( d, value, NB_POSE_FORMAT ) == false )
	{
		nbReadFail( r, value >= 0 ? value : root, "the format is not " NB_POSE_FORMAT );
	}
	int version = 0;
	value = nbJson_Find( d, root, "version" );
	if ( r->failed == false &&
		 ( nbReadInt( r, value, &version, "the version is a whole number" ) == false || version != NB_POSE_VERSION ) )
	{
		nbReadFail( r, value, "this library reads version 1 of the format" );
	}

	// The bones first, the keyframes name them
	int boneList = nbJson_Find( d, root, "bones" );
	if ( r->failed == false && nbReadType( r, boneList, nb_jsonArray, "a clip needs the names of its bones" ) )
	{
		int count = nbJson_Get( d, boneList )->count;
		if ( count < 1 )
		{
			nbReadFail( r, boneList, "a clip needs the names of its bones" );
		}
		else
		{
			clip = nbAllocPoseClip( count );
			int item = boneList + 1;
			for ( int i = 0; i < count && r->failed == false; ++i )
			{
				if ( nbReadType( r, item, nb_jsonString, "a bone is named by a string" ) )
				{
					nbCopyName( clip->boneNames[i], NB_BONE_NAME_LENGTH, nbJson_String( d, item ) );
					for ( int j = 0; j < i; ++j )
					{
						if ( strcmp( clip->boneNames[i], clip->boneNames[j] ) == 0 )
						{
							nbReadFail( r, item, "two bones of the same name" );
						}
					}
				}
				item = nbJson_Next( d, item );
			}
		}
	}

	if ( r->failed == false )
	{
		value = nbJson_Find( d, root, "skeleton" );
		if ( value >= 0 && nbReadType( r, value, nb_jsonString, "the skeleton is a string" ) )
		{
			nbCopyName( clip->skeleton, NB_NAME_LENGTH, nbJson_String( d, value ) );
		}
		value = nbJson_Find( d, root, "name" );
		if ( value >= 0 && nbReadType( r, value, nb_jsonString, "the name is a string" ) )
		{
			nbCopyName( clip->name, NB_NAME_LENGTH, nbJson_String( d, value ) );
		}
		value = nbJson_Find( d, root, "kind" );
		if ( value >= 0 )
		{
			int kind = nbNameIndex( s_kindNames, nb_clipKindCount, d, value );
			if ( kind < 0 )
			{
				nbReadFail( r, value, "the kind is authored, computed or simulated" );
			}
			clip->kind = kind < 0 ? nb_clipAuthored : (nbPoseClipKind)kind;
		}
		value = nbJson_Find( d, root, "frameRate" );
		if ( value >= 0 && nbReadInt( r, value, &clip->frameRate, "the frame rate is a whole number" ) &&
			 ( clip->frameRate < 1 || clip->frameRate > 1000 ) )
		{
			nbReadFail( r, value, "the frame rate lies between 1 and 1000" );
		}
		value = nbJson_Find( d, root, "loop" );
		if ( value >= 0 && nbReadType( r, value, nb_jsonBool, "loop is true or false" ) )
		{
			clip->loop = nbJson_Get( d, value )->boolean;
		}
	}

	int events = r->failed ? -1 : nbJson_Find( d, root, "events" );
	if ( events >= 0 && nbReadType( r, events, nb_jsonArray, "the events are a list" ) )
	{
		int item = events + 1;
		for ( int e = 0; e < nbJson_Get( d, events )->count && r->failed == false; ++e )
		{
			int frame = 0;
			int name = nbJson_Find( d, item, "name" );
			if ( nbReadType( r, item, nb_jsonObject, "an event has a frame and a name" ) &&
				 nbReadInt( r, nbJson_Find( d, item, "frame" ), &frame, "an event has a frame and a name" ) &&
				 nbReadType( r, name, nb_jsonString, "an event has a frame and a name" ) )
			{
				nbPoseClip_AddEvent( clip, frame, nbJson_String( d, name ) );
			}
			item = nbJson_Next( d, item );
		}
	}

	int keyframes = r->failed ? -1 : nbJson_Find( d, root, "keyframes" );
	if ( r->failed == false && nbReadType( r, keyframes, nb_jsonArray, "a clip needs a list of keyframes" ) )
	{
		int count = nbJson_Get( d, keyframes )->count;
		nbReserveKeyframes( clip, count );
		int item = keyframes + 1;
		for ( int k = 0; k < count && r->failed == false; ++k )
		{
			if ( nbReadType( r, item, nb_jsonObject, "a keyframe is an object" ) )
			{
				clip->keyframeCount = k + 1;
				nbReadKeyframe( r, clip, item, k );
			}
			item = nbJson_Next( d, item );
		}
	}

	nbJson_Free( &document );
	if ( r->failed )
	{
		nbDestroyPoseClip( clip );
		return NULL;
	}
	return clip;
}
