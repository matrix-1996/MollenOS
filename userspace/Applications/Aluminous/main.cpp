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
* MollenOS Terminal Implementation
* - Project Alumnious (First C++ Project)
*/

/* Alumnious Includes */
#include "Terminal/Terminal.h"

/* Entry Point */
int main(int argc, char* argv[])
{
	/* Start out by initializing console-env */
	Terminal *Term = new Terminal();

	/* Setup console stuff */
	Term->SetSize(80, 25);
	Term->SetFont("%Sys%/Fonts/DejaVuSansMono.ttf", 12);
	Term->SetTextColor(255, 255, 255, 255);
	Term->SetBackgroundColor(0, 0, 0, 255);

	/* Print header? maybe? */
	Term->PrintLine("MollenOS System Terminal %s\n", "V0.01-dev");

	/* Enter while loop */
	while (Term->IsAlive()) {
		Term->NewCommand();
	}

	/* Ok, exit time, cleanup */
	delete Term;

	/* Exit normally */
	return 0;
}