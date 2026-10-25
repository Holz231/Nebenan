// SPDX-License-Identifier: MIT

// Authored clips, see the roadmap of the game, chapters 31.2, 32.5, 32.6, 32.8, 33.2 and 33.5: the format read and written
// to the bit, approved keyframes that nothing but an unlock changes, proposals that come in around them, corrections that
// spread to proposed neighbours, the checks for limits, ground, jumps and sliding feet, poses from the keypoints of a pose
// estimator, clips played, recorded step by step and posed on figures, and get-up clips as authored clips and back.

#include "test_macros.h"

#include "nebenan/nebenan.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct Humanoid
{
	nbBoneDef bones[nb_humanoidBoneCount];
	nbRigDef rig;
	float rootHeight;
} Humanoid;

static void MakeHumanoid( Humanoid* h )
{
	h->rootHeight = nbMakeHumanoidBones( 1.8f, 75.0f, h->bones, nb_humanoidBoneCount );
	h->rig = nbDefaultRigDef();
	h->rig.bones = h->bones;
	h->rig.boneCount = nb_humanoidBoneCount;
}

static void RestPose( b3Quat* pose )
{
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		pose[i] = b3Quat_identity;
	}
}

static b3Quat TurnX( float angle )
{
	return b3MakeQuatFromAxisAngle( b3Vec3_axisX, angle );
}

// The text of a clip, allocated, the length asked first like snprintf
static char* WriteClip( const nbPoseClip* clip )
{
	int length = nbWritePoseClip( clip, NULL, 0 );
	char* text = malloc( (size_t)length + 1 );
	int written = nbWritePoseClip( clip, text, length + 1 );
	if ( written != length )
	{
		free( text );
		return NULL;
	}
	return text;
}

static bool SameKeyframes( const nbPoseClip* a, int ka, const nbPoseClip* b, int kb )
{
	nbPoseKeyframe x = nbPoseClip_GetKeyframe( a, ka );
	nbPoseKeyframe y = nbPoseClip_GetKeyframe( b, kb );
	size_t bones = (size_t)nbPoseClip_GetBoneCount( a );
	return x.frame == y.frame && x.status == y.status && x.origin == y.origin && x.sourceFrame == y.sourceFrame &&
		   x.contacts == y.contacts && memcmp( &x.root, &y.root, sizeof( b3Transform ) ) == 0 &&
		   memcmp( x.rotations, y.rotations, sizeof( b3Quat ) * bones ) == 0 &&
		   memcmp( x.confidence, y.confidence, sizeof( float ) * bones ) == 0;
}

// Everything of two clips the same, to the bit
static bool SameClips( const nbPoseClip* a, const nbPoseClip* b )
{
	if ( strcmp( nbPoseClip_GetSkeleton( a ), nbPoseClip_GetSkeleton( b ) ) != 0 ||
		 strcmp( nbPoseClip_GetName( a ), nbPoseClip_GetName( b ) ) != 0 || nbPoseClip_GetKind( a ) != nbPoseClip_GetKind( b ) ||
		 nbPoseClip_GetFrameRate( a ) != nbPoseClip_GetFrameRate( b ) || nbPoseClip_IsLooping( a ) != nbPoseClip_IsLooping( b ) ||
		 nbPoseClip_GetBoneCount( a ) != nbPoseClip_GetBoneCount( b ) ||
		 nbPoseClip_GetKeyframeCount( a ) != nbPoseClip_GetKeyframeCount( b ) ||
		 nbPoseClip_GetEventCount( a ) != nbPoseClip_GetEventCount( b ) )
	{
		return false;
	}
	for ( int i = 0; i < nbPoseClip_GetBoneCount( a ); ++i )
	{
		if ( strcmp( nbPoseClip_GetBoneName( a, i ), nbPoseClip_GetBoneName( b, i ) ) != 0 )
		{
			return false;
		}
	}
	for ( int k = 0; k < nbPoseClip_GetKeyframeCount( a ); ++k )
	{
		if ( SameKeyframes( a, k, b, k ) == false )
		{
			return false;
		}
	}
	for ( int e = 0; e < nbPoseClip_GetEventCount( a ); ++e )
	{
		nbPoseEvent x = nbPoseClip_GetEvent( a, e );
		nbPoseEvent y = nbPoseClip_GetEvent( b, e );
		if ( x.frame != y.frame || strcmp( x.name, y.name ) != 0 )
		{
			return false;
		}
	}
	return true;
}

