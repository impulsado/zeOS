#######################
## MILESTONE 1
#######################

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





#######################
## MILESTONE 2
#######################

IDEA
----
"Volem que quan l'usuari premi una tecla --> S'executi una funcio."
El workflow que volem es algo aixi: "Usuari_main --> Sistema --> (Usuari_func) --> Sistema --> Usuari_main"
Problemes principals:
- PROBLEMA #1: Podriem crear recursivitat si dins "Usuari_func" arriba una altra lletra.
- PROBLEMA #2: Com guardo la funcio d'usuari i l'executo perque surti correctament.
- PROBLEMA #3: La pila d'usuari conviuen variables de "main" i les de "func".
- PROBLEMA #4: Hi ha dos contextes d'usuari: Usuari_main, Usuari_func.

struct task_struct
------------------
S'afegeixen els camps:
- in_keyboard_event per solucionar "PROBLEMA #1"
- keyboard_handler per solucionar "PROBLEMA 2,3,4"
- keyboard_wrapper per solucionar "PROBLEMA #2"
- saved_ctx[5+11] per solucionar "PROBLEMA #4"

La saved_ctx es un nou struct de "struct keyboard_context" on simplement guardem els contextes:
- Hardware (5 REGs)
- Software (11 REGs)
No fa falta guardar res mes perque no ho necessitem (valors auxiliars de funcions de sistema es poden perdre)

struct tls_block
----------------
S'afegeixen els camps:
- char keyboard_aux_stack[KEYBOARD_AUX_STACK_SIZE] per solucionar "PROBLEMA #4"

KEYBOARD_AUX_STACK_SIZE = 256 pel seguent motiu:
- keyboard_handler: key + pressed + handler + off = 16B
- keyboard_wrapper: key + pressed + %ebp = 12B
- user_func: @ret + [...] = 4B + xB
TOTAL = 32B + xB --> Per seguretat fiquem 256B i tindrem suficient (probablement)

WORKFLOW
--------
1. Usuari registra funcio a executar quan keyboard event
2. Quan hi ha una tecla salta el handler de teclat i:
- Guarda el ctx usuari en PCB
- Crea la pila auxiliar per user_func
- Marcar que dins tractament
3. Saltem a keyboard_wrapper (no directament a user_func)
- Preparar pila auxiliar 
- Saltar a user_func
- Exectuar "int 0x2b"
4. Saltem al handler de la syscall que ens preparara per tornar a usuari original
5. Saltem a la rutina de la syscall que:
- Restaura el ctx original
- Marca que fora tractament
6. Saltem on deuriem abans d'haver rebut la tecla

IMPLEMENTACIO
-------------
Determinem que nomes hi ha un Wrapper perque sigui mes facil el pas de parametres i no haguem de pensar en casuistiques
Determinem que la pila d'usuari estara en TLS i no aprofitant la pila actual d'usuari perque aixi la tornada sigui mes facil de gestionar.
    (Exemple: Saber que tindrem sempre l'espai disponible)

KeyboardEvent
-------------
Registra en la struct del thread la funcio que li ha passat l'usuari.
El handler .S tambe guarda la @wrapper perque sys_KeyboardEvent la guardi.
Aixo ho fem replicant el comportament de ThreadCreate

