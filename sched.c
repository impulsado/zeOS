/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <utils.h>  // Per a writeMSR
#include <types.h>  // Per accedir a TSS

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;

struct list_head freequeue;
struct list_head readyqueue;

struct task_struct idle_task;

int actual_quantum;

void end_task_switch(unsigned long *kernel_esp_of_current, unsigned int new_esp_value);

struct task_struct *list_head_to_task_struct(struct list_head *l)
{
	return (struct task_struct*)((unsigned int)l & 0xfffff000);
}

union task_union *list_head_to_task_union(struct list_head *l)
{
	return (union task_union*)((unsigned int)l & 0xfffff000);
}

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	/*
	IMPORTANT
	---------
	1 Process <--> 1 P.D. pq. nomes tenim CR3 per apuntar a P.D.
	1 P.D. <--> 1 P.T. pq. nomes aprofitem entrada [0] de la P.D.
	"pos" es el 'i' dins de "task[]"

	Aprofitem el fet de que:
	Si estem assignant memoria al task[i], significa que la posicio 'i' estava lliure.
	Com que hi ha una relacio 1:1, significa que dir_pages[i] tambe esta lliure.
	Com a consequencia, pagusr_table[i] tambe estara lliure
		(En aquesta funcio no ens interesa saber que fer amb aquest fet. Veure "mm.c - set_user_pages()")
	*/

	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	// NOTA: Page Directory tambe fa servir "page_table_entry".
	// Hi ha atributs que canvien el significat (Veure "types.h")
	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
	/*
	OBSERVACIO
	----------
	Aqui no fa falta fer set_user_pages.
	IDLE es un process que executa codi de kernel (en ZeOS).
	Aixo implica que fara servir les entrades de la T.P. relacionades amb el Kernel (0 - 255)
	Aquestes ja estan sempre mapejades a totes les T.P.  (Fet en "init_table_pages()")
	*/

	// Obtindre el primer element
	struct list_head *pIdle = list_first(&freequeue);

	// Treure-lo de la llista de "tasks" lliures
	list_del(pIdle);

	// Obtindre IDLE
	union task_union *idle_union = list_head_to_task_union(pIdle);
	struct task_struct *idle_pcb = (struct task_struct *)idle_union;
	DWord *idle_stack = (DWord *)idle_union;

	// Asignar PID
	idle_pcb->PID = 0;

	// Assignar la P.D. corresponent
	allocate_DIR(idle_pcb);

	// Crear la pila de sistema
	// NOTA: Aquest process nomes sera executat desde context_switch i necessita (%ebp ; @ret)
	// NOTA: ebp, donat que sabem que la funcio cpu_idle es un bucle, no te importancia (pero necessari)
	// IMPO: Recorda que no podem accedir a KERNEL_STACK_SIZE (que aquest es el limit)
	idle_stack[KERNEL_STACK_SIZE-2] = 0;  // ebp 
	idle_stack[KERNEL_STACK_SIZE-1] = (DWord)cpu_idle;  // ret

	// Guardar kernel_esp
	// NOTA: Apunta al ebp pq. context_switch() ho necessita per desempilar.
	idle_pcb->kernel_esp = (DWord)&idle_stack[KERNEL_STACK_SIZE-2];

	// RR
	idle_pcb->quantum = DEFAULT_QUANTUM;

	// Hierarchy
	idle_pcb->father = idle_pcb;  // Es ell mateix (Tampoc morira com a tal)
	INIT_LIST_HEAD(&idle_pcb->child_list);  // Inicialment empty
	INIT_LIST_HEAD(&idle_pcb->child_node);  // No esta en cap llista 

	// Guardar globalment per millor gestio
	idle_task = *idle_pcb;
}

void init_task1(void)
{
	// Obtindre el primer element
	struct list_head *pInit = list_first(&freequeue);

	// Treure-lo de la llista de "tasks" lliures
	list_del(pInit);

	// Obtindre INIT
	union task_union *init_union = list_head_to_task_union(pInit);
	struct task_struct *init_pcb = (struct task_struct *)init_union;
	DWord *init_stack = (DWord *)init_union; 

	// Assignar PID
	init_pcb->PID = 1;

	// Assignar la P.D. corresponent
	allocate_DIR(init_pcb);

	// Assignar frames disponibles
	// NOTA: Aixo es perque INIT s'executara en mode usuari i necessitara frames per funcionar
	// OBS: Diem que necessitara perque no sabem quin programa assignarem al process i aquest en pot requerir
	set_user_pages(init_pcb);

	// Guardar kernel_esp
	// NOTA: No es necessari pq. se que inicialment esta buida.
	// Ho faig perque aixi te simetria amb "idle" 
	init_pcb->kernel_esp = (DWord)&init_pcb[KERNEL_STACK_SIZE-1];

	/*
	TEORIA
	------
	Un process te dues piles:
		- Mode usuari: Esta en USER_DATA
		- Mode sistema: La que hi ha en el union
	# No hi ha una pila de kernel com a tal. Aquesta no es res.
	Necessitem assignar els reg. que farem servir per automatitzar el trovar la pila de sistema del process
	Es a dir, on esta el bottom (top en realitat) "init_stack":
		- Assignar-ho a la TSS (esp0) 
		- Assignar-ho a MSR[0x175]

	"return_gate()" fica i treu (amb iret) els valors necessaris a la pila de sistema del process.
	Aquests valors son:
		- EIP: @usr_main (que esta en user.c)
		- PSW: ???
		- ESP: Es la @logica base de USER_DATA. Aquesta apunta a la pila de mode usuari (que fara servir "main" de user.c) 
	*/

	// Actualitzar TSS
	// NOTA: Inicialment la pila esta buida.
	// NOTA: Aixo tambe ho fa el context_switch, pero com que aquest cas "init" inicia forzadament.
	// OBS: Ho podem fer pq. (nosaltres com a programadors) sabem que sera el primer process en executar-se.
	tss.esp0 = (DWord)&init_stack[KERNEL_STACK_SIZE];

	// Actualitzar MSR[0x175] (System Stack)
	writeMSR(0x175, (unsigned int)&init_stack[KERNEL_STACK_SIZE]);

	// Finalment assignem a cr3 la @T.D que fara servir "init".
	set_cr3(get_DIR(init_pcb));

	// RR
	init_pcb->quantum = DEFAULT_QUANTUM;

	// Hierarchy
	init_pcb->father = init_pcb;  // Es ell mateix
	INIT_LIST_HEAD(&init_pcb->child_list);  // Inicialment empty
	INIT_LIST_HEAD(&init_pcb->child_node);  // No esta en cap llista 

	/*
	OBSERVACIO
	----------
	No l'assignem a ready_queue pq. no farem servir el "context_switch" per iniciar-lo.
	Ja hem forçat tot el necessari perque sigui el process a executar-se (TSS, MSR, CR3)
	*/
}

