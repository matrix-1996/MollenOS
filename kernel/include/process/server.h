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
 * MollenOS MCore - Server & Process Management
 * - The process/server manager is known as Phoenix
 */

#ifndef _MCORE_SERVER_H_
#define _MCORE_SERVER_H_

/* Includes
* - C-Library */
#include <os/osdefs.h>

/* Includes 
 * - System */
#include <process/phoenix.h>
#include <bitmap.h>

/* The base of an server process, servers
 * are derived from Ashes, and merely extensions
 * to support userland stuff, and run in a larger
 * memory segment to allow for device memory */
typedef struct _MCoreServer
{
	/* We derive from Ashes */
	MCoreAsh_t	Base;

	/* We want to be able to keep track 
	 * of some driver-features that we have
	 * available, like io-space memory */
	Bitmap_t	*DriverMemory;

	/* Also we allow for data arguments to
	 * to be copied into the userspace on startup */
	void		*ArgumentBuffer;
	size_t		ArgumentLength;

	/* Since we only have one server per driver
	 * but multiple instances, we keep track of devinfo */
	DevInfo_t	VendorId;
	DevInfo_t	DeviceId;
	DevInfo_t	DeviceClass;
	DevInfo_t	DeviceSubClass;

} MCoreServer_t;

/* PhoenixCreateServer
 * This function loads the executable and
 * prepares the ash-server-environment, at this point
 * it won't be completely running yet, it needs its own thread for that */
KERNELAPI UUId_t PhoenixCreateServer(MString_t *Path, void *Arguments, size_t Length);

/* PhoenixCleanupServer
 * Cleans up all the server-specific resources allocated
 * this this AshServer, and afterwards call the base-cleanup */
KERNELAPI void PhoenixCleanupServer(MCoreServer_t *Server);

/* PhoenixGetServer
 * This function looks up a server structure by id */
KERNELAPI
MCoreServer_t*
KERNELABI
PhoenixGetServer(
	_In_ UUId_t ServerId);

/* PhoenixGetCurrentServer
 * If the current running process is a server then it
 * returns the server structure, otherwise NULL */
KERNELAPI
MCoreServer_t*
KERNELABI
PhoenixGetCurrentServer(void);

/* GetServerByDriver
 * Retrieves a running server by driver-information
 * to avoid spawning multiple servers */
KERNELAPI MCoreServer_t *PhoenixGetServerByDriver(DevInfo_t VendorId,
	DevInfo_t DeviceId, DevInfo_t DeviceClass, DevInfo_t DeviceSubClass);

#endif //!_MCORE_SERVER_H_
