#include <asm.h>

/*
	TEORIA
	------
		current process stack
	-----------------------------
	|							|
	|	%ebp_inner_task_switch	| <-- ebp, esp
	|	@ret_inner_task_switch	| +4
	| 	&current()->kernel_esp	| +8
	|	new_pcb->kernel_esp		| +12
	|			...				|
	-----------------------------

		new process stack
	-----------------------------
	|							|
	|	%ebp_inner_task_switch	|
	|	@ret_inner_task_switch	|
	|			...				|
	-----------------------------

	Volem acabar de fer el context_switch i per aixo haurem de moure %esp.
	El problema es que al fer-ho, ja apuntarem al nou union.
	Aixo implica que les seguents instruccions seran executades pel nou process.
	Per sort, podem aprofitar el fet de que sabem que el nou process SEMPRE tindra en el top del seu stack:
		ebp ; ret ; ...
	
	OBS: 
	Aquest canvi no afecta als REG. que hem guardat en task_switch.
	Sabem que si el nou process que ha passat pel scheduler (en algun moment salta task_switch)
		--> tambe els haura guardat --> els tindra en la stack --> els restaura sense problema 


	NOTA: 
	Podriem haver fet 2 funcions
		- Actualitzar el current kernel_esp
		- Canviar el esp

*/


ENTRY(end_task_switch)
	# Dynamic link
	push %ebp  # Guardar ebp de inner_task_switch (justament el que volem guardar en kernel_esp)
	movl %esp, %ebp

	# === Guardar current()->kernel_esp = %ebp
	# Guardar reg. que farem servir
	push %ebx

	# Obtindre el valor de ebp de inner_task_switch
	movl (%ebp), %ebx

	# Guardar-lo en la struct de current
	movl %ebx, 8(%ebp)

	# Restaurar el reg.
	pop %ebx

	# === Actualitzar %esp = new_pcb->kernel_esp
	# Assignar el nou valor de esp 
	movl 12(%ebp), %esp

	# ! IMPORTANT !
	# A PARTIR D'AQUEST PUNT JA ES EL NOU "union task_union"

	# Retornar al inner_task_switch del nou process 
	pop %ebp
	ret
