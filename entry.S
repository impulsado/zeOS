/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include <errno.h>

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

	# 1. Guardar contexte SW
	SAVE_ALL
	
	# 2. Habilitar interrupcions pq ja les estem tractant
	EOI

	call clock_routine

	# 3. Recuperar contexte sw
	RESTORE_ALL
		
	# 4. Saltar de system mode a user mode
	iret

ENTRY(keyboard_handler)
	SAVE_ALL
	call keyboard_routine
	EOI
	RESTORE_ALL
	iret

ENTRY(page_fault_handler_new)
	call page_fault_routine_new
	iret;  # Tot i que aixo no passara

ENTRY(writeMSR)
	# MSR[ecx] <-- edx:eax (Com que estem en 32b --> edx = 0)
	push %ebp
	movl %esp, %ebp

	movl 8(%ebp), %ecx;  # ecx <-- param1 = number of the MSR
	movl 12(%ebp), %eax;  # eax <-- param2 = value of MSR
	movl $0, %edx

	wrmsr

	popl %ebp
	ret

ENTRY(system_call_handler)
	# 1. Guardar ctx hw
	/*

|	SS	| <- esp
|ebp_user	| <- ebp
|	@ret	|
|user_data 	|
| ...		|
	*/

	push $__USER_DS;
	push %ebp;  # Tot i que volem guardar esp, aquest ha augmentat amb el push previ. Pero recordem que ebp i esp, al fer dynamic-link --> apunten a la mateixa pos. inicialment
	pushfl;  # Guardar PSW
	push $__USER_CS
	push 4(%ebp);  # La proxima inst a executar es el "@ret" que s'ha guardat automaticament quan hem saltat al handler. Equival a la del wrapper

	# 2. Guardar ctx sw
	SAVE_ALL	
	
	# 3. Comprovar entrada (eax)
	cmpl $0, %eax;  # if (eax < 0) jump 
	jl invalid_eax

	cmpl $MAX_SYSCALL, %eax;  # if (eax > MAX_SYSCALL) jump
	jg invalid_eax

	# 4. Saltar a la rutina 
	call *sys_call_table(,%eax,0x04);  # '*' --> Saltar a la @ que hi ha dins. Fer valor del punter.
	jmp sysenter_fin
	
invalid_eax:
	movl $-ENOSYS, %eax;
sysenter_fin:
	# 5. Guardar el valor de retorn al ctx sw
	# NOTA: No ho podem fer despres pq. sino perderem l'actual eax fent "pop eax" (RESTORE_ALL)
	# NOTA: Dibuixar pila per saber la pos.
	movl %eax, 24(%esp)

	# 6. Restaurem ctx sw
	RESTORE_ALL

	# 7. Restaurem ctx hw
	

	# 8. Assignar valors a reg. per a sysexit
	# ecx <-- @ user_esp
	# edx <-- @ user_ret
	movl 12(%esp), %ecx
	movl (%esp), %edx

	# 9. Saltar user mod
	# NOTA: No fem "iret" pq no hem entrat amb "int"
	sysexit
