/* MollenOS
*
* Copyright 2011 - 2014, Philip Meulengracht
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
* MollenOS X86-32 PS/2 Keyboard Driver
*/

/* Includes */
#include <Arch.h>
#include <Drivers\Ps2\Ps2.h>
#include <Drivers\Ps2\Keyboard\Ps2Keyboard.h>
#include <Heap.h>
#include <stdio.h>
#include <string.h>
#include <InputManager.h>

/* Scansets */
#include <ScancodeSets\ScancodeSet2.h>

/* Irq Handler */
int Ps2KeyboadIrqHandler(void *Args)
{
	/* Get datastructure */
	Ps2KeyboardDevice_t *Ps2Dev = (Ps2KeyboardDevice_t*)Args;
	uint8_t Scancode = 0;
	ImButtonEvent_t bEvent;

	/* Get scancode */
	Scancode = Ps2ReadData(1);

	/* Special Cases */
	if (Scancode == EXTENDED)
	{
		Ps2Dev->Flags |= X86_PS2_KBD_FLAG_EXTENDED;
		Ps2Dev->Buffer = 0xE000;
	}
	else if (Scancode == RELEASED)
		Ps2Dev->Flags |= X86_PS2_KBD_FLAG_RELEASED;
	else if (Scancode < (uint8_t)EXTENDED
			&& Scancode > 0x00)
	{
		/* Oh god actual scancode */
		Ps2Dev->Buffer |= Scancode;
		bEvent.Type = MCORE_INPUT_TYPE_KEYBOARD;
		bEvent.State = 
			((Ps2Dev->Flags & X86_PS2_KBD_FLAG_RELEASED) == 1) ? 
							MCORE_INPUT_BUTTON_RELEASED : MCORE_INPUT_BUTTON_CLICKED;
		
		/* Convert scancode to a shared keycode format */
		if (Ps2Dev->ScancodeSet == 2)
			bEvent.Data = ScancodeSet2ToMCore((ScancodeSet2)Ps2Dev->Buffer);

		/* Reset */
		Ps2Dev->Flags = 0;
		Ps2Dev->Buffer = 0;

		/* Send */
		InputManagerCreateButtonEvent(&bEvent);
	}

	/* Done! */
	return X86_IRQ_HANDLED;
}

/* Setup */
void Ps2KeyboardInit(int Port, int Translation)
{
	uint8_t Response = 0;

	/* Allocate Data Structure */
	Ps2KeyboardDevice_t *Ps2Dev = (Ps2KeyboardDevice_t*)kmalloc(sizeof(Ps2KeyboardDevice_t));
	memset(Ps2Dev, 0, sizeof(Ps2KeyboardDevice_t));
	Ps2Dev->Port = Port;
	Ps2Dev->Flags = 0;
	Ps2Dev->Buffer = 0;

	/* Install Irq */
	if (Port == 1)
		InterruptInstallISA(X86_PS2_PORT1_INTERRUPT, INTERRUPT_PS2_PORT1, Ps2KeyboadIrqHandler, (void*)Ps2Dev);
	else
		InterruptInstallISA(X86_PS2_PORT2_INTERRUPT, INTERRUPT_PS2_PORT2, Ps2KeyboadIrqHandler, (void*)Ps2Dev);

	/* Set scancode set to 2 */
	if (Port == 2)
		Ps2SendCommand(X86_PS2_CMD_SELECT_PORT2);
	
	/* Select command */
	Ps2WriteData(X86_PS2_CMD_SET_SCANCODE);
	Response = Ps2ReadData(1);

	if (Port == 2)
		Ps2SendCommand(X86_PS2_CMD_SELECT_PORT2);

	/* Send value */
	Ps2WriteData(0x2);
	Response = Ps2ReadData(1);

	if (Response == 0xFA)
		Ps2Dev->ScancodeSet = 2;

	/* Either disable or enable */
	if (Port == 2)
		Ps2SendCommand(X86_PS2_CMD_SELECT_PORT2);
	Ps2WriteData(Translation ? 0x65 : 0x25);

	/* Ack */
	Response = Ps2ReadData(1);

	/* Enable scan, 0xF4 */
	if (Port == 2)
		Ps2SendCommand(X86_PS2_CMD_SELECT_PORT2);
	Ps2WriteData(0xF4);

	/* Ack */
	Response = Ps2ReadData(1);
}