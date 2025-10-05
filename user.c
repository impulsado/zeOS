#include <libc.h>
#include <types.h>

char buff[24];

int pid;

int __attribute__ ((__section__(".text.main"))) main(void)
{
    	/* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     	/* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

	int ticks = gettime();
	if (ticks >= 0)
	{
		char buf[4];
		itoa(ticks, buf);
		write(1, "Temps que ha passat: ", 21);
		write(1, buf, strlen(buf));
	}


	char *p = 0;
	*p = 'x';

  	while (1);
}
