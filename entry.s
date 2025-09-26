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
# 70 "entry.S"
.globl clock_handler; .type clock_handler, @function; .align 0; clock_handler:





 pushl %gs; pushl %fs; pushl %es; pushl %ds; pushl %eax; pushl %ebp; pushl %edi; pushl %esi; pushl %ebx; pushl %ecx; pushl %edx; movl $0x18, %edx; movl %edx, %ds; movl %edx, %es


 movb $0x20, %al; outb %al, $0x20

 call clock_routine


 pop %edx; pop %ecx; pop %ebx; pop %esi; pop %edi; pop %ebp; pop %eax; pop %ds; pop %es; pop %fs; pop %gs


 iret
