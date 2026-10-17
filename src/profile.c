// SPDX-License-Identifier: MIT

#include "world.h"

#include <stdarg.h>
#include <stdio.h>

// The names of the stages, see nbProfileStage
static const char* nb_profileNames[] = {
	"total",	   "step",		  "pairs",		 "createContacts", "collide", "contactStates",	 "solve",
	"solverSetup", "constraints", "overflow",	 "transforms",	   "update",  "collisionDamage", "settle",
	"budget",	   "release",	  "typeChanges", "impacts",		   "cells",	  "cellsAlone",
};
_Static_assert( sizeof( nb_profileNames ) / sizeof( nb_profileNames[0] ) == nb_profileStageCount, "a name per stage" );

// How deep a stage sits in the table under the stage that holds it, and whether it runs on the calling thread alone
static const int nb_profileDepths[] = { 0, 1, 2, 3, 2, 3, 2, 3, 3, 4, 2, 1, 2, 2, 2, 2, 1, 1, 1, 2 };
static const bool nb_profileAlone[] = { false, false, false, true, false, true, false, true,  false, true,
										false, false, false, true, true,  true, true,  false, false, true };
_Static_assert( sizeof( nb_profileDepths ) / sizeof( nb_profileDepths[0] ) == nb_profileStageCount, "a depth per stage" );
_Static_assert( sizeof( nb_profileAlone ) / sizeof( nb_profileAlone[0] ) == nb_profileStageCount, "a flag per stage" );

// The thread counts of nbProfile::islandFloor
static const int nb_islandFloorThreads[4] = { 2, 4, 8, 16 };

// The most columns of different worker counts in a report
#define NB_REPORT_COLUMNS 8

const char* nbGetProfileStageName( nbProfileStage stage )
{
	return 0 <= (int)stage && stage < nb_profileStageCount ? nb_profileNames[stage] : "unknown";
}

void nbProfileStep( nbWorld* world )
{
	nbProfileSums* sums = &world->profile;
	b3Profile step = b3World_GetProfile( world->physicsWorld );
	sums->stages[nb_profileStep] += step.step;
	sums->stages[nb_profilePairs] += step.pairs;
	sums->stages[nb_profileCreateContacts] += step.createContacts;
	sums->stages[nb_profileCollide] += step.collide;
	sums->stages[nb_profileContactStates] += step.contactStates;
	sums->stages[nb_profileSolve] += step.solve;
	sums->stages[nb_profileSolverSetup] += step.solverSetup;
	sums->stages[nb_profileConstraints] += step.constraints;
	sums->stages[nb_profileOverflow] += step.overflow;
	sums->stages[nb_profileTransforms] += step.transforms;
	sums->stepCount += 1;

	// The packs give the largest island to one thread, see b3Counters::islandWork. With more than half of the work in it,
	// Box3D solves by colors instead, as b3BuildPacks decides.
	b3Counters counters = b3World_GetCounters( world->physicsWorld );
	sums->overflowCount += counters.colorCounts[B3_GRAPH_COLOR_COUNT - 1];
	sums->knotContactCount += counters.knotContactCount;
	sums->knotBatchCount += counters.knotBatchCount;
	if ( counters.islandWork > 0 )
	{
		double share = (double)counters.largestIslandWork / (double)counters.islandWork;
		sums->largestIsland += share;
		sums->islandStepCount += 1;
		if ( 2 * counters.largestIslandWork > counters.islandWork )
		{
			sums->colorStepCount += 1;
		}
		else
		{
			for ( int i = 0; i < 4; ++i )
			{
				double even = 1.0 / (double)nb_islandFloorThreads[i];
				sums->islandFloor[i] += share > even ? share : even;
			}
		}
	}
}

nbProfile nbWorld_GetProfile( nbWorldId worldId )
{
	nbProfile profile = { 0 };
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return profile;
	}

	const nbProfileSums* sums = &world->profile;
	profile.workerCount = b3World_GetWorkerCount( world->physicsWorld );
	profile.stepCount = sums->stepCount;
	profile.impactCount = sums->impactCount;

	double steps = sums->stepCount > 0 ? (double)sums->stepCount : 1.0;
	for ( int i = 0; i < nb_profileStageCount; ++i )
	{
		profile.stages[i] = (float)( sums->stages[i] / steps );
	}
	profile.stages[nb_profileTotal] =
		profile.stages[nb_profileStep] + profile.stages[nb_profileUpdate] + profile.stages[nb_profileImpacts];
	profile.freezeCount = (float)( sums->freezeCount / steps );
	profile.thawCount = (float)( sums->thawCount / steps );
	profile.overflowCount = (float)( sums->overflowCount / steps );
	profile.knotContactCount = (float)( sums->knotContactCount / steps );
	profile.knotBatchCount = (float)( sums->knotBatchCount / steps );

	if ( sums->islandStepCount > 0 )
	{
		profile.largestIsland = (float)( sums->largestIsland / (double)sums->islandStepCount );
	}
	int packSteps = sums->islandStepCount - sums->colorStepCount;
	if ( packSteps > 0 )
	{
		for ( int i = 0; i < 4; ++i )
		{
			profile.islandFloor[i] = (float)( sums->islandFloor[i] / (double)packSteps );
		}
	}
	profile.islandStepCount = sums->islandStepCount;
	profile.colorStepCount = sums->colorStepCount;

	if ( sums->wakeCount > 0 )
	{
		profile.wakeTime = (float)( 1000.0 * sums->wakeTime / (double)sums->wakeCount );
		profile.longestWake = 1000.0f * sums->longestWake;
	}
	profile.wakeCount = sums->wakeCount;
	return profile;
}

