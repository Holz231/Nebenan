// SPDX-License-Identifier: MIT

#include <stdio.h>

int PolyTest( void );
int WorldTest( void );

int main( void )
{
	printf( "Starting Nebenan unit tests\n" );
	printf( "======================================\n" );

	if ( PolyTest() != 0 || WorldTest() != 0 )
	{
		printf( "======================================\n" );
		printf( "Unit tests failed\n" );
		return 1;
	}

	printf( "======================================\n" );
	printf( "All Nebenan tests passed!\n" );
	return 0;
}