// The format of chapter 32.6 to the bit and back, the same text again, and texts that are no clip
static int PoseClipFormatTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseClip* clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "punch", 30 );
	ENSURE( clip != NULL );
	ENSURE( nbPoseClip_GetBoneCount( clip ) == nb_humanoidBoneCount );
	ENSURE( strcmp( nbPoseClip_GetBoneName( clip, nb_humanoidUpperArmLeft ), "upper arm left" ) == 0 );

	b3Quat pose[nb_humanoidBoneCount];
	RestPose( pose );
	b3Transform root = { { 0.1f, h.rootHeight, -0.2f }, b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.3f ) };
	ENSURE( nbPoseClip_AddKeyframe( clip, 0, pose, root, nb_originHand ) == 0 );
	pose[nb_humanoidUpperArmRight] = TurnX( 1.3f );
	pose[nb_humanoidForearmRight] = TurnX( 0.1f / 3.0f );
	ENSURE( nbPoseClip_AddKeyframe( clip, 9, pose, root, nb_originEstimator ) == 1 );
	pose[nb_humanoidForearmRight] = TurnX( 2.0f );
	root.p.z += 0.123456789f;
	ENSURE( nbPoseClip_AddKeyframe( clip, 4, pose, root, nb_originAssistant ) == 1 );
	ENSURE( nbPoseClip_GetKeyframeCount( clip ) == 3 );
	ENSURE( nbPoseClip_GetKeyframe( clip, 2 ).frame == 9 );
	ENSURE( nbPoseClip_GetLength( clip ) == 9 );
	ENSURE( nbPoseClip_FindKeyframe( clip, 4 ) == 1 && nbPoseClip_FindKeyframe( clip, 5 ) == -1 );

	float sure[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		sure[i] = 1.0f - 0.05f * (float)i;
	}
	ENSURE( nbPoseClip_SetConfidence( clip, 2, sure ) );
	ENSURE( nbPoseClip_SetContacts( clip, 0, nb_contactLeftFoot | nb_contactRightFoot ) );
	ENSURE( nbPoseClip_SetSourceFrame( clip, 2, 117 ) );
	nbPoseClip_Approve( clip, 0, true );
	ENSURE( nbPoseClip_AddEvent( clip, 6, "impact \"quoted\"" ) == 0 );
	ENSURE( nbPoseClip_AddEvent( clip, 4, "windup" ) == 0 );
	nbPoseClip_SetLooping( clip, true );
	nbPoseClip_SetKind( clip, nb_clipComputed );

	char* text = WriteClip( clip );
	ENSURE( text != NULL );
	nbPoseClipError error;
	nbPoseClip* read = nbParsePoseClip( text, (int)strlen( text ), &error );
	ENSURE( read != NULL );
	ENSURE( SameClips( clip, read ) );
	char* again = WriteClip( read );
	ENSURE( again != NULL && strcmp( text, again ) == 0 );
	printf( "clip of 3 keyframes and 2 events: %d characters, read back to the bit\n", (int)strlen( text ) );
	free( again );
	free( text );
	nbDestroyPoseClip( read );
	nbDestroyPoseClip( clip );

	// Texts that are no clip say where and why
	struct
	{
		const char* text;
		const char* reason;
		int line;
	} bad[] = {
		{ "{ \"format\": \"nebenan-pose-clip\" \"version\": 1 }", "comma", 1 },
		{ "{ \"format\": \"other\", \"version\": 1, \"bones\": [\"root\"], \"keyframes\": [] }", "format", 1 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 2, \"bones\": [\"root\"], \"keyframes\": [] }", "version 1", 1 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 1,\n \"bones\": [\"root\"],\n \"keyframes\": [ { \"frame\": 0,\n "
		  "\"rotations\": { \"root\": [0, 0, 0, 1],\n \"spine\": [0, 0, 0, 1] } } ] }",
		  "does not have", 5 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 1, \"bones\": [\"root\", \"spine\"], "
		  "\"keyframes\": [ { \"frame\": 0, \"rotations\": { \"root\": [0, 0, 0, 1] } } ] }",
		  "every bone", 1 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 1, \"bones\": [\"root\"], \"keyframes\": [ { \"frame\": 3, "
		  "\"rotations\": { \"root\": [0, 0, 0, 1] } }, { \"frame\": 2, \"rotations\": { \"root\": [0, 0, 0, 1] } } ] }",
		  "increasing", 1 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 1, \"bones\": [\"root\"], \"keyframes\": [ { \"frame\": 0, "
		  "\"status\": \"maybe\", \"rotations\": { \"root\": [0, 0, 0, 1] } } ] }",
		  "proposed or approved", 1 },
		{ "{ \"format\": \"nebenan-pose-clip\", \"version\": 1, \"bones\": [\"root\"], \"keyframes\": [ { \"frame\": 0, "
		  "\"rotations\": { \"root\": [0, 0, 1] } } ] }",
		  "four numbers", 1 },
	};
	for ( int i = 0; i < (int)( sizeof( bad ) / sizeof( bad[0] ) ); ++i )
	{
		nbPoseClip* none = nbParsePoseClip( bad[i].text, (int)strlen( bad[i].text ), &error );
		if ( none != NULL || strstr( error.message, bad[i].reason ) == NULL || error.line != bad[i].line )
		{
			printf( "text %d: line %d column %d: %s\n", i, error.line, error.column, error.message );
		}
		ENSURE( none == NULL );
		ENSURE( strstr( error.message, bad[i].reason ) != NULL );
		ENSURE( error.line == bad[i].line );
	}

	// A small clip by hand, with defaults for what it leaves out
	const char* small =
		"{ \"format\": \"nebenan-pose-clip\", \"version\": 1, \"frameRate\": 24, \"bones\": [\"root\", \"arm\"],\n"
		"  \"keyframes\": [ { \"frame\": 0, \"rotations\": { \"arm\": [0, 0, 0, 1], \"root\": [0, 0, 0, 1] } },\n"
		"                 { \"frame\": 12, \"status\": \"approved\", \"contacts\": [\"rightHand\"],\n"
		"                   \"rotations\": { \"root\": [0, 0, 0, 1], \"arm\": [0.5, 0, 0, 0.8660254] } } ] }";
	read = nbParsePoseClip( small, (int)strlen( small ), &error );
	ENSURE( read != NULL );
	ENSURE( nbPoseClip_GetFrameRate( read ) == 24 && nbPoseClip_GetKeyframeCount( read ) == 2 );
	nbPoseKeyframe second = nbPoseClip_GetKeyframe( read, 1 );
	ENSURE( second.status == nb_keyframeApproved && second.origin == nb_originHand && second.sourceFrame == -1 );
	ENSURE( second.contacts == nb_contactRightHand && second.confidence[1] == 1.0f );
	ENSURE( second.rotations[1].v.x == 0.5f && second.rotations[1].s == 0.8660254f );
	nbDestroyPoseClip( read );
	return 0;
}

// Chapters 32.5, 32.8 and 33.5: an approved keyframe is a hard constraint of authoring. Nothing but an unlock changes it,
// proposals of an assistant come in around it, and a correction spreads to proposed neighbours only.
static int PoseClipLockTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseClip* clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "kick", 30 );
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root = { { 0.0f, h.rootHeight, 0.0f }, b3Quat_identity };
	for ( int k = 0; k < 3; ++k )
	{
		RestPose( pose );
		pose[nb_humanoidThighRight] = TurnX( 0.3f * (float)k );
		pose[nb_humanoidUpperArmLeft] = TurnX( 0.2f * (float)k );
		nbPoseClip_AddKeyframe( clip, 10 * k, pose, root, nb_originHand );
	}
	nbPoseClip_Approve( clip, 1, true );
	ENSURE( nbPoseClip_GetKeyframe( clip, 1 ).status == nb_keyframeApproved );

	// A copy of the approved keyframe, to compare to the bit
	nbPoseClip* snapshot = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "snapshot", 30 );
	nbPoseKeyframe approved = nbPoseClip_GetKeyframe( clip, 1 );
	nbPoseClip_AddKeyframe( snapshot, approved.frame, approved.rotations, approved.root, approved.origin );
	nbPoseClip_Approve( snapshot, 0, true );

	// The approved keyframe refuses every change by hand until it is unlocked
	RestPose( pose );
	pose[nb_humanoidHead] = TurnX( 0.4f );
	ENSURE( nbPoseClip_SetPose( clip, 1, pose, root, 0 ) == false );
	ENSURE( nbPoseClip_MoveKeyframe( clip, 1, 12 ) == false );
	ENSURE( nbPoseClip_RemoveKeyframe( clip, 1 ) == false );
	ENSURE( nbPoseClip_SetContacts( clip, 1, nb_contactLeftFoot ) == false );
	ENSURE( nbPoseClip_SetSourceFrame( clip, 1, 3 ) == false );
	ENSURE( nbPoseClip_AddKeyframe( clip, 10, pose, root, nb_originHand ) == -1 );

	// Proposals of an assistant: one between, one on the approved keyframe, one on a proposal
	nbPoseClip* proposals = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "assistant", 30 );
	for ( int k = 0; k < 3; ++k )
	{
		RestPose( pose );
		pose[nb_humanoidForearmLeft] = TurnX( 1.0f + 0.1f * (float)k );
		nbPoseClip_AddKeyframe( proposals, 5 + 5 * k + 5 * ( k / 2 ), pose, root, nb_originAssistant );
	}
	ENSURE( nbPoseClip_GetKeyframe( proposals, 2 ).frame == 20 );
	int refused = 0;
	int merged = nbPoseClip_MergeProposals( clip, proposals, &refused );
	printf( "proposals at frames 5, 10 and 20: %d in, %d refused by the approved keyframe at 10\n", merged, refused );
	ENSURE( merged == 2 && refused == 1 );
	ENSURE( nbPoseClip_GetKeyframeCount( clip ) == 4 );
	ENSURE( nbPoseClip_FindKeyframe( clip, 5 ) == 1 && nbPoseClip_FindKeyframe( clip, 10 ) == 2 );
	ENSURE( SameKeyframes( clip, 2, snapshot, 0 ) );
	nbPoseKeyframe proposed = nbPoseClip_GetKeyframe( clip, 3 );
	ENSURE( proposed.frame == 20 && proposed.origin == nb_originAssistant && proposed.status == nb_keyframeProposed );
	ENSURE( memcmp( proposed.rotations, nbPoseClip_GetKeyframe( proposals, 2 ).rotations, sizeof( pose ) ) == 0 );

	// Proposals for another frame rate or other bones do not fit
	nbPoseClip* fast = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "fast", 60 );
	nbPoseClip_AddKeyframe( fast, 3, pose, root, nb_originAssistant );
	ENSURE( nbPoseClip_MergeProposals( clip, fast, &refused ) == -1 );
	nbDestroyPoseClip( fast );

	// A correction of the keyframe at 20 by hand spreads 15 frames: to the proposal at 5 with a sixteenth of it, not to
	// the approved keyframe at 10 and not to the keyframe at 0, 20 frames away
	nbPoseKeyframe at0 = nbPoseClip_GetKeyframe( clip, 0 );
	b3Quat before0 = at0.rotations[nb_humanoidHead];
	b3Quat before5 = nbPoseClip_GetKeyframe( clip, 1 ).rotations[nb_humanoidHead];
	memcpy( pose, proposed.rotations, sizeof( pose ) );
	pose[nb_humanoidHead] = b3MulQuat( pose[nb_humanoidHead], TurnX( 0.8f ) );
	ENSURE( nbPoseClip_SetPose( clip, 3, pose, root, 15 ) );
	float moved5 = b3GetQuatAngle( b3InvMulQuat( before5, nbPoseClip_GetKeyframe( clip, 1 ).rotations[nb_humanoidHead] ) );
	printf( "a correction of 0.8 rad at frame 20 moves the proposal at frame 5 by %.4f rad\n", moved5 );
	ENSURE( fabsf( moved5 - 0.8f / 16.0f ) < 1.5e-3f );
	ENSURE( memcmp( &before0, &nbPoseClip_GetKeyframe( clip, 0 ).rotations[nb_humanoidHead], sizeof( b3Quat ) ) == 0 );
	ENSURE( SameKeyframes( clip, 2, snapshot, 0 ) );
	ENSURE( nbPoseClip_GetKeyframe( clip, 3 ).origin == nb_originHand );

	// Unlocked, the keyframe takes a change
	nbPoseClip_Approve( clip, 2, false );
	ENSURE( nbPoseClip_SetPose( clip, 2, pose, root, 0 ) );
	ENSURE( nbPoseClip_MoveKeyframe( clip, 2, 12 ) && nbPoseClip_GetKeyframe( clip, 2 ).frame == 12 );
	ENSURE( nbPoseClip_RemoveKeyframe( clip, 2 ) && nbPoseClip_GetKeyframeCount( clip ) == 3 );

	nbDestroyPoseClip( proposals );
	nbDestroyPoseClip( snapshot );
	nbDestroyPoseClip( clip );
	return 0;
}

