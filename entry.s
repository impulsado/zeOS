# 0 "entry.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "entry.S"




# 1 "include/asm.h" 1
# 6 "entry.S" 2
# 1 "include/segment.h" 1
# 7 "entry.S" 2
# 1 "include/errno.h" 1
# 8 "entry.S" 2
# 71 "entry.S"
.globl clock_handler; .type clock_handler, @function; .align 0; clock_handler:




 # 1. Guardar contexte SW
 pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es

 # 2. Habilitar interrupcions pq ja les estem tractant
 movb $0x20, %al; outb %al, $0x20

 call clock_routine

 # 3. Recuperar contexte sw
 pop %edx; pop %ecx; pop %ebx; pop %esi; pop %edi; pop %ebp; pop %eax; pop %ds; pop %es; pop %fs; pop %gs

 # 4. Saltar de system mode a user mode
 iret

.globl keyboard_handler; .type keyboard_handler, @function; .align 0; keyboard_handler:
 pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es
 call keyboard_routine
 movb $0x20, %al; outb %al, $0x20
 pop %edx; pop %ecx; pop %ebx; pop %esi; pop %edi; pop %ebp; pop %eax; pop %ds; pop %es; pop %fs; pop %gs
 iret

.globl page_fault_handler_new; .type page_fault_handler_new, @function; .align 0; page_fault_handler_new:
 call page_fault_routine_new
 iret; # Tot i que aixo no passara

.globl writeMSR; .type writeMSR, @function; .align 0; writeMSR:
 # MSR[ecx] <-- edx:eax (Com que estem en 32b --> edx = 0)
 push %ebp
 movl %esp, %ebp

 movl 8(%ebp), %ecx; # ecx <-- param1 = number of the MSR
 movl 12(%ebp), %eax; # eax <-- param2 = value of MSR
 movl $0, %edx

 wrmsr

 popl %ebp
 ret

.globl system_call_handler; .type system_call_handler, @function; .align 0; system_call_handler:
 # 1. Guardar ctx hw
# 145 "entry.S"
 push $0x2B;
 # OBS: Toca guardar esp, pero aquest ja apunta a pila de sistema (i volem la de l'usuari)
 # Amb el canvi que hem fet (desde sysenter), %ebp no s'ha modificat i continua apuntant a la pila de l'usuari.
 # Aprofitem que com que hem fet "fake dynamic-link" en el wrapper abans de saltar, %ebp apunta al top de la pila d'usuari (equival al %esp que volem guardar)
 push %ebp;
 pushfl; # Guardar PSW
 push $0x23
 push 4(%ebp); # La proxima inst a executar es el "@ret" que s'ha guardat automaticament quan hem saltat al handler. Equival a la del wrapper

 # 2. Guardar ctx sw
 pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es

 # 3. Comprovar entrada (eax)
 cmpl $0, %eax; # if (eax < 0) jump
 jl invalid_eax

 cmpl $MAX_SYSCALL, %eax; # if (eax > MAX_SYSCALL) jump
 jg invalid_eax

 # 4. Saltar a la rutina
 call *sys_call_table(,%eax,0x04); # '*' --> Saltar a la @ que hi ha dins. Fer valor del punter.
 jmp sysenter_fin

invalid_eax:
 movl $-88, %eax;
sysenter_fin:
 # 5. Guardar el valor de retorn al ctx sw
 # NOTA: No ho podem fer despres pq. sino perderem l'actual eax fent "pop eax" (RESTORE_ALL)
 # NOTA: Dibuixar pila per saber la pos.
 movl %eax, 24(%esp)

 # 6. Restaurem ctx sw
 pop %edx; pop %ecx; pop %ebx; pop %esi; pop %edi; pop %ebp; pop %eax; pop %ds; pop %es; pop %fs; pop %gs

 # 7. Restaurem ctx hw
 # OBS: Aqui queda tota la informacio del Ctx. HW, pero no importa.
 # El que passara es que com que MSR[0x175] apunta SEMPRE a la base de la pila de sistema, el proxim salt sobre escriurem el ctx. hw anterior

 # 8. Assignar valors a reg. per a sysexit
 # ecx <-- @ user_esp
 # edx <-- @ user_ret
 movl 12(%esp), %ecx
 movl (%esp), %edx

 # 9. Habilitar interrupcions
 sti; # NO OBLIDAR

 # 10. Saltar user mod
 # NOTA: No fem "iret" pq no hem entrat amb "int"
 sysexit
