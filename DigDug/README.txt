EXPLICACIO
-----------

|-----------------------| <-- 0
|       KERNEL          |    
|-----------------------| <-- PAG_LOG_INIT_CODE
|       CODE            |
|-----------------------| <-- PAG_LOG_INIT_DATA
|       DATA            |
|-----------------------| <-- THREAD_STACK_SLOT_LIMIT_PAGE(0)
|       page 7          |
| - - - - - - - - - - - |
|                       |
|   Slot 0 (Thread 1)   |
|                       |
| - - - - - - - - - - - | <-- THREAD_STACK_SLOT_INIT_PAGE(0)
|       page 0          |
|                       |
|  .  .  .  .  .  .  .  | <-- THREAD_TLS_VADDR(0)
|           tls         |
|-----------------------|
|          ...          |
|-----------------------| <-- THREAD_STACK_SLOT_LIMIT_PAGE(THREAD_MAX_STACK_SLOTS)
| Slot MAX (Thread MAX) |
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
Cada thread te a la direccio mes alta una regio reservada per a la TLS.

- thread_group
    Estructura compartida entre tots els threads del proces que conté la llista de membres i el slot_mask global.
    El slot_mask indica amb el bit i-essim si el slot i-essim esta ocupat.

- tls.h
    Nova estructura TLS en espai d'usuari que te cada thread.
    Actualment nomes hi ha errno.
    Fem els trucs de saber com s'esta disenyant la pila per trobar l'ubicacio dins d'aquesta de TLS.
    Sempre es mappeiga en la direccio mes alta de la pila (la base).
    // NOTA: Aixo fa que no ens haguem de preocupar en si un overflow superior pogues sobreescriure.

page_fault
----------
Els page faults dins del rang del slot fan que s'assgini un frame fins a omplir les 8 pagines.
Si no es pot assignar mes pagines al slot o es una direccio invalida --> while (1).

sys_fork
--------
El fork copia sempre totes les 8 pagines del slot del pare perque el fill ha de tindre la mateixa pila.
Donat que ara no fem us de DATA com a tal, nomes copiem les pagines d'aquesta regio que estiguin usades.
// OBS: Fill tindra logicament el mateix slot que pare pero en espai fisic diferent.

sys_exit
--------
Qualsevol thread que invoqui sys_exit elimina tots els threads del proccess.
Allibera els slots assignats i retorna tots els TCB al freequeue.
Elimina el grup que estaven fent servir els threads

ThreadExit
----------
Allibera el slot assignat. 
Si era l'ultim thread del proces o l'unic del grup, es fa un sys_exit per eliminar el proccess complet.

ThreadCreate
------------
Primerament replica el contingut del thread creador.
Despres modifiquem els valors especifics del nou thread.
Creem la nova pila d'usuari (anterior en @) a TLS.
Modifiquem la pila de sistema per a saltar a ThreadWrapper