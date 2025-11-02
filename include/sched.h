/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024
#define DEFAULT_QUANTUM 3

extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct task_struct idle_task;

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct 
{
  //=== General
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry *dir_pages_baseAddr;
  DWord kernel_esp;
  
  //=== RR
  int quantum;

  //=== Hierarchy
  struct task_struct *father;
  struct list_head child_list;
  struct list_head child_node;  // node dins la llista de fills

  //=== State
  int pending_blocks;

  //=== List
  // IMPO: Un node MAI pot estar en dues llistes a l'hora --> Node per llista
  // OBS:  Nosaltres sabrem que no podra estar en {ready,free}queue a la vegada
  struct list_head list;  // node en la llista {readyqueue, freequeue}
};

union task_union 
{
  struct task_struct task;
  DWord stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union task[NR_TASKS]; /* Vector de tasques */

#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

struct task_struct *current();

void task_switch(union task_union* new);

struct task_struct *list_head_to_task_struct(struct list_head *l);
union task_union *list_head_to_task_union(struct list_head *l);


int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr(void);
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr(void);
void update_sched_data_rr(void);

void scheduler(void);

int get_quantum(struct task_struct *t);
void set_quantum(struct task_struct *t, int new_quantum);

#endif  /* __SCHED_H__ */
