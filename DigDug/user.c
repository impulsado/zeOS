#include <libc.h>
#include <tls.h>
#include <types.h>

// NOTA: Ho faig variable (i no "define") per demo que tambe puc accedir a DATA
// NOTA: No es exactament 8 perque ja fem servir una pagina inicial i pot ser que compilador per culpa de offset acabi necessitan mes pagines
int NUM_SLOTS = 5;  // Num pag. a comprovar (> 5 --> page_fault)

void write_text(const char *msg)
{
  write(1, (char*)msg, strlen((char*)msg));
}

void write_line(const char *msg)
{
  write_text(msg);
  write(1, "\n", 1);
}

void write_errno(const char *tag)
{
  char buf[32];
  itoa(errno, buf);
  write_text(tag);
  write_text(": errno=");
  write_line(buf);
}

void test_slot(int depth)
{
  volatile char page[4096];  // IMPO: Que sigui volatile perque sino compilador optimitza

  page[0] = 'p';
  page[sizeof(page) - 1] = 'a';

  // Fer recursivitat
  if (depth > 0)
  {
    test_slot(depth-1);
    page[(sizeof(page) - 1)/2] = 'u';
  }
}

void test_stack(const char *tag)
{
  write_text(tag);
  write_line(": stack realloc test");

  test_slot(NUM_SLOTS);

  write_text(tag);
  write_line(": SUCCESS");
}

void thread_func(void *arg)
{
  test_stack((const char *) arg);
  write_text((const char *) arg);
  write_line(": exiting");
}

void tls_thread(void *arg)
{
  errno = 69;
  write_errno("TLS THREAD");
}

int __attribute__ ((__section__(".text.main"))) main(void)
{
  int ret;
  int pid;

  write_line("MAIN: Check stack slot");
  test_stack("MAIN");

  errno = 420;
  write_errno("MAIN: before TLS thread");
  ret = ThreadCreate(tls_thread, NULL);
  if (ret < 0)
  {
    write_line("ERROR: Creating TLS thread");
  }
  else
  {
    write_errno("MAIN: after TLS thread");
  }

  ret = ThreadCreate(thread_func, "THREAD 1");
  if (ret < 0)
  {
    write_line("ERROR: Creating base thread");
  }
  else
  {
    write_line("SUCCESS: Thread created");
  }
  
  pid = fork();
  if (pid == 0)
  {
    write_line("THREAD 0 (CHILD): entering test");
    test_stack("THREAD 0 (CHILD)");
    ThreadCreate(thread_func, "THREAD 1 (CHILD)");
  }
  else if (pid > 0)
  {
    write_line("THREAD 0 (FATHER): fork done");
    test_stack("THREAD 0 (FATHER)");
  }
  else
  {
    write_line("ERROR: fork failed");
  }
  
  while(1) { }
}
