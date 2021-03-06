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
 * MollenOS X86 CMOS & RTC (Clock) Driver
 * http://wiki.osdev.org/CMOS#The_Real-Time_Clock
 */

/* Includes 
 * - System */
#include <os/driver/contracts/timer.h>
#include <os/driver/contracts/clock.h>
#include <os/driver/acpi.h>
#include "cmos.h"

/* Includes
 * - Library */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Since there only exists a single cmos
 * chip on-board we keep some static information
 * in this driver */
static Cmos_t *GlbCmos = NULL;

/* CmosRead
 * Read the byte at given register offset
 * from the CMOS-Chip */
uint8_t CmosRead(uint8_t Register)
{
	// Variables
	uint8_t Tmp = 0;
	
	// Keep NMI if disabled
	Tmp = ReadIoSpace(&GlbCmos->IoSpace, CMOS_IO_SELECT, 1) & CMOS_NMI_BIT;

	// Select Register (but do not change NMI)
	WriteIoSpace(&GlbCmos->IoSpace, CMOS_IO_SELECT, (Tmp | (Register & CMOS_ALLBITS_NONMI)), 1);
	return (uint8_t)ReadIoSpace(&GlbCmos->IoSpace, CMOS_IO_DATA, 1);
}

/* CmosRead
 * Writes a byte to the given register offset
 * from the CMOS-Chip */
void CmosWrite(uint8_t Register, uint8_t Data)
{
	// Variables
	uint8_t Tmp = 0;

	// Keep NMI if disabled
	Tmp = (uint8_t)ReadIoSpace(&GlbCmos->IoSpace, CMOS_IO_SELECT, 1) & CMOS_NMI_BIT;

	// Select Register (but do not change NMI)
	WriteIoSpace(&GlbCmos->IoSpace, CMOS_IO_SELECT,
		(Tmp | (Register & CMOS_ALLBITS_NONMI)), 1);
	WriteIoSpace(&GlbCmos->IoSpace, CMOS_IO_DATA, Data, 1);
}

/* CmosGetTime
 * Retrieves the current time and stores it in
 * the c-library time structure */
void CmosGetTime(struct tm *Time)
{
	// Variables
	int Sec, Counter;
	uint8_t Century = 0;

	// Do we support century?
	if (GlbCmos->AcpiCentury != 0) {
		Century = CmosRead(GlbCmos->AcpiCentury);
	}	

	// Get Clock (Stable, thats why we loop)
	while (CmosRead(CMOS_REGISTER_SECONDS) != Time->tm_sec
		|| CmosRead(CMOS_REGISTER_MINUTES) != Time->tm_min
		|| CmosRead(CMOS_REGISTER_HOURS) != Time->tm_hour
		|| CmosRead(CMOS_REGISTER_DAYS) != Time->tm_mday
		|| CmosRead(CMOS_REGISTER_MONTHS) != Time->tm_mon
		|| CmosRead(CMOS_REGISTER_YEARS) != Time->tm_year) {
		// Reset variables
		Sec = -1;
		Counter = 0;

		// Update Seconds
		while (Counter < 2) {
			if (CmosRead(CMOS_REGISTER_STATUS_A) & CMOSA_UPDATE_IN_PROG) {
				continue;
			}
			Time->tm_sec = CmosRead(CMOS_REGISTER_SECONDS);

			// Seconds changed.  First from -1, then because the
			// clock ticked, which is what we're waiting for to
			// get a precise reading.
			if (Time->tm_sec != Sec) {
				Sec = Time->tm_sec;
				Counter++;
			}
		}

		// Read the other registers.
		Time->tm_min = CmosRead(CMOS_REGISTER_MINUTES);
		Time->tm_hour = CmosRead(CMOS_REGISTER_HOURS);
		Time->tm_mday = CmosRead(CMOS_REGISTER_DAYS);
		Time->tm_mon = CmosRead(CMOS_REGISTER_MONTHS);
		Time->tm_year = CmosRead(CMOS_REGISTER_YEARS);
	}

	// Convert time format? 
	// - Convert BCD to binary (default RTC mode).
	if (!(CmosRead(CMOS_REGISTER_STATUS_B) & CMOSB_BCD_FORMAT)) {
		Time->tm_year = CMOS_BCD_TO_DEC(Time->tm_year);
		Time->tm_mon = CMOS_BCD_TO_DEC(Time->tm_mon);
		Time->tm_mday = CMOS_BCD_TO_DEC(Time->tm_mday);
		Time->tm_hour = CMOS_BCD_TO_DEC(Time->tm_hour);
		Time->tm_min = CMOS_BCD_TO_DEC(Time->tm_min);
		Time->tm_sec = CMOS_BCD_TO_DEC(Time->tm_sec);
		if (Century != 0) {
			Century = CMOS_BCD_TO_DEC(Century);
		}	
	}

	// Counts from 0
	Time->tm_mon--;

	// Correct the year
	if (Century != 0) {
		Time->tm_year += Century * 100;
	}
	else {
		Time->tm_year += (CMOS_CURRENT_YEAR / 100) * 100;
		if (Time->tm_year < CMOS_CURRENT_YEAR) {
			Time->tm_year += 100;
		}
	}
}

/* OnInterrupt
 * Is called when one of the registered devices
 * produces an interrupt. On successful handled
 * interrupt return OsSuccess, otherwise the interrupt
 * won't be acknowledged */
