/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>
#include <utils.h>
#include <io.h>
#include <mm.h>
#include <mm_address.h>
#include <sched.h>
#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

extern unsigned int zeos_ticks;
extern struct list_head blocked;

// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -EACCES; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -ENOSYS; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int ret_from_fork(void)
{
	return 0;
}

int sys_fork()
{
	// 0. Variables
	struct task_struct *father_task;
	struct task_struct *child_task;
	union task_union *father_union;
	union task_union *child_union;
	int free_frames[NUM_PAG_DATA];
	page_table_entry *father_PT;
	page_table_entry *child_PT;

	// 1. Obtindre union lliure per a fill
	if (list_empty(&freequeue))
		return -EAGAIN;

	struct list_head *pchild = list_first(&freequeue);
	list_del(pchild);

	father_task = current();
	father_union = (union task_union *)current();
	child_union = list_head_to_task_union(pchild);
	child_task = list_head_to_task_struct(pchild);

	// 2. Copiar tot union identic de pare --> fill
	// NOTA: El size sera la mida del stack pero ja esta be perque totes dues coses resideixen en mateixa memoria
	copy_data(father_union, child_union, sizeof(union task_union));

	// 3. Assignar Page Directory al fill (Nivell de desreferencia extra per a la TP)
	// IMPO: Recorda que hi ha relacio 1:1:1 entre task[i] <--> dir_pages[i] <--> paguser_table[i]  (En ZeOS)
	allocate_DIR(child_task);

	// 4. Buscar NUM_PAG_DATA frames lliures per al fill
	// NOTA: KERNEL+CODE apuntaran a mateixos frames fisics per optimitzar/necessitat.
	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		free_frames[i] = alloc_frame();
		if (free_frames[i] != -1)
			continue;

		// 4.1. Hi ha hagut error --> Fer rollback
		// Alliberar frames
		for (int j = 0; j < i; j++)
		{
			free_frame(free_frames[j]);
		}

		// Retornar PCB
		list_add_tail(pchild, &freequeue);

		return -ENOMEM;
	}

	/*
		IMPORTANT
		---------
		Tot i que el fill sigui una copia del pare, la nova P.D./P.T. es completament invalida (No sabem que tenia abans)
		Si que veiem que modificarem les entrades de DATA+STACK, pero les altres entrades han d'estar actualitzades tambe.
		Aixo fa que haguem de forçar al fill que apunti a mateixos frames de memoria que el pare en cas de KERNEL+CODE

		Del contrari, al retornar a mode usuari estarem usant unes entrades (en el rang de PAG_LOG_INIT_CODE) que no sabem on apunten.
		NOTA: Entrar al task_switch (i totes les funcions abans del sched()) no petaran.
		Aixo es perque al iniciar ZeOS hem mapejat a TOTES les P.T. les entrades corresponents a KERNEL.
		Una optimitzacio d'aquesta funcio es treure el bucle de KERNEL perque no fa falta.
	*/

	father_PT = get_PT(father_task);
	child_PT = get_PT(child_task);

	/*
		RECORDA
		-------
		Dins de "init_table_pages()" ja fem mapping de les entrades kernel a totes les T.P.
		No fa falta fer-ho ara

		for (int i = 0; i < NUM_PAG_KERNEL; i++)
		{
			set_ss_pag(child_PT, i, father_PT[i].bits.pbase_addr);
		}
	*/

	// Copiar les entrades de CODE de pare --> fill
	for (int i = 0; i < NUM_PAG_CODE; i++)
	{
		set_ss_pag(child_PT, PAG_LOG_INIT_CODE+i, 
					father_PT[PAG_LOG_INIT_CODE+i].bits.pbase_addr);
	}

	// 5. Assignar al fill aquesta nous frames obtinguts perque els faci servir a la seva regio de DATA+STACK
	// NOTA: Aixo implicara sobreescriure les entrades de la TP corresponents a DATA+STACK (perque ara apunten a frames fisics de pare)
	// OBS: En free_frames[i] guardem els id. dels frames fisics. 
	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		set_ss_pag(child_PT, PAG_LOG_INIT_DATA+i, free_frames[i]);
	}

	// 6. Mapejar frames DATA+STACK de pare --> fill
	/*
		TEORIA
		------
		Pare i fill tenen P.D. (i P.T.) diferents.
		Nomes tenim 1 cr3 per apuntar a un P.D.
		Aixo implica que NO podem fer canvi "child_PT[i] = father_PT[i]".

		El que farem sera:
		1. En la T.P (en entrades lliures) del pare assignarem els frames de DATA+STACK del fill (els free_frames anteriors).
		2. Des del pare copiarem el seu DATA als frames DATA del fill (ara fill ja tindra copia)
		3. Treiem les entrades temporals al pare de la seva T.P.

		Finalment, en TLB (que esta fent us el pare) hi haura cachejades les entrades a la mem. del fill i el pare no deuria.
		L'unica forma d'eliminar-ho es fent flush de la TLB --> modificar cr3.

		
		OBSERVACIO
		----------
		Com que som programadors sabem que a partir de CODE --> Tot lliure.

		Entrades de la T.P. (NO ES ESQUEMA DE MEMORIA)
		|---------------| 0x000 [0]
		| 	KERNEL 		|
		|---------------| 0x100 [NUM_PAG_KERNEL]
		|	DATA+STACK  |
		|---------------| 0x114 [NUM_PAG_KERNEL + NUM_PAG_DATA]
		| 	CODE		| 
		|---------------| 0x11C [NUM_PAG_KERNEL + NUM_PAG_DATA + NUM_PAG_CODE]
		| 	FREE		|
		|---------------| 0x400

		RECORDA: Tot i que estem iterant la part de DATA+STACK [0] --> [NUM_PAG_DATA], en realitat [NUM_PAG_DATA] es el bottom.
	*/

	int PAG_LOG_INIT_FREE = PAG_LOG_INIT_CODE + NUM_PAG_CODE;

	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		set_ss_pag(father_PT, PAG_LOG_INIT_FREE+i, free_frames[i]);
	}

	/*
		IMPORTANT
		---------
		Idea del que volem fer: "Guardar el que hi hagi en el frame fisic PT[i] --> PT[j]" 
		Pero donat que PT treballa amb direccions fisiques, no podem fer-ho tal qual.
		La solucio esta en forçar que hi hagi la traduccio corresponent per accedir a l'entrada 'i' i 'j'.
		Aixo ho aconseguim ficant en els bits corresponents de "TABLE" el 'i' i 'j'.
		
		copy_data() treballa amb punters a direccions de memoria logica.
		
		31			21		11		 0
		|-----------|-------|--------|
		| DIRECTORY | TABLE | OFFSET |
		|----------------------------|

		Amb aquesta direccio la MMU agafa els bits corresponents per accedir a les estructures i fer la traduccio.
		La traduccio com a tal la fa agafant la pbase_addr de la entrada TABLE[i] i fer "pbase_addr<< 12 | offset".
		Simplement fa falta ficar entre els bits 21-11 el num. d'entrada de la T.P que s'ha de fer servir
		
		NOTA
		----
		Sabem que "DIRECTORY = 0" SEMPRE perque nomes fem servir la primera entrada (la 0)
		Sabem que offset 0 perque estem copiant desde l'inici.
	*/

	int dir_data_start = 0 | (PAG_LOG_INIT_DATA<<12) | 0;
	int dir_free_start = 0 | (PAG_LOG_INIT_FREE<<12) | 0;

	copy_data((void *)dir_data_start, (void *)dir_free_start, PAGE_SIZE*NUM_PAG_DATA);

	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		del_ss_pag(father_PT, PAG_LOG_INIT_FREE+i);
	}

	// Reiniciar la TLB
	// IMPO: NO volem saltar a fill, aixi que volem que cr3 continu apuntant al pare
	set_cr3(get_DIR(father_task));

	// 7. Gestionem al fill
	// IMPO: Si no fiquem 1000 pot colisionar amb el pid de init (1)
	// 7.1. PID
	child_task->PID = 1000 + zeos_ticks;  // Pot ser un pseudoaleatori
	
	// 7.2. RR
	child_task->quantum = DEFAULT_QUANTUM;

	// 7.3. Hierarchy
	child_task->father = father_task;
	INIT_LIST_HEAD(&child_task->list);
    INIT_LIST_HEAD(&child_task->child_node);
    INIT_LIST_HEAD(&child_task->child_list);

	// 7.4. Sempahore
	// PREGUNTAR si s'hereden blocks
	child_task->state = ST_READY;

	// 8. Actualitzem reg. necessaris
	// No se a que es refereix el document.

	// 9. Modifiquem la pila del fill per quan retorni.

	/*
		TEORIA
		------
		Com que fill sera iniciat pel scheduler --> top del stack ha de tindre "ebp" i "ret".
		El "ebp" no importa perque voldrem sortir de mode sistema i en el wrapper del fork tenim el ebp de mode usuari.
		// Recorda que fill continua executant just on ho ha deixat el pare.
		El "ret" no volem que sigui automaticament el del handler perque volem retornar 0 (donat que fork() desde fill retorna 0)
		Finalment haurem d'actualitzar el kernel_esp del fill perque apunti a la direccio correcta (el task_switch ho necessita)
		
		OBSERVACIO
		----------
		El pare (que ha copiat tot identicament al fill) podria ser que tingues mes coses a la pila o inclus altres.
		El que sabem del cert (i ens interesa) es el que hi ha a l'esquema.
		Com que el fill te el kernel_esp apuntant on li hem dit, totes les coses "superiors" de la pila estan "brutes" i ni interessen.

		|			%ebp_trash		|
		|		@ret_from_fork		|  Aqui %ebp de sys_fork ???
		|	@ret_sysenter_handler	|  (1)
		| 			Ctx. SW 		|  (11)
		|			Ctx. HW			|  (5)
		|---------------------------|
	*/ 

	child_union->stack[(KERNEL_STACK_SIZE-1) - (5+11+1)] = (DWord)ret_from_fork;
	child_union->stack[(KERNEL_STACK_SIZE-1) - (5+11+1+1)] = 0;
	child_task->kernel_esp = (DWord)&(child_union->stack[(KERNEL_STACK_SIZE-1) - (5+11+1+1)]);

	// 10. Fiquem al fill en readyqueue perque el scheduler ja el comenci a tindre en compte
	// NOTA: Recorda que scheduler estem fent FIFO --> Afegir al final
	list_add_tail(pchild, &readyqueue);

	// 11. Assignem a pare el nou fill
	list_add_tail(&child_task->child_node, &father_task->child_list);

	// 12. Retornem PID del fill
	return child_task->PID;
}

