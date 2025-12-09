/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <types.h>
#include <hardware.h>
#include <segment.h>
#include <sched.h>
#include <mm.h>
#include <io.h>
#include <utils.h>
#include <p_stats.h>
#include <errno.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS + 2]
    __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

// Free task structs
struct list_head freequeue;
// Ready queue
struct list_head readyqueue;
// Tick wait queue - Threads esperant WaitForTick
struct list_head tick_waitqueue;

struct thread_group thread_groups[NR_TASKS];
struct keyboard_info keyboard_infos[NR_TASKS];

struct thread_group *thread_group_alloc(void)
{
  for (int i = 0; i < NR_TASKS; i++)
  {
    if (thread_groups[i].active)
      continue;

    thread_groups[i].active = 1;
    INIT_LIST_HEAD(&thread_groups[i].members);
    thread_groups[i].slot_mask = 0;
    return &thread_groups[i];
  }
  return NULL;
}

void thread_group_free(struct thread_group *group)
{
  if (group == NULL)
    return;

  group->active = 0;
  group->slot_mask = 0;
  INIT_LIST_HEAD(&group->members);
}

struct keyboard_info *kbd_info_alloc(void)
{
  for (int i = 0; i < NR_TASKS; i++)
  {
    if (keyboard_infos[i].handler == NULL && keyboard_infos[i].wrapper == NULL)
      return &keyboard_infos[i];
  }
  return NULL;
}

void kbd_info_free(struct keyboard_info *info)
{
  if (info == NULL)
    return;

  info->handler = NULL;
  info->wrapper = NULL;
}

void thread_group_add_task(struct thread_group *group, struct task_struct *task)
{
  if (group == NULL || task == NULL)
    return;

  task->group = group;
  INIT_LIST_HEAD(&(task->thread_node));
  list_add_tail(&(task->thread_node), &group->members);
}

void thread_group_remove_task(struct task_struct *task)
{
  if (task == NULL || task->group == NULL)
    return;

  list_del(&(task->thread_node));
  INIT_LIST_HEAD(&(task->thread_node));
  task->group = NULL;
}

void thread_reset_tls_area(struct task_struct *t)
{
  // === BASE CASE
  if (t == NULL || t->slot_num == THREAD_STACK_SLOT_NONE)
    return;

  // === GENERAL CASE
  unsigned char *tls_base = (unsigned char *)THREAD_TLS_VADDR(t->slot_num);
  memset(tls_base, 0, THREAD_TLS_SIZE); // Ficar a 0 poden pensar amb reservar-ho
}

void init_stats(struct stats *s)
{
  s->user_ticks = 0;
  s->system_ticks = 0;
  s->blocked_ticks = 0;
  s->ready_ticks = 0;
  s->elapsed_total_ticks = get_ticks();
  s->total_trans = 0;
  s->remaining_ticks = get_ticks();
}

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry *get_DIR(struct task_struct *t)
{
  return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry *get_PT(struct task_struct *t)
{
  return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr)) << 12);
}

int allocate_DIR(struct task_struct *t)
{
  int pos;

  pos = ((int)t - (int)task) / sizeof(union task_union);

  t->dir_pages_baseAddr = (page_table_entry *)&dir_pages[pos];

  return 1;
}

void cpu_idle(void)
{
  __asm__ __volatile__("sti" : : : "memory");

  while (1)
  {
    ;
  }
}

#define DEFAULT_QUANTUM 10

int remaining_quantum = 0;

int get_quantum(struct task_struct *t)
{
  return t->total_quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
  t->total_quantum = new_quantum;
}

struct task_struct *idle_task = NULL;

void update_sched_data_rr(void)
{
  remaining_quantum--;
}

int needs_sched_rr(void)
{
  if ((remaining_quantum == 0) && (!list_empty(&readyqueue)))
    return 1;
  if (remaining_quantum == 0)
    remaining_quantum = get_quantum(current());
  return 0;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue)
{
  if (t->state != ST_RUN)
    list_del(&(t->list));
  if (dst_queue != NULL)
  {
    list_add_tail(&(t->list), dst_queue);
    if (dst_queue != &readyqueue)
      t->state = ST_BLOCKED;
    else
    {
      update_stats(&(t->p_stats.system_ticks), &(t->p_stats.elapsed_total_ticks));
      t->state = ST_READY;
    }
  }
  else
    t->state = ST_RUN;
}

void sched_next_rr(void)
{
  struct list_head *e;
  struct task_struct *t;

  if (!list_empty(&readyqueue))
  {
    e = list_first(&readyqueue);
    list_del(e);

    t = list_head_to_task_struct(e);
  }
  else
    t = idle_task;

  t->state = ST_RUN;
  remaining_quantum = get_quantum(t);

  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
  update_stats(&(t->p_stats.ready_ticks), &(t->p_stats.elapsed_total_ticks));
  t->p_stats.total_trans++;

  task_switch((union task_union *)t);
}

