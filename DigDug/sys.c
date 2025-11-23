/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return current()->PID;
}

int global_PID=1000;
static unsigned int global_TID=0;

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  
  return uchild->task.PID;
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
char localbuffer [TAM_BUFFER];
int bytes_left;
int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	bytes_left = nbytes;
	while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	}
	if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	}
	return (nbytes-bytes_left);
}


extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

void sys_exit()
{  
  int i;

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  /* Free task_struct */
  task_release_stack_slot(current());
  list_add_tail(&(current()->list), &freequeue);
  
  current()->PID=-1;
  
  /* Restarts execution of the next process */
  sched_next_rr();
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}

int sys_ThreadCreate(void (*function)(void* arg), void* parameter, void (*_wrapper)(void* arg))
{
  // === BASE CASE
  // Validar parametres
  if (function == NULL || _wrapper == NULL) return -EINVAL;
  if (!access_ok(VERIFY_READ, function, sizeof(void (*)(void *)))) return -EFAULT;
  if (parameter && !access_ok(VERIFY_READ, _wrapper, sizeof(void (*)(void *)))) return -EFAULT;

  // Hi ha algun TCB(PCB) dispo
  if (list_empty(&freequeue)) return -ENOMEM;

  // === GENERAL CASE
  // Obtindre TCB
  struct list_head *free_entry = list_first(&freequeue);
  list_del(free_entry);
  union task_union *new_union = (union task_union*)list_head_to_task_struct(free_entry);
  struct task_struct *new_task = &new_union->task;

  // Replicar el TCB
  copy_data(current(), new_union, sizeof(union task_union));

  // Obtindre master_thread
  struct task_struct *master_thread = task_initial_thread(current());
  if (master_thread == NULL)
  {
    list_add_tail(free_entry, &freequeue);
    return -EINVAL;
  }

  // Assignar parametres al nou TCB
  init_stats(&(new_task->p_stats));
  new_task->TID = ++global_TID;  // Incrementar abans d'assignar
  new_task->state = ST_READY;
  new_task->initial_thread = master_thread;
  new_task->slot_mask = 0;
  new_task->slot_num = THREAD_STACK_SLOT_NONE;
  new_task->thread_count = 1;
  INIT_LIST_HEAD(&new_task->thread_list);
  INIT_LIST_HEAD(&new_task->thread_node);

  // Assignar slot de user stack
  int ret = task_alloc_stack_slot(new_task);
  if (ret < 0)
  {
    task_release_stack_slot(new_task);
    list_add_tail(free_entry, &freequeue);
    return ret;
  }

  // Preparar la pila usuari
  // NOTA: Accedir a la page+1 per agafar la @bottom de la pagina (perque son sequencials)
  // NOTA: Fer "--i" i no "i--" perque en la primera versio decrementem abans d'accedir
  unsigned int init_page = get_slot_init_page(new_task->slot_num);

  DWord user_stack = ((DWord)(init_page + 1)) << 12;
  DWord *user_esp = (DWord *)user_stack;
  *(--user_esp) = (DWord)parameter;
  *(--user_esp) = (DWord)function;
  *(--user_esp) = 0;  // ebp trash
  DWord initial_user_esp = (DWord)user_esp;

  // Preparar la pila sistema
  // OBS: Fem servir el ret_from_fork i au
  DWord *kernel_stack = new_union->stack;
  kernel_stack[KERNEL_STACK_SIZE - 19] = 0;  // ebp_trash
  kernel_stack[KERNEL_STACK_SIZE - 18] = (DWord)&ret_from_fork;
  kernel_stack[KERNEL_STACK_SIZE - 5] = (DWord)_wrapper;  // user_eip
  kernel_stack[KERNEL_STACK_SIZE - 2] = initial_user_esp;  // user_esp
  new_task->register_esp = (int)&kernel_stack[KERNEL_STACK_SIZE - 19];

  // Actualitzar el master
  list_add_tail(&(new_task->thread_node), &(master_thread->thread_list));
  master_thread->thread_count++;

  // Encolar nou thread a readyqueue
  list_add_tail(&(new_task->list), &readyqueue);

  return new_task->TID;
}

int sys_ThreadExit(void)
{
  // === BASE CASE
  // Es invalid o fem exit del master
  struct task_struct *curr = current();
  struct task_struct *leader = task_initial_thread(curr);

  if (leader == NULL || leader == curr)
  {
    sys_exit();
    return 0;
  }

  // === GENERAL CASE
  // Eliminar thread de la llista del master
  list_del(&(curr->thread_node));

  // Decrementar el comptador de threads del process 
  if (leader->thread_count > 1)
    leader->thread_count--;

  // Alliberar el slot assignat
  task_release_stack_slot(curr);
  
  // Assignar TCB(PCB) com a disponible
  list_add_tail(&(curr->list), &freequeue);

  // Forsar canvi de thread/process
  sched_next_rr();
  return 0;
}