void sys_exit(void)
{
	// 1. Alliberar frames (que no siguin KERNEL) del proces
	// IMPORTANT: NO alliberar code perque potser el pare el continua necessitant (esta compartit)
    
	// PREGUNTAR: Aqui tambe haig d'alliberar tot el posterior de CODE 
	page_table_entry *PT = get_PT(current());

	for (unsigned int i = 0; i < NUM_PAG_DATA; i++)
	{
		free_frame(get_frame(PT, PAG_LOG_INIT_DATA + i));
	}

	// 2. Gestionar jerarquia = Assignar nou pare als fills
	struct list_head *plist_child;
	struct list_head *aux;  // Per a list_for_each_safe
	struct list_head *pcurrent_childs_list = &(current()->child_list);
	
	if (!list_empty(pcurrent_childs_list))
	{
		struct task_struct *pchild_task;
		struct task_struct *pnew_father;

		// Determinar qui es el nou pare
		if (current()->father == current())
			pnew_father = &idle_task;
		else
			pnew_father = current()->father;

		// Canviar de pare als fills de la generacio mes proxima
		// IMPO: Usar list_for_each_safe perque estem modificant la llista
		list_for_each_safe(plist_child, aux, pcurrent_childs_list) 
		{
			// Obtenir el task_struct del fill
			// IMPO: Obtenir des del node corresponent a la llista de fills
			pchild_task = list_entry(plist_child, struct task_struct, child_node);

			// 1. Treure el fill de la llista del current
			list_del(plist_child);

			// 2. Assignar el nou pare al fill
			pchild_task->father = pnew_father;

			// 3. Afegir el fill a la llista de fills del nou pare
			list_add_tail(plist_child, &(pnew_father->child_list));
		}
	}

	// 3. Treure'ns de la child_list del nostre pare
    if (current()->father != current())
		list_del(&current()->child_node);

	// 4. Encolar PCB de current (que es el que s'esta fent exit) a freequeue
	list_add_tail(&(current()->list), &freequeue);

	// Cridar al scheduler
	// NOTA: Aixi forcem assignar un nou process automaticament
	sched_next_rr();
}