void schedule()
{
  struct list_head *pos, *tmp;
  struct task_struct *t;

  // Desbloquejar threads que esperaven un tick
  // NOTA: Nosaltres sabem que aqui dins sempre hi haura threads en estat ST_BLOCKED
  list_for_each_safe(pos, tmp, &tick_waitqueue)
  {
    t = list_entry(pos, struct task_struct, list);
    update_process_state_rr(t, &readyqueue);
  }

  update_sched_data_rr();
  if (needs_sched_rr())
  {
    update_process_state_rr(current(), &readyqueue);
    sched_next_rr();
  }
}

void init_idle(void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union *)c;

  c->PID = 0;

  c->total_quantum = DEFAULT_QUANTUM;

  init_stats(&c->p_stats);

  allocate_DIR(c);

  // Iniciar threads
  // NOTA: No es fara servir perque no saltem a mode usuari. Pero per seguretat.
  c->group = NULL;
  INIT_LIST_HEAD(&c->thread_node);
  c->TID = 0;
  c->slot_num = THREAD_STACK_SLOT_NONE;

  // Iniciar keyboard event
  c->kbd_info = NULL;
  c->in_keyboard_event = 0;

  uc->stack[KERNEL_STACK_SIZE - 1] = (DWord)&cpu_idle; /* Return address */
  uc->stack[KERNEL_STACK_SIZE - 2] = 0;                /* register ebp */

  c->register_esp = (int)&(uc->stack[KERNEL_STACK_SIZE - 2]); /* top of the stack */

  idle_task = c;
}

void setMSR(DWord msr_number, DWord high, DWord low);

void init_task1(void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union *)c;

  c->PID = 1;

  c->total_quantum = DEFAULT_QUANTUM;

  c->state = ST_RUN;

  remaining_quantum = c->total_quantum;

  init_stats(&c->p_stats);

  allocate_DIR(c);

  set_user_pages(c);

  // Iniciar threads
  // NOTA: Aqui no pot fallar res per aixo no comprovem res
  struct thread_group *group = thread_group_alloc();
  if (group == NULL)
    return;
  thread_group_add_task(group, c);
  c->TID = 0;
  c->slot_num = THREAD_STACK_SLOT_NONE;
  thread_alloc_specific_stack_slot(c, 0);

  // Iniciar keyboard
  struct keyboard_info *kbd = kbd_info_alloc();
  c->kbd_info = kbd;
  c->in_keyboard_event = 0;

  tss.esp0 = (DWord) & (uc->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (DWord) & (uc->stack[KERNEL_STACK_SIZE]));

  set_cr3(c->dir_pages_baseAddr);
}

void init_freequeue()
{
  int i;

  INIT_LIST_HEAD(&freequeue);

  /* Insert all task structs in the freequeue */
  for (i = 0; i < NR_TASKS; i++)
  {
    task[i].task.PID = -1;
    task[i].task.group = NULL;
    INIT_LIST_HEAD(&(task[i].task.thread_node));
    task[i].task.TID = 0;
    task[i].task.slot_num = THREAD_STACK_SLOT_NONE;
    task[i].task.kbd_info = NULL;
    task[i].task.in_keyboard_event = 0;
    list_add_tail(&(task[i].task.list), &freequeue);
  }
}

void init_sched()
{
  init_freequeue();
  INIT_LIST_HEAD(&readyqueue);
  INIT_LIST_HEAD(&tick_waitqueue);
}

struct task_struct *current()
{
  int ret_value;

  return (struct task_struct *)(((unsigned int)&ret_value) & 0xfffff000);
}

struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return (struct task_struct *)((int)l & 0xfffff000);
}

/* Do the magic of a task switch */
void inner_task_switch(union task_union *new)
{
  page_table_entry *new_DIR = get_DIR(&new->task);

  /* Update TSS and MSR to make it point to the new stack */
  tss.esp0 = (int)&(new->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (DWord) & (new->stack[KERNEL_STACK_SIZE]));

  /* TLB flush. New address space */
  set_cr3(new_DIR);

  switch_stack(&current()->register_esp, new->task.register_esp);
}

/* Force a task switch assuming that the scheduler does not work with priorities */
void force_task_switch()
{
  update_process_state_rr(current(), &readyqueue);

  sched_next_rr();
}

/* Agafar el bit de la posicio slot i comprovar si es 1 o 0*/
static unsigned int get_slot_status(unsigned int mask, unsigned int slot)
{
  return (mask >> slot) & 1;
}

static void set_slot_status(struct thread_group *group, unsigned int slot, unsigned int value)
{
  // === BASE CASE
  if (group == NULL)
    return;

  // === GENERAL CASE
  unsigned int bit_pos = 1 << slot;
  unsigned int mask = group->slot_mask & ~bit_pos; // Ficar nomes el bit en la posicio a 0
  group->slot_mask = (mask | (value << slot));     // Assignar al bit el valor
}

