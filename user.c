#include <libc.h>
#include <types.h>


char buff[24];

int pid;

int __attribute__ ((__section__(".text.main"))) main(void)
{
    	/* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     	/* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

	char buf[4];

	int pid = fork();
	itoa(pid, buf);

	if (pid == 0)
	{
		while (1)
		{
			write(1, "Soc fill\n", 9);
		}
	}
	else
	{
		while (1)
		{
			write(1, "Soc pare\n", 10);
		}
	}

  	while (1);
}
