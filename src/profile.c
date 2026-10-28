// SPDX-License-Identifier: MIT

#include "world.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// The names of the stages, see nbProfileStage
static const char* nb_profileNames[] = {
	"total",		 "step",		  "pairs",		   "createContacts", "collide",	   "gatherContacts",  "updateContacts",
	"mergeContacts", "contactStates", "solve",		   "solverSetup",	 "setupPacks", "setupTasks",	  "setupKnots",
	"setupWait",	 "constraints",	  "overflow",	   "transforms",	 "update",	   "collisionDamage", "settle",
	"gatherMoving",	 "checkMoving",	  "searchSupport", "freezeRubble",	 "budget",	   "release",		  "figures",
	"typeChanges",	 "impacts",		  "cells",		   "cellsAlone",
};
_Static_assert( sizeof( nb_profileNames ) / sizeof( nb_profileNames[0] ) == nb_profileStageCount, "a name per stage" );

// How deep a stage sits in the table under the stage that holds it, and whether it runs on the calling thread alone
static const int nb_profileDepths[] = { 0, 1, 2, 3, 2, 3, 3, 3, 3, 2, 3, 4, 4, 4, 4, 3,
										4, 2, 1, 2, 2, 3, 3, 3, 3, 2, 2, 2, 1, 1, 1, 2 };
static const bool nb_profileAlone[] = { false, false, false, true, false, true,	 false, true,  true,  false, true,
										true,  false, true,	 true, false, false, false, false, false, false, true,
										false, true,  true,	 true, true,  true,	 true,  false, false, true };
_Static_assert( sizeof( nb_profileDepths ) / sizeof( nb_profileDepths[0] ) == nb_profileStageCount, "a depth per stage" );
_Static_assert( sizeof( nb_profileAlone ) / sizeof( nb_profileAlone[0] ) == nb_profileStageCount, "a flag per stage" );

// The thread counts of nbProfile::islandFloor
static const int nb_islandFloorThreads[4] = { 2, 4, 8, 16 };

// The most columns of different worker counts in a report
#define NB_REPORT_COLUMNS 8

