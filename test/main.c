// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int PolyTest( void );
int WorldTest( void );
int ArticulationTest( void );
int BalanceTest( void );

int main( int argc, char** argv )
{
	printf( "Starting Nebenan unit tests\n" );
	printf( "======================================\n" );

	// One group alone: nebenan_test articulation, nebenan_test balance
	const char* group = argc > 1 ? argv[1] : "";
	int failed;
	if ( strcmp( group, "articulation" ) == 0 )
	{
		failed = ArticulationTest();
	}
	else if ( strcmp( group, "balance" ) == 0 )
	{
		failed = BalanceTest();
	}
	else
	{
		failed = PolyTest() != 0 || WorldTest() != 0 || ArticulationTest() != 0 || BalanceTest() != 0;
	}
	if ( failed != 0 )
	{
		printf( "======================================\n" );
		printf( "Unit tests failed\n" );
		return 1;
	}

	printf( "======================================\n" );
	printf( "All Nebenan tests passed!\n" );
	return 0;
}
