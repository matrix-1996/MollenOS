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
 * http://wiki.osdev.org/RTC
 */

/* Includes 
 * - System */
#include <os/driver/contracts/clock.h>
#include "cmos.h"

/* Includes
 * - Library */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* RtcInterrupt
 * Handles the rtc interrupt and acknowledges
 * the interrupt by reading cmos */
OsStatus_t RtcInterrupt(Cmos_t *Chip)
{
	/* Update Peroidic Tick Counter */
	Chip->NsCounter += Chip->NsTick;

	/* Acknowledge interrupt by reading register C */
	CmosRead(CMOS_REGISTER_STATUS_C);

	/* Done! */
	return OsNoError;
}

/* RtcInitialize
 * Initializes the rtc-part of the cmos chip
 * and installs the interrupt needed */
OsStatus_t RtcInitialize(Cmos_t *Chip)
{
	/* Variables for rtc initialization */
	uint8_t StateB = 0;
	uint8_t Rate = 0x08; /* must be between 3 and 15 */

	/* Ms is .97, 1024 ints per sec */
	/* Frequency = 32768 >> (rate-1), 15 = 2, 14 = 4, 13 = 8/s (125 ms) */
	Chip->NsTick = 976;
	Chip->NsCounter = 0;
	Chip->AlarmTicks = 0;

	/* Initialize the rtc-contract */
	InitializeContract(&Chip->Timer, UUID_INVALID, 1,
		ContractTimer, "CMOS RTC Timer Interface");

	/* Disable RTC Irq */
	StateB = CmosRead(CMOS_REGISTER_STATUS_B);
	StateB &= ~(0x70);
	CmosWrite(CMOS_REGISTER_STATUS_B, StateB);
	
	/* Update state_b */
	StateB = CmosRead(CMOS_REGISTER_STATUS_B);

	/* Set Frequency */
	CmosWrite(CMOS_REGISTER_STATUS_A, 0x20 | Rate);

	/* Clear pending interrupt */
	CmosRead(CMOS_REGISTER_STATUS_C);

	/* Install interrupt in system */
	//TODO

	/* Enable Periodic Interrupts */
	StateB = CmosRead(CMOS_REGISTER_STATUS_B);
	StateB |= CMOSB_RTC_PERIODIC;
	CmosWrite(CMOS_REGISTER_STATUS_B, StateB);

	/* Clear pending interrupt again */
	CmosRead(CMOS_REGISTER_STATUS_C);

	/* Done! */
	return OsNoError;
}

/* RtcCleanup
 * Disables the rtc and cleans up resources */
OsStatus_t RtcCleanup(Cmos_t *Chip)
{
	/* Variables for rtc cleanup */
	uint8_t StateB = 0;

	/* Disable RTC Irq */
	StateB = CmosRead(CMOS_REGISTER_STATUS_B);
	StateB &= ~(0x70);
	CmosWrite(CMOS_REGISTER_STATUS_B, StateB);

	/* Uninstall interrupt in system */
	_CRT_UNUSED(Chip);
	//TODO

	/* Done */
	return OsNoError;
}