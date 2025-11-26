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

int global_PID=1000;
static unsigned int global_TID=0;

void * get_ebp();
int sys_ThreadExit(void);

#define TEMP_STACK_COPY_BASE (TOTAL_PAGES - THREAD_STACK_SLOT_PAGES)
#define TEMP_DATA_COPY_PAGE (TEMP_STACK_COPY_BASE - 1)

static int clone_stack_slot_pages(struct task_struct *father, struct task_struct *child)
{
  // === BASE CASE
  // Pare no te slot
  unsigned int slot = father->slot_num;
  if (slot == THREAD_STACK_SLOT_NONE)
    return -EINVAL;

  // === GENERAL CASE
  unsigned int lower_page = get_slot_limit_page(slot);  // IMPO: Tot i que sigui la top, esta en la part baixa de la mem
  unsigned int temp_base = TEMP_STACK_COPY_BASE;
  page_table_entry *father_PT = get_PT(father);
  page_table_entry *child_PT = get_PT(child);
  unsigned int child_frames[THREAD_STACK_SLOT_PAGES];

  // Buscar THREAD_STACK_SLOT_PAGES pagines lliures
  for (int i = 0; i < THREAD_STACK_SLOT_PAGES; i++)
  {
    child_frames[i] = alloc_frame();
    if (child_frames[i] != -1)
	    continue;
    
    // Hi ha hagut algun error
    for (int j = 0; j < i; j++)
		{
			free_frame(child_frames[j]);
		}
    return -ENOMEM;
  }

  // Assignar els frames a les pagines corresponents
  // Fill: En slot que li toca
  // Pare: En zona lliure 
  for (int i = 0; i < THREAD_STACK_SLOT_PAGES; i++)
  {
    unsigned int temp_page = temp_base + i;
    unsigned int child_page = lower_page + i;
    set_ss_pag(child_PT, child_page, child_frames[i]);
    set_ss_pag(father_PT, temp_page, child_frames[i]);
  }

  // Fer flush TLB
  set_cr3(get_DIR(father));

  // Fer copia de tot el slot
  copy_data((void*)(lower_page<<12), (void*)(temp_base<<12), THREAD_STACK_SLOT_PAGES * PAGE_SIZE);

  // Treure del pare el mapeig temporal
  for (int i = 0; i < THREAD_STACK_SLOT_PAGES; i++)
  {
    del_ss_pag(father_PT, temp_base + i);
  }

  // Reiniciar la TLB
	// IMPO: NO volem saltar a fill, aixi que volem que cr3 continu apuntant al pare
	set_cr3(get_DIR(father));

  return 0;
}

