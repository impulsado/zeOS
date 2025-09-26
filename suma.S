#include <asm.h>

ENTRY(addASM)	
	# Dynamic link
	push %ebp
	mov %esp, %ebp	# %esp <-- %ebp
	
	# Create Local Variables 
	nop

	# Operate
	mov 8(%ebp), %eax  	# %eax <-- par2
	mov 12(%ebp), %edx	# %edx <-- par1
	add %edx, %eax		# %eax <-- %eax + %edx	
	
	# Restore previous %ebp
	pop %ebp
	
	# Return
	ret
