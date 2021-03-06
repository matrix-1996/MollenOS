; MollenOS
;
; Copyright 2011-2014, Philip Meulengracht
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation?, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
;
; MollenOS x86-32 Descriptor Assembly Functions
;
bits 32
segment .text

;Functions in this asm
global _init_fpu
global _save_fpu
global _load_fpu
global _clear_ts
global _set_ts
global _rdtsc
global __yield
global _enter_thread
global _enter_signal

; void _yield(void)
; Yields
__yield:
	; call int 0x81
	int 0x81

	; Return
	ret 

; void save_fpu(uintptr_t *buffer)
; Save MMX and MMX registers
_save_fpu:
	; Stack Frame
	push ebp
	mov ebp, esp

	; Save EAX
	push eax

	; Save FPU to argument 1
	mov eax, [ebp + 8]
	fxsave [eax]

	; Restore EAX
	pop eax

	; Release stack frame
	pop ebp
	ret 

; void load_fpu(uintptr_t *buffer)
; Load MMX and MMX registers
_load_fpu:
	; Stack Frame
	push ebp
	mov ebp, esp

	; Save EAX
	push eax

	; Save FPU to argument 1
	mov eax, [ebp + 8]
	fxrstor [eax]

	; Restore EAX
	pop eax

	; Release stack frame
	pop ebp
	ret 

; void set_ts()
; Sets the Task-Switch register
_set_ts:
	; Save eax
	push eax

	; Set TS
	mov eax, cr0
	bts eax, 3
	mov cr0, eax

	; Restore
	pop eax
	ret 

; void clear_ts()
; Clears the Task-Switch register
_clear_ts:
	; clear
	clts

	; Return
	ret 

; void init_fpu()
; Initializes the FPU
_init_fpu:
	; fpu init
	finit

	; Return
	ret 

; void rdtsc(uint64_t *value)
; Gets the CPU time-stamp counter
_rdtsc:
	; Stack Frame
	push ebp
	mov ebp, esp

	; Save stuff
	push eax
	push ebx

	; Get pointer
	mov ebx, [ebp + 8]
	rdtsc
	mov [ebx], eax
	mov [ebx + 4], edx

	; Restore
	pop ebx
	pop eax

	; Release stack frame
	pop ebp
	ret 

; void enter_thread(registers_t *stack)
; Switches stack and far jumps to next task
_enter_thread:

	; Get pointer
	mov eax, [esp + 4]
	mov esp, eax

	; When we return, restore state
	popad

	; Restore segments
	pop gs
	pop fs
	pop es
	pop ds

	; Cleanup IrqNum & IrqErrorCode from stack
	add esp, 0x8

	; Return
	iret

; void enter_signal(registers_t *stack, uintptr_t handler, int sig)
; Switches stack and far jumps to next task
_enter_signal:

	; Get pointer
	mov eax, [esp + 4]
	mov ebx, [esp + 8]
	mov ecx, [esp + 12]
	mov edx, [esp + 16]
	mov esp, eax

	; Push signal information
	push ecx ; push sig
	push edx ; push return-addr

	; Set user segments
	mov ax, 0x23
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; Get stack
	mov eax, esp
	push 0x00000023
	push eax

	; Fix eflags, we need ints
	pushf
	pop eax
	or eax, 0x200
	push eax

	; push code-selector 
	push 0x0000001B

	; Push handler
	push ebx

	; Return
	iret