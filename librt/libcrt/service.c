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
 * MollenOS C Library - Server Entry 
 */

/* Includes 
 * - System */
#include <os/threadpool.h>
#include <os/mollenos.h>

/* Includes
 * - Driver */
#include <os/driver/service.h>

/* Includes
 * - C-Library */
#include <ds/list.h>
#include <stdlib.h>

/* Extern
 * - C/C++ Initialization
 * - C/C++ Cleanup */
__EXTERN void __CppInit(void);
__EXTERN void __CppFinit(void);
MOSAPI void __CppInitVectoredEH(void);

/* CRT Initialization sequence
 * for a shared C/C++ environment
 * call this in all entry points */
void _mCrtInit(ThreadLocalStorage_t *Tls)
{
	__CppInit();
	TLSInitInstance(Tls);
	TLSInit();
	__CppInitVectoredEH();
}

/* Server event entry point
 * Used in multi-threading environment as means to cleanup
 * all allocated resources properly */
int _mDrvEvent(void *Argument)
{
	// Initiate the message pointer
	MRemoteCall_t *Message = (MRemoteCall_t*)Argument;
	OsStatus_t Result = OnEvent(Message);
	
	// Cleanup and return result
	RPCCleanup(Message);
	free(Message);
	return Result == OsSuccess ? 0 : -1;
}

/* Server Entry Point
 * Use this entry point for servers */
void _mDrvCrt(void)
{
	// Variables
	ThreadLocalStorage_t Tls;
	MRemoteCall_t Message;
#ifdef __SERVER_MULTITHREADED
	ThreadPool_t *ThreadPool;
#endif
	int IsRunning = 1;

	// Initialize environment
	_mCrtInit(&Tls);

	// Initialize default pipes
	PipeOpen(PIPE_RPCOUT);
	PipeOpen(PIPE_RPCIN);

	// Call the driver load function 
	// - This will be run once, before loop
	if (OnLoad() != OsSuccess) {
		OnUnload();
		goto Cleanup;
	}

	// Initialize threadpool
#ifdef __SERVER_MULTITHREADED
	if (ThreadPoolInitialize(THREADPOOL_DEFAULT_WORKERS, 
		&ThreadPool) != OsSuccess) {
		OnUnload();
		goto Cleanup;
	}

	// Initialize the server event loop
	while (IsRunning) {
		if (RPCListen(&Message) == OsSuccess) {
			MRemoteCall_t *RpcCopy = (MRemoteCall_t*)malloc(sizeof(MRemoteCall_t));
			memcpy(RpcCopy, &Message, sizeof(MRemoteCall_t));
			ThreadPoolAddWork(ThreadPool, _mDrvEvent, RpcCopy);
		}
		else {}
	}

	// Wait for threads to finish
	if (ThreadPoolGetWorkingCount(ThreadPool) != 0) {
		ThreadPoolWait(ThreadPool);
	}

	// Destroy thread-pool
	ThreadPoolDestroy(ThreadPool);

#else
	// Initialize the server event loop
	while (IsRunning) {
		if (RPCListen(&Message) == OsSuccess) {
			OnEvent(&Message);
			RPCCleanup(&Message);
		}
		else {}
	}
#endif

	// Call unload, so driver can cleanup
	OnUnload();

Cleanup:
	// Cleanup allocated resources
	// and perform a normal exit
	PipeClose(PIPE_RPCOUT);
	PipeClose(PIPE_RPCIN);
	exit(-1);
}
