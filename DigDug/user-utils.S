#include <asm.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $SYSENTER_RETURN
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(SYSENTER_RETURN)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

/* int write(int fd, char *buffer, int size) */
ENTRY(write)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $4, %eax
	movl 0x8(%ebp), %ebx;	//fd
	movl 0xC(%ebp), %ecx;	//buffer
	movl 0x10(%ebp), %edx;	//size
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

/* Common code for negative return */
nok:
	neg %eax
	pushl %eax
	call __errno_location
	movl %eax, %ecx;  // &(tls->errno)
	popl %eax
	movl %eax, (%ecx); // *(tls->errno) = error_ret_value
	mov $-1, %eax
	popl %ebp
	ret

/* int gettime() */
ENTRY(gettime)
	pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int WaitForTick() */
ENTRY(WaitForTick)
	pushl %ebp
	movl %esp, %ebp
	movl $11, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int getpid() */
ENTRY(getpid)
	pushl %ebp
	movl %esp, %ebp
	movl $20, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	movl $2, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* void exit() */
ENTRY(exit)
	pushl %ebp
	movl %esp, %ebp
	movl $1, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int yield() */
ENTRY(yield)
	pushl %ebp
	movl %esp, %ebp
	movl $13, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int get_stats(int pid, struct stats *st) */
ENTRY(get_stats)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $35, %eax
	movl 0x8(%ebp), %ebx;	//pid
	movl 0xC(%ebp), %ecx;	//st
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok;	// if (eax < 0) -->
	popl %ebp
	ret

/* int ThreadCreate(void (*function)(void* arg), void* parameter) */
ENTRY(ThreadCreate)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $5, %eax
	movl 8(%ebp), %ebx; // function
	movl 12(%ebp), %ecx;  // parameter
	movl $ThreadWrapper, %edx;  // ThreadWrapper
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok;	// if (eax < 0) -->
	popl %ebp
	ret

/* void ThreadExit(void) */
ENTRY(ThreadExit)
	pushl %ebp
	movl %esp, %ebp
	movl $6, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int KeyboardEvent(void (*func)(char key, int pressed)) */
/* NOTA: Internament passa KeyboardWrapper com a segon parametre */
ENTRY(KeyboardEvent)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX
	movl $36, %eax
	movl 8(%ebp), %ebx;  // func
	movl $KeyboardWrapper, %ecx;  // wrapper (igual que thread)
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok;	// if (eax < 0) -->
	popl %ebp
	ret

/* void KeyboardWrapper(void (*func)(char, int), char key, int pressed) */
/* NOTA: Necessari per poder cridar, despres d'executar funcio usuari, "int 0x2b" */
/* IMPO: Aqui guardem els parametres cap adalt perque sino quan fem el call, aquest afegeix la @ret i aixo fa que hi hagi un +8 per al offset del primer param */
/* IMPO: func es un punter a una funcio, llavors no serveix en accedir a la mem */
ENTRY(KeyboardWrapper)
	pushl %ebp
	movl %esp, %ebp
	
	/* Preparar arguments per cridar func(key, pressed) */
	/*
		|	 ebp_old	| <- ebp
		| 		0		|
		|	  func		|
		|	  key		|
		|	pressed		|
		|---------------| <- @Fin de la TLS
	*/
	pushl 16(%ebp);  // pressed
	pushl 12(%ebp);  // key
	movl 8(%ebp), %eax;  // func

	call %eax;
	addl $8, %esp
	
	/* Tornar al context original */
	int $0x2b
	
	/* No arribem aqui, pero per seguretat */
	popl %ebp
	ret