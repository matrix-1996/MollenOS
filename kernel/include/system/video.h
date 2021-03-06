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
 * MollenOS Video Interface (Boot)
 * - Contains the shared kernel video interface
 *   that all sub-layers / architectures must conform to
 */

#ifndef _MCORE_SYSVIDEO_H_
#define _MCORE_SYSVIDEO_H_

/* Includes 
 * - Library */
#include <os/osdefs.h>
#include <os/spinlock.h>

/* Includes
 * - System */
#include <os/driver/contracts/video.h>
#include <arch.h>

PACKED_TYPESTRUCT(BootTerminal, {
	Flags_t						Type;
	Spinlock_t					Lock;
	VideoDescriptor_t			Info;

	unsigned					CursorX;
	unsigned					CursorY;
	
	unsigned					CursorStartX;
	unsigned					CursorStartY;
	unsigned					CursorLimitX;
	unsigned					CursorLimitY;

	uint32_t					FgColor;
	uint32_t					BgColor;
});

/* Video Type Definitions
 * Currently only two kind of video types needs
 * to be supported for boot - textmode or lfbmode */
#define VIDEO_NONE				0x00000000
#define VIDEO_TEXT				0x00000001
#define VIDEO_GRAPHICS			0x00000002

/* VideoGetTerminal
 * Retrieves the current terminal information */
KERNELAPI
BootTerminal_t*
KERNELABI
VideoGetTerminal(void);

/* VideoDrawPixel
 * Draws a pixel of the given color at the specified
 * pixel-position */
KERNELAPI
OsStatus_t
KERNELABI
VideoDrawPixel(
	_In_ unsigned X, 
	_In_ unsigned Y, 
	_In_ uint32_t Color);

/* VideoDrawCharacter
 * Renders a character of the given color(s) 
 * at the specified pixel-position */
KERNELAPI
OsStatus_t
KERNELABI
VideoDrawCharacter(
	_In_ unsigned X, 
	_In_ unsigned Y, 
	_In_ int Character, 
	_In_ uint32_t Bg, 
	_In_ uint32_t Fg);

/* VideoPutCharacter
 * Renders a character with default colors
 * at the current terminal position */
KERNELAPI
OsStatus_t
KERNELABI
VideoPutCharacter(
	_In_ int Character);

#endif //!_MCORE_SYSVIDEO_H_