static int CountIssues( const nbPoseIssue* issues, int count, nbPoseIssueKind kind, int bone )
{
	int found = 0;
	for ( int i = 0; i < count; ++i )
	{
		found += issues[i].kind == kind && ( bone < 0 || issues[i].bone == bone ) ? 1 : 0;
	}
	return found;
}

// Chapters 31.2 and 32.5: the same checks for clips by hand, from an estimator and from an assistant
static int PoseClipCheckTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseCheckDef def = nbDefaultPoseCheckDef();
	nbPoseIssue issues[64];
	b3Quat pose[nb_humanoidBoneCount];
	b3Transform root = { { 0.0f, h.rootHeight, 0.0f }, b3Quat_identity };

	// Standing at rest on the ground, both feet down: nothing to find
	nbPoseClip* clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "stand", 30 );
	RestPose( pose );
	nbPoseClip_AddKeyframe( clip, 0, pose, root, nb_originHand );
	nbPoseClip_AddKeyframe( clip, 30, pose, root, nb_originHand );
	nbPoseClip_SetContacts( clip, 0, nb_contactLeftFoot | nb_contactRightFoot );
	nbPoseClip_SetContacts( clip, 1, nb_contactLeftFoot | nb_contactRightFoot );
	ENSURE( nbPoseClip_Check( clip, &h.rig, &def, issues, 64 ) == 0 );
	nbDestroyPoseClip( clip );

	// One of every kind
	clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "faults", 30 );
	RestPose( pose );
	pose[nb_humanoidShinLeft] = TurnX( 0.5f );
	pose[nb_humanoidForearmRight] = (b3Quat){ { 0.0f, 0.0f, 0.0f }, 1.1f };
	nbPoseClip_AddKeyframe( clip, 0, pose, root, nb_originEstimator );
	float sure[nb_humanoidBoneCount];
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		sure[i] = i == nb_humanoidHead ? 0.2f : 0.9f;
	}
	nbPoseClip_SetConfidence( clip, 0, sure );
	nbPoseClip_SetContacts( clip, 0, nb_contactLeftFoot );

	RestPose( pose );
	pose[nb_humanoidUpperArmLeft] = TurnX( 1.0f );
	b3Transform low = { { 0.5f, h.rootHeight - 0.1f, 0.0f }, b3Quat_identity };
	nbPoseClip_AddKeyframe( clip, 1, pose, low, nb_originHand );
	nbPoseClip_SetContacts( clip, 1, nb_contactLeftFoot );

	int count = nbPoseClip_Check( clip, &h.rig, &def, issues, 64 );
	for ( int i = 0; i < count && i < 64; ++i )
	{
		printf( "  issue %d: keyframe %d, bone %d, %.3f\n", issues[i].kind, issues[i].keyframe, issues[i].bone, issues[i].value );
	}
	ENSURE( CountIssues( issues, count, nb_issueJointLimit, nb_humanoidShinLeft ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueNotNormalized, nb_humanoidForearmRight ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueUnsure, nb_humanoidHead ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueGround, nb_humanoidFootLeft ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueGround, nb_humanoidFootRight ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueJump, nb_humanoidUpperArmLeft ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueJump, -1 ) == 2 );
	bool rootJump = false;
	for ( int i = 0; i < count && i < 64; ++i )
	{
		rootJump = rootJump || ( issues[i].kind == nb_issueJump && issues[i].bone == -1 && issues[i].value > 15.0f );
	}
	ENSURE( rootJump );
	ENSURE( CountIssues( issues, count, nb_issueSliding, nb_humanoidFootLeft ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueSliding, nb_humanoidFootRight ) == 0 );
	nbDestroyPoseClip( clip );

	// A foot that slides slowly, 1.3 cm a keyframe, less than slideDistance each time: found every 3 cm from where it stood
	clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "slow slide", 30 );
	RestPose( pose );
	for ( int k = 0; k < 10; ++k )
	{
		b3Transform at = { { 0.013f * (float)k, h.rootHeight, 0.0f }, b3Quat_identity };
		nbPoseClip_AddKeyframe( clip, k, pose, at, nb_originHand );
		nbPoseClip_SetContacts( clip, k, nb_contactLeftFoot );
	}
	count = nbPoseClip_Check( clip, &h.rig, &def, issues, 64 );
	ENSURE( count == 3 && CountIssues( issues, count, nb_issueSliding, nb_humanoidFootLeft ) == 3 );
	ENSURE( issues[0].keyframe == 3 && issues[1].keyframe == 6 && issues[2].keyframe == 9 );
	ENSURE( fabsf( issues[0].value - 0.039f ) < 1.0e-4f );

	// A rig with a bone of another name does not fit at all
	Humanoid other;
	MakeHumanoid( &other );
	other.bones[nb_humanoidHead].name = "skull";
	count = nbPoseClip_Check( clip, &other.rig, &def, issues, 64 );
	ENSURE( count == 2 );
	ENSURE( CountIssues( issues, count, nb_issueBoneMissing, nb_humanoidHead ) == 1 );
	ENSURE( CountIssues( issues, count, nb_issueBoneUnknown, nb_humanoidHead ) == 1 );

	nbDestroyPoseClip( clip );
	return 0;
}

