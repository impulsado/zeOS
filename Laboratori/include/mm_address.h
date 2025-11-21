#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define ENTRY_DIR_PAGES       0

#define TOTAL_PAGES 1024
#define NUM_PAG_KERNEL 256
#define PAG_LOG_INIT_CODE (PAG_LOG_INIT_DATA+NUM_PAG_DATA)
#define FRAME_INIT_CODE (PH_USER_START>>12)
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (L_USER_START>>12)
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000

/* Memory distribution */
/***********************/


#define KERNEL_START     0x10000
#define L_USER_START     0x100000
#define USER_ESP	L_USER_START+(NUM_PAG_DATA)*PAGE_SIZE-16

#define USER_FIRST_PAGE	(L_USER_START>>12)

#define PH_PAGE(x) (x>>12)

#endif

/*
    TEORIA
    ------

    Resoldre el dubte de si jo tinc fins 0x01000000 aquesta direccio fent el tall de 10 | 10 | 12
    ja correspon a una entrada del page directory que no tinc, no?


    1 Pagina = 4 kB = 2^12 B
    1024 pagines = 2^10 pagines

    |-------------------| 0x00000000
    |       FREE        | 
    |-------------------| 0x00010000
    |       KERNEL      |
    |-------------------| 0x00100000 <== (0x00010000) + 256pag * 4kB/pag
    |     USER_DATA     |
    |-------------------| 0x00114000 <== (0x00100000) + 20pag * 4kB/pag
    |     USER_CODE     |
    |-------------------| 0x0011C000 <== (0x00114000) + 8pag * 4kB/pag
    |       FREE        |
    |-------------------| 0x01000000 <== 1024pag * 4kB/pag
    |   NO ACCESSIBLE   |
    |-------------------| 0xFFFFFFFC

*/