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
	ret = sys_write_console(buffer, size);
	if (ret < 0)
		return -1;  // NO se quin

	return 0;

}