unsigned int get_slot_init_page(unsigned int slot)
{
  return THREAD_STACK_SLOT_INIT_PAGE(slot);
}

unsigned int get_slot_limit_page(unsigned int slot)
{
  return THREAD_STACK_SLOT_LIMIT_PAGE(slot);
}

static int map_stack_page(struct task_struct *t, unsigned int logical_page)
{
  // === BASE CASE
  // Comprovar que no estigui ocupada
  page_table_entry *process_PT = get_PT(t);
  if (process_PT[logical_page].bits.present)
    return -EBUSY;

  // === GENERAL CASE
  // Agafar frame fisic
  int frame = alloc_frame();
  if (frame < 0)
    return -ENOMEM;

  // Assignar el frame
  set_ss_pag(process_PT, logical_page, frame);

  // Fer flush TLB
  set_cr3(get_DIR(t));

  return 0;
}

int thread_alloc_stack_slot(struct task_struct *t)
{
  // === BASE CASE
  struct thread_group *group = t->group;
  if (group == NULL)
    return -EINVAL;

  // === GENERAL CASE
  // Buscar un slot dispo
  unsigned int slot = THREAD_STACK_SLOT_NONE;
  for (unsigned int i = 0; i < THREAD_MAX_STACK_SLOTS; i++)
  {
    if (!get_slot_status(group->slot_mask, i))
    {
      slot = i;
      break;
    }
  }

  if (slot == THREAD_STACK_SLOT_NONE)
    return -EAGAIN;

  // Assignar el slot trobat
  return thread_alloc_specific_stack_slot(t, slot);
}

int thread_alloc_specific_stack_slot(struct task_struct *t, unsigned int slot)
{
  // === BASE CASE
  struct thread_group *group = t->group;
  if (group == NULL)
    return -EINVAL;

  // IDLE no te slot (es kernel mode)
  if (t->PID == 0)
    return -EINVAL;

  // Num de slot invalid
  if (slot >= THREAD_MAX_STACK_SLOTS || slot < 0)
    return -EINVAL;

  // Ja esta ocupat
  if (get_slot_status(group->slot_mask, slot))
    return -EBUSY;

  // === GENERAL CASE
  // Assignar num slot
  t->slot_num = slot;
  set_slot_status(group, slot, 1);

  // Mappeig de la primera pagina del slot
  unsigned int init_page = get_slot_init_page(slot);
  int ret = map_stack_page(t, init_page);
  if (ret < 0)
  {
    set_slot_status(group, slot, 0);
    t->slot_num = THREAD_STACK_SLOT_NONE;
    return ret;
  }

  thread_reset_tls_area(t);
  return 0;
}

void thread_release_stack_slot(struct task_struct *t)
{
  // === BASE CASE
  struct thread_group *group = t->group;
  if (group == NULL)
    return;

  // No esta assignat a cap slot
  if (t->slot_num == THREAD_STACK_SLOT_NONE)
    return;

  // === GENERAL CASE
  unsigned int slot = t->slot_num;
  page_table_entry *process_PT = get_PT(t);
  unsigned int upper_page = get_slot_init_page(slot);
  unsigned int lower_page = get_slot_limit_page(slot);

  // Desasignar frames
  for (unsigned int page = lower_page; page <= upper_page; page++)
  {
    if (process_PT[page].bits.present)
    {
      unsigned int frame = get_frame(process_PT, page);
      del_ss_pag(process_PT, page);
      free_frame(frame);
    }
  }

  // Fer flush de TLB
  set_cr3(get_DIR(t));

  set_slot_status(group, slot, 0);
  t->slot_num = THREAD_STACK_SLOT_NONE;
}

int kbd_slot_alloc(struct task_struct *t)
{
  // === GENERAL CASE
  page_table_entry *process_PT = get_PT(t);
  unsigned int init_page = get_slot_init_page(KBD_SLOT);

  int frame = alloc_frame();
  if (frame < 0)
    return -ENOMEM;

  set_ss_pag(process_PT, init_page, frame);
  set_cr3(get_DIR(t));
  return 0;
}

void kbd_slot_free(struct task_struct *t)
{
  // === GENERAL CASE
  page_table_entry *process_PT = get_PT(t);
  unsigned int upper_page = get_slot_init_page(KBD_SLOT);
  unsigned int lower_page = get_slot_limit_page(KBD_SLOT);

  // Desmapejar les pagines que estiguin mapejades
  for (unsigned int page = lower_page; page <= upper_page; page++)
  {
    unsigned int frame = get_frame(process_PT, page);
    del_ss_pag(process_PT, page);
    free_frame(frame);
  }

  // Flush TLB
  set_cr3(get_DIR(t));
}