static int PoseClipSampleTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseClip* clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "sample", 30 );
	b3Quat pose[nb_humanoidBoneCount];
	RestPose( pose );
	nbPoseClip_AddKeyframe( clip, 10, pose, (b3Transform){ { 0.0f, 1.0f, 0.0f }, b3Quat_identity }, nb_originHand );
	pose[nb_humanoidUpperArmLeft] = TurnX( 1.0f );
	nbPoseClip_AddKeyframe( clip, 20, pose, (b3Transform){ { 2.0f, 1.0f, 0.0f }, b3Quat_identity }, nb_originHand );

	b3Quat sampled[nb_humanoidBoneCount];
	b3Transform root;
	nbPoseClip_Sample( clip, 15.0f, sampled, &root );
	ENSURE( fabsf( b3GetQuatAngle( sampled[nb_humanoidUpperArmLeft] ) - 0.5f ) < 0.01f );
	ENSURE( fabsf( root.p.x - 1.0f ) < 1.0e-6f );
	nbPoseClip_Sample( clip, 20.0f, sampled, &root );
	ENSURE( memcmp( sampled, nbPoseClip_GetKeyframe( clip, 1 ).rotations, sizeof( sampled ) ) == 0 );
	nbPoseClip_Sample( clip, 3.0f, sampled, &root );
	ENSURE( root.p.x == 0.0f && b3GetQuatAngle( sampled[nb_humanoidUpperArmLeft] ) == 0.0f );
	nbPoseClip_Sample( clip, 40.0f, sampled, &root );
	ENSURE( root.p.x == 2.0f );
	nbDestroyPoseClip( clip );
	return 0;
}

// The keypoints a pose estimator would see on a figure in a pose: joints at the pivots of the bones, ears and nose on the
// head, the middle of the fingers on the hand, heel and toe under the foot
static void KeypointsOf( const Humanoid* h, b3Transform root, const b3Quat* pose, nbKeypointLayout layout, b3Vec3* points )
{
	b3Transform t[nb_humanoidBoneCount];
	nbComputePoseTransforms( &h->rig, root, pose, t );
	float s = h->rootHeight / 0.98f;
	b3Vec3 nose = b3TransformPoint( t[nb_humanoidHead], (b3Vec3){ 0.0f, 0.08f * s, 0.10f * s } );
	b3Vec3 earLeft = b3TransformPoint( t[nb_humanoidHead], (b3Vec3){ 0.07f * s, 0.10f * s, 0.0f } );
	b3Vec3 earRight = b3TransformPoint( t[nb_humanoidHead], (b3Vec3){ -0.07f * s, 0.10f * s, 0.0f } );
	b3Vec3 handLeft = b3TransformPoint( t[nb_humanoidHandLeft], (b3Vec3){ 0.0f, 0.09f * s, 0.0f } );
	b3Vec3 handRight = b3TransformPoint( t[nb_humanoidHandRight], (b3Vec3){ 0.0f, 0.09f * s, 0.0f } );
	b3Vec3 heelLeft = b3TransformPoint( t[nb_humanoidFootLeft], (b3Vec3){ 0.0f, -0.04f * s, -0.06f * s } );
	b3Vec3 heelRight = b3TransformPoint( t[nb_humanoidFootRight], (b3Vec3){ 0.0f, -0.04f * s, -0.06f * s } );
	b3Vec3 toeLeft = b3TransformPoint( t[nb_humanoidFootLeft], (b3Vec3){ 0.0f, 0.22f * s, -0.06f * s } );
	b3Vec3 toeRight = b3TransformPoint( t[nb_humanoidFootRight], (b3Vec3){ 0.0f, 0.22f * s, -0.06f * s } );

	if ( layout == nb_keypointsCoco17 )
	{
		b3Vec3 coco[17] = { nose,
							nose,
							nose,
							earLeft,
							earRight,
							t[nb_humanoidUpperArmLeft].p,
							t[nb_humanoidUpperArmRight].p,
							t[nb_humanoidForearmLeft].p,
							t[nb_humanoidForearmRight].p,
							t[nb_humanoidHandLeft].p,
							t[nb_humanoidHandRight].p,
							t[nb_humanoidThighLeft].p,
							t[nb_humanoidThighRight].p,
							t[nb_humanoidShinLeft].p,
							t[nb_humanoidShinRight].p,
							t[nb_humanoidFootLeft].p,
							t[nb_humanoidFootRight].p };
		memcpy( points, coco, sizeof( coco ) );
		return;
	}

	for ( int i = 0; i < 33; ++i )
	{
		points[i] = nose;
	}
	points[7] = earLeft;
	points[8] = earRight;
	points[11] = t[nb_humanoidUpperArmLeft].p;
	points[12] = t[nb_humanoidUpperArmRight].p;
	points[13] = t[nb_humanoidForearmLeft].p;
	points[14] = t[nb_humanoidForearmRight].p;
	points[15] = t[nb_humanoidHandLeft].p;
	points[16] = t[nb_humanoidHandRight].p;
	points[17] = handLeft;
	points[18] = handRight;
	points[19] = handLeft;
	points[20] = handRight;
	points[23] = t[nb_humanoidThighLeft].p;
	points[24] = t[nb_humanoidThighRight].p;
	points[25] = t[nb_humanoidShinLeft].p;
	points[26] = t[nb_humanoidShinRight].p;
	points[27] = t[nb_humanoidFootLeft].p;
	points[28] = t[nb_humanoidFootRight].p;
	points[29] = heelLeft;
	points[30] = heelRight;
	points[31] = toeLeft;
	points[32] = toeRight;
}

// The largest angle between the bones of two poses, in the world, the root at a transform
static float LargestTurn( const Humanoid* h, b3Transform rootA, const b3Quat* a, b3Transform rootB, const b3Quat* b, int skip )
{
	b3Transform ta[nb_humanoidBoneCount];
	b3Transform tb[nb_humanoidBoneCount];
	nbComputePoseTransforms( &h->rig, rootA, a, ta );
	nbComputePoseTransforms( &h->rig, rootB, b, tb );
	float largest = 0.0f;
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		if ( i == skip )
		{
			continue;
		}
		float angle = b3GetQuatAngle( b3InvMulQuat( ta[i].q, tb[i].q ) );
		angle = angle > B3_PI ? 2.0f * B3_PI - angle : angle;
		largest = b3MaxFloat( largest, angle );
	}
	return largest;
}

