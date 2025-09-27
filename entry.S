/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>

/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %edx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %ebx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %ebx; \
      pushl %ecx; \
      pushl %edx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define EOI \
	movb $0x20, %al; \
	outb %al, $0x20

#define RESTORE_ALL \
	pop %edx; \
	pop %ecx; \
	pop %ebx; \
	pop %esi; \
	pop %edi; \
	pop %ebp; \
	pop %eax; \
	pop %ds; \
	pop %es; \
	pop %fs; \
	pop %gs

ENTRY(clock_handler)
	/*
	Aqui el HW ja ha guardat els REG automaticament del cntx HW 
	*/

	// 1. Guardar contexte SW
	SAVE_ALL
	
	// 2. Habilitar interrupcions pq ja les estem tractant
	EOI

	call clock_routine

	// 3. Recuperar contexte sw
	RESTORE_ALL
		
	// 4. Saltar de system mode a user mode
	iret

ENTRY(keyboard_handler)
	SAVE_ALL
	call keyboard_routine
	EOI
	RESTORE_ALL
	iret
