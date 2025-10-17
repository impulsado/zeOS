#include <libc.h>
#include <types.h>


char buff[24];

int pid;

int __attribute__ ((__section__(".text.main"))) main(void)
{
    	/* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     	/* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

	int pid = getpid();
	char buf[4];
	itoa(pid, buf);

	write(1, "PID de user_main: ", 18);
	write(1, buf, 4);

  	while (1);
}
