#include <asm.h>

ENTRY(write)
	# 1. Crear dynamic link per poder rertornar a la funcio "write(...)" que ha cridat usuari
	push %ebp
	movl %esp, %ebp
	
	# 2. Guardar ecx, edx donat que se que els modificara sysexit perque pugui retornar al handler
	push %ebx;  # IMPO: Aquest tambe l'hem de guardar pq. tambe el modifiquem.
	push %ecx
	push %edx

	# 3. Set-up params.
	# NOTA: Venim d'una altra funcio que ha deixat els param en la pila.
	# Aixi que dibuixem la pila i veiem el offset
	/*
		PILA USUARI
		-----------

		| 	edx				| <-- %esp
		| 	ecx				|	
		| 	ebx				|
		| 	ebp_user_prog	| <-- %ebp
		| 	@ret_user_prog	| +4
		| 	param1			| +8
		| 	param2			| +12
		|		 ...		|
		|...................|
	*/
	# IMPO: Recorda que en el SW. CTX. es EDX > ECX > EBX --> param1 ha d'anar a edx
	movl 8(%ebp), %edx;  # edx <-- fd
	movl 12(%ebp), %ecx;  # ecx <-- buffer
	movl 16(%ebp), %ebx;  # ebx <-- size

	# 4. Ficar valor eax del write (0x04)
	movl $0x04, %eax

	# 5. Crear fake dynamic-link (Pq. a l'hora de fer sysexit pugui retornar al wrapper)
	# NOTA: Ho fem aqui perque el handler no ho fara
	# OBS: com que hem fet un push del %ebp, al retornar el recuperem amb un pop i no perdem el seu valor.
	push $write_post_sysenter; # per a sysexit edx (return address)
	push %ebp;  # per a sysexit ecx  (user stack pointer)
	movl %esp, %ebp;  # Forçar que ebp apunti a esp (pq no ho fara el handler)


	# 6. Crida a sistema
	sysenter

write_post_sysenter:
	# 7. Recuperar registres de la pila
	popl %ebp
	addl $4, %esp  # Treure "@write_post_sysenter"
	popl %edx
	popl %ecx
	popl %ebx

	# 8. Processem el resultat
	cmpl $0, %eax; # if (*eax < 0) errno
	jge write_no_error
	
	negl %eax;  # obtenim el valor que aniraa a errno (valor absolut)
	movl %eax, errno;  # errno esta en libc.c 
	movl $-1, %eax;  # al programa usuari li indiquem que ha anat malament--> -1
write_no_error:
	popl %ebp
	ret  # Retornar a user code 


ENTRY(gettime)
	# Dynamic Link
	push %ebp
	movl %esp, %ebp

	# Save ECX, EDX
	push %edx
	push %ecx
	
	# Set params.
	# No params needed

	# Set EAX value
	movl $10, %eax
	
	# Create fake dynamic-link for the sysenter handler
	push $getTime_post_sysenter
	push %ebp
	movl %esp, %ebp
	
	# Make syscall
	sysenter

getTime_post_sysenter:
	# Clear the stack
	pop %ebp;  # restore ebp_wrapper
	addl $4, %esp;  # Remove @getTime_post_sysenter
	pop %ecx
	pop %edx

	# Process the result
	cmpl $0, %eax; # if (eax < 0) error
	jge getTime_no_error

	# It has an error
	negl %eax
	movl %eax, errno
	movl $-1, %eax

getTime_no_error:
	pop %ebp;  # restore ebp_user_prog
	ret;  # goto usr_prog
	

ENTRY(getpid)
	# Dynamic link
	push %ebp
	movl %esp, %ebp

	# Guardar ebx, edx (necessaris/modificats per sysexit)
	push %ebx
	push %edx

	# Ficar el numero %eax corresponent al getpid_routine
	movl $20, %eax

	# Fake Dynamic-link
	push $getpid_post_sysenter
	push %ebp
	movl %esp, %ebp

	# Entrar a mode sistema
	sysenter

getpid_post_sysenter:
	pop %ebp  # restaurar el ebp del wrapper
	addl $4, %esp
	pop %edx
	pop %ecx

	cmpl $0, %eax  # if (eax < 0) error
	jge getpid_no_error

	negl %eax
	movl %eax, errno
	movl $-1, %eax

getpid_no_error:
	pop %ebp
	ret


ENTRY(fork)
	# Dynamic Link
	push %ebp
	movl %esp, %ebp

	# Save ECX, EDX
	push %ecx
	push %edx

	# Select routine entry
	movl $2, %eax

	# Fake Dynamic Link
	push $fork_post_sysenter
	push %ebp
	movl %esp, %ebp

	# Entrar mode sistema
	sysenter

fork_post_sysenter:
	# Restaurar pila
	pop %ebp
	addl $4, %esp
	pop %edx
	pop %ecx

	# Comprovar return code
	cmpl $0, %eax  # if (eax < 0) error
	jge fork_no_error

	# Assignar return code
	negl %eax
	movl %eax, errno
	movl $-1, %eax

fork_no_error:
	
	# Return
	pop %ebp
	ret


ENTRY(exit)
	# Dynamic link
	push %ebp
	movl %esp, %ebp

	# Save ECX, EDX
	push %ecx
	push %edx

	# Assign syscall code
	mov $1, %eax

	# Crear fake-dynamic link
	push $exit_post_sysenter
	push %ebp
	movl %esp, %ebp

	# Saltar
	sysenter

exit_post_sysenter:
	# Restaurar pila
	pop %ebp
	addl $4, %esp
	pop %edx
	pop %ecx

	# Comprovar codi error
	cmpl $0, %eax  # if (eax < 0) errno
	jge exit_no_error

	# Assignar return code
	negl %eax
	movl %eax, errno
	movl $-1, %eax

exit_no_error:
	pop %ebp
	ret