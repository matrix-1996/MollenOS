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
 * MollenOS x86 Common Threading Interface
 * - Contains shared x86 threading routines
 */

/* Includes 
 * - System */
#include <threading.h>
#include <process/phoenix.h>
#include <interrupts.h>
#include <thread.h>
#include <memory.h>
#include <heap.h>
#include <apic.h>
#include <gdt.h>
#include <log.h>

/* Includes
 * - Library */
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Extern access, we need access
 * to the timer-quantum and a bunch of 
 * assembly functions */
__EXTERN size_t GlbTimerQuantum;
__EXTERN void save_fpu(uintptr_t *buffer);
__EXTERN void set_ts(void);
__EXTERN void _yield(void);
__EXTERN void enter_thread(Context_t *Regs);
__EXTERN void enter_signal(Context_t *Regs, uintptr_t Handler, int Signal, uintptr_t Return);
__EXTERN void RegisterDump(Context_t *Regs);

/* Globals,
 * Keep track of whether or not init code has run */
static int __GlbThreadX86Initialized = 0;

/* The yield interrupt code for switching tasks
 * and is controlled by software interrupts, the yield interrupt
 * also like the apic switch need to reload the apic timer as it
 * controlls the primary switch */
InterruptStatus_t ThreadingYield(void *Args)
{
	/* Variables we will need for loading
	 * a new task */
	Context_t *Regs = NULL;
	UUId_t CurrCpu = ApicGetCpu();

	/* These will be assigned from the 
	 * _switch function, but set them in
	 * case threading is not initialized yet */
	size_t TimeSlice = 20;
	int TaskPriority = 0;

	/* Before we do anything, send EOI so 
	 * we don't forget :-) */
	ApicSendEoi(APIC_NO_GSI, INTERRUPT_YIELD);

	/* Switch Task, if there is no threading enabled yet
	 * it should return the same structure as we give */
	Regs = _ThreadingSwitch((Context_t*)Args, 0, &TimeSlice, &TaskPriority);

	/* If we just got hold of idle task, well fuck it disable timer 
	 * untill we get another task */
	if (!ThreadingIsCurrentTaskIdle(CurrCpu)) {
		ApicSetTaskPriority(61 - TaskPriority);
		ApicWriteLocal(APIC_INITIAL_COUNT, GlbTimerQuantum * TimeSlice);
	}
	else {
		ApicSetTaskPriority(0);
		ApicWriteLocal(APIC_INITIAL_COUNT, 0);
	}

	/* Enter new thread */
	enter_thread(Regs);

	/* Never reached */
	return InterruptHandled;
}

/* IThreadCreate
 * Initializes a new x86-specific thread context
 * for the given threading flags, also initializes
 * the yield interrupt handler first time its called */
void *IThreadCreate(Flags_t ThreadFlags, uintptr_t EntryPoint)
{
	/* Variables for initialization */
	MCoreInterrupt_t Interrupt;
	x86Thread_t *Thread = NULL;

	// Allocate a new thread context (x86) 
	// and zero it out
	Thread = (x86Thread_t*)kmalloc(sizeof(x86Thread_t));
	memset(Thread, 0, sizeof(x86Thread_t));

	// Allocate a new buffer for FPU operations  
	// and zero out the buffer space
	Thread->FpuBuffer = kmalloc_a(0x1000);
	memset(Thread->FpuBuffer, 0, 0x1000);

	// Initialize rest of params
	Thread->Flags = X86_THREAD_FPU_INITIALISED | X86_THREAD_USEDFPU;

	// Don't create contexts for idle threads 
	// Otherwise setup a kernel stack 
	if (!(ThreadFlags & THREADING_IDLE)) {
		Thread->Context = ContextCreate(ThreadFlags, EntryPoint, NULL);
	}

	// If its the first time we run, install
	// the yield interrupt
	if (__GlbThreadX86Initialized == 0) {
		__GlbThreadX86Initialized = 1;
		Interrupt.Data = NULL;
		Interrupt.Vectors[0] = INTERRUPT_YIELD;
		Interrupt.Line = APIC_NO_GSI;
		Interrupt.Pin = APIC_NO_GSI;
		Interrupt.FastHandler = ThreadingYield;
		InterruptRegister(&Interrupt, INTERRUPT_SOFTWARE | INTERRUPT_KERNEL);
	}

	/* Done */
	return Thread;
}

/* IThreadSetupUserMode
 * Initializes user-mode data for the given thread, and
 * allocates all neccessary resources (x86 specific) for
 * usermode operations */
void IThreadSetupUserMode(MCoreThread_t *Thread, uintptr_t StackAddress)
{
	/* Initialize a pointer to x86 specific data */
	x86Thread_t *tData = (x86Thread_t*)Thread->ThreadData;
	_CRT_UNUSED(StackAddress);

	/* Initialize a user/driver-context based on
	 * the requested runmode */
	tData->UserContext = ContextCreate(Thread->Flags,
		(uintptr_t)Thread->Function, (uintptr_t*)Thread->Args);

	/* Disable all port-access */
	memset(&tData->IoMap[0], 0xFF, GDT_IOMAP_SIZE);
}

/* IThreadDestroy
 * Free's all the allocated resources for x86
 * specific threading operations */
void IThreadDestroy(MCoreThread_t *Thread)
{
	/* Initialize a pointer to x86 specific data */
	x86Thread_t *tData = (x86Thread_t*)Thread->ThreadData;

	/* Cleanup both contexts */
	kfree(tData->Context);
	if (tData->UserContext != NULL) {
		kfree(tData->UserContext);
	}

	/* Free fpu buffer and the
	 * base structure */
	kfree(tData->FpuBuffer);
	kfree(tData);
}