// Chapter 32.5: a pose from the keypoints of an estimator. MediaPipe shows every bone, the bent limbs to their twist;
// COCO lacks hands and feet, they keep their rest pose with confidence 0. A limb of a wrong length is found.
static int KeypointPoseTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	b3Quat pose[nb_humanoidBoneCount];
	RestPose( pose );
	pose[nb_humanoidTorso] = b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.2f );
	pose[nb_humanoidHead] = b3MulQuat( TurnX( -0.2f ), b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.3f ) );
	pose[nb_humanoidUpperArmLeft] = b3MulQuat( TurnX( 1.0f ), b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.4f ) );
	pose[nb_humanoidForearmLeft] = TurnX( 1.2f );
	pose[nb_humanoidHandLeft] = TurnX( 0.3f );
	pose[nb_humanoidUpperArmRight] = b3MulQuat( b3MakeQuatFromAxisAngle( b3Vec3_axisZ, 0.8f ), TurnX( 0.5f ) );
	pose[nb_humanoidForearmRight] = TurnX( 0.9f );
	pose[nb_humanoidThighLeft] = b3MulQuat( TurnX( 0.6f ), b3MakeQuatFromAxisAngle( b3Vec3_axisY, -0.3f ) );
	pose[nb_humanoidShinLeft] = TurnX( -0.9f );
	pose[nb_humanoidThighRight] = TurnX( -0.2f );
	pose[nb_humanoidShinRight] = TurnX( -0.4f );
	pose[nb_humanoidFootRight] = TurnX( 0.2f );
	b3Transform root = { { 0.3f, 0.95f, -0.2f }, b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.5f ) };

	b3Vec3 points[33];
	float sure[33];
	for ( int i = 0; i < 33; ++i )
	{
		sure[i] = 0.9f;
	}
	KeypointsOf( &h, root, pose, nb_keypointsMediaPipe33, points );
	b3Quat found[nb_humanoidBoneCount];
	float confidence[nb_humanoidBoneCount];
	b3Transform foundRoot;
	nbPoseIssue issues[10];
	int count =
		nbHumanoidPoseFromKeypoints( 1.8f, nb_keypointsMediaPipe33, points, sure, found, confidence, &foundRoot, issues, 10 );
	float turn = LargestTurn( &h, root, pose, foundRoot, found, -1 );
	float rootError = b3Distance( root.p, foundRoot.p );
	printf( "MediaPipe: root %.5f m off, bones at most %.5f rad off, %d issues\n", rootError, turn, count );
	ENSURE( count == 0 );
	ENSURE( rootError < 1.0e-4f );
	ENSURE( turn < 2.0e-3f );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		ENSURE( confidence[i] == 0.9f );
	}

	// COCO: the pose without hands and feet, those at rest and unsure
	KeypointsOf( &h, root, pose, nb_keypointsCoco17, points );
	count = nbHumanoidPoseFromKeypoints( 1.8f, nb_keypointsCoco17, points, sure, found, confidence, &foundRoot, issues, 10 );
	ENSURE( count == 0 );
	ENSURE( confidence[nb_humanoidHandLeft] == 0.0f && confidence[nb_humanoidFootRight] == 0.0f );
	ENSURE( b3GetQuatAngle( found[nb_humanoidHandLeft] ) == 0.0f && b3GetQuatAngle( found[nb_humanoidFootRight] ) == 0.0f );
	b3Quat atRest[nb_humanoidBoneCount];
	memcpy( atRest, pose, sizeof( atRest ) );
	atRest[nb_humanoidHandLeft] = b3Quat_identity;
	atRest[nb_humanoidHandRight] = b3Quat_identity;
	atRest[nb_humanoidFootLeft] = b3Quat_identity;
	atRest[nb_humanoidFootRight] = b3Quat_identity;
	turn = LargestTurn( &h, root, atRest, foundRoot, found, -1 );
	printf( "COCO: bones at most %.5f rad off with hands and feet at rest\n", turn );
	ENSURE( turn < 2.0e-3f );

	// A wrist the estimator put a third too far: the forearm does not fit
	KeypointsOf( &h, root, pose, nb_keypointsMediaPipe33, points );
	points[15] = b3Add( points[13], b3MulSV( 1.35f, b3Sub( points[15], points[13] ) ) );
	count = nbHumanoidPoseFromKeypoints( 1.8f, nb_keypointsMediaPipe33, points, sure, found, confidence, &foundRoot, issues, 10 );
	ENSURE( count == 1 );
	ENSURE( issues[0].kind == nb_issueBoneLength && issues[0].bone == nb_humanoidForearmLeft );
	printf( "a forearm 35 %% too long: found, %.2f of its length\n", issues[0].value );
	ENSURE( issues[0].value > 1.3f && issues[0].value < 1.4f );

	// Another person on another scale is no misfit
	KeypointsOf( &h, root, pose, nb_keypointsMediaPipe33, points );
	for ( int i = 0; i < 33; ++i )
	{
		points[i] = b3MulSV( 0.9f, points[i] );
	}
	count = nbHumanoidPoseFromKeypoints( 1.8f, nb_keypointsMediaPipe33, points, sure, found, confidence, &foundRoot, issues, 10 );
	ENSURE( count == 0 );
	return 0;
}

// A file of keypoints becomes a clip of proposals, which an approved keyframe refuses
static int KeypointFileTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	b3Quat pose[nb_humanoidBoneCount];
	RestPose( pose );
	pose[nb_humanoidUpperArmLeft] = TurnX( 0.7f );
	pose[nb_humanoidForearmLeft] = TurnX( 0.9f );
	b3Transform root = { { 0.0f, h.rootHeight, 0.0f }, b3Quat_identity };

	// Two frames of COCO, the second with an unsure left wrist
	b3Vec3 points[17];
	KeypointsOf( &h, root, pose, nb_keypointsCoco17, points );
	char text[8192];
	int length = snprintf( text, sizeof( text ),
						   "{ \"format\": \"nebenan-keypoints\", \"version\": 1, \"layout\": \"coco17\", \"frameRate\": 30,\n"
						   "  \"name\": \"wave\", \"frames\": [" );
	for ( int f = 0; f < 2; ++f )
	{
		length += snprintf( text + length, sizeof( text ) - (size_t)length,
							"%s\n { \"frame\": %d, \"sourceFrame\": %d, \"points\": [", f == 0 ? "" : ",", 6 * f, 100 + 6 * f );
		for ( int i = 0; i < 17; ++i )
		{
			length += snprintf( text + length, sizeof( text ) - (size_t)length, "%s[%.9g, %.9g, %.9g]", i == 0 ? "" : ", ",
								(double)points[i].x, (double)points[i].y, (double)points[i].z );
		}
		length += snprintf( text + length, sizeof( text ) - (size_t)length, "], \"confidence\": [" );
		for ( int i = 0; i < 17; ++i )
		{
			length += snprintf( text + length, sizeof( text ) - (size_t)length, "%s%s", i == 0 ? "" : ", ",
								f == 1 && i == 9 ? "0.3" : "0.95" );
		}
		length += snprintf( text + length, sizeof( text ) - (size_t)length, "] }" );
	}
	length += snprintf( text + length, sizeof( text ) - (size_t)length, " ] }" );

	nbPoseClipError error;
	nbPoseClip* proposals = nbParseKeypointClip( text, length, 1.8f, &error );
	if ( proposals == NULL )
	{
		printf( "line %d column %d: %s\n", error.line, error.column, error.message );
	}
	ENSURE( proposals != NULL );
	ENSURE( strcmp( nbPoseClip_GetName( proposals ), "wave" ) == 0 );
	ENSURE( strcmp( nbPoseClip_GetSkeleton( proposals ), NB_HUMANOID_SKELETON ) == 0 );
	ENSURE( nbPoseClip_GetKeyframeCount( proposals ) == 2 );
	nbPoseKeyframe second = nbPoseClip_GetKeyframe( proposals, 1 );
	ENSURE( second.frame == 6 && second.sourceFrame == 106 && second.origin == nb_originEstimator );
	ENSURE( second.confidence[nb_humanoidForearmLeft] == 0.3f && second.confidence[nb_humanoidTorso] == 0.95f );
	ENSURE( fabsf( b3GetQuatAngle( second.rotations[nb_humanoidForearmLeft] ) - b3GetQuatAngle( TurnX( 0.9f ) ) ) < 1.0e-3f );

	// Into a clip whose keyframe at 6 is approved: the proposal at 0 comes in, the one at 6 is refused
	nbPoseClip* clip = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "wave", 30 );
	RestPose( pose );
	nbPoseClip_AddKeyframe( clip, 6, pose, root, nb_originHand );
	nbPoseClip_Approve( clip, 0, true );
	int refused = 0;
	ENSURE( nbPoseClip_MergeProposals( clip, proposals, &refused ) == 1 && refused == 1 );
	ENSURE( b3GetQuatAngle( nbPoseClip_GetKeyframe( clip, 1 ).rotations[nb_humanoidForearmLeft] ) == 0.0f );

	// A file of the wrong layout says so
	const char* wrong = "{ \"format\": \"nebenan-keypoints\", \"version\": 1, \"layout\": \"openpose\", \"frames\": [] }";
	ENSURE( nbParseKeypointClip( wrong, (int)strlen( wrong ), 1.8f, &error ) == NULL );
	ENSURE( strstr( error.message, "coco17" ) != NULL );

	nbDestroyPoseClip( clip );
	nbDestroyPoseClip( proposals );
	return 0;
}

