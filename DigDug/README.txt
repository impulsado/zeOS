#######################
### MILESTONE 1
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
### MILESTONE 2
#######################

IDEA
----
"Volem que quan l'usuari premi una tecla --> S'executi una funcio."
Aquesta funcio esta relacionada amb el process i no exclusiu del thread.
El workflow que volem es algo aixi: "Usuari_main --> Sistema --> (Usuari_func) --> Sistema --> Usuari_main"
Problemes principals:
- PROBLEMA #1: Podriem crear recursivitat si dins "Usuari_func" arriba una altra lletra.
- PROBLEMA #2: Com guardo la funcio d'usuari i l'executo perque surti correctament.
- PROBLEMA #3: Necessitem una pila nova per a la funcio que s'executa.
- PROBLEMA #4: Hi ha dos contextes d'usuari: Usuari_main, Usuari_func.
- PROBLEMA #5: Ha de ser global al process.


NOVES ESTRUCTURES
-----------------

struct keyboard_info
--------------------
Idea similar al thread_group de la Milestone 1.
Estructura comuna a tots els threads del process on es guarda al funcio i el wrapper que hem d'usar.
D'aquesta forma aconseguim que sigui global a tots els processos.

struct task_struct
------------------
S'afegeixen els camps:
- in_keyboard_event per solucionar "PROBLEMA #1"
- *kbd_info per solucionar "PROBLEMA #5"
- saved_ctx[5+11] per solucionar "PROBLEMA #4"

La saved_ctx es un nou struct de "struct keyboard_context" on simplement guardem els contextes:
- Hardware (5 REGs)
- Software (11 REGs)
No fa falta guardar res mes perque no ho necessitem (valors auxiliars de funcions de sistema es poden perdre)
NOTA: Com que guardem informacio que no deuria de ser accessible desde mode usuari, la guardem en el union i aixi ja tenim seguretat.


WORKFLOW
--------
1. Usuari registra funcio a executar quan keyboard event

2. Quan hi ha una tecla salta el handler de teclat que:
2.1. Reservar un slot (com els implementats en Thread) temporal per a la funcio
2.2. Guarda el ctx usuari en PCB
2.3. Crea la pila auxiliar per user_func
2.4. Modificar el contexte hardware perque "torni" al wrapper en comptes de al codi principal de l'usuari 
2.5. Marcar que dins tractament

3. Saltem a keyboard_wrapper (no directament a user_func)
3.1. Preparar pila auxiliar
3.2. Saltar a user_func
3.3. Exectuar "int 0x2b"

4. Saltem al handler de la syscall que ens preparara per tornar a usuari original

5. Saltem a la rutina de la syscall que:
5.1. Alliberar el slot exclusiu de la funcio
5.2. Restaura el ctx original
5.3. Marca que ja estem fora tractament

6. Saltem on deuriem == abans d'haver rebut la tecla


IMPLEMENTACIO
-------------

init_task1
----------
Afegir struct d'info de teclat

sys_exit
--------
Eliminar la informacio relacionada amb el teclat i alliberar el struct

sys_fork
--------
Assignar un struct d'informacio de teclat al nou process fill
Inicialitzar els camps d'aquesta struct pertinents

sys_ThreadCreate
----------------
Iniciar que no estem en cap tractament de teclat (seguir patro de creacio)

sys_KeyboardEvent
-----------------
Inicialitzar els camps de la struct d'informacio de teclat:
- funcio
- Wrapper (Aquest es sempre el mateix per facilitar la implementacio del pas de param)
Soluciona "PROBLEMA #2"

keyboard_routine
----------------
(Explicat en WORKFLOW 2.*)
Soluciona "PROBLEMA #3, 4"

keyboard_return_routine
-----------------------
(Explicat en WORKFLOW 5.*)
Soluciona "PROBLEMA #3, 4"





#######################
### MILESTONE 3
#######################

IDEA
----
Tractar la pantalla com un buffer.
Per no haver de fer multiples crides a la pantalla i pecar de llarga latencia, creem un vector que simula la pantalla.
Quan tinguem el resultat final, enviem el vector temporal al write perque aquest ho pinti per pantalla.


IMPLEMENTACIO
-------------

sys_write
---------
Afegir la implementacio que usa un "copy_from_user" les dades del vector temporal cap a la zona de memoria especifica de la pantalla.

check_fd
--------
Accepta el fd == 10


JUSTIFICACIO
------------
Sabem que s'executen 18 ticks/segon.
Sabem que fps = frames_total / segon
Contem la quantitat de ticks en executar un bucle que pinta la pantalla simulant que:
    - Cada pintada == frame
    - Total iteracions = total_frames
Sapiguent els ticks que han passat (fent una resta) podem fer el calcul dels





#######################
### MILESTONE 4
#######################

IDEA
----
Tindre una cua, igual que readyqueue, per ficar els threads que estan pendents de rebre interrupcio rellotge.


NOVES ESTRUCTURES
-----------------

struct list_head tick_waitqueue
--------------------------------
S'afegeix aquesta estructura per a poder diferenciar el threads que estan "BLOCK" per causa de "WaitForTick"
Si no la tinguessim i nomes hi hagues una cua "blockedqueue" a l'hora d'iterarla i despertar els threads, hauriem de fer 
comprovacions i revisar per quin motiu es.


WORKFLOW
--------
1. Usuari crida la funcio de sistema desde un thread
2. Aquest thread s'afegeix a una cua especial
3. Cada vegada que hi hagi un tick, es revisa aquesta cua
4. Per a tots els threads en aquesta, passen a la cua de ready


IMPLEMENTACIO
-------------

WaitForTick
-----------
Handler simple.

sys_WaitForTick
---------------
Comprova que no estiguem en tractament de teclat.
Si no ho estem, fica el thread a la cua especial i forsa un canvi de context.

schedule
--------
Itera tota la cua de forma segura canviant els threads perque estiguin en la readyqueue.

init_sched
----------
Inicialitzada la nova cua.