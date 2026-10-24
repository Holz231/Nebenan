// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int PolyTest( void );
int WorldTest( void );
int ArticulationTest( void );

int main( int argc, char** argv )
{
	printf( "Starting Nebenan unit tests\n" );
	printf( "======================================\n" );

	// One group alone: nebenan_test articulation
	bool onlyArticulation = argc > 1 && strcmp( argv[1], "articulation" ) == 0;
	if ( onlyArticulation ? ArticulationTest() != 0 : PolyTest() != 0 || WorldTest() != 0 || ArticulationTest() != 0 )
	{
		printf( "======================================\n" );
		printf( "Unit tests failed\n" );
		return 1;
	}

	printf( "======================================\n" );
	printf( "All Nebenan tests passed!\n" );
	return 0;
}
