# 0 "suma.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "suma.S"
# 1 "include/asm.h" 1
# 2 "suma.S" 2

.globl addASM; .type addASM, @function; .align 0; addASM:
 # Dynamic link
 push %ebp
 mov %esp, %ebp # %esp <-- %ebp

 # Create Local Variables
 nop

 # Operate
 mov 8(%ebp), %eax # %eax <-- par2
 mov 12(%ebp), %edx # %edx <-- par1
 add %edx, %eax # %eax <-- %eax + %edx

 # Restore previous %ebp
 pop %ebp

 # Return
 ret