InterruptStatus_t OnInterrupt(void *InterruptData)
{
	// Since the cmos doesn't use
	// interrupts, just redirect the interrupt
	// to the RTC code
	return RtcInterrupt((Cmos_t*)InterruptData);
}

/* OnLoad
 * The entry-point of a driver, this is called
 * as soon as the driver is loaded in the system */
OsStatus_t OnLoad(void)
{
	// Variables
	AcpiDescriptor_t Acpi;

	// Allocate a new instance of the cmos-data
	GlbCmos = (Cmos_t*)malloc(sizeof(Cmos_t));
	memset(GlbCmos, 0, sizeof(Cmos_t));

	// Create the io-space, again we have to create
	// the io-space ourselves
	GlbCmos->IoSpace.Type = IO_SPACE_IO;
	GlbCmos->IoSpace.PhysicalBase = CMOS_IO_BASE;
	GlbCmos->IoSpace.Size = CMOS_IO_LENGTH;
	GlbCmos->UseRTC = 1;

	// Initialize the interrupt request
	GlbCmos->Interrupt.Line = CMOS_RTC_IRQ;
	GlbCmos->Interrupt.Pin = INTERRUPT_NONE;
	GlbCmos->Interrupt.Vectors[0] = INTERRUPT_NONE;
	GlbCmos->Interrupt.FastHandler = OnInterrupt;
	GlbCmos->Interrupt.Data = GlbCmos;

	// Create the io-space in system
	if (CreateIoSpace(&GlbCmos->IoSpace) != OsSuccess) {
		return OsError;
	}

	// Query the system for acpi-information 
	// - Check for century register
	// - Check if we should disable rtc
	if (AcpiQueryStatus(&Acpi) == OsSuccess) {
		GlbCmos->AcpiCentury = Acpi.Century;
		if (Acpi.BootFlags & ACPI_IA_NO_CMOS_RTC) {
			GlbCmos->UseRTC = 0;
		}
	}

	// No problem, last thing is to acquire the
	// io-space, and just return that as result
	if (AcquireIoSpace(&GlbCmos->IoSpace) != OsSuccess) {
		return OsError;
	}

	// Initialize the cmos-contract
	InitializeContract(&GlbCmos->Clock, UUID_INVALID, 1, 
		ContractClock, "CMOS Clock Interface");

	// Last part is to initialize the rtc
	// chip if it present in system
	if (GlbCmos->UseRTC) {
		return RtcInitialize(GlbCmos);
	}
	else {
		return OsSuccess;
	}
}

/* OnUnload
 * This is called when the driver is being unloaded
 * and should free all resources allocated by the system */
OsStatus_t OnUnload(void)
{
	// Cleanup rtc before we cleanup
	// the io-space
	if (GlbCmos->UseRTC) {
		RtcCleanup(GlbCmos);
	}

	// Destroy the io-space we created
	if (GlbCmos->IoSpace.Id != 0) {
		ReleaseIoSpace(&GlbCmos->IoSpace);
		DestroyIoSpace(GlbCmos->IoSpace.Id);
	}

	// Free up allocated resources
	free(GlbCmos);
	return OsSuccess;
}

/* OnRegister
 * Is called when the device-manager registers a new
 * instance of this driver for the given device */
OsStatus_t OnRegister(MCoreDevice_t *Device)
{
	// Update contracts to bind to id 
	// The CMOS/RTC is a fixed device
	// and thus we don't support multiple instances
	if (GlbCmos->Clock.DeviceId == UUID_INVALID) {
		GlbCmos->Clock.DeviceId = Device->Id;
		GlbCmos->Timer.DeviceId = Device->Id;
	}

	// Now register the clock contract
	RegisterContract(&GlbCmos->Clock);

	// Only register the RTC if we use it
	if (GlbCmos->UseRTC) {
		return RegisterContract(&GlbCmos->Timer);
	}
	else {
		return OsSuccess;
	}
}

/* OnUnregister
 * Is called when the device-manager wants to unload
 * an instance of this driver from the system */
OsStatus_t OnUnregister(MCoreDevice_t *Device)
{
	// The CMOS/RTC is a fixed device
	// and thus we don't support multiple instances
	_CRT_UNUSED(Device);
	return OsSuccess;
}

/* OnQuery
 * Occurs when an external process or server quries
 * this driver for data, this will correspond to the query
 * function that is defined in the contract */
OsStatus_t 
OnQuery(_In_ MContractType_t QueryType, 
		_In_ int QueryFunction, 
		_In_Opt_ RPCArgument_t *Arg0,
		_In_Opt_ RPCArgument_t *Arg1,
		_In_Opt_ RPCArgument_t *Arg2, 
		_In_ UUId_t Queryee, 
		_In_ int ResponsePort)
{
	// Static storage to avoid 
	// allocation and freeing
	struct tm _time;
	
	// Unused parameters
	_CRT_UNUSED(Arg0);
	_CRT_UNUSED(Arg1);
	_CRT_UNUSED(Arg2);

	// Which kind of query type is being done?
	if (QueryType == ContractClock
		&& QueryFunction == __CLOCK_QUERY) {
		CmosGetTime(&_time);
		PipeSend(Queryee, ResponsePort, &_time, sizeof(struct tm));
	}
	else if (QueryType == ContractTimer
		&& QueryFunction == __TIMER_QUERY) {
		PipeSend(Queryee, ResponsePort, &GlbCmos->Ticks, sizeof(clock_t));
	}

	// Done!
	return OsSuccess;
}
