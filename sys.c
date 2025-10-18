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
	// TODO: Podriem optimitzar aquesta funcio per fer rollback en cas de fallada de algo

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
	// NOTA: KERNEL i CODE apuntaran a mateixos frames fisics per optimitzar/necessitat.
	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		free_frames[i] = alloc_frame();
		if (free_frames[i] == -1)
			return -ENOMEM;
	}

	// 5. Assignar al fill aquesta nous frames obtinguts perque els faci servir a la seva regio de DATA+STACK
	// NOTA: Aixo implicara sobreescriure les entrades de la TP corresponents a DATA+STACK (perque ara apunten a pare)
	// NOTA: Faig servir PAG_LOG_INIT_DATA pq. replico "set_user_pages()".
	// OBS: En free_frames[i] guardem els id. dels frames fisics. Aixo es perque @mem_fisica final la genera MMU:  @(P.D. | P.T. | off) = 32b
	child_PT = get_PT(child_task);

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

		El farem sera:
		1. En la T.P (en entrades lliures) del pare assignarem els frames de DATA+STACK del fill.
		2. Des del pare copiarem el seu DATA als frames DATA del fill (ara fill ja tindra copia)
		3. Treiem les entrades temporals al pare

		Finalment, en TLB hi haura cachejades les entrades a la mem. del fill i el pare no deuria.
		L'unica forma que hi ha es fent flush de la TLB --> modificar cr3.

		
		OBSERVACIO
		----------
		Com que som programadors sabem que a partir de CODE --> Tot lliure.

		Entrades de la T.P. (NO ES ESQUEMA DE MEMORIA)
		|---------------| 0x000 [0]
		| 	KERNEL 		|
		|---------------| 0x100 [256]
		|	DATA+STACK  |
		|---------------| 0x114 [256 + 20]
		| 	CODE		| 
		|---------------| 0x11C [256 + 20 + 8]
		| 	FREE		|
		|---------------| 0x400

		RECORDA: Tot i que estem iterant la part de DATA+STACK [0] --> [NUM_PAG_DATA], en realitat [NUM_PAG_DATA] es el bottom.
	*/

	father_PT = get_PT(father_task);
	int PAG_LOG_INIT_FREE = PAG_LOG_INIT_CODE + 8;

	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		set_ss_pag(father_PT, PAG_LOG_INIT_FREE+i, free_frames[i]);
	}

	/*
		IMPORTANT
		---------
		copy_data() treballa amb punters a direccions de memoria.
		No tenim un punter a direccio de memoria --> L'haurem de crear
		
		31			21		11		 0
		|-----------|-------|--------|
		| DIRECTORY | TABLE | OFFSET |
		|----------------------------|

		Amb aquesta direccio la MMU agafa els bits corresponents per accedir a les estructures i fer la traduccio.
		La traduccio com a tal la fa agafant la pbase_addr de la entrada TABLE[i].
		Sabem que "DIRECTORY = 0" SEMPRE perque nomes fem servir la primera entrada (la 0)
		Sabem que offset 0 perque estem copiant desde l'inici.
		Simplement fa falta ficar entre els bits 21-11 el num. d'entrada de la T.P que ha de fer servir
	*/
	int dir_data_start = 0 | (PAG_LOG_INIT_DATA<<12) | 0;
	int dir_free_start = 0 | (PAG_LOG_INIT_FREE<<12) | 0;

	copy_data((void *)dir_data_start, (void *)dir_free_start, PAGE_SIZE*NUM_PAG_DATA);

	for (int i = 0; i < NUM_PAG_DATA; i++)
	{
		del_ss_pag(father_PT, PAG_LOG_INIT_FREE+i);
	}

	set_cr3(get_DIR(child_task));

	// 7. Assignem un PID al fill
	child_task->PID = zeos_ticks;  // Pot ser un pseudoaleatori

	// 8. Actualitzem reg. necessaris
	// No se a que es refereix el document.

	// 9. Modifiquem la pila del fill per quan retorni.

	/*
		TEORIA
		------
		Com que fill sera iniciat pel scheduler --> top del stack ha de tindre "ebp" i una "ret".
		El "ebp" no importa perque voldrem sortir de mode sistema i en el wrapper del fork tenim el ebp de mode usuari.
		El "ret" no volem que sigui automaticament el del handler perque volem retornar 0 (donat que fork() desde fill retorna 0)
		Finalment haurem d'actualitzar el kernel_esp del fill perque apunti a la direccio correcta (el task_switch ho necessita)
		
		|			%ebp_trash		|
		|		@ret_from_fork		|
		|	@ret_sysenter_handler	| (1)
		| 			Ctx. SW 		| (11)
		|			Ctx. HW			| (5)
		|---------------------------|
	*/ 

	child_union->stack[KERNEL_STACK_SIZE - (5+11+1)] = (DWord)ret_from_fork;
	child_union->stack[KERNEL_STACK_SIZE - (5+11+1+1)] = 0;
	child_task->kernel_esp = (DWord)&(child_union->stack[KERNEL_STACK_SIZE - (5+11+1+1)]);

	// 10. Fiquem al fill en readyqueue perque el scheduler ja el comenci a tindre en compte
	// NOTA: Recorda que scheduler estem fent FIFO --> Afegir al final
	list_add_tail(&readyqueue, pchild);

	// 11. Retornem PID del fill
	return child_task->PID;
}

void sys_exit()
{  
}

int sys_write(int fd, char *buffer, int size)
{
	// NOTA: Revisar "man 2 write" per saber els ERRNO
	int ret;

	// Base Case
	ret = check_fd(fd, ESCRIPTURA);
	if (ret != 0)
		return ret;

	if (buffer == 0)  // No tenim null
		return -EFAULT;

	if (size < 0)
		return -EINVAL;  

	// General case
	const int CHUNK = 256;  // valor trivial
	int total = 0;
	char k_buffer[CHUNK];
	int remaining;
	int temp_size;

	while (total < size)
	{
		remaining = size - total;
		temp_size = (size > remaining) ? CHUNK : remaining;
		
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