/* IThreadWakeCpu
 * Wake's the target cpu from an idle thread
 * by sending it an yield IPI */
void IThreadWakeCpu(UUId_t Cpu)
{
	/* Send an IPI to the cpu */
	ApicSendIpi(Cpu, INTERRUPT_YIELD);
}

/* IThreadYield
 * Yields the current thread control to the scheduler */
void IThreadYield(void)
{
	_yield();
}

/* Dispatches a signal to the given process 
 * signals will always be dispatched to main thread */
OsStatus_t
SignalDispatch(
	_In_ MCoreAsh_t *Ash, 
	_In_ MCoreSignal_t *Signal)
{
	// Variables
	MCoreThread_t *Thread = ThreadingGetThread(Ash->MainThread);
	x86Thread_t *Thread86 = (x86Thread_t*)Thread->ThreadData;
	Context_t *Regs = NULL;

	/* User or kernel mode thread? */
	if (Thread->Flags & THREADING_USERMODE) {
		Regs = Thread86->UserContext;
	}
	else {
		Regs = Thread86->Context;
	}

	// Store current context
	memcpy(&Signal->Context, Regs, sizeof(Context_t));

	// Now we can enter the signal context 
	// handler, we cannot return from this function 
	enter_signal(Regs, Signal->Handler, Signal->Signal, MEMORY_LOCATION_SIGNAL_RET);

	// We don't reach this
	return OsSuccess;
}

/* This function switches the current runtime-context
 * out with the given thread context, this should only
 * be used as a temporary way of impersonating another
 * thread */
void IThreadImpersonate(MCoreThread_t *Thread)
{
	/* Variables we will need for the
	* context switch */
	x86Thread_t *Tx = NULL;
	UUId_t Cpu = 0;

	/* Load cpu info */
	Cpu = ApicGetCpu();

	/* Initiate the x86 specific thread data pointer */
	Tx = (x86Thread_t*)Thread->ThreadData;

	/* Load io-map */
	TssUpdateIo(Cpu, &Tx->IoMap[0]);

	/* Load the address space */
	MmVirtualSwitchPageDirectory(Cpu,
		(PageDirectory_t*)Thread->AddressSpace->PageDirectory,
		Thread->AddressSpace->Cr3);
}

/* This function loads a new task from the scheduler, it
 * implements the task-switching functionality, which MCore leaves
 * up to the underlying architecture */
Context_t *_ThreadingSwitch(Context_t *Regs,
	int PreEmptive, size_t *TimeSlice, int *TaskQueue)
{
	/* Variables we will need for the
	 * context switch */
	MCoreThread_t *Thread = NULL;
	x86Thread_t *Tx = NULL;
	UUId_t Cpu = 0;

	/* Start out by sanitizing the state
	 * of threading, don't schedule */
	if (ThreadingIsEnabled() != 1) {
		return Regs;
	}

	/* Lookup cpu and threading info */
	Cpu = ApicGetCpu();
	Thread = ThreadingGetCurrentThread(Cpu);

	/* ASsert some sanity in this function! */
	assert(Thread != NULL && Regs != NULL);

	/* Initiate the x86 specific thread data pointer */
	Tx = (x86Thread_t*)Thread->ThreadData;

	/* Save FPU/MMX/SSE information if it's
	 * been used, otherwise skip this and save time */
	if (Tx->Flags & X86_THREAD_USEDFPU) {
		save_fpu(Tx->FpuBuffer);
	}

	/* Save stack, we have a few cases here. 
	 * We are using kernel stack in case of two things:
	 * 1. Transitioning threads
	 * 2. Kernel threads (surprise!) */
	if ((THREADING_RUNMODE(Thread->Flags) == THREADING_KERNELMODE)
		|| (Thread->Flags & THREADING_SWITCHMODE)) {
		Tx->Context = Regs;
	}
	else {
		Tx->UserContext = Regs;
	}
	
	/* Lookup a new thread and initiate our pointers */
	Thread = ThreadingSwitch(Cpu, Thread, PreEmptive);
	Tx = (x86Thread_t*)Thread->ThreadData;

	/* Update scheduler variables */
	*TimeSlice = Thread->TimeSlice;
	*TaskQueue = Thread->Queue;

	/* Update addressing space */
	MmVirtualSwitchPageDirectory(Cpu, 
		(PageDirectory_t*)Thread->AddressSpace->PageDirectory, 
		Thread->AddressSpace->Cr3);

	/* Update TSS information (stack/iomap) */
	TssUpdateStack(Cpu, (uintptr_t)Tx->Context);
	TssUpdateIo(Cpu, &Tx->IoMap[0]);

	/* Clear FPU/MMX/SSE flags */
	Tx->Flags &= ~X86_THREAD_USEDFPU;

	/* We want to handle any signals if neccessary
	 * before we handle the transition */
	SignalHandle(Thread->Id);

	/* Handle the transition, we have to remove
	 * the bit as we now have transitioned */
	if (Thread->Flags & THREADING_TRANSITION) {
		Thread->Flags &= ~(THREADING_SWITCHMODE | THREADING_TRANSITION);
	}

	/* Set TS bit in CR0 */
	set_ts();

	/* Return new stack */
	if ((THREADING_RUNMODE(Thread->Flags) == THREADING_KERNELMODE)
		|| (Thread->Flags & THREADING_SWITCHMODE)) {
		return Tx->Context;
	}	
	else {
		return Tx->UserContext;
	}
}