void sys_exit()
{  
  int i;
  struct task_struct *curr = current();
  struct task_struct *master_thread = task_initial_thread(curr);

  // Si no es el master, fer ThreadExit
  if (curr != master_thread)
  {
    sys_ThreadExit();
    return;
  }

  // Eliminar tots els threads del process
  struct list_head *pos;
  struct list_head *tmp;
  list_for_each_safe(pos, tmp, &(master_thread->thread_list))
  {
    struct task_struct *thread = list_entry(pos, struct task_struct, thread_node);
    list_del(pos);
    list_del(&(thread->list));
    task_release_stack_slot(thread);
    thread->PID = -1;
    thread->initial_thread = NULL;
    INIT_LIST_HEAD(&(thread->thread_list));
    INIT_LIST_HEAD(&(thread->thread_node));
    list_add_tail(&(thread->list), &freequeue);
  }
  INIT_LIST_HEAD(&(master_thread->thread_list));
  master_thread->slot_mask = 0;

  // Treure les entrades de DATA
  page_table_entry *process_PT = get_PT(master_thread);
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    unsigned int logical = PAG_LOG_INIT_DATA + i;
    free_frame(get_frame(process_PT, logical));
    del_ss_pag(process_PT, logical);
  }
  
  // Alliberar task_struct
  master_thread->PID=-1;
  master_thread->initial_thread = NULL;
  list_add_tail(&(master_thread->list), &freequeue);
  
  // Forsar canvi
  sched_next_rr();
}

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

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  struct task_struct *father = current();
  
  // === BASE CASE
  // No hi ha cap TCB lliure
  if (list_empty(&freequeue)) return -ENOMEM;

  // Si el pare no es process d'usuari, no pot fer fork
  if (father->slot_num == THREAD_STACK_SLOT_NONE)
    return -EINVAL;

  // === GENERAL CASE
  // Obtindre TCB
  lhcurrent=list_first(&freequeue);
  list_del(lhcurrent);
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  // Copiar tota la part de sistema al fill (replicar)
  copy_data(father, uchild, sizeof(union task_union));
  
  // Assignar una TD al fill (no volem que comparteixin mem)
  allocate_DIR((struct task_struct*)uchild);
  
  /* Copy father's SYSTEM and CODE to child. */
  int new_ph_pag, pag, slot_result;
  int register_ebp;
  page_table_entry *process_PT = get_PT(&uchild->task);
  page_table_entry *father_PT = get_PT(father);

  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(father_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(father_PT, PAG_LOG_INIT_CODE+pag));
  }

  /* Allocate pages for DATA+STACK */
  // Nomes mapeig la primera (Legacy)
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    unsigned int logical = PAG_LOG_INIT_DATA + pag;
    process_PT[logical].entry = 0;
  }

  unsigned int data_logical = PAG_LOG_INIT_DATA;
  new_ph_pag = alloc_frame();
  if (new_ph_pag == -1)
  {
    list_add_tail(lhcurrent, &freequeue);
    return -EAGAIN;
  }
  set_ss_pag(process_PT, data_logical, new_ph_pag);

  /* Copy father's DATA to child using a temporary logical page */
  unsigned int temp_data_page = TEMP_DATA_COPY_PAGE;
  // Comprovar que el pare no l'estigui fent servir
  if (father_PT[temp_data_page].bits.present)
  {
    free_frame(get_frame(process_PT, data_logical));
    del_ss_pag(process_PT, data_logical);
    list_add_tail(lhcurrent, &freequeue);
    return -EIO;
  }

  // Copiar tot el que hi hagi en la pagina
  set_ss_pag(father_PT, temp_data_page, get_frame(process_PT, data_logical));
  copy_data((void*)(data_logical<<12), (void*)(temp_data_page<<12), PAGE_SIZE);
  del_ss_pag(father_PT, temp_data_page);

  // Assignar informacio al fill
  uchild->task.initial_thread = &(uchild->task);
  INIT_LIST_HEAD(&(uchild->task.thread_list));
  INIT_LIST_HEAD(&(uchild->task.thread_node));
  uchild->task.slot_mask = 0;
  uchild->task.slot_num = THREAD_STACK_SLOT_NONE;
  uchild->task.TID = 0;

  // Assignar el mateix slot que pare
  slot_result = task_alloc_specific_stack_slot(&(uchild->task), father->slot_num);
  if (slot_result < 0)
  {
    free_user_pages(&(uchild->task));
    list_add_tail(lhcurrent, &freequeue);
    return slot_result;
  }

  // Clonar el contingut del slot
  slot_result = clone_stack_slot_pages(father, &(uchild->task));
  if (slot_result < 0)
  {
    task_release_stack_slot(&(uchild->task));
    free_user_pages(&(uchild->task));
    list_add_tail(lhcurrent, &freequeue);
    return slot_result;
  }

  /* Deny access to the child's memory space */
  set_cr3(get_DIR(father));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;

  /* Map father's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)father) + (int)(uchild);

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

  // Encolar nou thread a readyqueue
  list_add_tail(&(new_task->list), &readyqueue);

  return new_task->TID;
}

int sys_ThreadExit(void)
{
  // === BASE CASE
  // Es invalid o fem exit del master
  struct task_struct *curr = current();
  struct task_struct *master_thread = task_initial_thread(curr);

  if (master_thread == NULL || master_thread == curr)
  {
    sys_exit();
    return 0;
  }

  // === GENERAL CASE
  // Eliminar thread de la llista del master
  list_del(&(curr->thread_node));

  // Alliberar el slot assignat
  task_release_stack_slot(curr);
  curr->PID = -1;
  curr->initial_thread = NULL;
  INIT_LIST_HEAD(&(curr->thread_list));
  INIT_LIST_HEAD(&(curr->thread_node));
  
  // Assignar TCB(PCB) com a disponible
  list_add_tail(&(curr->list), &freequeue);

  // Forsar canvi de thread/process
  sched_next_rr();
  return 0;
}