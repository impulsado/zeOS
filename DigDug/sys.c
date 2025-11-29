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

#include <tls.h>

#define LECTURA 0
#define ESCRIPTURA 1

int global_PID=1000;
static unsigned int global_TID=0;

void * get_ebp();

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
  struct task_struct *curr = current();
  struct thread_group *group = curr->group;

  // === BASE CASE
  // Evitar que peti.
  // NOTA: Aixo no deuria passar
  if (group == NULL)
  {
    free_user_pages(curr);
    list_add_tail(&(curr->list), &freequeue);
    sched_next_rr();
    return;  // No arriba
  }

  // === GENERAL CASE
  // Matar tots els threads del process
  struct list_head *pos;
  struct list_head *tmp;
  list_for_each_safe(pos, tmp, &(group->members))
  {
    struct task_struct *thread = list_entry(pos, struct task_struct, thread_node);
    list_del(&(thread->list));  // Eliminar de la readyqueue si estava. Si estava en freequeue despres la torno a afegir
    task_release_stack_slot(thread);
    thread_group_remove_task(thread);
    thread->PID = -1;
    thread->TID = 0;
    list_add_tail(&(thread->list), &freequeue);
  }

  free_user_pages(curr);
  thread_group_destroy(group);

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
  int pag, slot_result;
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
  unsigned int logical;
  int data_frames[NUM_PAG_DATA];

  // Iniciar els valors perque no es queixi el compilador
  for (pag = 0; pag < NUM_PAG_DATA; pag++)
    data_frames[pag] = -1;

  // Comprovar que pare tingui pagina disponible temporal
  unsigned int temp_data_page = TEMP_DATA_COPY_PAGE;
  if (father_PT[temp_data_page].bits.present)
  {
    list_add_tail(lhcurrent, &freequeue);
    return -EIO;
  }

  for (pag = 0; pag < NUM_PAG_DATA; pag++)
  {
    logical = PAG_LOG_INIT_DATA + pag;

    if (!father_PT[logical].bits.present)  // Si el pare no la fa servir, el fill no la necessita
      continue;

    int frame = alloc_frame();
    if (frame < 0)  // Fer Rollback
    {
      for (int j = 0; j < NUM_PAG_DATA; j++)
      {
        if (data_frames[j] > 0)
        {
          del_ss_pag(process_PT, PAG_LOG_INIT_DATA + j);
          free_frame(data_frames[j]);
        }
      }
      list_add_tail(lhcurrent, &freequeue);
      return -ENOMEM;
    }

    data_frames[pag] = frame;

    // Mappeig del frame
    set_ss_pag(process_PT, logical, frame);
    set_ss_pag(father_PT, temp_data_page, frame);

    // Copiar Pare[DATA+pag] --> Pare[TEMP] == Fill[DATA+pag]
    copy_data((void*)(logical<<12), (void*)(temp_data_page<<12), PAGE_SIZE);

    // Eliminar el mapeig temporal al pare
    del_ss_pag(father_PT, temp_data_page);
  }

  set_cr3(get_DIR(current()));

  // Assignar informacio general al fill
  INIT_LIST_HEAD(&(uchild->task.thread_node));
  struct thread_group *group = thread_group_create();  // Creem un grup perque ja es indpendent al pare
  if (group == NULL)
  {
    free_user_pages(&(uchild->task));
    list_add_tail(lhcurrent, &freequeue);
    return -EAGAIN;
  }
  thread_group_add_task(group, &(uchild->task));
  uchild->task.slot_num = THREAD_STACK_SLOT_NONE;  // Seguretat
  
  // Assignar slot a fill (Ha de ser mateix num que pare)
  slot_result = task_alloc_specific_stack_slot(&(uchild->task), father->slot_num);
  if (slot_result < 0)
  {
    thread_group_remove_task(&(uchild->task));
    thread_group_destroy(uchild->task.group);
    free_user_pages(&(uchild->task));
    list_add_tail(lhcurrent, &freequeue);
    return slot_result;
  }

  // Clonar el contingut del slot
  slot_result = clone_stack_slot_pages(father, &(uchild->task));
  if (slot_result < 0)
  {
    task_release_stack_slot(&(uchild->task));
    thread_group_remove_task(&(uchild->task));
    thread_group_destroy(uchild->task.group);
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
  // NOTA: Aixo ja petara en mode usuari (Dit pel profe)
  //if (function == NULL || _wrapper == NULL) return -EINVAL;
  //if (!access_ok(VERIFY_READ, function, sizeof(void (*)(void *)))) return -EFAULT;
  //if (parameter && !access_ok(VERIFY_READ, _wrapper, sizeof(void (*)(void *)))) return -EFAULT;

  // Hi ha algun TCB(PCB) dispo
  if (list_empty(&freequeue)) 
    return -ENOMEM;

  // === GENERAL CASE
  // Obtindre TCB
  struct list_head *free_entry = list_first(&freequeue);
  list_del(free_entry);
  union task_union *new_union = (union task_union*)list_head_to_task_struct(free_entry);
  struct task_struct *new_task = &new_union->task;

  // Replicar el TCB
  copy_data(current(), new_union, sizeof(union task_union));

  struct task_struct *parent = current();
  struct thread_group *group = parent->group;
  if (group == NULL)
  {
    list_add_tail(free_entry, &freequeue);
    return -EINVAL;
  }

  // Assignar parametres al nou TCB
  init_stats(&(new_task->p_stats));
  new_task->TID = ++global_TID;  // Incrementar abans d'assignar
  new_task->state = ST_READY;
  new_task->group = NULL;
  new_task->slot_num = THREAD_STACK_SLOT_NONE;
  INIT_LIST_HEAD(&new_task->thread_node);

  // Ficar al nou thread en el grup que li toca
  thread_group_add_task(group, new_task);

  // Assignar slot de user stack
  int ret = task_alloc_stack_slot(new_task);
  if (ret < 0)
  {
    thread_group_remove_task(new_task);
    list_add_tail(free_entry, &freequeue);
    return ret;
  }

  // Preparar la pila usuari just per sota del bloc TLS
  // NOTA: Fer "--i" i no "i--" perque en la primera versio decrementem abans d'accedir
  // NOTA: Faig aquesta implementacio per aixo no haver de calcular USER_STACK_SIZE
  // IMPO: Parteixo d'on comensa el TLS per aixi no trepitjar res 
  DWord *user_esp = (DWord *)(THREAD_TLS_VADDR(new_task->slot_num));
  *(--user_esp) = (DWord)parameter;
  *(--user_esp) = (DWord)function;
  *(--user_esp) = 0;  // Offset per a ThreadWrapper (que pugui accedir a "function" com a parametre)

  // Preparar la pila sistema
  DWord *kernel_stack = new_union->stack;
  kernel_stack[KERNEL_STACK_SIZE - 5] = (DWord)_wrapper;  // user_eip
  kernel_stack[KERNEL_STACK_SIZE - 2] = (DWord)user_esp;  // user_esp

  DWord register_ebp = (DWord)get_ebp();
  register_ebp = (register_ebp - (DWord)current()) + (DWord)new_task;
  new_task->register_esp = register_ebp;

  // Encolar nou thread a readyqueue
  list_add_tail(&(new_task->list), &readyqueue);

  return 0;  // POSIX
}

void sys_ThreadExit(void)
{
  // === BASE CASE
  struct task_struct *curr = current();
  struct thread_group *group = curr->group;
  // Evitar que peti
  // NOTA: No deuria passar pero seguretat
  if (group == NULL)
  {
    sys_exit();
    return;  // No arriba
  }

  // Es l'unic thread del grup
  if (group->members.next == &(curr->thread_node) &&
      group->members.prev == &(curr->thread_node))
  {
    sys_exit();
    return;  // No arriba
  }

  // === GENERAL CASE
  // Alliberar el slot assignat
  task_release_stack_slot(curr);
  thread_group_remove_task(curr);
  curr->PID = -1;
  // Assignar TCB(PCB) com a disponible
  list_add_tail(&(curr->list), &freequeue);

  // Forsar canvi de thread/process
  sched_next_rr();
}