_Static_assert( sizeof( b3TeamStages ) == NB_TEAM_STAGE_VALUES * sizeof( float ), "b3TeamStages holds only floats" );

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
	sums->stages[nb_profileGatherContacts] += step.gatherContacts;
	sums->stages[nb_profileUpdateContacts] += step.updateContacts;
	sums->stages[nb_profileMergeContacts] += step.mergeContacts;
	sums->stages[nb_profileContactStates] += step.contactStates;
	sums->stages[nb_profileSolve] += step.solve;
	sums->stages[nb_profileSolverSetup] += step.solverSetup;
	sums->stages[nb_profileSetupPacks] += step.setupPacks;
	sums->stages[nb_profileSetupTasks] += step.setupTasks;
	sums->stages[nb_profileSetupKnots] += step.setupKnots;
	sums->stages[nb_profileSetupWait] += step.setupWait;
	sums->stages[nb_profileConstraints] += step.constraints;
	sums->stages[nb_profileOverflow] += step.overflow;
	sums->stages[nb_profileTransforms] += step.transforms;
	sums->stepCount += 1;

	// The packs give the largest island to one thread, see b3Counters::islandWork, unless a team solves its pack, see
	// b3Counters::teamWork
	b3Counters counters = b3World_GetCounters( world->physicsWorld );
	sums->overflowCount += counters.colorCounts[B3_GRAPH_COLOR_COUNT - 1];
	sums->packWait += step.packWait;
	sums->packStepCount += counters.packCount > 0 ? 1 : 0;
	sums->domainStepCount += counters.domainCount > 0 ? 1 : 0;
	if ( counters.solverSample != 0 )
	{
		sums->solverSampleCount += 1;
		sums->solverWork += counters.islandWork;
		sums->solverConstraints += step.constraints;
		sums->packSolveTime += step.packSolveTime;
		sums->packSolveWork += counters.packSolveWork;
	}
	if ( counters.teamSample != 0 )
	{
		sums->teamSampleCount += 1;
		sums->teamSampleWork += counters.teamWork;
		sums->teamTime += step.teamTime;
		sums->teamOverflow += step.teamOverflow;
		sums->teamStageWait += step.teamStageWait;
		double workers = (double)b3MaxInt( counters.teamWorkerCount - 1, 1 );
		sums->teamWorkerWait += step.teamWorkerWait / workers;
		sums->teamWorkerPacks += step.teamWorkerPacks / workers;
		sums->teamStageCount += counters.teamStageCount;
		sums->teamYieldCount += counters.teamYieldCount / workers;

		b3TeamStages stages = b3World_GetTeamStages( world->physicsWorld );
		float values[NB_TEAM_STAGE_VALUES];
		memcpy( values, &stages, sizeof( values ) );
		for ( int i = 0; i < NB_TEAM_STAGE_VALUES; ++i )
		{
			sums->teamStages[i] += values[i];
		}
	}
	if ( counters.collideSample != 0 )
	{
		sums->collideSampleCount += 1;
		sums->collideLoop += step.updateContacts;
		sums->collideWork += step.collideWork;
		sums->collideChecks += step.collideChecks;
		sums->collideNarrowphase += step.collideNarrowphase;
		sums->collideLate += step.collideLate;
		sums->collideClaim += step.collideClaim;
		sums->collideDone += step.collideDone;
		sums->collideLongestBlock += step.collideLongestBlock;
		sums->collideTaskCount += counters.collideTaskCount;
		sums->collideBlockCount += counters.collideBlockCount;
		sums->collideMissedCount += counters.collideMissedCount;
		sums->collideContactCount += counters.awakeContactCount;
		sums->collideNarrowphaseCount += counters.narrowphaseCount;
	}
	sums->knotContactCount += counters.knotContactCount;
	sums->knotBatchCount += counters.knotBatchCount;
	sums->knotGroupBatchCount += counters.knotGroupBatchCount;
	sums->knotCopyCount += counters.knotCopyCount;
	if ( counters.islandWork > 0 )
	{
		double share = (double)counters.largestIslandWork / (double)counters.islandWork;
		sums->largestIsland += share;
		sums->islandStepCount += 1;
		if ( counters.teamWork > 0 )
		{
			sums->teamShare += (double)counters.teamWork / (double)counters.islandWork;
			sums->teamStepCount += 1;
			sums->teamJoinStepCount += counters.teamPackCount > 1 ? 1 : 0;
		}
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
	profile.knotGroupBatchCount = (float)( sums->knotGroupBatchCount / steps );
	profile.knotCopyCount = (float)( sums->knotCopyCount / steps );

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
	if ( sums->teamStepCount > 0 )
	{
		profile.teamShare = (float)( sums->teamShare / (double)sums->teamStepCount );
	}
	profile.teamStepCount = sums->teamStepCount;
	profile.teamJoinStepCount = sums->teamJoinStepCount;
	profile.packStepCount = sums->packStepCount;
	profile.domainStepCount = sums->domainStepCount;

	if ( sums->wakeCount > 0 )
	{
		profile.wakeTime = (float)( 1000.0 * sums->wakeTime / (double)sums->wakeCount );
		profile.longestWake = 1000.0f * sums->longestWake;
	}
	profile.wakeCount = sums->wakeCount;

	if ( sums->settleSplitCount > 0 )
	{
		profile.settleWorkerShare = (float)( sums->settleWorkerShare / (double)sums->settleSplitCount );
	}

	if ( sums->teamSampleCount > 0 )
	{
		double samples = (double)sums->teamSampleCount;
		profile.teamTime = (float)( sums->teamTime / samples );
		profile.teamOverflow = (float)( sums->teamOverflow / samples );
		profile.teamStageWait = (float)( sums->teamStageWait / samples );
		profile.teamWorkerWait = (float)( sums->teamWorkerWait / samples );
		profile.teamWorkerPacks = (float)( sums->teamWorkerPacks / samples );
		profile.teamStageCount = (float)( sums->teamStageCount / samples );
		profile.teamYieldCount = (float)( sums->teamYieldCount / samples );
		profile.teamSampleWork = (float)( sums->teamSampleWork / samples );

		float values[NB_TEAM_STAGE_VALUES];
		for ( int i = 0; i < NB_TEAM_STAGE_VALUES; ++i )
		{
			values[i] = (float)( sums->teamStages[i] / samples );
		}
		memcpy( &profile.teamStages, values, sizeof( values ) );
	}
	profile.teamSampleCount = sums->teamSampleCount;
	profile.packWait = (float)( sums->packWait / steps );

	if ( sums->solverSampleCount > 0 )
	{
		double samples = (double)sums->solverSampleCount;
		profile.solverWork = (float)( sums->solverWork / samples );
		profile.solverConstraints = (float)( sums->solverConstraints / samples );
		profile.packSolveTime = (float)( sums->packSolveTime / samples );
		profile.packSolveWork = (float)( sums->packSolveWork / samples );
	}
	profile.solverSampleCount = sums->solverSampleCount;

	if ( sums->collideSampleCount > 0 )
	{
		double samples = (double)sums->collideSampleCount;
		profile.collideLoop = (float)( sums->collideLoop / samples );
		profile.collideWork = (float)( sums->collideWork / samples );
		profile.collideChecks = (float)( sums->collideChecks / samples );
		profile.collideNarrowphase = (float)( sums->collideNarrowphase / samples );
		profile.collideLate = (float)( sums->collideLate / samples );
		profile.collideClaim = (float)( sums->collideClaim / samples );
		profile.collideDone = (float)( sums->collideDone / samples );
		profile.collideLongestBlock = (float)( sums->collideLongestBlock / samples );
		profile.collideTaskCount = (float)( sums->collideTaskCount / samples );
		profile.collideBlockCount = (float)( sums->collideBlockCount / samples );
		profile.collideMissedCount = (float)( sums->collideMissedCount / samples );
		profile.collideContactCount = (float)( sums->collideContactCount / samples );
		profile.collideNarrowphaseCount = (float)( sums->collideNarrowphaseCount / samples );
	}
	profile.collideSampleCount = sums->collideSampleCount;

	if ( sums->settleSampleCount > 0 )
	{
		double samples = (double)sums->settleSampleCount;
		profile.settleCall = (float)( sums->settleCall / samples );
		profile.settleWork = (float)( sums->settleWork / samples );
		profile.settleSupportWork = (float)( sums->settleSupportWork / samples );
		profile.settleLate = (float)( sums->settleLate / samples );
		profile.settleClaim = (float)( sums->settleClaim / samples );
		profile.settleDone = (float)( sums->settleDone / samples );
		profile.settleLongestBlock = (float)( sums->settleLongestBlock / samples );
		profile.settleTaskCount = (float)( sums->settleTaskCount / samples );
		profile.settleBlockCount = (float)( sums->settleBlockCount / samples );
		profile.settleMissedCount = (float)( sums->settleMissedCount / samples );
		profile.settleMovingCount = (float)( sums->settleMovingCount / samples );
		profile.settleQuietCount = (float)( sums->settleQuietCount / samples );
		profile.settleCallerCount = (float)( sums->settleCallerCount / samples );
	}
	profile.settleSampleCount = sums->settleSampleCount;
	profile.fractureWorkerItems = nbFractureWorkerItems( world );
	profile.settleWorkerPieces = nbSettleWorkerPieces( world );
	if ( sums->settleWakeCount > 0 )
	{
		profile.settleWakeTime = (float)( 1000.0 * sums->settleWakeTime / (double)sums->settleWakeCount );
	}
	profile.settleSplitCount = sums->settleSplitCount;
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

// The names of the rows of b3TeamStages
static void nbGetTeamRowName( int row, char* name, int capacity )
{
	switch ( row )
	{
		case b3_teamPrepare:
			snprintf( name, (size_t)capacity, "prepare" );
			break;
		case b3_teamIntegrate:
			snprintf( name, (size_t)capacity, "integrate" );
			break;
		case b3_teamKnots:
			snprintf( name, (size_t)capacity, "knots" );
			break;
		case b3_teamStore:
			snprintf( name, (size_t)capacity, "store" );
			break;
		default:
			snprintf( name, (size_t)capacity, "color %d", row - b3_teamColors );
			break;
	}
}

// The stages of a team by row, then what the calling thread did between them and how late the workers came in, see
// b3TeamStages
static void nbAppendTeamStages( nbText* out, const b3TeamStages* stages, int workers )
{
	nbAppend( out, "The stages of the team with %d workers, per timed step\n", workers );
	nbAppend( out, "%-10s %7s %6s %6s %7s %8s %8s %6s %7s %6s %6s %8s\n", "stages", "barrier", "alone", "blocks", "items",
			  "open ms", "alone ms", "work %", "packs %", "late %", "done %", "caller %" );

	b3TeamStage all = { 0 };
	for ( int r = 0; r <= b3_teamRowCount; ++r )
	{
		const b3TeamStage* row = r < b3_teamRowCount ? stages->rows + r : &all;
		if ( r < b3_teamRowCount )
		{
			all.barrierCount += row->barrierCount;
			all.aloneCount += row->aloneCount;
			all.blockCount += row->blockCount;
			all.callerBlockCount += row->callerBlockCount;
			all.openTime += row->openTime;
			all.aloneTime += row->aloneTime;
			all.workTime += row->workTime;
			all.packTime += row->packTime;
			all.lateTime += row->lateTime;
			all.doneTime += row->doneTime;
		}

		float stageCount = row->barrierCount + row->aloneCount;
		if ( stageCount <= 0.0f )
		{
			continue;
		}

		// The items count bodies, constraints or groups, depending on the row, so all rows have no sum of them
		char name[16];
		char items[16] = "";
		if ( r == b3_teamRowCount )
		{
			snprintf( name, sizeof( name ), "all" );
		}
		else
		{
			nbGetTeamRowName( r, name, (int)sizeof( name ) );
			snprintf( items, sizeof( items ), "%.1f", row->itemCount / stageCount );
		}

		float threadTime = row->workTime + row->packTime + row->lateTime + row->doneTime;
		float share = threadTime > 0.0f ? 100.0f / threadTime : 0.0f;
		float blocks = row->barrierCount > 0.0f ? row->blockCount / row->barrierCount : 0.0f;
		float caller = row->blockCount > 0.0f ? 100.0f * row->callerBlockCount / row->blockCount : 0.0f;
		nbAppend( out, "%-10s %7.1f %6.1f %6.1f %7s %8.3f %8.3f %6.0f %7.0f %6.0f %6.0f %8.0f\n", name, row->barrierCount,
				  row->aloneCount, blocks, items, row->openTime, row->aloneTime, share * row->workTime, share * row->packTime,
				  share * row->lateTime, share * row->doneTime, caller );
	}

	nbAppend( out,
			  "The calling thread alone %.3f ms between the stages, %.3f ms of it merging copies of knots, and %.3f ms after the "
			  "last stage\n",
			  stages->serialTime, stages->mergeTime, stages->tailTime );
	if ( stages->entryCount > 0.0f )
	{
		nbAppend( out, "A worker came into a stage %.1f us after it opened, time in other packs aside",
				  1000.0f * stages->entryLateTime / stages->entryCount );
		if ( stages->yieldEntryCount > 0.0f )
		{
			nbAppend( out, ", in %.0f %% of the entries after giving up its core %.1f us",
					  100.0f * stages->yieldEntryCount / stages->entryCount,
					  1000.0f * stages->yieldLateTime / stages->yieldEntryCount );
		}
		nbAppend( out, ", and missed %.1f stages per timed step, %.0f %% of them in another pack\n",
				  stages->missedCount / (float)b3MaxInt( workers - 1, 1 ),
				  stages->missedCount > 0.0f ? 100.0f * stages->missedPackCount / stages->missedCount : 0.0f );
	}
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
	double overflow = 0.0, knotContacts = 0.0, knotBatches = 0.0, knotGroupBatches = 0.0, knotCopies = 0.0;
	double island = 0.0, floors[4] = { 0.0, 0.0, 0.0, 0.0 }, islandSteps = 0.0, colorSteps = 0.0;
	double teamShare = 0.0, teamSteps = 0.0, teamJoinSteps = 0.0;
	double wake = 0.0, wakes = 0.0;
	float longestWake = 0.0f;
	double workerSteps = 0.0, settleSplits = 0.0, settleShare = 0.0, settleWake = 0.0, settleTasks = 0.0;
	double packSteps = 0.0, domainSteps = 0.0;
	for ( int i = 0; i < count; ++i )
	{
		const nbProfile* p = profiles + i;
		domainSteps += (double)p->domainStepCount;
		freezes += p->freezeCount * (double)p->stepCount;
		thaws += p->thawCount * (double)p->stepCount;
		overflow += p->overflowCount * (double)p->stepCount;
		knotContacts += p->knotContactCount * (double)p->stepCount;
		knotBatches += p->knotBatchCount * (double)p->stepCount;
		knotGroupBatches += p->knotGroupBatchCount * (double)p->stepCount;
		knotCopies += p->knotCopyCount * (double)p->stepCount;
		steps += (double)p->stepCount;
		island += p->largestIsland * (double)p->islandStepCount;
		for ( int f = 0; f < 4; ++f )
		{
			floors[f] += p->islandFloor[f] * (double)( p->islandStepCount - p->colorStepCount );
		}
		islandSteps += (double)p->islandStepCount;
		colorSteps += (double)p->colorStepCount;
		teamShare += p->teamShare * (double)p->teamStepCount;
		teamSteps += (double)p->teamStepCount;
		teamJoinSteps += (double)p->teamJoinStepCount;
		wake += p->wakeTime * (double)p->wakeCount;
		wakes += (double)p->wakeCount;
		longestWake = b3MaxFloat( longestWake, p->longestWake );

		// A split update gives a task to every worker but the calling thread
		if ( p->workerCount > 1 )
		{
			double tasks = (double)p->settleSplitCount * (double)( p->workerCount - 1 );
			workerSteps += (double)p->stepCount;
			packSteps += (double)p->packStepCount;
			settleSplits += (double)p->settleSplitCount;
			settleShare += p->settleWorkerShare * (double)p->settleSplitCount;
			settleWake += p->settleWakeTime * tasks;
			settleTasks += tasks;
		}
	}

	if ( steps > 0.0 )
	{
		nbAppend( &out, "Freezes %.1f and thaws %.1f per step\n", freezes / steps, thaws / steps );
		nbAppend( &out, "Overflow of the constraint graph %.1f per step, %.1f contacts of them at knots in %.1f batches\n",
				  overflow / steps, knotContacts / steps, knotBatches / steps );
		nbAppend( &out, "Knot batches one thread solves in a row %.1f per step, copies of knots in several groups %.1f\n",
				  knotGroupBatches / steps, knotCopies / steps );
	}
	if ( islandSteps > 0.0 )
	{
		nbAppend( &out, "Largest island %.1f %% of the solver work in %.0f steps, more than half in %.1f %% of them\n",
				  100.0 * island / islandSteps, islandSteps, 100.0 * colorSteps / islandSteps );
		if ( teamSteps > 0.0 )
		{
			nbAppend( &out,
					  "A team of all workers solved the largest pack in %.1f %% of these steps, with further packs in %.1f %% of "
					  "them, and held %.1f %% of their work\n",
					  100.0 * teamSteps / islandSteps, 100.0 * teamJoinSteps / teamSteps, 100.0 * teamShare / teamSteps );
		}
		else
		{
			nbAppend( &out, "No step had a team for the largest pack\n" );
		}
		double sharedSteps = islandSteps - colorSteps;
		if ( sharedSteps > 0.0 )
		{
			nbAppend( &out, "In the steps with at most half in one island, the packs without a team take at least" );
			for ( int f = 0; f < 4; ++f )
			{
				nbAppend( &out, "%s %.1f %% of the work with %d workers", f == 0 ? "" : ",", 100.0 * floors[f] / sharedSteps,
						  nb_islandFloorThreads[f] );
			}
			nbAppend( &out, "\n" );
		}
	}
	else
	{
		nbAppend( &out, "Largest island not measured, it needs a profile with more than one worker\n" );
	}
	// The packs need workers, the domains come with any number of them, see nbProfile::packStepCount
	if ( workerSteps > 0.0 )
	{
		nbAppend( &out, "Whole islands per thread in %.1f %% of the steps with workers, domains of large islands in %.1f %% of "
						"all steps\n",
				  100.0 * packSteps / workerSteps, steps > 0.0 ? 100.0 * domainSteps / steps : 0.0 );
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
	// Where the time of the team goes, per number of workers, from the profiles whose steps timed it
	int teamColumns[NB_REPORT_COLUMNS];
	int teamColumnCount = 0;
	for ( int c = 0; c < columnCount; ++c )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( profiles[i].workerCount == columns[c] && profiles[i].teamSampleCount > 0 )
			{
				teamColumns[teamColumnCount++] = columns[c];
				break;
			}
		}
	}

	if ( teamColumnCount > 0 )
	{
		nbAppend( &out, "The team in every eighth step with one, milliseconds per timed step\n" );
		nbAppend( &out, "%-30s", "part" );
		for ( int c = 0; c < teamColumnCount; ++c )
		{
			nbAppend( &out, " %5d w", teamColumns[c] );
		}
		nbAppend( &out, "\n" );

		enum
		{
			rowCount = 8
		};
		const char* rowNames[rowCount] = {
			"timed steps",
			"team on the calling thread",
			"  overflow",
			"  waits at the end of stages",
			"  stages with a barrier",
			"per worker waits for a stage",
			"  other packs",
			"  yields of the core",
		};
		for ( int r = 0; r < rowCount; ++r )
		{
			nbAppend( &out, "%-30s", rowNames[r] );
			for ( int c = 0; c < teamColumnCount; ++c )
			{
				double sum = 0.0, samples = 0.0;
				for ( int i = 0; i < count; ++i )
				{
					const nbProfile* p = profiles + i;
					if ( p->workerCount != teamColumns[c] || p->teamSampleCount == 0 )
					{
						continue;
					}

					double values[rowCount] = { 0.0,
												p->teamTime,
												p->teamOverflow,
												p->teamStageWait,
												p->teamStageCount,
												p->teamWorkerWait,
												p->teamWorkerPacks,
												p->teamYieldCount };
					sum += values[r] * (double)p->teamSampleCount;
					samples += (double)p->teamSampleCount;
				}

				if ( r == 0 )
				{
					nbAppend( &out, " %7.0f", samples );
				}
				else
				{
					nbAppend( &out, r == 4 || r == 7 ? " %7.1f" : " %7.3f", sum / samples );
				}
			}
			nbAppend( &out, "\n" );
		}

		// How the team spent its stages with the most workers that timed it, mean over their profiles
		int workers = teamColumns[teamColumnCount - 1];
		double sums[NB_TEAM_STAGE_VALUES] = { 0.0 };
		double samples = 0.0;
		for ( int i = 0; i < count; ++i )
		{
			const nbProfile* p = profiles + i;
			if ( p->workerCount != workers || p->teamSampleCount == 0 )
			{
				continue;
			}

			float values[NB_TEAM_STAGE_VALUES];
			memcpy( values, &p->teamStages, sizeof( values ) );
			for ( int v = 0; v < NB_TEAM_STAGE_VALUES; ++v )
			{
				sums[v] += values[v] * (double)p->teamSampleCount;
			}
			samples += (double)p->teamSampleCount;
		}

		float values[NB_TEAM_STAGE_VALUES];
		for ( int v = 0; v < NB_TEAM_STAGE_VALUES; ++v )
		{
			values[v] = (float)( sums[v] / samples );
		}
		b3TeamStages stages;
		memcpy( &stages, values, sizeof( values ) );
		nbAppendTeamStages( &out, &stages, workers );
	}

	// What a unit of the solver's work costs, per number of workers, from the profiles that sampled their solver. A thread
	// alone solves the colors one after the other. With workers a team solves the largest packs color by color, and the
	// threads solve the other packs whole, one thread a pack, see b3Counters::solverSample.
	int solverColumns[NB_REPORT_COLUMNS];
	int solverColumnCount = 0;
	for ( int c = 0; c < columnCount; ++c )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( profiles[i].workerCount == columns[c] && profiles[i].solverSampleCount > 0 )
			{
				solverColumns[solverColumnCount++] = columns[c];
				break;
			}
		}
	}

	if ( solverColumnCount > 0 )
	{
		nbAppend( &out,
				  "The solver in every eighth step, microseconds per 1000 units of its work, see b3Counters::islandWork\n" );
		nbAppend( &out, "%-30s", "part" );
		for ( int c = 0; c < solverColumnCount; ++c )
		{
			nbAppend( &out, " %5d w", solverColumns[c] );
		}
		nbAppend( &out, "\n" );

		enum
		{
			rowCount = 7
		};
		const char* rowNames[rowCount] = {
			"sampled steps",
			"work per sampled step",
			"constraints on calling thread",
			"in packs, over all threads",
			"  work in packs %",
			"in team blocks, all threads",
			"team on the calling thread",
		};
		for ( int r = 0; r < rowCount; ++r )
		{
			nbAppend( &out, "%-30s", rowNames[r] );
			for ( int c = 0; c < solverColumnCount; ++c )
			{
				double samples = 0.0, work = 0.0, constraints = 0.0, packTime = 0.0, packWork = 0.0;
				double teamWork = 0.0, teamBlocks = 0.0, teamTime = 0.0;
				for ( int i = 0; i < count; ++i )
				{
					const nbProfile* p = profiles + i;
					if ( p->workerCount != solverColumns[c] || p->solverSampleCount == 0 )
					{
						continue;
					}

					double n = (double)p->solverSampleCount;
					samples += n;
					work += n * p->solverWork;
					constraints += n * p->solverConstraints;
					packTime += n * p->packSolveTime;
					packWork += n * p->packSolveWork;

					// The time of the team's blocks over all threads, from the steps that timed their team, see b3TeamStage
					double m = (double)p->teamSampleCount;
					double blocks = 0.0;
					for ( int row = 0; row < b3_teamRowCount; ++row )
					{
						blocks += p->teamStages.rows[row].workTime;
					}
					teamWork += m * p->teamSampleWork;
					teamBlocks += m * blocks;
					teamTime += m * p->teamTime;
				}

				// Milliseconds per unit of work times a million make microseconds per 1000 units. A part the profiles do not
				// have gets a dash.
				double value = -1.0;
				if ( r == 0 )
				{
					nbAppend( &out, " %7.0f", samples );
					continue;
				}
				else if ( r == 1 )
				{
					nbAppend( &out, " %7.0f", samples > 0.0 ? work / samples : 0.0 );
					continue;
				}
				else if ( r == 2 && work > 0.0 )
				{
					value = 1.0e6 * constraints / work;
				}
				else if ( r == 3 && packWork > 0.0 )
				{
					value = 1.0e6 * packTime / packWork;
				}
				else if ( r == 4 && work > 0.0 )
				{
					value = 100.0 * packWork / work;
				}
				else if ( r == 5 && teamWork > 0.0 )
				{
					value = 1.0e6 * teamBlocks / teamWork;
				}
				else if ( r == 6 && teamWork > 0.0 )
				{
					value = 1.0e6 * teamTime / teamWork;
				}

				if ( value < 0.0 )
				{
					nbAppend( &out, " %7s", "-" );
				}
				else
				{
					nbAppend( &out, " %7.1f", value );
				}
			}
			nbAppend( &out, "\n" );
		}
	}

	// How the threads spent the contact loop, per number of workers, from the profiles whose steps timed it
	int collideColumns[NB_REPORT_COLUMNS];
	int collideColumnCount = 0;
	for ( int c = 0; c < columnCount; ++c )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( profiles[i].workerCount == columns[c] && profiles[i].collideSampleCount > 0 )
			{
				collideColumns[collideColumnCount++] = columns[c];
				break;
			}
		}
	}

	if ( collideColumnCount > 0 )
	{
		nbAppend( &out, "The contact loop in every eighth step, milliseconds per timed step, over all tasks of the loop\n" );
		nbAppend( &out, "%-30s", "part" );
		for ( int c = 0; c < collideColumnCount; ++c )
		{
			nbAppend( &out, " %5d w", collideColumns[c] );
		}
		nbAppend( &out, "\n" );

		enum
		{
			rowCount = 16
		};
		const char* rowNames[rowCount] = {
			"timed steps",
			"loop on the calling thread",
			"in blocks",
			"  checks",
			"  narrowphase",
			"before the first block",
			"between the blocks",
			"after the last block",
			"longest block",
			"tasks",
			"  without a block",
			"blocks",
			"contacts",
			"  to the narrowphase",
			"ns in blocks per contact",
			"in blocks of the tasks %",
		};
		for ( int r = 0; r < rowCount; ++r )
		{
			nbAppend( &out, "%-30s", rowNames[r] );
			for ( int c = 0; c < collideColumnCount; ++c )
			{
				double sums[rowCount] = { 0.0 };
				double samples = 0.0;
				for ( int i = 0; i < count; ++i )
				{
					const nbProfile* p = profiles + i;
					if ( p->workerCount != collideColumns[c] || p->collideSampleCount == 0 )
					{
						continue;
					}

					double values[rowCount] = { 0.0,
												p->collideLoop,
												p->collideWork,
												p->collideChecks,
												p->collideNarrowphase,
												p->collideLate,
												p->collideClaim,
												p->collideDone,
												p->collideLongestBlock,
												p->collideTaskCount,
												p->collideMissedCount,
												p->collideBlockCount,
												p->collideContactCount,
												p->collideNarrowphaseCount,
												0.0,
												0.0 };
					for ( int v = 0; v < rowCount; ++v )
					{
						sums[v] += values[v] * (double)p->collideSampleCount;
					}
					samples += (double)p->collideSampleCount;
				}

				// The last two rows relate the time in blocks to the contacts and to the time of the tasks in the loop, which
				// the time in blocks, before the first, between them and after the last add up to
				double taskTime = sums[2] + sums[5] + sums[6] + sums[7];
				if ( r == 0 )
				{
					nbAppend( &out, " %7.0f", samples );
				}
				else if ( r == rowCount - 2 )
				{
					nbAppend( &out, " %7.1f", sums[12] > 0.0 ? 1.0e6 * sums[2] / sums[12] : 0.0 );
				}
				else if ( r == rowCount - 1 )
				{
					nbAppend( &out, " %7.1f", taskTime > 0.0 ? 100.0 * sums[2] / taskTime : 0.0 );
				}
				else
				{
					nbAppend( &out, r >= 9 ? " %7.1f" : " %7.3f", sums[r] / samples );
				}
			}
			nbAppend( &out, "\n" );
		}
	}

	// How the threads spent the checks of the update, per number of workers, from the profiles whose updates timed them
	int settleColumns[NB_REPORT_COLUMNS];
	int settleColumnCount = 0;
	for ( int c = 0; c < columnCount; ++c )
	{
		for ( int i = 0; i < count; ++i )
		{
			if ( profiles[i].workerCount == columns[c] && profiles[i].settleSampleCount > 0 )
			{
				settleColumns[settleColumnCount++] = columns[c];
				break;
			}
		}
	}

	if ( settleColumnCount > 0 )
	{
		nbAppend( &out, "The checks of the update in every eighth update, milliseconds per timed update, over all tasks of "
						"the checks\n" );
		nbAppend( &out, "%-30s", "part" );
		for ( int c = 0; c < settleColumnCount; ++c )
		{
			nbAppend( &out, " %5d w", settleColumns[c] );
		}
		nbAppend( &out, "\n" );

		enum
		{
			rowCount = 18
		};
		const char* rowNames[rowCount] = {
			"timed updates",
			"checks on the calling thread",
			"in blocks",
			"  checks for rest",
			"  support of quiet pieces",
			"before the first block",
			"between the blocks",
			"after the last block",
			"longest block",
			"tasks",
			"  without a block",
			"blocks",
			"moving pieces",
			"  quiet",
			"  on the calling thread %",
			"ns per check for rest",
			"us per support of a piece",
			"in blocks of the tasks %",
		};
		for ( int r = 0; r < rowCount; ++r )
		{
			nbAppend( &out, "%-30s", rowNames[r] );
			for ( int c = 0; c < settleColumnCount; ++c )
			{
				double sums[rowCount] = { 0.0 };
				double samples = 0.0;
				for ( int i = 0; i < count; ++i )
				{
					const nbProfile* p = profiles + i;
					if ( p->workerCount != settleColumns[c] || p->settleSampleCount == 0 )
					{
						continue;
					}

					double values[rowCount] = { 0.0,
												p->settleCall,
												p->settleWork,
												p->settleWork - p->settleSupportWork,
												p->settleSupportWork,
												p->settleLate,
												p->settleClaim,
												p->settleDone,
												p->settleLongestBlock,
												p->settleTaskCount,
												p->settleMissedCount,
												p->settleBlockCount,
												p->settleMovingCount,
												p->settleQuietCount,
												p->settleCallerCount,
												0.0,
												0.0,
												0.0 };
					for ( int v = 0; v < rowCount; ++v )
					{
						sums[v] += values[v] * (double)p->settleSampleCount;
					}
					samples += (double)p->settleSampleCount;
				}

				// The last rows relate the time in blocks to the pieces and to the time of the tasks in the checks, which
				// the time in blocks, before the first, between them and after the last add up to
				double taskTime = sums[2] + sums[5] + sums[6] + sums[7];
				if ( r == 0 )
				{
					nbAppend( &out, " %7.0f", samples );
				}
				else if ( r == 14 )
				{
					nbAppend( &out, " %7.1f", sums[12] > 0.0 ? 100.0 * sums[14] / sums[12] : 0.0 );
				}
				else if ( r == 15 )
				{
					nbAppend( &out, " %7.1f", sums[12] > 0.0 ? 1.0e6 * sums[3] / sums[12] : 0.0 );
				}
				else if ( r == 16 )
				{
					nbAppend( &out, " %7.2f", sums[13] > 0.0 ? 1.0e3 * sums[4] / sums[13] : 0.0 );
				}
				else if ( r == 17 )
				{
					nbAppend( &out, " %7.1f", taskTime > 0.0 ? 100.0 * sums[2] / taskTime : 0.0 );
				}
				else
				{
					nbAppend( &out, r >= 9 ? " %7.1f" : " %7.3f", sums[r] / samples );
				}
			}
			nbAppend( &out, "\n" );
		}
	}

	double packWait = 0.0;
	for ( int i = 0; i < count; ++i )
	{
		packWait += profiles[i].packWait * (double)profiles[i].stepCount;
	}
	if ( steps > 0.0 && packWait > 0.0 )
	{
		nbAppend( &out, "The calling thread waited for the last packs of islands %.3f ms per step\n", packWait / steps );
	}

	if ( settleSplits > 0.0 )
	{
		nbAppend( &out,
				  "The checks of the update went to the workers in %.1f %% of the steps with workers, they checked %.1f %% "
				  "of the moving debris there and started %.0f us after on average\n",
				  100.0 * settleSplits / workerSteps, 100.0 * settleShare / settleSplits, settleWake / settleTasks );
	}
	else if ( workerSteps > 0.0 )
	{
		nbAppend( &out, "The checks of the update stayed on the calling thread, too little debris moved for the workers\n" );
	}

	// The thresholds at the end of the profile with the most workers, see nbProfile::fractureWorkerItems
	const nbProfile* most = profiles;
	for ( int i = 1; i < count; ++i )
	{
		most = profiles[i].workerCount >= most->workerCount ? profiles + i : most;
	}
	if ( most->workerCount > 1 )
	{
		nbAppend( &out,
				  "With %d workers they helped with a fracture from %d items on and with the checks of the update from %d moving "
				  "pieces on\n",
				  most->workerCount, most->fractureWorkerItems, most->settleWorkerPieces );
	}
	return out.length;
}
