// SPDX-License-Identifier: MIT

#include "nebenan/nebenan.h"

// How a bone moves the skin: its world transform against its rest pose. A point of the skin that lies at v in the rest
// pose of the figure the skin is written for, a figure scale times as large, lies with the bone at move + turn * (
// scale * v ).
static b3WorldTransform nbSkinBone( const nbSkin* skin, const b3WorldTransform* bones, int bone, float scale )
{
	b3Transform rest = skin->rest[bone];
	b3WorldTransform out;
	out.q = b3MulQuat( bones[bone].q, b3Conjugate( rest.q ) );
	out.p = b3OffsetPos( bones[bone].p, b3Neg( b3RotateVector( out.q, b3MulSV( scale, rest.p ) ) ) );
	return out;
}

// The poses of a skin, see nbSkin. A pose between two bones turns by its share from the first to the second about the
// pivot, where the joint between them is: the pivot goes where the share between the two bones puts it, they agree on
// it while the joint holds, and the rotation blends by the shorter way.
int nbPoseSkin( const nbSkin* skin, const b3WorldTransform* bones, int boneCount, float scale, b3WorldTransform* poses,
				int capacity )
{
	if ( skin == NULL || bones == NULL || poses == NULL || boneCount != skin->boneCount )
	{
		return 0;
	}

	int count = b3MinInt( capacity, skin->poseCount );
	for ( int i = 0; i < count; ++i )
	{
		const nbSkinPose* pose = skin->poses + i;
		b3WorldTransform first = nbSkinBone( skin, bones, pose->first, scale );
		if ( pose->share > 0.0f )
		{
			b3WorldTransform second = nbSkinBone( skin, bones, pose->second, scale );
			b3Quat other = second.q;
			if ( b3DotQuat( first.q, other ) < 0.0f )
			{
				other = ( b3Quat ){ b3Neg( other.v ), -other.s };
			}

			b3Vec3 pivot = b3MulSV( scale, pose->pivot );
			b3Pos from = b3OffsetPos( first.p, b3RotateVector( first.q, pivot ) );
			b3Pos to = b3OffsetPos( second.p, b3RotateVector( second.q, pivot ) );
			first.q = b3NLerp( first.q, other, pose->share );
			first.p = b3OffsetPos( b3LerpPosition( from, to, pose->share ), b3Neg( b3RotateVector( first.q, pivot ) ) );
		}

		poses[i] = first;
	}

	return count;
}