int sys_write(int fd, char *buffer, int size)
{
	// NOTA: Revisar "man 2 write" per saber els ERRNO
	int ret;

	// Base Case
	ret = check_fd(fd, ESCRIPTURA);
	if (ret != 0)
		return ret;

	if (buffer == NULL)  // No tenim null
		return -EFAULT;

	if (size < 0)
		return -EINVAL;
	
	if (size == 0)
		return 0;

	// General case
	const int CHUNK = 256;  // valor trivial
	int total = 0;
	char k_buffer[CHUNK];
	int remaining;
	int temp_size;

	while (total < size)
	{
		remaining = size - total;
		temp_size = (remaining > CHUNK) ? CHUNK : remaining;
		
		ret = copy_from_user(buffer + total, k_buffer, temp_size);
		if (ret != 0)
			return -EFAULT;

		ret = sys_write_console(k_buffer, temp_size);
		// sempre retorna temp_size

		total += temp_size;
	}
	

	return total;
}

int sys_gettime(void)
{
	return zeos_ticks;
}

void sys_block(void)
{
	//=== Base Case
	if (current()->PID == 0)
		return;

	if (current()->pending_unblocks > 0)
	{
		current()->pending_unblocks--;
		return;
	}

	//=== General Case 
	struct list_head *pcurrent_list = &current()->list; 
	list_del(pcurrent_list);  // Remove from readyqueue
	current()->state = ST_BLOCKED;
	list_add_tail(pcurrent_list, &blocked);  // Add to blockedqueue
	sched_next_rr();  // Per anar mes rapids
}

int sys_unblock(int pid)
{
	//=== Base Case
	// 1. Comprovar que el pid sigui fill de current
	struct task_struct *pchild_task = 0;
	struct list_head *aux_list;
	struct task_struct *aux_task;

	list_for_each(aux_list, &current()->child_list)
	{
        aux_task = list_entry(aux_list, struct task_struct, child_node);
		if (aux_task->PID == pid)
		{
			pchild_task = aux_task;
			break;
		}
	}

	if (pchild_task == 0)
		return -1;

	//=== General Case
	// 2. Gestionar accio amb el fill
	// 2.1. Si no esta blocked --> "No fer res"
	if (pchild_task->state != ST_BLOCKED)
	{
		pchild_task->pending_unblocks++;
		return 0;
	}

	// 2.2. Esta blocked --> Volem unblock
	list_del(&(pchild_task->list));  // Treure de blockedqueue
	pchild_task->state = ST_READY;
	list_add_tail(&(pchild_task->list), &readyqueue);  //  Afegir de blockedqueue
	
	return 0;
}