void nbWorld_ResetProfile( nbWorldId worldId )
{
	nbWorld* world = nbGetWorldFromId( worldId );
	if ( world == NULL )
	{
		return;
	}

	world->profile = (nbProfileSums){ 0 };
}

int nbGetScalingWorkerCounts( int* counts, int capacity )
{
	int recommended = nbGetRecommendedWorkerCount( false );
	int count = 0;
	for ( int workers = 1; workers < recommended && count < capacity; workers *= 2 )
	{
		counts[count++] = workers;
	}
	if ( count < capacity )
	{
		counts[count++] = recommended;
	}
	return count;
}

nbScaling nbFitScaling( const nbProfile* profiles, int count )
{
	nbScaling scaling = { 0 };
	if ( profiles == NULL || count <= 0 )
	{
		return scaling;
	}
	scaling.profileCount = count;

	// Least squares in x = 1 / n. The work is the same in every profile, only the workers differ.
	double sx = 0.0, sxx = 0.0;
	for ( int i = 0; i < count; ++i )
	{
		double x = 1.0 / (double)( profiles[i].workerCount > 0 ? profiles[i].workerCount : 1 );
		sx += x;
		sxx += x * x;
	}
	double det = (double)count * sxx - sx * sx;
	bool spread = det > 1.0e-9 * (double)count * sxx;

	for ( int k = 0; k < nb_profileStageCount; ++k )
	{
		double sy = 0.0, sxy = 0.0;
		for ( int i = 0; i < count; ++i )
		{
			double x = 1.0 / (double)( profiles[i].workerCount > 0 ? profiles[i].workerCount : 1 );
			sy += profiles[i].stages[k];
			sxy += x * profiles[i].stages[k];
		}

		// Neither part is negative: if one would be, the other alone fits
		double mean = sy / (double)count;
		double serial = mean;
		double parallel = 0.0;
		if ( spread )
		{
			parallel = ( (double)count * sxy - sx * sy ) / det;
			serial = ( sy - parallel * sx ) / (double)count;
			if ( parallel < 0.0 )
			{
				parallel = 0.0;
				serial = mean;
			}
			else if ( serial < 0.0 )
			{
				serial = 0.0;
				parallel = sxy / sxx;
			}
		}
		scaling.serial[k] = (float)serial;
		scaling.parallel[k] = (float)parallel;
	}
	return scaling;
}

float nbScaling_Project( const nbScaling* scaling, nbProfileStage stage, int workerCount )
{
	if ( scaling == NULL || (int)stage < 0 || stage >= nb_profileStageCount )
	{
		return 0.0f;
	}
	return scaling->serial[stage] + scaling->parallel[stage] / (float)( workerCount > 0 ? workerCount : 1 );
}

// Text that counts what it would need beyond its capacity, like snprintf
typedef struct nbText
{
	char* data;
	int capacity;
	int length;
} nbText;

static void nbAppend( nbText* text, const char* format, ... )
{
	int room = text->capacity - text->length;
	va_list args;
	va_start( args, format );
	int written = vsnprintf( room > 0 ? text->data + text->length : NULL, room > 0 ? (size_t)room : 0, format, args );
	va_end( args );
	text->length += written > 0 ? written : 0;
}

