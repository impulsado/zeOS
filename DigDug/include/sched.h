/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>


#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024
#define THREAD_STACK_SLOT_NONE 0xFFFFFFFFu    /* No esta en Slot com a tal (INIT, IDLE) */

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

/*
  EXPLICACIO
  ----------
  1 PCB = 1 TCB
  En aquesta implementacio: "Un nou PCB, si PID es igual --> Correspon a un nou thread"
  D'aquesta forma ens estalviem crear un nou thread_struct i crear jerarquia amb punters.
  Tambe aproiftem el fet que una "task" es una union i ja podem aprofitar el thread_kernel_stack = task->stack.

  slot_mask
  ---------
  "unsigned int" que on el bit i-essim indica si el slot i-essim esta ocupat.
  Si "slot_mask[i] == 1" significa que el slot "i" esta ocupat per algun thread. 

  initial_thread
  --------------
  Thread que guarda informacio general sobre els threads del process. NOMES AQUEST.
  Poder centralitzar la gestio de slot_mask i thread_count.
*/

struct task_struct 
{
  // === GENERAL
  int PID;			                            /* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list;	                  /* Task struct enqueuing */
  int register_esp;		                      /* position in the stack */
  struct stats p_stats;		                  /* Process stats */
  
  // === STATE
  enum state_t state;		                    /* State of the process */
  int total_quantum;		                    /* Total quantum of the process */

  // ==== THREADING
  unsigned int TID;

  unsigned int slot_num;    /* Num del slot */
  unsigned int slot_mask;   /* Veure EXPLICACIO */
  
  struct task_struct *initial_thread;  /* Veure EXPLICACIO */
  struct list_head thread_list;
  struct list_head thread_node;
  unsigned int thread_count;
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void schedule(void);

struct task_struct * current();

void task_switch(union task_union*t);
void switch_stack(int * save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

struct task_struct *task_initial_thread(struct task_struct *t);
int task_alloc_stack_slot(struct task_struct *t);
void task_release_stack_slot(struct task_struct *t);
int task_map_initial_stack_page(struct task_struct *t);

#endif  /* __SCHED_H__ */
