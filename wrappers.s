# 0 "wrappers.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "wrappers.S"
# 1 "include/asm.h" 1
# 2 "wrappers.S" 2

.globl write; .type write, @function; .align 0; write:
 # 1. Crear dynamic link per poder rertornar a la funcio "write(...)" que ha cridat usuari
 push %ebp
 movl %esp, %ebp

 # 2. Guardar ecx, edx donat que se que els modificara sysexit perque pugui retornar al handler
 push %ebx; # IMPO: Aquest tambe l'hem de guardar pq. tambe el modifiquem.
 push %ecx
 push %edx

 # 3. Set-up params.
 # NOTA: Venim d'una altra funcio que ha deixat els param en la pila.
 # Aixi que dibuixem la pila i veiem el offset
# 25 "wrappers.S"
 movl 8(%ebp), %edx; # ebx <-- fd
 movl 12(%ebp), %ecx; # ecx <-- buffer
 movl 16(%ebp), %ebx; # edx <-- size

 # 4. Ficar valor eax del write (0x04)
 movl $0x04, %eax

 # 5. Crear fake dynamic-link (Pq. a l'hora de fer sysexit pugui retornar al wrapper)
 # NOTA: Ho fem aqui perque el handler no ho fara
 push $write_post_sysenter; # per a sysexit edx (return address)
 push %ebp; # per a sysexit ecx (user stack pointer)
 movl %esp, %ebp; # Forçar que ebp apunti a esp (pq no ho fara el handler)

 # 6. Crida a sistema
 sysenter

write_post_sysenter:
 # 7. Recuperar registres de la pila
 popl %ebp
 addl $4, %esp # Treure "@write_post_sysenter"
 popl %edx
 popl %ecx
 popl %ebx

 # 8. Processem el resultat
 cmpl $0, %eax; # if (*eax < 0) errno
 jge write_no_error

 negl %eax; # obtenim el valor que aniraa a errno (valor absolut)
 movl %eax, errno; # errno esta en libc.c
 movl $-1, %eax; # al programa usuari li indiquem que ha anat malament--> -1
write_no_error:
 popl %ebp
 ret # Retornar a user code