int nbFormatScalingReport( const nbProfile* profiles, int count, char* text, int capacity )
{
	nbText out = { text, capacity > 0 && text != NULL ? capacity : 0, 0 };
	if ( out.capacity > 0 )
	{
		text[0] = 0;
	}
	if ( profiles == NULL || count <= 0 )
	{
		return 0;
	}

	// One column per number of workers, in order, with the mean of the profiles that had it
	int columns[NB_REPORT_COLUMNS];
	int columnCount = 0;
	for ( int i = 0; i < count; ++i )
	{
		int workers = profiles[i].workerCount;
		int at = 0;
		while ( at < columnCount && columns[at] < workers )
		{
			at += 1;
		}
		if ( ( at < columnCount && columns[at] == workers ) || columnCount == NB_REPORT_COLUMNS )
		{
			continue;
		}
		for ( int k = columnCount; k > at; --k )
		{
			columns[k] = columns[k - 1];
		}
		columns[at] = workers;
		columnCount += 1;
	}

	int minSteps = profiles[0].stepCount, maxSteps = profiles[0].stepCount;
	for ( int i = 1; i < count; ++i )
	{
		minSteps = b3MinInt( minSteps, profiles[i].stepCount );
		maxSteps = b3MaxInt( maxSteps, profiles[i].stepCount );
	}

	nbScaling scaling = nbFitScaling( profiles, count );
	nbAppend( &out, "Scaling over %d profiles of %d to %d steps, milliseconds per step, fit T(n) = serial + parallel / n\n",
			  count, minSteps, maxSteps );
	nbAppend( &out, "%-22s", "stage" );
	for ( int c = 0; c < columnCount; ++c )
	{
		nbAppend( &out, " %5d w", columns[c] );
	}
	nbAppend( &out, " | serial parallel |   8 w  16 w\n" );

	for ( int k = 0; k < nb_profileStageCount; ++k )
	{
		char name[32];
		snprintf( name, sizeof( name ), "%*s%s%s", 2 * nb_profileDepths[k], "", nb_profileNames[k],
				  nb_profileAlone[k] ? " *" : "" );
		nbAppend( &out, "%-22s", name );
		for ( int c = 0; c < columnCount; ++c )
		{
			double sum = 0.0;
			int n = 0;
			for ( int i = 0; i < count; ++i )
			{
				if ( profiles[i].workerCount == columns[c] )
				{
					sum += profiles[i].stages[k];
					n += 1;
				}
			}
			nbAppend( &out, " %7.3f", n > 0 ? sum / (double)n : 0.0 );
		}
		nbAppend( &out, " | %6.3f %8.3f | %5.3f %5.3f\n", scaling.serial[k], scaling.parallel[k],
				  nbScaling_Project( &scaling, (nbProfileStage)k, 8 ), nbScaling_Project( &scaling, (nbProfileStage)k, 16 ) );
	}
	nbAppend( &out, "* on the calling thread alone\n" );

	float total8 = nbScaling_Project( &scaling, nb_profileTotal, 8 );
	if ( total8 > 0.0f )
	{
		nbAppend( &out, "Serial with 8 workers: %.0f %% of %.3f ms per step\n", 100.0f * scaling.serial[nb_profileTotal] / total8,
				  total8 );
	}

	// What does not depend on the workers, from the profiles that have it
	double freezes = 0.0, thaws = 0.0, steps = 0.0;
	double overflow = 0.0, knotContacts = 0.0, knotBatches = 0.0;
	double island = 0.0, floors[4] = { 0.0, 0.0, 0.0, 0.0 }, islandSteps = 0.0, colorSteps = 0.0;
	double wake = 0.0, wakes = 0.0;
	float longestWake = 0.0f;
	for ( int i = 0; i < count; ++i )
	{
		const nbProfile* p = profiles + i;
		freezes += p->freezeCount * (double)p->stepCount;
		thaws += p->thawCount * (double)p->stepCount;
		overflow += p->overflowCount * (double)p->stepCount;
		knotContacts += p->knotContactCount * (double)p->stepCount;
		knotBatches += p->knotBatchCount * (double)p->stepCount;
		steps += (double)p->stepCount;
		island += p->largestIsland * (double)p->islandStepCount;
		for ( int f = 0; f < 4; ++f )
		{
			floors[f] += p->islandFloor[f] * (double)( p->islandStepCount - p->colorStepCount );
		}
		islandSteps += (double)p->islandStepCount;
		colorSteps += (double)p->colorStepCount;
		wake += p->wakeTime * (double)p->wakeCount;
		wakes += (double)p->wakeCount;
		longestWake = b3MaxFloat( longestWake, p->longestWake );
	}

	if ( steps > 0.0 )
	{
		nbAppend( &out, "Freezes %.1f and thaws %.1f per step\n", freezes / steps, thaws / steps );
		nbAppend( &out, "Overflow of the constraint graph %.1f per step, %.1f contacts of them at knots in %.1f batches\n",
				  overflow / steps, knotContacts / steps, knotBatches / steps );
	}
	if ( islandSteps > 0.0 )
	{
		nbAppend( &out,
				  "Largest island %.1f %% of the solver work in %.0f steps, more than half in %.1f %% of them, which solve by "
				  "colors instead of packs\n",
				  100.0 * island / islandSteps, islandSteps, 100.0 * colorSteps / islandSteps );
		double packSteps = islandSteps - colorSteps;
		if ( packSteps > 0.0 )
		{
			nbAppend( &out, "In the steps with packs these take at least" );
			for ( int f = 0; f < 4; ++f )
			{
				nbAppend( &out, "%s %.1f %% of the work with %d workers", f == 0 ? "" : ",", 100.0 * floors[f] / packSteps,
						  nb_islandFloorThreads[f] );
			}
			nbAppend( &out, "\n" );
		}
	}
	else
	{
		nbAppend( &out, "Largest island not measured, it needs a profile with more than one worker\n" );
	}
	if ( wakes > 0.0 )
	{
		nbAppend( &out, "Workers started %.0f us after a task on average, at most %.0f us, over %.0f tasks\n", wake / wakes,
				  longestWake, wakes );
	}
	else
	{
		nbAppend( &out, "Workers not measured, no fracture had enough cells for them\n" );
	}
	return out.length;
}
