/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>

#define NR_TASKS 10
#define KERNEL_STACK_SIZE 1024
#define THREAD_STACK_SLOT_NONE 0xFFFFFFFF /* No esta en Slot com a tal (INIT, IDLE) */

/* Keyboard event context */
#define KEYBOARD_SAVED_CONTEXT_SIZE 11

struct keyboard_context
{
  DWord saved_regs[KEYBOARD_SAVED_CONTEXT_SIZE];
  DWord eip;
  DWord cs;
  DWord eflags;
  DWord esp;
  DWord ss;
};

enum state_t
{
  ST_RUN,
  ST_READY,
  ST_BLOCKED
};

/* Keyboard info - Compartit per tots els threads del proces */
struct keyboard_info
{
  void (*handler)(char key, int pressed); /* Funcio a executar quan events del teclat */
  void *wrapper;                          /* Wrapper (sempre KeyboardWrapper) */
};

struct thread_group
{
  int active;
  unsigned int slot_mask;
  struct list_head members;
};

struct task_struct
{
  // === GENERAL
  int PID; /* Process ID. This MUST be the first field of the struct. */
  page_table_entry *dir_pages_baseAddr;
  struct list_head list; /* Task struct enqueuing */
  int register_esp;      /* position in the stack */
  struct stats p_stats;  /* Process stats */

  // === STATE
  enum state_t state; /* State of the process */
  int total_quantum;  /* Total quantum of the process */

  // ==== THREADING
  unsigned int TID;

  unsigned int slot_num; /* Num del slot */
  struct thread_group *group;
  struct list_head thread_node;

  // ==== KEYBOARD EVENT
  int in_keyboard_event;             /* 1 si estem executant el handler del teclat */
  struct keyboard_info *kbd_info;    /* Punter a info del keyboard (handler + wrapper) */
  struct keyboard_context saved_ctx; /* Context guardat per restaurar despres del handler */
};

union task_union
{
  struct task_struct task;
  DWord stack[KERNEL_STACK_SIZE]; /* pila de sistema, per proces */
};

extern union task_union protected_tasks[NR_TASKS + 2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;

#define KERNEL_ESP(t) (DWord) & (t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP KERNEL_ESP(&task[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct list_head tick_waitqueue; /* Threads esperant WaitForTick */

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void schedule(void);

struct task_struct *current();

void task_switch(union task_union *t);
void switch_stack(int *save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry *get_PT(struct task_struct *t);

page_table_entry *get_DIR(struct task_struct *t);

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

unsigned int get_slot_init_page(unsigned int slot);
unsigned int get_slot_limit_page(unsigned int slot);

int thread_alloc_stack_slot(struct task_struct *t);
int thread_alloc_specific_stack_slot(struct task_struct *t, unsigned int slot);
void thread_release_stack_slot(struct task_struct *t);
void thread_reset_tls_area(struct task_struct *t);

int kbd_slot_alloc(struct task_struct *t);
void kbd_slot_free(struct task_struct *t);

struct thread_group *thread_group_alloc(void);
void thread_group_free(struct thread_group *group);
void thread_group_add_task(struct thread_group *group, struct task_struct *task);
void thread_group_remove_task(struct task_struct *task);

struct keyboard_info *kbd_info_alloc(void);
void kbd_info_free(struct keyboard_info *info);

#endif /* __SCHED_H__ */
