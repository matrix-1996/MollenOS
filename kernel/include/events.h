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
* MollenOS MCore - Generic Event System
*/

#ifndef _MCORE_EVENTS_H_
#define _MCORE_EVENTS_H_

/* Includes 
 * - C Library */
#include <os/osdefs.h>
#include <ds/list.h>

/* Includes
 * - System */
#include <Semaphore.h>

/* This describes the current state
 * of an event, this means the sender
 * can check the status all the sender
 * wants*/
typedef enum _EventState
{
	EventPending,
	EventInProgress,
	EventOk,
	EventFailed,
	EventCancelled

} EventState_t;

/* Generic format used for the
 * different enums.. they are 
 * all ints anyway */
typedef int Enum_t;

/* Generic Event/Request 
 * Describes the BASE of an 
 * event that might occur, 
 * it's important to 'inherit' this */
typedef struct _MCoreEvent
{
	/* Owner 
	 * The thread that owns this 
	 * event */
	UUId_t Owner;

	/* Type 
	 * This the type of event 
	 * that can happen */
	Enum_t Type;

	/* State 
	 * This is the current state
	 * of the event */
	EventState_t State;

	/* Queue
	 * This keeps track of people
	 * waiting on this request to finish */
	Semaphore_t Queue;

	/* Auto Cleanup 
	 * This means the event-handler
	 * must cleanup this request */
	int Cleanup;

} MCoreEvent_t;

/* Define the default event callback
 * it passes on the event that has been
 * sent */
typedef int (*EventCallback)(void*, MCoreEvent_t*);

/* Generic Event Handler 
 * This describes an event handler 
 * The event handler keeps track of 
 * events and locks */
typedef struct _MCoreEventHandler
{
	/* Name of event handler */
	MString_t *Name;
	UUId_t ThreadId;

	/* Status of event-handler */
	int Running;

	/* Event Stuff */
	List_t *Events;
	Semaphore_t *Lock;

	/* Callback */
	EventCallback Callback;
	void *UserData;

} MCoreEventHandler_t;

/* Prototypes */

/* Event Init/Destruct 
 * Starts or stops handling events 
 * with the given callback */
KERNELAPI MCoreEventHandler_t *EventInit(const char *Name, EventCallback Callback, void *Data);
KERNELAPI void EventDestruct(MCoreEventHandler_t *EventHandler);

/* Event Create 
 * Queues up a new event for the
 * event handler to process 
 * Asynchronous operation */
KERNELAPI void EventCreate(MCoreEventHandler_t *EventHandler, MCoreEvent_t *Event);

/* Event Wait 
 * Waits for a specific event 
 * to either complete, fail or 
 * be cancelled */
KERNELAPI void EventWait(MCoreEvent_t *Event, size_t Timeout);

/* Event Cancel 
 * Cancels a specific event, 
 * event might not be cancelled 
 * immediately */
KERNELAPI void EventCancel(MCoreEvent_t *Event);

#endif //!_MCORE_EVENTS_H_