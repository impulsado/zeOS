/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>
#include <mm_address.h>
#include <tls.h>

struct tls_block *get_tls_block(void)
{
  // 1. Saber el "esp" actual
  volatile unsigned int temp = 0;  // IMPO: No vull que el compilador optimitzi o faci servir un reg, vull que guardi espai en stack
  unsigned int sp = (unsigned int)&temp;
  
  // 2. Saber en quin slot esta el thread
  unsigned int page = sp >> 12;
  if (page < THREAD_STACK_REGION_FIRST_PAGE) return 0;
  unsigned int relative = page - THREAD_STACK_REGION_FIRST_PAGE;
  unsigned int slot = relative / THREAD_STACK_SLOT_PAGES;

  // 3. Retornar la direccio
  unsigned int tls_addr = THREAD_TLS_VADDR(slot);
  return (struct tls_block *)tls_addr;
}

int *__errno_location(void)
{
  return &get_tls_block()->errno_value;
}

int REGS[7]; // Space to save REGISTERS

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

void perror()
{
  char buffer[256];

  itoa(errno, buffer);

  write(1, buffer, strlen(buffer));
}

void ThreadWrapper(void (*function)(void* arg), void* parameter)
{
  function(parameter);
  ThreadExit();
}