void init_sched()
{
	// 1. Inicialitzar les queue
	// Head de la llista
	INIT_LIST_HEAD(&freequeue);

	// Inicialitzar freequeue
	for (int i = 0; i < NR_TASKS; i++)
	{
		list_add_tail(&task[i].task.list, &freequeue);
	}

	// Inicialitzar readyqueue (Nomes el HEAD pq. de moment no tenim cap process)
	INIT_LIST_HEAD(&readyqueue);
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void inner_task_switch(union task_union* new_union)
{
	// Inicialitzar 
	struct task_struct *new_pcb = &(new_union->task);

	// Canviar de contexte la memoria
	// IMPO: Ho podem fer aqui perque els unions estan en KERNEL.
	// Com que el kernel esta mapped en totes les entrades (0-255) igual, no afecta dins aquesta funcio.
	set_cr3(get_DIR(new_pcb));

	// Actualitzem en la TSS per si entrem amb int
	tss.esp0 = (DWord)&new_union->stack[KERNEL_STACK_SIZE];
	
	// Actualitzar el MSR[0x175] donat que fem servir sysenter 
	// OBS: Nota que si no ho fiquessim i entres una interrupcio, no ho guardariem en la pila que toca.
	writeMSR(0x175, (unsigned int)&new_union->stack[KERNEL_STACK_SIZE]);

	// Acabar de fer el task_switch de vertitat
	// NOTA: Veure implementacio de "end_task_switch" per a mes info.
	end_task_switch(&current()->kernel_esp, new_pcb->kernel_esp);
}

void task_switch(union task_union* new)
{
	// Guardar REG  {EDI, ESI, EBX}
	__asm__(
		"push %edi; \n"
		"push %esi; \n"
		"push %ebx; \n"
	);

	// Saltar al task_switch "de veritat"
	inner_task_switch(new);

	// Recuperar els REG
	__asm__(
		"pop %ebx; \n"
		"pop %esi; \n"
		"pop %edi; \n"
	);
}

void sched_next_rr(void)
{
	union task_union *pnext_process_union = (union task_union *)&idle_task;  // Default IDLE

	if (!list_empty(&readyqueue))
	{
		struct list_head *pnext_process;
		
		// Get the top process
		pnext_process = list_first(&readyqueue);
		list_del(pnext_process);
		pnext_process_union = list_head_to_task_union(pnext_process);
	}

	actual_quantum = get_quantum((struct task_struct *)pnext_process_union);
	task_switch(pnext_process_union);
}

void update_process_state_rr(struct task_struct *t, struct list_head *dest)
{
	// === Base Case (if null --> dont change) 
	if (dest == 0)
		return;

	// === General Case
	list_add_tail(&(t->list), dest);
}

int needs_sched_rr(void)
{
	if (actual_quantum <= 0)
		return 1;

	return 0;
}

void update_sched_data_rr(void)
{
	actual_quantum--;
}

void scheduler(void)
{
	int idle_is_current = (current()->PID == 0);
	int must_change = 1;
	
	// 1. Actualitzar estat sigui quin sigui
	update_sched_data_rr();

	// 2. Si IDLE no es current, mirar si a l'actual se li ha acabat el quantum
	if (!idle_is_current)
		must_change = needs_sched_rr();

    // 3.1. Si no hi ha disponibles --> Continuar igual
    if (list_empty(&readyqueue))
		return;
    
    // 3.2. Si a l'actual no se li ha acabat quantum --> Continuar igual
    if (!must_change)
		return;
	
    // 4. S'ha de fer canvi
    // IMPO: IDLE no va a readyqueue
    if (!idle_is_current)
		update_process_state_rr(current(), &readyqueue);

	sched_next_rr();
}

int get_quantum(struct task_struct *t)
{
	return t->quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
	t->quantum = new_quantum;
}