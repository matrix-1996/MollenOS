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
* MollenOS Scene - Sapphire
*/

#ifndef _SAPPHIRE_SCENE_H_
#define _SAPPHIRE_SCENE_H_

/* Includes */
#include <crtdefs.h>
#include <stddef.h>
#include <stdint.h>

/* Ui Includes */
#include <SDL.h>

/* List -> Windows */
#include <os/MollenOS.h>
#include "../Common/List.h"
#include "Window.h"

/* Definitions */


/* Structures */
typedef struct _sScene {

	/* Scene Id */
	int Id;

	/* Scene dimensions */
	Rect_t Dimensions;

	/* Background */
	SDL_Texture *Background;

	/* Backbuffer */
	SDL_Texture *Texture;

	/* List of windows */
	list_t *Windows;

} Scene_t;


/* Prototypes */

/* Initializor
 * Creates a new scene */
EXTERN Scene_t *SceneCreate(int Id, Rect_t *Dimensions, SDL_Renderer *Renderer);

/* Destructor
 * Cleans up all windows
 * and releases resources allocated */
EXTERN void SceneDestroy(Scene_t *Scene);

/* Add Window
 * Adds a newly created window to the
 * given scene. The window is not immediately
 * rendered before a call to Render */
EXTERN void SceneAddWindow(Scene_t *Scene, Window_t *Window);

/* Update
 * This updates any changes to windows
 * for this scene */
EXTERN void SceneUpdate(Scene_t *Scene);

/* Render
 * This renders all windows for this scene */
EXTERN void SceneRender(Scene_t *Scene, SDL_Renderer *Renderer);

#endif //!_SAPPHIRE_SCENE_H_