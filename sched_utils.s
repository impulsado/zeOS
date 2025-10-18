# 0 "sched_utils.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "sched_utils.S"
# 1 "include/asm.h" 1
# 2 "sched_utils.S" 2
# 44 "sched_utils.S"
.globl end_task_switch; .type end_task_switch, @function; .align 0; end_task_switch:
 # Dynamic link
 push %ebp # Guardar ebp de inner_task_switch (justament el que volem guardar en kernel_esp)
 movl %esp, %ebp

 # === Guardar current()->kernel_esp = %ebp
 # Guardar reg. que farem servir
 push %ebx
 push %edx

 # Obtindre el valor de ebp de inner_task_switch
 movl (%ebp), %ebx

 # Guardar-lo en la struct de current
 movl 8(%ebp), %edx
 movl %ebx, (%edx)

 # Restaurar el reg.
 pop %edx
 pop %ebx

 # === Actualitzar %esp = new_pcb->kernel_esp
 # Assignar el nou valor de esp
 movl 12(%ebp), %esp

 # ! IMPORTANT !
 # A PARTIR D'AQUEST PUNT JA ES EL NOU "union task_union"

 # Retornar al inner_task_switch del nou process
 pop %ebp
 ret
