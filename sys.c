/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

// 
extern unsigned int zeos_ticks;

// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -EACCES; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -ENOSYS; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
}

int sys_write(int fd, char *buffer, int size)
{
	// NOTA: Revisar "man 2 write" per saber els ERRNO
	int ret;

	// Base Case
	ret = check_fd(fd, ESCRIPTURA);
	if (ret != 0)
		return ret;

	if (buffer == 0)  // No tenim null
		return -EFAULT;

	if (size < 0)
		return -EINVAL;  

	// General case
	const int CHUNK = 256;  // valor trivial
	int total = 0;
	char k_buffer[CHUNK];
	int remaining;
	int temp_size;

	while (total < size)
	{
		remaining = size - total;
		temp_size = (size > remaining) ? CHUNK : remaining;
		
		ret = copy_from_user(buffer + total, k_buffer, temp_size);
		if (ret != 0)
			return -EFAULT;

		ret = sys_write_console(k_buffer, temp_size);
		// sempre retorna temp_size

		total += temp_size;
	}
	

	return total;
}

int sys_gettime(void)
{
	return zeos_ticks;
}