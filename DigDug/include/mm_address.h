#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#include <tls.h>

#define ENTRY_DIR_PAGES 0

#define TOTAL_PAGES 1024
#define NUM_PAG_KERNEL 256
#define PAG_LOG_INIT_CODE (L_USER_START >> 12)
#define FRAME_INIT_CODE (PH_USER_START >> 12)
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (PAG_LOG_INIT_CODE + NUM_PAG_CODE)
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000

#define THREAD_STACK_SLOT_PAGES 8 /* Cada thread nou rep un bloc de 8 pagines */
#define THREAD_STACK_SLOT_SIZE (THREAD_STACK_SLOT_PAGES * PAGE_SIZE)
#define THREAD_TLS_SIZE (sizeof(struct tls_block))
#define THREAD_MAX_STACK_SLOTS NR_TASKS - 2 /* Nombre de slots que podrem assignar (treure idle que no te threads i el init (thread0))*/

/* Slot reservat per al keyboard handler (fora del rang normal) */
#define KBD_SLOT THREAD_MAX_STACK_SLOTS

#define THREAD_STACK_REGION_FIRST_PAGE (PAG_LOG_INIT_DATA + NUM_PAG_DATA)                                        /* Delimitar on comencen els slots*/
#define THREAD_STACK_SLOT_LIMIT_PAGE(slot) (THREAD_STACK_REGION_FIRST_PAGE + ((slot) * THREAD_STACK_SLOT_PAGES)) /* Delimitar tope del slot (limit) */
#define THREAD_STACK_SLOT_INIT_PAGE(slot) (THREAD_STACK_SLOT_LIMIT_PAGE(slot) + THREAD_STACK_SLOT_PAGES - 1)     /* Delimitar inici del slot */
#define THREAD_STACK_SLOT_TOP_ADDR(slot) (((THREAD_STACK_SLOT_INIT_PAGE(slot) + 1) << 12))                       /* Dir immediata sobre el slot (limit del seguent slot) */
#define THREAD_TLS_VADDR(slot) (THREAD_STACK_SLOT_TOP_ADDR(slot) - THREAD_TLS_SIZE)

/* Primera pagina lliure despres de tots els slots (incloent el de keyboard) */
#define PAG_LOG_INIT_FREE (THREAD_STACK_SLOT_LIMIT_PAGE(KBD_SLOT + 1))

/* Memory distribution */
/***********************/

#define KERNEL_START 0x10000
#define L_USER_START 0x100000
#define PH_USER_START 0x100000
// IMPO: Com que ara INIT esta en el primer slot, hem d'apuntar a aquest nou desde l'inici.
#define USER_ESP L_USER_START + (NUM_PAG_CODE + NUM_PAG_DATA + THREAD_STACK_SLOT_PAGES) * 0x1000 - 16

#define USER_FIRST_PAGE (L_USER_START >> 12)

#define PH_PAGE(x) (x >> 12)

#endif
