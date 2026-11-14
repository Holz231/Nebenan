// dump_bones.c: the bones of the humanoid of nbMakeHumanoidBones at 1.8 m as JSON: name, parent, rest transform, shape
#include "nebenan/nebenan.h"
#include <stdio.h>
int main( void )
{
	nbBoneDef bones[32];
	float h = nbMakeHumanoidBones( 1.8f, 75.0f, bones, 32 );
	printf( "{\"rootHeight\": %.6f, \"bones\": [\n", h );
	for ( int i = 0; i < nb_humanoidBoneCount; ++i )
	{
		const nbBoneDef* b = bones + i;
		printf( " {\"name\": \"%s\", \"parent\": %d, \"p\": [%.6f, %.6f, %.6f], \"q\": [%.7f, %.7f, %.7f, %.7f], \"shape\": %d, "
				"\"start\": [%.6f, %.6f, %.6f], \"end\": [%.6f, %.6f, %.6f], \"radius\": %.6f, \"center\": [%.6f, %.6f, %.6f], "
				"\"half\": [%.6f, %.6f, %.6f]}%s\n",
				b->name, b->parent, (double)b->restTransform.p.x, (double)b->restTransform.p.y, (double)b->restTransform.p.z,
				(double)b->restTransform.q.v.x, (double)b->restTransform.q.v.y, (double)b->restTransform.q.v.z,
				(double)b->restTransform.q.s, (int)b->shape, (double)b->start.x, (double)b->start.y, (double)b->start.z,
				(double)b->end.x, (double)b->end.y, (double)b->end.z, (double)b->radius, (double)b->center.x, (double)b->center.y,
				(double)b->center.z, (double)b->halfExtents.x, (double)b->halfExtents.y, (double)b->halfExtents.z,
				i + 1 < nb_humanoidBoneCount ? "," : "" );
	}
	printf( "]}\n" );
	return 0;
}
