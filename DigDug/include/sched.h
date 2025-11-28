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
#define THREAD_STACK_SLOT_NONE 0xFFFFFFFF    /* No esta en Slot com a tal (INIT, IDLE) */

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct thread_group
{
  int active;
  unsigned int slot_mask;
  struct list_head members;
};

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
  struct thread_group *group;
  struct list_head thread_node;
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

unsigned int get_slot_init_page(unsigned int slot);
unsigned int get_slot_limit_page(unsigned int slot);

int task_alloc_stack_slot(struct task_struct *t);
int task_alloc_specific_stack_slot(struct task_struct *t, unsigned int slot);
void task_release_stack_slot(struct task_struct *t);

struct thread_group *thread_group_create(void);
void thread_group_destroy(struct thread_group *group);
void thread_group_add_task(struct thread_group *group, struct task_struct *task);
void thread_group_remove_task(struct task_struct *task);

#endif  /* __SCHED_H__ */
