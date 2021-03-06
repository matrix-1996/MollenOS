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
* MollenOS Window - Sapphire
*/

#ifndef _SAPPHIRE_WINDOW_H_
#define _SAPPHIRE_WINDOW_H_

/* Includes */
#include <crtdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <os/Ipc.h>

/* Ui Includes */
#include <SDL.h>

/* Definitions */
#define WINDOW_CONSOLE	0x1
#define WINDOW_INHERIT	0x2

/* Structures */
typedef struct _sWindow {

	/* Window Information
	 * Id, owner, flags */
	int Id;
	IpcComm_t Owner;
	int Flags;

	/* Window Z-Index */
	int zIndex;

	/* Window Dimensions */
	Rect_t FullDimensions;
	Rect_t ContentDimensions;

	/* Specials */
	double Rotation;
	SDL_RendererFlip Flip;

	/* User-Backbuffer */
	void *Backbuffer;
	void *BackbufferHandle;
	size_t BackbufferSize;
	
	/* The render surface */
	SDL_Renderer *Renderer;
	SDL_Texture *Texture;
	void *Terminal;

} Window_t;


/* Prototypes */

/* Constructor 
 * Allocates a new window of the given
 * dimensions and initializes it */
__CRT_EXTERN Window_t *WindowCreate(IpcComm_t Owner, Rect_t *Dimensions, int Flags, SDL_Renderer *Renderer);

/* Destructor
 * Cleans up and releases 
 * resources allocated */
__CRT_EXTERN void WindowDestroy(Window_t *Window);

/* Update
 * Updates all neccessary state and 
 * buffers before rendering */
__CRT_EXTERN void WindowUpdate(Window_t *Window, Rect_t *DirtyArea);

/* Render
 * Renders the window to the 
 * given renderer */
__CRT_EXTERN void WindowRender(Window_t *Window, SDL_Renderer *Renderer, Rect_t *DirtyArea);

#endif //!_SAPPHIRE_WINDOW_H_