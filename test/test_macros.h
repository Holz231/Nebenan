// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdio.h>

#define RUN_TEST( T )                                                                                                            \
	do                                                                                                                           \
	{                                                                                                                            \
		int result = T();                                                                                                        \
		if ( result == 1 )                                                                                                       \
		{                                                                                                                        \
			printf( "test failed: " #T "\n" );                                                                                   \
			return 1;                                                                                                            \
		}                                                                                                                        \
		printf( "test passed: " #T "\n" );                                                                                       \
	}                                                                                                                            \
	while ( false )

#define ENSURE( C )                                                                                                              \
	do                                                                                                                           \
	{                                                                                                                            \
		if ( ( C ) == false )                                                                                                    \
		{                                                                                                                        \
			printf( "condition false: " #C " (%s:%d)\n", __FILE__, __LINE__ );                                                 \
			return 1;                                                                                                            \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )

#define ENSURE_SMALL( C, tol )                                                                                                   \
	do                                                                                                                           \
	{                                                                                                                            \
		double ensureValue_ = (double)( C );                                                                                     \
		if ( ensureValue_ < -( tol ) || ( tol ) < ensureValue_ )                                                                 \
		{                                                                                                                        \
			printf( "condition false: abs(" #C ") = %g < %g (%s:%d)\n", ensureValue_, (double)( tol ), __FILE__, __LINE__ );    \
			return 1;                                                                                                            \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )
