# 0 "sys_call_table.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "sys_call_table.S"
# 1 "include/asm.h" 1
# 2 "sys_call_table.S" 2
# 1 "include/segment.h" 1
# 3 "sys_call_table.S" 2

.globl sys_call_table; .type sys_call_table, @function; .align 0; sys_call_table:
 .long sys_ni_syscall #0
 .long sys_ni_syscall #1
 .long sys_ni_syscall #2
 .long sys_ni_syscall #3
 .long sys_write #4
 .long sys_ni_syscall #5
 .long sys_ni_syscall #6
 .long sys_ni_syscall #7
 .long sys_ni_syscall #8
 .long sys_ni_syscall #9
 .long sys_gettime #10

.globl MAX_SYSCALL
MAX_SYSCALL = (. - sys_call_table)/4