typedef struct ClipScene
{
	b3WorldId physicsWorld;
	nbWorldId world;
} ClipScene;

// Ground to stand on, on one thread, or with a pool of threads and the steps on a thread of their own
static ClipScene CreateClipScene( nbTaskSystem* pool, int workerCount )
{
	ClipScene scene;
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.workerCount = (uint32_t)workerCount;
	if ( pool != NULL )
	{
		worldDef.enqueueTask = nbEnqueueTask;
		worldDef.finishTask = nbFinishTask;
		worldDef.userTaskContext = pool;
	}
	scene.physicsWorld = b3CreateWorld( &worldDef );
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = b3ToPos( (b3Vec3){ 0.0f, -1.0f, 0.0f } );
	b3BodyId ground = b3CreateBody( scene.physicsWorld, &bodyDef );
	b3BoxHull box = b3MakeBoxHull( 50.0f, 1.0f, 50.0f );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.8f;
	b3CreateHullShape( ground, &shapeDef, &box.base );
	nbWorldDef def = nbDefaultWorldDef();
	def.physicsWorld = scene.physicsWorld;
	def.workerCount = workerCount;
	def.taskSystem = pool;
	def.asyncStep = pool != NULL;
	scene.world = nbCreateWorld( &def );
	return scene;
}

static void DestroyClipScene( ClipScene* scene )
{
	nbDestroyWorld( scene->world );
	b3DestroyWorld( scene->physicsWorld );
}

static nbArticulationId CreateFigure( ClipScene* scene, const Humanoid* h )
{
	nbMotorProfile stand = nbDefaultMotorProfile();
	b3Vec3 position = { 0.0f, h->rootHeight, 0.0f };
	return nbCreateArticulation( scene->world, &h->rig, b3ToPos( position ), b3Quat_identity, &stand );
}

// The clip of three keyframes of chapter 33.4 as a file: the left arm comes up, the elbow bends, the arm goes down
static nbPoseClip* MakeArmPoseClip( const Humanoid* h )
{
	nbPoseClip* clip = nbCreatePoseClip( &h->rig, NB_HUMANOID_SKELETON, "arm", 30 );
	b3Quat poses[3][nb_humanoidBoneCount];
	for ( int k = 0; k < 3; ++k )
	{
		RestPose( poses[k] );
	}
	poses[1][nb_humanoidUpperArmLeft] = TurnX( 1.3f );
	poses[1][nb_humanoidForearmLeft] = TurnX( 1.5f );
	poses[2][nb_humanoidUpperArmLeft] = TurnX( 0.2f );
	poses[2][nb_humanoidUpperArmRight] = TurnX( 0.8f );
	b3Transform root = { { 0.0f, h->rootHeight, 0.0f }, b3Quat_identity };
	for ( int k = 0; k < 3; ++k )
	{
		nbPoseClip_AddKeyframe( clip, 24 * k, poses[k], root, nb_originHand );
		nbPoseClip_Approve( clip, k, true );
	}
	return clip;
}

static uint32_t HashBodies( nbArticulationId id )
{
	b3WorldTransform transforms[nb_humanoidBoneCount];
	nbArticulation_GetBoneTransforms( id, transforms, nb_humanoidBoneCount );
	uint32_t hash = 2166136261u;
	const unsigned char* bytes = (const unsigned char*)transforms;
	for ( size_t i = 0; i < sizeof( transforms ); ++i )
	{
		hash = ( hash ^ bytes[i] ) * 16777619u;
	}
	return hash;
}

// Chapters 33.2 and 33.5: a clip from a file plays on a figure as the clip of the figure would, without a jump of the
// pose, a simulation is baked into a clip of its own and leaves the authored one alone, and a figure set into a pose
// gives it back
static int PoseClipFigureTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseClip* authored = MakeArmPoseClip( &h );
	char* text = WriteClip( authored );
	nbPoseClipError error;
	nbPoseClip* clip = nbParsePoseClip( text, (int)strlen( text ), &error );
	ENSURE( clip != NULL );

	// The same clip through the file and as the clip of the figure: the same bits
	uint32_t hashes[2];
	for ( int run = 0; run < 2; ++run )
	{
		ClipScene scene = CreateClipScene( NULL, 1 );
		nbArticulationId figure = CreateFigure( &scene, &h );
		for ( int i = 0; i < 30; ++i )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
		}
		if ( run == 0 )
		{
			ENSURE( nbArticulation_PlayPoseClip( figure, clip, 1.0f ) );
		}
		else
		{
			nbKeyframe keyframes[3];
			for ( int k = 0; k < 3; ++k )
			{
				nbPoseKeyframe key = nbPoseClip_GetKeyframe( clip, k );
				keyframes[k].time = (float)key.frame / (float)nbPoseClip_GetFrameRate( clip );
				keyframes[k].rotations = key.rotations;
			}
			nbClipDef def = { keyframes, 3, nb_humanoidBoneCount, false };
			nbArticulation_PlayClip( figure, &def, 1.0f );
		}

		// No jump of the pose while it plays, and a simulation baked every third frame
		nbPoseClip* baked = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "arm baked", 30 );
		nbPoseClip_SetKind( baked, nb_clipSimulated );
		b3WorldTransform before[nb_humanoidBoneCount];
		b3WorldTransform after[nb_humanoidBoneCount];
		nbArticulation_GetBoneTransforms( figure, before, nb_humanoidBoneCount );
		float jump = 0.0f;
		for ( int step = 0; step < 120; ++step )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			nbArticulation_GetBoneTransforms( figure, after, nb_humanoidBoneCount );
			for ( int i = 0; i < nb_humanoidBoneCount; ++i )
			{
				jump = b3MaxFloat( jump, b3Length( b3Sub( b3ToVec3( after[i].p ), b3ToVec3( before[i].p ) ) ) );
			}
			memcpy( before, after, sizeof( before ) );
			if ( step % 6 == 5 )
			{
				ENSURE( nbPoseClip_RecordArticulation( baked, figure, ( step + 1 ) / 2 ) >= 0 );
			}
		}
		hashes[run] = HashBodies( figure );
		if ( run == 0 )
		{
			b3Quat physical[nb_humanoidBoneCount];
			nbArticulation_GetPose( figure, physical, nb_humanoidBoneCount );
			nbPoseKeyframe last = nbPoseClip_GetKeyframe( baked, nbPoseClip_GetKeyframeCount( baked ) - 1 );
			printf( "clip from the file: a bone moved at most %.3f m in a step, %d frames baked, the elbow ends %.3f rad bent\n",
					jump, nbPoseClip_GetKeyframeCount( baked ), b3GetQuatAngle( physical[nb_humanoidForearmLeft] ) );
			ENSURE( jump < 0.05f );
			ENSURE( nbPoseClip_GetKeyframeCount( baked ) == 20 );
			ENSURE( last.frame == 60 && last.origin == nb_originSimulation && last.status == nb_keyframeProposed );
			ENSURE( memcmp( last.rotations, physical, sizeof( physical ) ) == 0 );
			b3WorldTransform pelvis = b3Body_GetTransform( nbArticulation_GetBody( figure, nb_humanoidPelvis ) );
			ENSURE( memcmp( &last.root.q, &pelvis.q, sizeof( b3Quat ) ) == 0 );
		}
		nbDestroyPoseClip( baked );
		DestroyClipScene( &scene );
	}
	printf( "the same clip from the file and as the clip of the figure: 0x%08x and 0x%08x\n", hashes[0], hashes[1] );
	ENSURE( hashes[0] == hashes[1] );

	// The authored clip did not change through playing and baking
	char* again = WriteClip( clip );
	ENSURE( strcmp( text, again ) == 0 );
	free( again );
	free( text );

	// A figure set into a pose holds it and gives it back, where the pose of the rig puts it
	ClipScene scene = CreateClipScene( NULL, 1 );
	nbArticulationId figure = CreateFigure( &scene, &h );
	b3Quat pose[nb_humanoidBoneCount];
	RestPose( pose );
	pose[nb_humanoidUpperArmRight] = TurnX( 1.0f );
	pose[nb_humanoidForearmRight] = TurnX( 0.8f );
	pose[nb_humanoidThighLeft] = TurnX( 0.3f );
	pose[nb_humanoidShinLeft] = TurnX( -0.4f );
	b3WorldTransform place = { b3ToPos( (b3Vec3){ 1.0f, h.rootHeight + 0.05f, 0.5f } ),
							   b3MakeQuatFromAxisAngle( b3Vec3_axisY, 0.7f ) };
	nbArticulation_PlacePose( figure, place, pose );
	b3Quat back[nb_humanoidBoneCount];
	ENSURE( nbArticulation_GetPose( figure, back, nb_humanoidBoneCount ) == nb_humanoidBoneCount );
	b3Transform placed = { b3ToVec3( place.p ), place.q };
	b3Transform drawn[nb_humanoidBoneCount];
	nbComputePoseTransforms( &h.rig, placed, pose, drawn );
	float offset = 0.0f;
	float turn = 0.0f;
	for ( int i = 1; i < nb_humanoidBoneCount; ++i )
	{
		turn = b3MaxFloat( turn, 1.0f - fabsf( b3DotQuat( back[i], pose[i] ) ) );
		b3Vec3 body = b3ToVec3( b3Body_GetPosition( nbArticulation_GetBody( figure, i ) ) );
		offset = b3MaxFloat( offset, b3Distance( body, drawn[i].p ) );
	}
	ENSURE( turn < 1.0e-6f );
	ENSURE( offset < 1.0e-5f );
	DestroyClipScene( &scene );

	nbDestroyPoseClip( clip );
	nbDestroyPoseClip( authored );
	return 0;
}

