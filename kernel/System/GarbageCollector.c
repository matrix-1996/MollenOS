/* MollenOS
*
* Copyright 2011 - 2016, Philip Meulengracht
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation ? , either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.If not, see <http://www.gnu.org/licenses/>.
*
*
* MollenOS Garbage Collector
* Cleans up:
* - Threads
* - Processes
* - Heap (Not yet)
*/

/* Includes */
#include <GarbageCollector.h>
#include <ProcessManager.h>
#include <Semaphore.h>
#include <Threading.h>
#include <Log.h>

/* Prototypes */
void GcWorker(void *Args);

/* Globals */
Semaphore_t *GlbGcEventLock = NULL;
int GlbGcInitialized = 0;

/* Initializes the gc */
void GcInit(void)
{
	/* Debug */
	LogInformation("GCLL", "Initializing Garbage Collector");

	/* Setup */
	GlbGcEventLock = SemaphoreCreate(0);
	GlbGcInitialized = 1;

	/* Start worker */
	ThreadingCreateThread("Garbage Collector", GcWorker, NULL, 0);
}

/* Adds garbage to the collector */
void GcAddWork(void)
{
	/* Sanity */
	if (GlbGcInitialized != 1)
		return;

	/* Signal */
	SemaphoreV(GlbGcEventLock);
}

/* The Gc Collect Thread */
void GcWorker(void *Args)
{
	/* No warnings */
	int Run = 1;

	/* Silence another warning */
	_CRT_UNUSED(Args);

	/* Run thread forever */
	while (Run)
	{
		/* Wait for an event */
		SemaphoreP(GlbGcEventLock);

		/* Cleanup Threads */
		ThreadingReapZombies();

		/* Cleanup Processes */
		PmReapZombies();

		/* Reap the Heap */
	}
}