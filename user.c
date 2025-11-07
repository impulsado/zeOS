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

	/*
			F
		C1		C2
	C11
	*/
	if (pid == 0)  // C1
	{
		pid = fork();

		if (pid == 0)  // C11
		{
			while (1)
			{
				write(1, "C11\n", 4);
			}
		}
		else  // C1
		{
			for (int i = 0; i < 50; i++)
			{
				write(1, "C1\n", 3);
			}

			exit();  // C11 deuria de recollirlo F i continuarse executant
		}
	}
	else  // F
	{
		pid = fork();
		if (pid == 0)  // C2
		{
			while (1)
			{
				write(1, "C2\n", 3);
			}
		}
		else  // F
		{
			while (1)
			{
				write(1, "F\n", 2);
			}
		}
	}

  	while (1);
}
