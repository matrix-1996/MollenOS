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
* MollenOS Common - Ring Buffer Implementation
*/

#ifndef _MCORE_RINGBUFFER_H_
#define _MCORE_RINGBUFFER_H_

/* Includes */
#include <arch.h>
#include <crtdefs.h>
#include <stdint.h>

/* Structure */
typedef struct _RingBuffer
{
	/* The buffer */
	uint8_t *Buffer;

	/* Length of buffer */
	size_t Length;

	/* Index */
	uint32_t IndexWrite;
	uint32_t IndexRead;

	/* Lock */
	Spinlock_t Lock;

} RingBuffer_t;

/* Prototypes */

/* Initialise a new ring buffer */
_CRT_EXTERN RingBuffer_t *RingBufferCreate(size_t Size);

/* Destroy Ringbuffer */
_CRT_EXTERN void RingBufferDestroy(RingBuffer_t *RingBuffer);

/* Write to buffer */
_CRT_EXTERN int RingBufferWrite(RingBuffer_t *RingBuffer, size_t SrcLength, uint8_t *Source);

/* Read from buffer */
_CRT_EXTERN int RingBufferRead(RingBuffer_t *RingBuffer, size_t DestLength, uint8_t *Destination);

/* How many bytes are available in buffer to be read */
_CRT_EXTERN size_t RingBufferSize(RingBuffer_t *RingBuffer);

/* How many bytes are ready for usage */
_CRT_EXTERN int RingBufferSpaceAvailable(RingBuffer_t *RingBuffer);

#endif // !_MCORE_RINGBUFFER_H_