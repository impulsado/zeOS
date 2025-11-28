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
    
    - thread_group
        Estructura compartida entre tots els threads del proces que conté la llista
        de membres i el `slot_mask` global.
        El `slot_mask` indica amb el bit i-essim si el slot i-essim esta ocupat.


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
    Qualsevol thread que invoqui sys_exit elimina tots els threads del proccess.
    Allibera els slots assignats i retorna tots els TCB al freequeue.

    ThreadExit
    ----------
    Allibera el slot assignat. 
    Si era l'ultim thread del proces, es fa un sys_exit per eliminar el proccess complet.

    ThreadCreate
    ------------
    Agafa slot, assigna recursos i executa el ThreadWrapper