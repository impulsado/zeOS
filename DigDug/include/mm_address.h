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
#define THREAD_STACK_SLOT_INIT_PAGE(slot) (THREAD_STACK_REGION_FIRST_PAGE + ((slot) * THREAD_STACK_SLOT_PAGES))     /* Delimitar inici del slot */
#define THREAD_STACK_SLOT_LIMIT_PAGE(slot) (THREAD_STACK_SLOT_INIT_PAGE(slot) + THREAD_STACK_SLOT_PAGES - 1)        /* Delimitar tope del slot (limit) */

#define PAG_LOG_INIT_FREE (THREAD_STACK_SLOT_LIMIT_PAGE(THREAD_MAX_STACK_SLOTS))

/* Memory distribution */
/***********************/

#define KERNEL_START 0x10000
#define L_USER_START 0x100000
#define PH_USER_START 0x100000
#define USER_ESP L_USER_START + (NUM_PAG_CODE + NUM_PAG_DATA) * 0x1000 - 16

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
    |   DATA (Thread 0)     |
    |-----------------------| <-- THREAD_STACK_SLOT_INIT_PAGE(0)
    |                       |
    |   Slot 0 (Thread 1)   |
    |                       |
    |-----------------------| <-- THREAD_STACK_SLOT_LIMIT_PAGE(0)
    |          ...          |
    |-----------------------|
    |       Slot MAX        |
    |-----------------------| <-- PAG_LOG_INIT_FREE
    |          ...          |
    |-----------------------|


    Thread0 continua fent servir les pagines de DATA (Legacy).
    Per a un nou thread --> Agafar un nou slot.
    Un slot son 8 pagines de les quals inicialment nomes esta mapejada la base (primera).
    Quan hi ha un page_fault, es mira si esta dins el rang del slot que li pertoca:
        - OK: Assignar un nou frame fisic i recuperar-se
        - KO: Page fault.

 */

#endif
