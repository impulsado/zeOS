/*
 * libc.c 
 */

#include <libc.h>
#include <errno.h>
#include <types.h>

int errno;

/*
 * Show scren message with the errno value
 * stdin = 0
 * stdout = 1
 * stderr = 2
 */
void perror(void)
{
	// Base Case
	if (errno == 0)
	{
		write(1, "errno is 0 | GLHF", 17);
		return;
	}	

	// General Case
	char ret_code[4];  // It will not be bigger than 2000
	const char* message;
	const char* general = "errno value: ";

	// Get the string format of the errno 
	itoa(errno, ret_code);

	write(1, (char*)general, strlen((char*)general));
	write(1, ret_code, strlen(ret_code));	

	if (errno == 9) message = "Bad file number";
	else if (errno == 13) message = "Permission denied";
	else if (errno == 14) message = "Bad address";
	else if (errno == 22) message = "Invalid argument";
	else if (errno == 88) message = "Function not implemented";
	else message = "message not implemented";

	write(1, (char*)message, strlen((char*)message));
}

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}