// The bits of the keyframes of a clip: frames, contacts, roots and rotations
static uint32_t HashClip( const nbPoseClip* clip )
{
	uint32_t hash = 2166136261u;
	for ( int k = 0; k < nbPoseClip_GetKeyframeCount( clip ); ++k )
	{
		nbPoseKeyframe key = nbPoseClip_GetKeyframe( clip, k );
		const void* parts[4] = { &key.frame, &key.contacts, &key.root, key.rotations };
		size_t sizes[4] = { sizeof( key.frame ), sizeof( key.contacts ), sizeof( key.root ),
							sizeof( b3Quat ) * (size_t)nbPoseClip_GetBoneCount( clip ) };
		for ( int p = 0; p < 4; ++p )
		{
			const unsigned char* bytes = parts[p];
			for ( size_t i = 0; i < sizes[p]; ++i )
			{
				hash = ( hash ^ bytes[i] ) * 16777619u;
			}
		}
	}
	return hash;
}

static int FindEvent( const nbPoseClip* clip, const char* name )
{
	for ( int i = 0; i < nbPoseClip_GetEventCount( clip ); ++i )
	{
		nbPoseEvent event = nbPoseClip_GetEvent( clip, i );
		if ( strcmp( event.name, name ) == 0 )
		{
			return event.frame;
		}
	}
	return -1;
}

// Chapter 33.2 E: a figure records the clip it plays into a clip of its own, frame by frame of the clip time, the same with
// a pool of threads and the steps on a thread of their own, with the feet it stands on as contacts and a fall of its
// balance as an event. The recording is measured against the authored clip and goes through the same checks.
static int PoseClipRecordTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbPoseClip* clip = MakeArmPoseClip( &h );
	nbPoseCheckDef def = nbDefaultPoseCheckDef();

	char* texts[2] = { NULL, NULL };
	uint32_t hashes[2] = { 0, 0 };
	nbTaskSystem* pool = nbCreateTaskSystem( 3 );
	for ( int run = 0; run < 2; ++run )
	{
		ClipScene scene = CreateClipScene( run == 0 ? NULL : pool, run == 0 ? 1 : 4 );
		nbArticulationId figure = CreateFigure( &scene, &h );
		for ( int i = 0; i < 30; ++i )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
		}

		// Without a clip there is nothing to record
		nbPoseClip* baked = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "arm baked", 30 );
		nbPoseClip_SetKind( baked, nb_clipSimulated );
		ENSURE( nbArticulation_RecordPoseClip( figure, baked, &def ) == false );
		ENSURE( nbArticulation_PlayPoseClip( figure, clip, 1.0f ) );
		ENSURE( nbArticulation_RecordPoseClip( figure, baked, NULL ) );

		// Frame f on step 2 f + 1 at 60 Hz, up to the last frame of the clip, and nothing after it
		int steps = 0;
		while ( nbArticulation_IsRecording( figure ) && steps < 200 )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
			steps += 1;
		}
		for ( int i = 0; i < 20; ++i )
		{
			nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
		}
		ENSURE( nbArticulation_IsRecording( figure ) == false );
		ENSURE( steps == 97 );
		ENSURE( nbPoseClip_GetKeyframeCount( baked ) == 49 );
		for ( int k = 0; k < 49; ++k )
		{
			nbPoseKeyframe key = nbPoseClip_GetKeyframe( baked, k );
			ENSURE( key.frame == k && key.origin == nb_originSimulation && key.status == nb_keyframeProposed );
			ENSURE( key.contacts == ( nb_contactLeftFoot | nb_contactRightFoot ) );
		}
		ENSURE( nbPoseClip_GetEventCount( baked ) == 0 );
		texts[run] = WriteClip( baked );
		hashes[run] = HashClip( baked );

		if ( run == 0 )
		{
			// How far the figure fell behind its clip, and what the checks find in what it did
			int frame;
			int bone;
			float behind = nbPoseClip_Compare( clip, baked, &frame, &bone );
			nbPoseIssue issues[64];
			int count = nbPoseClip_Check( baked, &h.rig, &def, issues, 64 );
			printf( "recorded %d frames in %d steps, at most %.3f rad behind the clip, bone %d at frame %d, %d issues\n",
					nbPoseClip_GetKeyframeCount( baked ), steps, behind, bone, frame, count );
			for ( int i = 0; i < count && i < 64; ++i )
			{
				printf( "  issue %d: keyframe %d, bone %d, %.3f\n", issues[i].kind, issues[i].keyframe, issues[i].bone,
						issues[i].value );
			}
			ENSURE( behind > 0.0f && behind < 0.1f && bone > 0 && frame >= 0 );
			ENSURE( CountIssues( issues, count, nb_issueSliding, -1 ) == 0 );
			ENSURE( CountIssues( issues, count, nb_issueGround, -1 ) == 0 );
			ENSURE( nbPoseClip_Compare( clip, clip, &frame, &bone ) < 1.0e-3f );
			nbPoseClip* empty = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "empty", 30 );
			ENSURE( nbPoseClip_Compare( clip, empty, &frame, &bone ) == 0.0f && frame == -1 && bone == -1 );
			nbDestroyPoseClip( empty );
		}
		nbDestroyPoseClip( baked );
		DestroyClipScene( &scene );
	}
	nbDestroyTaskSystem( pool );
	// The same bits on every platform
	printf( "the recording on one thread and on four with the steps on their own: %s, hash 0x%08x and 0x%08x\n",
			strcmp( texts[0], texts[1] ) == 0 ? "the same text" : "different texts", hashes[0], hashes[1] );
	ENSURE( strcmp( texts[0], texts[1] ) == 0 && hashes[0] == hashes[1] );
	ENSURE( hashes[0] == 0x61ab3a9eu );
	free( texts[0] );
	free( texts[1] );

	// With balance and a knockdown halfway: the recording says when the figure fell
	ClipScene scene = CreateClipScene( NULL, 1 );
	nbArticulationId figure = CreateFigure( &scene, &h );
	nbBalanceDef balance = nbDefaultBalanceDef();
	ENSURE( nbArticulation_EnableBalance( figure, &balance ) );
	for ( int i = 0; i < 30; ++i )
	{
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
	}
	nbPoseClip* fallen = nbCreatePoseClip( &h.rig, NB_HUMANOID_SKELETON, "arm fallen", 30 );
	ENSURE( nbArticulation_PlayPoseClip( figure, clip, 1.0f ) );
	ENSURE( nbArticulation_RecordPoseClip( figure, fallen, NULL ) );
	for ( int step = 0; step < 100; ++step )
	{
		if ( step == 30 )
		{
			nbArticulation_Knockdown( figure );
		}
		nbWorld_Step( scene.world, 1.0f / 60.0f, 4 );
	}
	for ( int i = 0; i < nbPoseClip_GetEventCount( fallen ); ++i )
	{
		nbPoseEvent event = nbPoseClip_GetEvent( fallen, i );
		printf( "  event %s at frame %d\n", event.name, event.frame );
	}
	ENSURE( FindEvent( fallen, "fall" ) == 15 );
	ENSURE( nbPoseClip_GetKeyframeCount( fallen ) == 49 );

	// Another clip ends the recording
	ENSURE( nbArticulation_PlayPoseClip( figure, clip, 1.0f ) );
	ENSURE( nbArticulation_RecordPoseClip( figure, fallen, NULL ) );
	ENSURE( nbArticulation_IsRecording( figure ) );
	ENSURE( nbArticulation_PlayPoseClip( figure, clip, 1.0f ) );
	ENSURE( nbArticulation_IsRecording( figure ) == false );
	DestroyClipScene( &scene );

	nbDestroyPoseClip( fallen );
	nbDestroyPoseClip( clip );
	return 0;
}

