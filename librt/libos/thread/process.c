/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
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
 * MollenOS MCore - Process Definitions & Structures
 * - This header describes the base process-structures, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

/* Includes 
 * - System */
#include <os/process.h>
#include <os/syscall.h>

/* Includes
 * - Library */
#include <stddef.h>

/* ProcessSpawn
 * Spawns a new process by the given path and
 * optionally the given parameters are passed 
 * returns UUID_INVALID in case of failure unless Asynchronous is set
 * then this call will always result in UUID_INVALID. */
UUId_t 
ProcessSpawn(
	_In_ __CONST char *Path,
	_In_Opt_ __CONST char *Arguments,
	_In_ int Asynchronous)
{
	// Sanitize parameters
	if (Path == NULL) {
		return UUID_INVALID;
	}

	// Redirect the call to the OS
	return (UUId_t)Syscall3(SYSCALL_PROCSPAWN,
		SYSCALL_PARAM(Path), SYSCALL_PARAM(Arguments),
		SYSCALL_PARAM(Asynchronous));
}

/* ProcessJoin
 * Waits for the given process to terminate and
 * returns the return-code the process exit'ed with */
int 
ProcessJoin(
	_In_ UUId_t Process)
{
	/* Sanitize the given id */
	if (Process == UUID_INVALID) {
		return -1;
	}

	/* Redirect call */
	return Syscall1(SYSCALL_PROCJOIN, SYSCALL_PARAM(Process));
}

/* ProcessKill
 * Terminates the process with the given id */
OsStatus_t 
ProcessKill(
	_In_ UUId_t Process)
{
	/* Sanitize the given id */
	if (Process == UUID_INVALID) {
		return OsError;
	}

	/* Redirect the call */
	return (OsStatus_t)Syscall1(SYSCALL_PROCKILL, SYSCALL_PARAM(Process));
}

/* ProcessQuery
 * Queries information about the given process
 * based on the function it returns the requested information */
OsStatus_t 
ProcessQuery(
	_In_ UUId_t Process, 
	_In_ ProcessQueryFunction_t Function, 
	_In_ void *Buffer, 
	_In_ size_t Length)
{
	/* Prep for syscall */
	return Syscall4(SYSCALL_PROCQUERY, SYSCALL_PARAM(Process),
		SYSCALL_PARAM(Function), SYSCALL_PARAM(Buffer),
		SYSCALL_PARAM(Length));
}
