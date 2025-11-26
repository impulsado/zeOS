#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define ENTRY_DIR_PAGES 0

#define TOTAL_PAGES 1024
#define NUM_PAG_KERNEL 256
#define PAG_LOG_INIT_CODE (L_USER_START >> 12)
#define FRAME_INIT_CODE (PH_USER_START >> 12)
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (PAG_LOG_INIT_CODE + NUM_PAG_CODE)
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000

#define THREAD_STACK_SLOT_PAGES 8           /* Cada thread nou rep un bloc de 8 pagines */
#define THREAD_MAX_STACK_SLOTS NR_TASKS - 2 /* Nombre de slots que podrem assignar (treure idle que no te threads i el init (thread0))*/

#define THREAD_STACK_REGION_FIRST_PAGE (PAG_LOG_INIT_DATA + NUM_PAG_DATA)                                           /* Delimitar on comencen els slots*/
#define THREAD_STACK_SLOT_LIMIT_PAGE(slot) (THREAD_STACK_REGION_FIRST_PAGE + ((slot) * THREAD_STACK_SLOT_PAGES))     /* Delimitar tope del slot (limit) */
#define THREAD_STACK_SLOT_INIT_PAGE(slot) (THREAD_STACK_SLOT_LIMIT_PAGE(slot) + THREAD_STACK_SLOT_PAGES - 1)        /* Delimitar inici del slot */

#define PAG_LOG_INIT_FREE (THREAD_STACK_SLOT_LIMIT_PAGE(THREAD_MAX_STACK_SLOTS))

/* Memory distribution */
/***********************/

#define KERNEL_START 0x10000
#define L_USER_START 0x100000
#define PH_USER_START 0x100000
// IMPO: Com que ara INIT esta en el primer slot, hem d'apuntar a aquest nou desde l'inici.
#define USER_ESP L_USER_START + (NUM_PAG_CODE + NUM_PAG_DATA + THREAD_STACK_SLOT_PAGES) * 0x1000 - 16

#define USER_FIRST_PAGE (L_USER_START >> 12)

#define PH_PAGE(x) (x >> 12)

/*
    DISTRIBUCIO MEM. PROCESS
    ------------------------

    |-----------------------| <-- 0
    |       KERNEL          |    
    |-----------------------| <-- PAG_LOG_INIT_CODE
    |       CODE            |
    |-----------------------| <-- PAG_LOG_INIT_DATA
    |       DATA            |
    |-----------------------| <-- THREAD_STACK_SLOT_LIMIT_PAGE(0)
    |       page 7          |
    | - - - - - - - - - - - |
    |   Slot 0 (Thread 1)   |
    | - - - - - - - - - - - | <-- THREAD_STACK_SLOT_INIT_PAGE(0)
    |       page 0          |
    |-----------------------|
    |          ...          |
    |-----------------------| <-- THREAD_STACK_SLOT_LIMIT_PAGE(THREAD_MAX_STACK_SLOTS)
    |       Slot MAX        |
    |-----------------------| <-- PAG_LOG_INIT_FREE
    |          ...          |
    |-----------------------|


    IDEA
    ----
    Fem servir task_struct per guardar la informacio dels threads (no crear una nova struct).
    Tots els threads (incloent el primer) utilitzen slots de 8 pagines per a la pila d'usuari.
    // NOTA: 8 es un valor trivial donat que es probable que 32kB siguin mes que necessaris.
    El segment DATA nomes te la primera pagina mapejada (Per tema legacy).
    Quan es crea un thread es reserva un slot complet i nomes la pagina de la base esta mapejada (inicialment).
    
    - initial_thread (o tambe master_thread)
        Punter al thread (TCB) que conte la informacio de gestio.
        Serverix per centralitzar la llista de threads d'un process.

    - slot_mask
        "unsigned int" que on el bit i-essim indica si el slot i-essim esta ocupat.
        Si "slot_mask[i] == 1" significa que el slot "i" esta ocupat per algun thread. 


    page_fault
    ----------
    Els page faults dins del rang del slot fan que s'assgini un frame fins a omplir les 8 pagines.
    Si no es pot assignar mes pagines al slot o es una direccio invalida --> while (1).
    
    sys_fork
    --------
    El fork copia sempre totes les 8 pagines del slot del pare perque el fill ha de tindre la mateixa pila.
    // OBS: Fill tindra logicament el mateix slot que pare pero en espai fisic diferent.

    sys_exit
    --------
    Si es el thread NO es l'inicial --> ThreadExit
    Altrament es va iterant els threads del process i es va fent ThreadExit de cadascun.
    Finalment es fa un sys_exit com sempre.

    ThreadExit
    ----------
    Allibera el slot assignat

    ThreadCreate
    ------------
    Agafa slot, assigna recursos i executa el ThreadWrapper
 */

#endif
