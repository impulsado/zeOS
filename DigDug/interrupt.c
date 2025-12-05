/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>
#include <utils.h>
#include <mm.h>
#include <sched.h>
#include <zeos_interrupt.h>
#include <p_stats.h>

Gate idt[IDT_ENTRIES];
Register idtR;

char char_map[] =
    {
        '\0', '\0', '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '\'', '�', '\0', '\0',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '`', '+', '\0', '\0', 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', '�',
        '\0', '�', '\0', '�', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '-', '\0', '*',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '7',
        '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '\0', '\0', '\0', '<', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0'};

int zeos_ticks = 0;

void clock_routine()
{
  zeos_show_clock();
  zeos_ticks++;

  schedule();
}

int keyboard_routine(DWord *stack)
{
  struct task_struct *curr = current();
  struct keyboard_info *kbd = curr->kbd_info;
  unsigned char c = inb(0x60);

  // === BASE CASE ===
  if (kbd == NULL || kbd->handler == NULL)
    return 0;

  // Ja estem processant un event de teclat (evitar recursivitat)
  if (curr->in_keyboard_event)
    return 0;

  // === GENERAL CASE ===
  // Reservar el slot KBD_SLOT
  if (kbd_slot_alloc(curr) < 0)
    return 0;

  // Guardar context original
  struct keyboard_context *ctx = &curr->saved_ctx;
  for (int i = 0; i < 11; i++)
    ctx->saved_regs[i] = stack[i];
  ctx->eip = stack[11];
  ctx->cs = stack[12];
  ctx->eflags = stack[13];
  ctx->esp = stack[14];
  ctx->ss = stack[15];

  // Preparar la pila del slot reservat
  DWord *user_esp = (DWord *)(THREAD_STACK_SLOT_TOP_ADDR(KBD_SLOT));  // NOTA: Aqui no tenim TLS aixi que no fa falta 
  *(--user_esp) = (c & 0x80) ? 0 : 1;
  *(--user_esp) = (DWord)(c & 0x7F);
  *(--user_esp) = (DWord)kbd->handler;
  *(--user_esp) = 0;

  // Modificar la pila per saltar al wrapper
  stack[11] = (DWord)kbd->wrapper; // EIP
  stack[14] = (DWord)user_esp;     // ESP

  // Marcar que estem en event de teclat
  curr->in_keyboard_event = 1;
  return 1;
}

int keyboard_return_routine(DWord *stack)
{
  struct task_struct *curr = current();

  // === BASE CASE ===
  // No estem en gestio d'event de teclat
  if (!curr->in_keyboard_event)
    return 0;

  // === GENERAL CASE ===
  // Alliberar el slot KBD_SLOT
  kbd_slot_free(curr);

  // Restaurar el context original
  struct keyboard_context *ctx = &curr->saved_ctx;
  for (int i = 0; i < 11; i++)
    stack[i] = ctx->saved_regs[i];
  stack[11] = ctx->eip;
  stack[12] = ctx->cs;
  stack[13] = ctx->eflags;
  stack[14] = ctx->esp;
  stack[15] = ctx->ss;

  // Marcar que ja no estem en event de teclat
  curr->in_keyboard_event = 0;
  return 1;
}

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00; /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags = flags;
  idt[vector].highOffset = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  // flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00; /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags = flags;
  idt[vector].highOffset = highWord((DWord)handler);
}

void clock_handler();
void keyboard_handler();
void keyboard_return_handler();
void system_call_handler();
void page_fault_handler_new();

void setMSR(DWord msr_number, DWord high, DWord low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (DWord)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;

  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);
  // IMPO: Ficar el nou tractament
  setInterruptHandler(14, page_fault_handler_new, 0);
  // Handler per retornar del keyboard event (int 0x2b = 43)
  setTrapHandler(43, keyboard_return_handler, 3);

  setSysenter();

  set_idt_reg(&idtR);
}

void page_fault_routine_new(unsigned int fault_addr)
{
  struct task_struct *curr = current();
  unsigned int page = (fault_addr >> 12);

  // === BASE CASE
  // Si no te slot es que thread es de sistema i no deuria passar
  if (curr->slot_num == THREAD_STACK_SLOT_NONE)
    goto invalid_addr;

  // Comprovar si es dins del slot principal del thread
  unsigned int lower_page = get_slot_limit_page(curr->slot_num);
  unsigned int upper_page = get_slot_init_page(curr->slot_num);
  int valid = (lower_page <= page) && (page <= upper_page);

  // Comprovar si es dins del slot KBD_SLOT (si estem en keyboard event)
  if (!valid && curr->in_keyboard_event)
  {
    lower_page = get_slot_limit_page(KBD_SLOT);
    upper_page = get_slot_init_page(KBD_SLOT);
    valid = (lower_page <= page) && (page <= upper_page);
  }

  if (!valid)
    goto invalid_addr;

  // === GENERAL CASE
  // Direccio valida
  page_table_entry *process_PT = get_PT(curr);
  int frame = alloc_frame();
  if (frame < 0)
  {
    printk("ERROR! Run out of memory...");
    while (1)
      ;
  }

  // Assignar el frame
  set_ss_pag(process_PT, page, frame);

  // Fer flush TLB
  set_cr3(get_DIR(curr));

  return;

invalid_addr:
  printk("\nProcess generates a PAGE FAULT exception at address: 0x");

  char buff[9];
  itohex(fault_addr, buff);
  printk(buff);
  printk(" PID=0x");
  itohex(curr->PID, buff);
  printk(buff);

  printk(" TID=0x");
  itohex(curr->TID, buff);
  printk(buff);

  printk(" slot=0x");
  itohex(curr->slot_num, buff);
  printk(buff);

  printk(" page=0x");
  itohex(page, buff);
  printk(buff);

  printk("\n");

  while (1)
    ;
}