// The roadmap B12: the get-up clips of the library as authored clips, for the editor and files, and back. The keyframes
// land on whole frames at 30 per second, the root lies on the ground first and stands last, the checks find nothing in
// the ground and no joint beyond its limits, and the clip read back has the times and rotations it had.
static int GetUpPoseClipTest( void )
{
	Humanoid h;
	MakeHumanoid( &h );
	nbBalanceDef balance = nbDefaultBalanceDef();
	nbPoseCheckDef def = nbDefaultPoseCheckDef();
	for ( int k = 0; k < 2; ++k )
	{
		nbKeyframe keyframes[NB_GETUP_KEYFRAMES];
		b3Quat rotations[NB_GETUP_KEYFRAMES * nb_humanoidBoneCount];
		int count = nbMakeGetUpClip( &balance, nb_humanoidBoneCount, k == 1, keyframes, rotations, NB_GETUP_KEYFRAMES );
		nbClipDef clip = { keyframes, count, nb_humanoidBoneCount, false };
		nbPoseClip* pose = nbCreatePoseClipFromClip( &h.rig, NB_HUMANOID_SKELETON,
													 k == 0 ? "up from the front" : "up from the back", 30, &clip );
		ENSURE( pose != NULL && nbPoseClip_GetKeyframeCount( pose ) == count );
		for ( int i = 0; i < count; ++i )
		{
			ENSURE( fabsf( (float)nbPoseClip_GetKeyframe( pose, i ).frame - 30.0f * keyframes[i].time ) < 1.0e-3f );
		}
		float lying = nbPoseClip_GetKeyframe( pose, 0 ).root.p.y;
		float standing = nbPoseClip_GetKeyframe( pose, count - 1 ).root.p.y;

		nbPoseIssue issues[64];
		int issueCount = nbPoseClip_Check( pose, &h.rig, &def, issues, 64 );
		printf( "get-up clip from the %s: %d keyframes, %d frames, the root at %.3f m lying and %.3f m standing, %d issues\n",
				k == 0 ? "front" : "back", count, nbPoseClip_GetLength( pose ), lying, standing, issueCount );
		for ( int i = 0; i < issueCount && i < 64; ++i )
		{
			printf( "  issue %d: keyframe %d, bone %d, %.3f\n", issues[i].kind, issues[i].keyframe, issues[i].bone,
					issues[i].value );
		}
		ENSURE( lying > 0.05f && lying < 0.25f );
		ENSURE( fabsf( standing - h.rootHeight ) < 0.01f );
		ENSURE( CountIssues( issues, issueCount, nb_issueGround, -1 ) == 0 );
		ENSURE( CountIssues( issues, issueCount, nb_issueJointLimit, -1 ) == 0 );

		// And back, with the turn of the root as the root bone's
		nbKeyframe back[NB_GETUP_KEYFRAMES];
		b3Quat backRotations[NB_GETUP_KEYFRAMES * nb_humanoidBoneCount];
		ENSURE( nbPoseClip_GetClip( pose, back, backRotations, count - 1 ) == 0 );
		ENSURE( nbPoseClip_GetClip( pose, back, backRotations, NB_GETUP_KEYFRAMES ) == count );
		float worst = 0.0f;
		for ( int i = 0; i < count; ++i )
		{
			ENSURE( fabsf( back[i].time - keyframes[i].time ) < 1.0e-5f );
			for ( int b = 0; b < nb_humanoidBoneCount; ++b )
			{
				worst = fmaxf( worst, 1.0f - fabsf( b3DotQuat( back[i].rotations[b], keyframes[i].rotations[b] ) ) );
			}
		}
		ENSURE( worst < 1.0e-6f );
		nbDestroyPoseClip( pose );
	}

	// A clip of other bones does not fit the rig
	nbKeyframe keyframes[NB_GETUP_KEYFRAMES];
	b3Quat rotations[NB_GETUP_KEYFRAMES * nb_humanoidBoneCount];
	int count = nbMakeGetUpClip( &balance, nb_humanoidBoneCount, false, keyframes, rotations, NB_GETUP_KEYFRAMES );
	nbClipDef wrong = { keyframes, count, nb_humanoidBoneCount - 1, false };
	ENSURE( nbCreatePoseClipFromClip( &h.rig, NB_HUMANOID_SKELETON, "wrong", 30, &wrong ) == NULL );
	return 0;
}

int PoseClipTest( void );

int PoseClipTest( void )
{
	RUN_TEST( PoseClipFormatTest );
	RUN_TEST( PoseClipLockTest );
	RUN_TEST( PoseClipCheckTest );
	RUN_TEST( PoseClipSampleTest );
	RUN_TEST( KeypointPoseTest );
	RUN_TEST( KeypointFileTest );
	RUN_TEST( PoseClipFigureTest );
	RUN_TEST( PoseClipRecordTest );
	RUN_TEST( GetUpPoseClipTest );
	return 0;
}
